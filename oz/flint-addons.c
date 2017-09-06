#include <omp.h>
#include "flint-addons.h"

/**
 * sets exp = base^n, where exp is an mpfr_t
 */
void fmpz_init_exp(fmpz_t exp, int base, int n) {
  fmpz_init(exp);
  fmpz_t tmp;
  fmpz_init_set_ui(tmp, base);
  fmpz_pow_ui(exp, tmp, n);
  fmpz_clear(tmp);
}

void fmpz_mat_mul_modp(fmpz_mat_t a, fmpz_mat_t b, fmpz_mat_t c, int n,
    fmpz_t p) {
  fmpz_mat_mul(a, b, c);
  for(int i = 0; i < n; i++) {
    for(int j = 0; j < n; j++) {
      fmpz_mod(fmpz_mat_entry(a, i, j), fmpz_mat_entry(a, i, j), p);
    }
  }
}

void fmpz_mat_modp(fmpz_mat_t m, int dim, fmpz_t p) {
  for(int i = 0; i < dim; i++) {
    for(int j = 0; j < dim; j++) {
      fmpz_mod(fmpz_mat_entry(m, i, j), fmpz_mat_entry(m, i, j), p);
    }
  }
}

/**
 * Code to find the inverse of a matrix, adapted from:
 * https://www.cs.rochester.edu/~brown/Crypto/assts/projects/adj.html
 */

// uses gaussian elimination to obtain the determinant of a matrix
void fmpz_mat_det_modp(fmpz_t det, fmpz_mat_t a, int n, fmpz_t p) {

  if(n == 1) {
    fmpz_set(det, fmpz_mat_entry(a, 0, 0));
    return;
  }

  if (n == 2) {
    fmpz_t tmp1;
    fmpz_init(tmp1);
    fmpz_mul(tmp1, fmpz_mat_entry(a,0,0), fmpz_mat_entry(a,1,1));
    fmpz_mod(tmp1, tmp1, p);
    fmpz_t tmp2;
    fmpz_init(tmp2);
    fmpz_mul(tmp2, fmpz_mat_entry(a,1,0), fmpz_mat_entry(a,0,1));
    fmpz_mod(tmp2, tmp2, p);
    fmpz_sub(det, tmp1, tmp2);
    fmpz_mod(det, det, p);
    fmpz_clear(tmp1);
    fmpz_clear(tmp2);
    return;
  }

  fmpz_mat_t m;
  fmpz_mat_init_set(m, a);

  fmpz_t tmp;
  fmpz_init(tmp);
  fmpz_t multfactor;
  fmpz_init(multfactor);

  int num_swaps = 0;

  for(int j = 0; j < n; j++) {
    for(int i = j+1; i < n; i++) {

      if(fmpz_is_zero(fmpz_mat_entry(m, j, j))) {
        // find first row that isn't a zero, and swap
        int was_swapped = 0;
        int h;
        for(h = j+1; h < n; h++) {
          if(fmpz_is_zero(fmpz_mat_entry(m, h, j))) {
            continue;
          }

          // swap row h with row j
          for(int k = 0; k < n; k++) {
            fmpz_set(tmp, fmpz_mat_entry(m, h, k));
            fmpz_set(fmpz_mat_entry(m, h, k), fmpz_mat_entry(m, j, k));
            fmpz_set(fmpz_mat_entry(m, j, k), tmp);
          }
          was_swapped = 1;
          break;
        }

        if(!was_swapped) {
          // matrix is not invertible!
          fmpz_set_ui(det, 0);
          fmpz_clear(multfactor);
          fmpz_clear(tmp);
          fmpz_mat_clear(m);
          return;
        }

        num_swaps++;
      }

      fmpz_invmod(multfactor, fmpz_mat_entry(m, j, j), p);
      fmpz_mul(multfactor, multfactor, fmpz_mat_entry(m, i, j));
      fmpz_mod(multfactor, multfactor, p);
      for(int k = j; k < n; k++) {
        fmpz_mul(tmp, fmpz_mat_entry(m, j, k), multfactor);
        fmpz_sub(fmpz_mat_entry(m, i, k), fmpz_mat_entry(m, i, k), tmp);
        fmpz_mod(fmpz_mat_entry(m, i, k), fmpz_mat_entry(m, i, k), p);
      }
    }
  }

  fmpz_clear(multfactor);
  fmpz_clear(tmp);

  fmpz_set_ui(det, 1);

  for(int j = 0; j < n; j++) {
    fmpz_mul(det, det, fmpz_mat_entry(m, j, j));
  }
  if(num_swaps % 2 == 1) {
    fmpz_neg(det, det);
  }
  fmpz_mod(det, det, p);
  fmpz_mat_clear(m);
}

