#include <stdarg.h>
#include "gghlite.h"
#include "gghlite-internals.h"

void gghlite_enc_init(fmpz_mod_poly_t op, gghlite_pk_t self) {
  assert(!fmpz_is_zero(self->q));
  fmpz_mod_poly_init(op, self->q);
}

void gghlite_rerand(fmpz_mod_poly_t rop, gghlite_pk_t self, fmpz_mod_poly_t op, long k, flint_rand_t randstate) {
  assert(k <= self->kappa && 1<=k);
  assert(self->rerand_mask && (1ULL<<(k-1)));
  assert(!fmpz_mod_poly_is_zero(self->x[k-1][0]) && !fmpz_mod_poly_is_zero(self->x[k-1][1]));
  assert(self->D_sigma_s);

  fmpz_mod_poly_t tmp;
  gghlite_enc_init(tmp, self);

  fmpz_mod_poly_set(rop, op);

  for(long i=0; i<2; i++) {
    fmpz_mod_poly_sample_D(tmp, self->D_sigma_s, randstate);
    fmpz_mod_poly_mulmod(tmp, tmp, self->x[k-1][i], self->modulus);
    fmpz_mod_poly_add(rop, rop, tmp);
  }
  fmpz_mod_poly_clear(tmp);
}

void gghlite_elevate(fmpz_mod_poly_t rop, gghlite_pk_t self, fmpz_mod_poly_t op, long k, long kprime, int rerand, flint_rand_t randstate) {
  assert(kprime <= k);
  assert(k <= self->kappa);
  if (k>kprime) {
    fmpz_mod_poly_t yk;
    gghlite_enc_init(yk, self);
    fmpz_mod_poly_set(yk, self->y);
    fmpz_mod_poly_powmod_ui_binexp(yk, yk, k-kprime, self->modulus);
    fmpz_mod_poly_mulmod(rop, op, yk, self->modulus);
    fmpz_mod_poly_truncate(rop, self->n);
    fmpz_mod_poly_clear(yk);
  }
  if(rerand) {
    gghlite_rerand(rop, self, rop, k, randstate);
  }
}


void gghlite_sample(fmpz_mod_poly_t rop, gghlite_pk_t self, long k, flint_rand_t randstate) {
  assert(self->kappa);
  assert(k >=0 && k <= self->kappa);

  fmpz_mod_poly_sample_D(rop, self->D_sigma_p, randstate);
  if (k == 0)
    return;
  gghlite_elevate(rop, self, rop, k, 0, 1, randstate);
}

/**
   .. todo:

       pick l as suggested in GGHLite paper
*/

void gghlite_extract(fmpz_poly_t rop, gghlite_pk_t self, fmpz_mod_poly_t op) {
  fmpz_mod_poly_t t;
  gghlite_enc_init(t, self);
  fmpz_mod_poly_mulmod(t, self->pzt, op, self->modulus);
  fmpz_poly_set_fmpz_mod_poly(rop, t);
  fmpz_mod_poly_clear(t);

  long logq = fmpz_sizeinbase(self->q,2)-1;
  for(long i=0; i<fmpz_poly_length(rop); i++) {
    for(long j=0; j<logq-2*self->lambda; j++) {
      fmpz_clrbit(rop->coeffs + i, j);
    }
  }
}

void gghlite_print_params(const gghlite_pk_t self) {
  assert(self->lambda);
  assert(self->kappa);
  assert(self->n);
  assert(!fmpz_is_zero(self->q));

  const long lambda = self->lambda;
  const long kappa = self->kappa;
  const long n = self->n;
  printf("        λ: %7ld\n",lambda);
  printf("        k: %7ld\n",kappa);
  printf("        n: %7ld\n",n);
  printf("   log(q): %7ld (check: %d)\n", fmpz_sizeinbase(self->q, 2), gghlite_check_sec(self));
  printf("   log(σ): %7.1f dp: (%7.1f)\n", log2(mpfr_get_d(self->sigma,   MPFR_RNDN)), log2(_gghlite_sigma(n)));
  printf(" log(ℓ_g): %7.1f dp: (%7.1f)\n", log2(mpfr_get_d(self->ell_g,   MPFR_RNDN)), log2(_gghlite_ell_g(n)));
  printf("  log(σ'): %7.1f dp: (%7.1f)\n", log2(mpfr_get_d(self->sigma_p, MPFR_RNDN)), log2(_gghlite_sigma_p(n)));
  printf(" log(ℓ_b): %7.1f dp: (%7.1f)\n", log2(mpfr_get_d(self->ell_b,   MPFR_RNDN)), log2(_gghlite_ell_b(n)));
  printf(" log(σ^*): %7.1f dp: (%7.1f)\n", log2(mpfr_get_d(self->sigma_s, MPFR_RNDN)), log2(_gghlite_sigma_s(n, lambda, kappa)));


  mpfr_t enc;
  mpfr_init2(enc, _gghlite_prec(self));

  _gghlite_get_q_mpfr(enc, self, MPFR_RNDN);
  mpfr_log2(enc, enc, MPFR_RNDN);
  mpfr_mul_ui(enc, enc, n, MPFR_RNDN);

  printf("    |enc|: %7.1f MB\n",mpfr_get_d(enc, MPFR_RNDN)/8.0/1024.0/1024.0);

  mpfr_t par;
  mpfr_init2(par, _gghlite_prec(self));
  mpfr_set(par, enc, MPFR_RNDN);
  mpfr_mul_ui(par, par, self->kappa*2 + 1 + 1, MPFR_RNDN);
  printf("    |par|: %7.1f MB\n",mpfr_get_d(par, MPFR_RNDN)/8.0/1024.0/1024.0);
}

void gghlite_check_params(const gghlite_pk_t self) {
  mpfr_t tmp;
  mpfr_init2(tmp, _gghlite_prec(self));
  mpfr_set_ui(tmp, self->n, MPFR_RNDN);
  mpfr_mul_ui(tmp, tmp, 3, MPFR_RNDN);
  mpfr_mul(tmp, tmp, self->sigma_s, MPFR_RNDN);
  mpfr_mul(tmp, tmp, self->sigma_p, MPFR_RNDN);
  mpfr_pow_ui(tmp, tmp, 8*self->kappa, MPFR_RNDN);
  mpfr_log2(tmp, tmp, MPFR_RNDN);

  printf("(8κ)·log((m+1)·n^{3/2}·σ^*σ') ≤ log(q): %10.2f ≤ %7ld\n", mpfr_get_d(tmp, MPFR_RNDN), fmpz_sizeinbase(self->q, 2));
  printf("                      8/3·n/λ ≥ log(q): %10.2f ≥ %7ld\n", 8.0/3.0 * self->n / (double)self->lambda, fmpz_sizeinbase(self->q, 2));
}