/*
   Find the cofactor matrix of a square matrix
*/
void fmpz_mat_cofactor_modp(fmpz_mat_t b, fmpz_mat_t a, int n, fmpz_t p) {
  int i,j,ii,jj,i1,j1;

  fmpz_t det;
  fmpz_init(det);

  fmpz_mat_t c;
  fmpz_mat_init(c, n-1, n-1);

  for (j=0;j<n;j++) {
    for (i=0;i<n;i++) {
      /* Form the adjoint a_ij */
      i1 = 0;
      for (ii=0;ii<n;ii++) {
        if (ii == i)
          continue;
        j1 = 0;
        for (jj=0;jj<n;jj++) {
          if (jj == j)
            continue;
          fmpz_set(fmpz_mat_entry(c, i1, j1), fmpz_mat_entry(a, ii, jj));
          j1++;
        }
        i1++;
      }

      /* Calculate the determinant */
      /* TODO: check if the determinant is zero */
      fmpz_mat_det_modp(det, c, n-1, p);

      /* Fill in the elements of the cofactor */
      if((i+j) % 2 == 1) {
        fmpz_negmod(det, det, p);
      }
      fmpz_mod(det, det, p);
      fmpz_set(fmpz_mat_entry(b, i, j), det);
    }
  }

  fmpz_clear(det);
  fmpz_mat_clear(c);
}

int fmpz_modp_matrix_inverse(fmpz_mat_t inv, fmpz_mat_t a, int dim, fmpz_t p) {
  fmpz_t det;
  fmpz_init(det);
  /* TODO: check if the determinant is zero */
  fmpz_mat_det_modp(det, a, dim, p);
  if(0 == fmpz_sgn(det)) {
    fmpz_clear(det);
    return 1;
  }
  fmpz_mat_t cofactor;
  fmpz_mat_init(cofactor, dim, dim);
  fmpz_mat_cofactor_modp(cofactor, a, dim, p);

  fmpz_t invmod;
  fmpz_init(invmod);
  fmpz_invmod(invmod, det, p);
  fmpz_t tmp;
  fmpz_init(tmp);
  for(int i = 0; i < dim; i++) {
    for(int j = 0; j < dim; j++) {
      fmpz_mod(tmp, fmpz_mat_entry(cofactor, j, i), p);
      fmpz_mul(tmp, tmp, invmod);
      fmpz_mod(tmp, tmp, p);
      fmpz_set(fmpz_mat_entry(inv,i,j), tmp);
    }
  }
  fmpz_clear(tmp);
  fmpz_clear(invmod);

  fmpz_clear(det);
  fmpz_mat_clear(cofactor);
  return 0;
}

void _fmpz_vec_eucl_norm_mpfr(mpfr_t rop, const fmpz *vec, const long len, const mpfr_rnd_t rnd) {
  fmpz_t tmp;
  fmpz_init(tmp);
  fmpz_zero(tmp);
  for (long i = 0; i < len; i++)
    fmpz_addmul(tmp, vec + i, vec + i);
  mpz_t tmp_g;
  mpz_init(tmp_g);
  fmpz_get_mpz(tmp_g, tmp);
  fmpz_clear(tmp);

  mpfr_set_z(rop, tmp_g, rnd);
  mpfr_sqrt(rop, rop, rnd);
  mpz_clear(tmp_g);
}

void _fmpq_vec_eucl_norm_mpfr(mpfr_t rop, const fmpz *num, const fmpz_t den, const long len, const mpfr_rnd_t rnd) {
  fmpz_t acc_num;
  fmpz_init(acc_num);
  fmpz_zero(acc_num);
  for (long i = 0; i < len; i++)
    fmpz_addmul(acc_num, num + i, num + i);

  fmpz_t acc_den;
  fmpz_init(acc_den);
  fmpz_mul(acc_den, den, den);
  fmpz_mul_ui(acc_den, acc_den, len);

  fmpq_t acc;
  fmpq_init(acc);

  fmpq_set_fmpz_frac(acc, acc_num, acc_den);
  fmpq_get_mpfr(rop, acc, rnd);
  mpfr_sqrt(rop, rop, rnd);

  fmpq_clear(acc);
  fmpz_clear(acc_den);
  fmpz_clear(acc_num);
}

void fmpq_poly_truncate_prec(fmpq_poly_t op, const mp_bitcnt_t prec) {
  mpq_t *tmp_q = (mpq_t*)calloc(fmpq_poly_length(op), sizeof(mpq_t));
  mpf_t tmp_f; mpf_init2(tmp_f, prec);

  for (int i=0; i<fmpq_poly_length(op); i ++) {
    mpq_init(tmp_q[i]);
    fmpq_poly_get_coeff_mpq(tmp_q[i], op, i);
    mpf_set_q(tmp_f, tmp_q[i]);
    mpq_set_f(tmp_q[i], tmp_f);
  }
  fmpq_poly_set_array_mpq(op, (const mpq_t*)tmp_q, fmpq_poly_length(op));

  mpf_clear(tmp_f);
  for (int i=0; i<fmpq_poly_length(op); i ++)
    mpq_clear(tmp_q[i]);
  free(tmp_q);
}

void _fmpz_poly_resultant_modular_bound(fmpz_t res, const fmpz * poly1, const slong len1,
                                        const fmpz * poly2, const slong len2, const mp_bitcnt_t bound) {
  mp_bitcnt_t pbits;
  slong i, num_primes;
  fmpz_comb_t comb;
  fmpz_comb_temp_t comb_temp;
  fmpz_t ac, bc, l;
  fmpz * A, * B, * lead_A, * lead_B;

  /* special case, one of the polys is a constant */
  if (len2 == 1) /* if len1 == 1 then so does len2 */ {
    fmpz_pow_ui(res, poly2, len1 - 1);
    return;
  }

  fmpz_init(ac);
  fmpz_init(bc);

  /* compute content of poly1 and poly2 */
  _fmpz_vec_content(ac, poly1, len1);
  _fmpz_vec_content(bc, poly2, len2);

  /* divide poly1 and poly2 by their content */
  A = _fmpz_vec_init(len1);
  B = _fmpz_vec_init(len2);
  _fmpz_vec_scalar_divexact_fmpz(A, poly1, len1, ac);
  _fmpz_vec_scalar_divexact_fmpz(B, poly2, len2, bc);

  /* get product of leading coefficients */
  fmpz_init(l);

  lead_A = A + len1 - 1;
  lead_B = B + len2 - 1;
  fmpz_mul(l, lead_A, lead_B);

  /* set size of first prime */
  pbits = FLINT_BITS -1;

  num_primes = (bound + pbits - 1)/pbits;
  mp_ptr parr = _nmod_vec_init(num_primes);
  mp_ptr rarr = _nmod_vec_init(num_primes);

  fmpz_zero(res);

  /* make space for polynomials mod p */

  const int num_threads = omp_get_max_threads();

  mp_ptr a[num_threads];
  mp_ptr b[num_threads];

  for(i=0; i<num_threads; i++) {
    a[i] = _nmod_vec_init(len1);
    b[i] = _nmod_vec_init(len2);
  }

  mp_limb_t p = (UWORD(1)<<pbits);
  for(i=0; i<num_primes;) {
    p = n_prevprime(p, 0);
    if (fmpz_fdiv_ui(l, p) == 0)
      continue;
    parr[i++] = p;
  }

#pragma omp parallel for
  for (i = 0; i<num_primes; i++) {
    nmod_t mod;
    nmod_init(&mod, parr[i]);

    const int id = omp_get_thread_num();
    /* reduce polynomials modulo p */
    _fmpz_vec_get_nmod_vec(a[id], A, len1, mod);
    _fmpz_vec_get_nmod_vec(b[id], B, len2, mod);
    /* compute resultant over Z/pZ */
    rarr[i] = _nmod_poly_resultant(a[id], len1, b[id], len2, mod);
  }

  fmpz_comb_init(comb, parr, num_primes);
  fmpz_comb_temp_init(comb_temp, comb);

  fmpz_multi_CRT_ui(res, rarr, comb, comb_temp, 1);

  fmpz_comb_temp_clear(comb_temp);
  fmpz_comb_clear(comb);

  for(i=0; i<num_threads; i++) {
    _nmod_vec_clear(a[i]);
    _nmod_vec_clear(b[i]);
  }

  _nmod_vec_clear(parr);
  _nmod_vec_clear(rarr);

  /* finally multiply by powers of content */
  if (!fmpz_is_one(ac)) {
    fmpz_pow_ui(l, ac, len2 - 1);
    fmpz_mul(res, res, l);
  }

  if (!fmpz_is_one(bc)) {
    fmpz_pow_ui(l, bc, len1 - 1);
    fmpz_mul(res, res, l);
  }

  fmpz_clear(l);

  _fmpz_vec_clear(A, len1);
  _fmpz_vec_clear(B, len2);

  fmpz_clear(ac);
  fmpz_clear(bc);
}

void fmpz_poly_resultant_modular_bound(fmpz_t res, const fmpz_poly_t poly1,
                                       const fmpz_poly_t poly2, const mp_bitcnt_t bound) {
  slong len1 = poly1->length;
  slong len2 = poly2->length;

  if (len1 == 0 || len2 == 0)
    fmpz_zero(res);
  else if (len1 >= len2)
    _fmpz_poly_resultant_modular_bound(res, poly1->coeffs, len1, poly2->coeffs, len2, bound);
  else {
    _fmpz_poly_resultant_modular_bound(res, poly2->coeffs, len2, poly1->coeffs, len1, bound);
    if ((len1 > 1) && (!(len1 & WORD(1)) & !(len2 & WORD(1))))
      fmpz_neg(res, res);
  }
}

static void _fmpq_poly_resultant_modular_bound(fmpz_t rnum, fmpz_t rden,
                                  const fmpz *poly1, const fmpz_t den1, slong len1,
                                  const fmpz *poly2, const fmpz_t den2, slong len2,
                                  const mp_bitcnt_t bound) {
  if (len2 == 1)  {
    if (len1 == 1) {
      fmpz_one(rnum);
      fmpz_one(rden);
    } else if (len1 == 2)  {
      fmpz_set(rnum, poly2);
      fmpz_set(rden, den2);
    } else {
      fmpz_pow_ui(rnum, poly2, len1 - 1);
      if (fmpz_is_one(den2)) {
        fmpz_one(rden);
      } else {
        fmpz_pow_ui(rden, den2, len1 - 1);
      }
    }
  } else { /* len1 >= len2 >= 2 */
    fmpz_t c1, c2;
    fmpz *prim1, *prim2, *g;
    slong lenG = len2;

    fmpz_init(c1);
    fmpz_init(c2);

    _fmpz_vec_content(c1, poly1, len1);
    _fmpz_vec_content(c2, poly2, len2);

    prim1 = _fmpz_vec_init(len1);
    prim2 = _fmpz_vec_init(len2);
    g     = _fmpz_vec_init(len2);

    _fmpz_vec_scalar_divexact_fmpz(prim1, poly1, len1, c1);
    _fmpz_vec_scalar_divexact_fmpz(prim2, poly2, len2, c2);

    _fmpz_poly_gcd(g, prim1, len1, prim2, len2);
    FMPZ_VEC_NORM(g, lenG);

    if (lenG > 1) {
      fmpz_zero(rnum);
      fmpz_one(rden);
    }  else { /* prim1, prim2 are coprime */
      fmpz_t t;
      fmpz_init(t);
      _fmpz_poly_resultant_modular_bound(rnum, prim1, len1, prim2, len2, bound);

      if (!fmpz_is_one(c1)) {
        fmpz_pow_ui(t, c1, len2 - 1);
        fmpz_mul(rnum, rnum, t);
      }
      if (!fmpz_is_one(c2)) {
        fmpz_pow_ui(t, c2, len1 - 1);
        fmpz_mul(rnum, rnum, t);
      }

      if (fmpz_is_one(den1)) {
        if (fmpz_is_one(den2))
          fmpz_one(rden);
        else
          fmpz_pow_ui(rden, den2, len1 - 1);
      } else {
        if (fmpz_is_one(den2))
          fmpz_pow_ui(rden, den1, len2 - 1);
        else {
          fmpz_pow_ui(rden, den1, len2 - 1);
          fmpz_pow_ui(t,    den2, len1 - 1);
          fmpz_mul(rden, rden, t);
        }
      }
      _fmpq_canonicalise(rnum, rden);
      fmpz_clear(t);
    }

    fmpz_clear(c1);
    fmpz_clear(c2);
    _fmpz_vec_clear(prim1, len1);
    _fmpz_vec_clear(prim2, len2);
    _fmpz_vec_clear(g, len2);
  }
}

void fmpq_poly_resultant_modular_bound(fmpq_t r, const fmpq_poly_t f, const fmpq_poly_t g, const mp_bitcnt_t bound) {
  const slong len1 = f->length;
  const slong len2 = g->length;

  if (len1 == 0 || len2 == 0)   {
    fmpq_zero(r);
  }  else {
    if (len1 >= len2) {
      _fmpq_poly_resultant_modular_bound(fmpq_numref(r), fmpq_denref(r),
                                         f->coeffs, f->den, len1,
                                         g->coeffs, g->den, len2,
                                         bound);
    } else  {
      _fmpq_poly_resultant_modular_bound(fmpq_numref(r), fmpq_denref(r),
                                         g->coeffs, g->den, len2,
                                         f->coeffs, f->den, len1,
                                         bound);

      if (((len1 | len2) & WORD(1)) == WORD(0))
        fmpq_neg(r, r);
    }
  }
}
