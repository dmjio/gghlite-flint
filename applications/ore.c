#include "ore.h"

#define CHECK(x) if(x < 0) { assert(0); }

int main(int argc, char *argv[]) {

  run_tests();
  exit(0);
  
  mife_challenge_gen(argc, argv);
  exit(0);

  cmdline_params_t cmdline_params;

  const char *name =  "Order Revealing Encryption";
  parse_cmdline(cmdline_params, argc, argv, name, NULL);

  int lambda = 80;
  int num_messages = 20;

  /* message space size will be base_d ^ base_n */
  int base_d = 10;
  int base_n = 10;

  mife_pp_t pp;

  fmpz_t message_space_size;
  fmpz_init_exp(message_space_size, base_d, base_n);
  ore_set_best_params(pp, lambda, message_space_size);

  printf("Using SHA256 seed: %s\n", cmdline_params->shaseed);  

  /* Generate the messages first with a copied randstate */
  flint_rand_t gen_randstate;
  flint_randinit_seed_crypto(gen_randstate, cmdline_params->shaseed, 0);
  fmpz_t *messages = malloc(num_messages * sizeof(fmpz_t));
  printf("The plaintexts:\n");
  for(int i = 0; i < num_messages; i++) {
    fmpz_init(messages[i]);
    fmpz_randm(messages[i], gen_randstate, message_space_size);
    printf("%lu\n", fmpz_get_ui(messages[i]));
  }
  fmpz_clear(message_space_size);
  flint_randclear(gen_randstate);
  printf("\n");
}

void mife_challenge_gen(int argc, char *argv[]) {
  cmdline_params_t cmdline_params;

  const char *name =  "Order Revealing Encryption";
  parse_cmdline(cmdline_params, argc, argv, name, NULL);

  int L = 80; // 2^L = # of total messages we can encrypt
  int lambda = cmdline_params->lambda; // security parameter
  int num_messages = 20;

  /* message space size will be base_d ^ base_n */
  int base_d = 10;
  int base_n = 10;

  mife_pp_t pp;
  mife_sk_t sk;

  fmpz_t message_space_size;
  fmpz_init_exp(message_space_size, base_d, base_n);
  ore_set_best_params(pp, lambda, message_space_size);

  /* Read in the messages */
  fmpz_t *messages = malloc(num_messages * sizeof(fmpz_t));
  FILE *fp = fopen("plaintexts.secret", "r");
  for(int i = 0; i < num_messages; i++) {
    unsigned long m;
    CHECK(fscanf(fp, "%lu\n", &m));
    fmpz_init_set_ui(messages[i], m);
  }
  fclose(fp);
  fmpz_clear(message_space_size);


  printf("Estimated ciphertext size for lambda = %d: ", lambda);
  const char *units[3] = {"KB","MB","GB"};
  double sd = pp->ct_size / 8.0;
  int i;
  for(i=0; i<3; i++) {
    if (sd < 1024.0)
      break;
    sd = sd/1024;
  }
  printf("%6.2f %s\n\n", sd, units[i-1]);

  mife_mbp_init(pp, 2, &ore_mbp_param, &ore_mbp_kilian, &ore_mbp_ordering,
     &ore_mbp_set_matrices, &ore_mbp_parse);

  gghlite_flag_t ggh_flags = GGHLITE_FLAGS_DEFAULT | GGHLITE_FLAGS_GOOD_G_INV;
  T = ggh_walltime(0);
  mife_setup(pp, sk, L, lambda, ggh_flags, cmdline_params->shaseed);

  int ci = cmdline_params->challenge_index;
 
  PRINT_ENCODING_PROGRESS = 1;
  ore_ciphertext_t *ciphertexts =
    malloc(num_messages * sizeof(ore_ciphertext_t));
  for(int i = 0; i < num_messages; i++) { 
    if(ci == 0 || i == ci-1) {
      printf("Encrypting message %d\n", i+1);
      NUM_ENCODINGS_GENERATED = 0;
      T = ggh_walltime(0);
      mife_encrypt(ciphertexts[i], messages[i], pp, sk);
      char *str = malloc(10 * sizeof(char));
      sprintf(str, "ct%d.out", i+1);
      fwrite_ore_ciphertext(pp, ciphertexts[i], str);
      free(str);
      ore_ciphertext_clear(pp, ciphertexts[i]);

    }
  }
  PRINT_ENCODING_PROGRESS = 0;

  fwrite_ore_pp(pp, "pp.out");

  free(ciphertexts);
  free(messages);

  mife_clear_pp(pp);
  mife_clear_sk(sk);
  mpfr_free_cache();
  flint_cleanup();
}

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

void generate_challenge(ore_ciphertext_t *ciphertexts, fmpz_t *messages,
    mife_pp_t pp, mife_sk_t sk, int num_messages, int base_d, int base_n) {
  int lambda = 80;
  int L = 80;

  // determine total message space size
  fmpz_t message_space_size;
  mpfr_t nt, dt, total;
  mpfr_init(total);
  mpfr_init_set_ui(dt, base_d, MPFR_RNDN);
  mpfr_init_set_ui(nt, base_n, MPFR_RNDN);
  mpfr_pow(total, dt, nt, MPFR_RNDN);  
  ore_set_best_params(pp, lambda, message_space_size);
  mpfr_clear(dt);
  mpfr_clear(nt);
  mpfr_clear(total);

  gghlite_flag_t ggh_flags = GGHLITE_FLAGS_QUIET | GGHLITE_FLAGS_GOOD_G_INV;
  mife_setup(pp, sk, L, lambda, ggh_flags, DEFAULT_SHA_SEED);

  for(int i = 0; i < num_messages; i++) {
    fmpz_randm(messages[i], sk->randstate, message_space_size);
  }

  for(int i = 0; i < num_messages; i++) {
    mife_encrypt(ciphertexts[i], messages[i], pp, sk);
  }

}

void flint_randinit_seed_crypto(flint_rand_t randstate,
    char *seed, int gmp) {
  flint_randinit(randstate);
  int cutoff = sizeof(long)  * 2;
  int base = 16;
  char *str_seed1 = malloc(sizeof(char) * (cutoff+1));
  char *str_seed2 = malloc(sizeof(char) * (cutoff+1));
  strncpy(str_seed1, seed, (cutoff+1));
  strncpy(str_seed2, seed+32, (cutoff+1));
  str_seed1[cutoff] = 0x0;
  str_seed2[cutoff] = 0x0;
  unsigned long seed1 = strtoul(str_seed1, NULL, base);
  unsigned long seed2 = strtoul(str_seed2, NULL, base);
  free(str_seed1);
  free(str_seed2);
  flint_randseed(randstate, seed1, seed2);
  if (gmp) {
    mpfr_t mpfr_seed;
    mpfr_init(mpfr_seed);
    mpfr_ui_pow_ui(mpfr_seed, 2, 64, MPFR_RNDN);
    mpfr_mul_2ui(mpfr_seed, mpfr_seed, seed1, MPFR_RNDN);
    mpfr_add_ui(mpfr_seed, mpfr_seed, seed2, MPFR_RNDN);
    mpz_t mpz_seed;
    mpz_init(mpz_seed);
    mpfr_get_z(mpz_seed, mpfr_seed, MPFR_RNDN);
    _flint_rand_init_gmp(randstate);
    gmp_randseed(randstate->gmp_state, mpz_seed);
    mpfr_clear(mpfr_seed);
    mpz_clear(mpz_seed);
  }
}



long dc_enc_size(int n) {
  int kappa = n+1;
  if(kappa < 2 || kappa > MAX_KAPPA_BENCH) {
    return -1;
  }
  return KAPPA_BENCH[kappa];
}

long mc_enc_size(int n) {
  int kappa = 2*n;
  if(kappa < 2 || kappa > MAX_KAPPA_BENCH) {
    return -1;
  }
  return KAPPA_BENCH[kappa];
}

int dc_num_enc(int d, int n) {
  return d*d*(n-1) + (d+1)*(4*n-2);
}

int mc_num_enc(int d, int n) {
  return 6*(n-1)*(d+2) + 4*d;
}

/**
 * The message space size is d^n.
 */
void ore_set_best_params(mife_pp_t pp, int lambda, fmpz_t message_space_size) {
  int max_base = 5;

  mpfr_t total;
  mpfr_init_set_ui(total, fmpz_get_ui(message_space_size), MPFR_RNDN);
  
  mpfr_t tmp1;
  mpfr_t tmp2;
  mpfr_init(tmp1);
  mpfr_init(tmp2);
  mpfr_t bt;
  mpfr_init(bt);

  long dc_vals[max_base+1];
  long mc_vals[max_base+1];
  long nmap[max_base+1];

  for(int base = 2; base <= max_base; base++) {
    // compute minimum n
    mpfr_set_ui(bt, base, MPFR_RNDN);
    mpfr_log(tmp1, total, MPFR_RNDN);
    mpfr_log(tmp2, bt, MPFR_RNDN);
    mpfr_div(tmp1, tmp1, tmp2, MPFR_RNDN);
    mpfr_ceil(tmp1, tmp1);
    unsigned long n = mpfr_get_ui(tmp1, MPFR_RNDN);
    nmap[base] = n;
    
    // num encodings #1, #2
    int dc_enc = dc_num_enc(base,n);
    int mc_enc = mc_num_enc(base,n);
   
    // use n to compute enc size
    dc_vals[base] = dc_enc * dc_enc_size(n);
    mc_vals[base] = mc_enc * mc_enc_size(n);
  }

  mpfr_clear(bt);
  mpfr_clear(tmp1);
  mpfr_clear(tmp2);
  mpfr_clear(total);

  long min_enc = dc_vals[5]; 
  int min_type = 0;
  int min_base;
  int i = 2;
  for(; i <= max_base; i++) {
    if(dc_vals[i] > 0) {
      min_type = 0;
      min_enc = dc_vals[i];
      break;
    }
    if(mc_vals[i] > 0) {
      min_type = 1;
      min_enc = mc_vals[i];
      break;
    }
  }
  min_base = i;

  for(; i <= max_base; i++) {
    if(dc_vals[i] > 0 && dc_vals[i] < min_enc) {
      min_type = 0;
      min_base = i;
      min_enc = dc_vals[i];
    }
    if(mc_vals[i] > 0 && mc_vals[i] < min_enc) {
      min_type = 1;
      min_base = i;
      min_enc = mc_vals[i];
    }
  }

  if(min_type == 1) {
    ORE_GLOBAL_FLAGS = ORE_MBP_DC;
  } else {
    ORE_GLOBAL_FLAGS = ORE_MBP_MC;
  }

  mife_init_params(pp, min_base, nmap[min_base], MIFE_DEFAULT);

  if(ORE_GLOBAL_FLAGS & ORE_MBP_DC) {
    pp->num_enc = dc_num_enc(pp->d, pp->bitstr_len);
  } else if(ORE_GLOBAL_FLAGS & ORE_MBP_MC) {
    pp->num_enc = mc_num_enc(pp->d, pp->bitstr_len);
  }
  pp->ct_size = min_enc;
}

void mife_init_params(mife_pp_t pp, int d, int bitstr_len, mife_flag_t flags) {
  pp->flags = flags;
  pp->d = d; 
  pp->bitstr_len = bitstr_len;
  // FIXME get rid of these, they are not useful
  pp->ct_size = -1;
  pp->num_enc = -1;
}

void gghlite_params_clear_read(gghlite_params_t self) {
  for(int i = 0; i < self->gamma; i++) {
    for(int j = 0; j < KAPPA; j++) {
      free(self->x[i][j]);
    }
    free(self->x[i]);
  }
  free(self->x);
  free(self->y);

  fmpz_mod_poly_clear(self->pzt);
  mpfr_clear(self->xi);
  mpfr_clear(self->sigma_s);
  mpfr_clear(self->ell_b);
  mpfr_clear(self->sigma_p);
  mpfr_clear(self->ell_g);
  mpfr_clear(self->sigma);
  fmpz_mod_poly_oz_ntt_precomp_clear(self->ntt);
  fmpz_clear(self->q);
  free(self);
}

/**
 * 
 * Members of pp that are not currently transferred:
 * params->x
 * params->y
 * params->D_sigma_p
 * params->D_sigma_s
 */
void fwrite_ore_pp(mife_pp_t pp, char *filepath) {
  int mpfr_base = 10;
  FILE *fp = fopen(filepath, "w");
  fprintf(fp, "%d %d %d %d %d %d %d %d\n",
    pp->num_inputs,
    pp->bitstr_len,
    pp->d,
    pp->L,
    pp->kappa,
    pp->numR,
    pp->flags,
    pp->num_enc
  );
  for(int i = 0; i < pp->num_inputs; i++) {
    fprintf(fp, "%d ", pp->n[i]);
  }
  fprintf(fp, "\n");
  for(int i = 0; i < pp->num_inputs; i++) {
    fprintf(fp, "%d ", pp->gammas[i]);
  }
  fprintf(fp, "\n");
  fmpz_fprint(fp, pp->p);
  fprintf(fp, "\n");
  fprintf(fp, "%zd %zd %zd %ld %ld %lu %d\n",
    (*pp->params_ref)->lambda,
    (*pp->params_ref)->gamma,
    (*pp->params_ref)->kappa,
    (*pp->params_ref)->n,
    (*pp->params_ref)->ell,
    (*pp->params_ref)->rerand_mask,
    (*pp->params_ref)->flags
  );
  fmpz_fprint(fp, (*pp->params_ref)->q);
  fprintf(fp, "\n");
  mpfr_out_str(fp, mpfr_base, 0, (*pp->params_ref)->sigma, MPFR_RNDN);
  fprintf(fp, "\n");
  mpfr_out_str(fp, mpfr_base, 0, (*pp->params_ref)->sigma_p, MPFR_RNDN);
  fprintf(fp, "\n");
  mpfr_out_str(fp, mpfr_base, 0, (*pp->params_ref)->sigma_s, MPFR_RNDN);
  fprintf(fp, "\n");
  mpfr_out_str(fp, mpfr_base, 0, (*pp->params_ref)->ell_b, MPFR_RNDN);
  fprintf(fp, "\n");
  mpfr_out_str(fp, mpfr_base, 0, (*pp->params_ref)->ell_g, MPFR_RNDN);
  fprintf(fp, "\n");
  mpfr_out_str(fp, mpfr_base, 0, (*pp->params_ref)->xi, MPFR_RNDN);
  fprintf(fp, "\n");
  gghlite_enc_fprint(fp, (*pp->params_ref)->pzt);
  fprintf(fp, "\n");
  fprintf(fp, "%zd\n", (*pp->params_ref)->ntt->n);
  fmpz_mod_poly_fprint(fp, (*pp->params_ref)->ntt->w);
  fprintf(fp, "\n");
  fmpz_mod_poly_fprint(fp, (*pp->params_ref)->ntt->w_inv);
  fprintf(fp, "\n");
  fmpz_mod_poly_fprint(fp, (*pp->params_ref)->ntt->phi);
  fprintf(fp, "\n");
  fmpz_mod_poly_fprint(fp, (*pp->params_ref)->ntt->phi_inv);
  fclose(fp);
}

void fread_ore_pp(mife_pp_t pp, char *filepath) {
  int mpfr_base = 10;
  FILE *fp = fopen(filepath, "r");
  int flag_int;
  CHECK(fscanf(fp, "%d %d %d %d %d %d %d %d\n",
    &pp->num_inputs,
    &pp->bitstr_len,
    &pp->d,
    &pp->L,
    &pp->kappa,
    &pp->numR,
    &flag_int,
    &pp->num_enc
  ));
  for(int i = 0; i < pp->num_inputs; i++) {
    CHECK(fscanf(fp, "%d ", &pp->n[i]));
  }
  CHECK(fscanf(fp, "\n"));
  for(int i = 0; i < pp->num_inputs; i++) {
    CHECK(fscanf(fp, "%d ", &pp->gammas[i]));
  }
  CHECK(fscanf(fp, "\n"));
  pp->flags = flag_int;
  fmpz_init(pp->p);
  fmpz_fread(fp, pp->p);
  CHECK(fscanf(fp, "\n"));

  pp->params_ref = malloc(sizeof(gghlite_params_t));
  size_t lambda, kappa, gamma, n, ell;
  uint64_t rerand_mask;
  int gghlite_flag_int;
  CHECK(fscanf(fp, "%zd %zd %zd %ld %ld %lu %d\n",
    &lambda,
    &gamma,
    &kappa,
    &n,
    &ell,
    &rerand_mask,
    &gghlite_flag_int
  ));
  
  gghlite_params_initzero(*pp->params_ref, lambda, kappa, gamma);
  (*pp->params_ref)->n = n;
  (*pp->params_ref)->ell = ell;
  (*pp->params_ref)->rerand_mask = rerand_mask;
  (*pp->params_ref)->flags = gghlite_flag_int;

  fmpz_fread(fp, (*pp->params_ref)->q);
  CHECK(fscanf(fp, "\n"));
  mpfr_inp_str((*pp->params_ref)->sigma, fp, mpfr_base, MPFR_RNDN);
  CHECK(fscanf(fp, "\n"));
  mpfr_inp_str((*pp->params_ref)->sigma_p, fp, mpfr_base, MPFR_RNDN);
  CHECK(fscanf(fp, "\n"));
  mpfr_inp_str((*pp->params_ref)->sigma_s, fp, mpfr_base, MPFR_RNDN);
  CHECK(fscanf(fp, "\n"));
  mpfr_inp_str((*pp->params_ref)->ell_b, fp, mpfr_base, MPFR_RNDN);
  CHECK(fscanf(fp, "\n"));
  mpfr_inp_str((*pp->params_ref)->ell_g, fp, mpfr_base, MPFR_RNDN);
  CHECK(fscanf(fp, "\n"));
  mpfr_inp_str((*pp->params_ref)->xi, fp, mpfr_base, MPFR_RNDN);
  CHECK(fscanf(fp, "\n"));
  
  gghlite_enc_init((*pp->params_ref)->pzt, *pp->params_ref);
  gghlite_enc_init((*pp->params_ref)->ntt->w, *pp->params_ref);
  gghlite_enc_init((*pp->params_ref)->ntt->w_inv, *pp->params_ref);
  gghlite_enc_init((*pp->params_ref)->ntt->phi, *pp->params_ref);
  gghlite_enc_init((*pp->params_ref)->ntt->phi_inv, *pp->params_ref);

  gghlite_enc_fread(fp, (*pp->params_ref)->pzt);
  CHECK(fscanf(fp, "\n"));
  CHECK(fscanf(fp, "%zd\n", &(*pp->params_ref)->ntt->n));
  gghlite_enc_fread(fp, (*pp->params_ref)->ntt->w);
  CHECK(fscanf(fp, "\n"));
  gghlite_enc_fread(fp, (*pp->params_ref)->ntt->w_inv);
  CHECK(fscanf(fp, "\n"));
  gghlite_enc_fread(fp, (*pp->params_ref)->ntt->phi);
  CHECK(fscanf(fp, "\n"));
  gghlite_enc_fread(fp, (*pp->params_ref)->ntt->phi_inv);
  fclose(fp);
}

void fwrite_ore_ciphertext(mife_pp_t pp, ore_ciphertext_t ct, char *filepath) {
  FILE *fp = fopen(filepath, "w");
  for(int i = 0; i < pp->num_inputs; i++) {
    for(int j = 0; j < pp->n[i]; j++) {
      fwrite_gghlite_enc_mat(pp, ct->enc[i][j], fp);
    }
  }
  fclose(fp);
}

void fwrite_gghlite_enc_mat(mife_pp_t pp, gghlite_enc_mat_t m, FILE *fp) {
  fprintf(fp, " %d ", m->nrows);
  fprintf(fp, " %d ", m->ncols);
  for(int i = 0; i < m->nrows; i++) {
    for(int j = 0; j < m->ncols; j++) {
      gghlite_enc_fprint(fp, m->m[i][j]);
      fprintf(fp, "\n");
    }
  }
}

void fread_ore_ciphertext(mife_pp_t pp, ore_ciphertext_t ct, char *filepath) {
  FILE *fp = fopen(filepath, "r");

  ct->enc = malloc(pp->num_inputs * sizeof(gghlite_enc_mat_t *));
  for(int i = 0; i < pp->num_inputs; i++) {
    ct->enc[i] = malloc(pp->n[i] * sizeof(gghlite_enc_mat_t));
    for(int j = 0; j < pp->n[i]; j++) {
      fread_gghlite_enc_mat(pp, ct->enc[i][j], fp);
    }
  }
  fclose(fp);
}

void fread_gghlite_enc_mat(mife_pp_t pp, gghlite_enc_mat_t m, FILE *fp) {
  int check1 = fscanf(fp, " %d ", &m->nrows);
  int check2 = fscanf(fp, " %d ", &m->ncols);
  assert(check1 == 1 && check2 == 1);
  m->m = malloc(m->nrows * sizeof(gghlite_enc_t *));
  assert(m->m);
  for(int i = 0; i < m->nrows; i++) {
    m->m[i] = malloc(m->ncols * sizeof(gghlite_enc_t));
    assert(m->m[i]);
    for(int j = 0; j < m->ncols; j++) {
      gghlite_enc_init(m->m[i][j], *pp->params_ref);
      int check3 = gghlite_enc_fread(fp, m->m[i][j]);
      assert(check3 == 1);
      CHECK(fscanf(fp, "\n"));
    }
  }
}

void ore_ciphertext_clear(mife_pp_t pp, ore_ciphertext_t ct) {
  for(int i = 0; i < pp->num_inputs; i++) {
    for(int j = 0; j < pp->n[i]; j++) {
      gghlite_enc_mat_clear(ct->enc[i][j]);
    }
    free(ct->enc[i]);
  }
  free(ct->enc);
}

void mife_clear_pp_read(mife_pp_t pp) {
  mife_clear_pp(pp);
  gghlite_params_clear_read(*pp->params_ref);
}

void mife_clear_pp(mife_pp_t pp) {
  fmpz_clear(pp->p);
  free(pp->n);
  free(pp->gammas);
}

void mife_clear_sk(mife_sk_t sk) {
  gghlite_sk_clear(sk->self, 1);
  for(int i = 0; i < sk->numR; i++) {
    fmpz_mat_clear(sk->R[i]);
    fmpz_mat_clear(sk->R_inv[i]);
  }
  free(sk->R);
  free(sk->R_inv);
  flint_randclear(sk->randstate);
}

void mife_mat_clr_clear(mife_pp_t pp, mife_mat_clr_t met) {
  for(int i = 0; i < pp->num_inputs; i++) {
    for(int j = 0; j < pp->n[i]; j++) {
      fmpz_mat_clear(met->clr[i][j]);
    }
    free(met->clr[i]);
  }
  free(met->clr);
}

void print_mife_mat_clr(mife_pp_t pp, mife_mat_clr_t met) {
  for(int i = 0; i < pp->num_inputs; i++) {
    printf("clr[%d] matrices: \n", i);
    for(int j = 0; j < pp->n[i]; j++) {
      fmpz_mat_print_pretty(met->clr[i][j]);
      printf("\n\n");
    }
  }
  printf("\n");
}

void mife_encrypt(ore_ciphertext_t ct, fmpz_t message, mife_pp_t pp,
    mife_sk_t sk) {
  // compute a random index in the range [0,2^L]
  fmpz_t index, powL, two;
  fmpz_init(index);
  fmpz_init(powL);
  fmpz_init_set_ui(two, 2);
  fmpz_pow_ui(powL, two, pp->L); // computes powL = 2^L
  fmpz_set_ui(index, 0);
  fmpz_randm(index, sk->randstate, powL);
  fmpz_clear(powL);
  fmpz_clear(two);
  
  mife_mat_clr_t met;
  pp->setfn(met, message, pp, sk);

  if(! (pp->flags & MIFE_NO_RANDOMIZERS)) {
    mife_apply_randomizers(met, pp, sk);     
  }
  mife_set_encodings(ct, met, index, pp, sk);
  fmpz_clear(index);
  mife_mat_clr_clear(pp, met);
}

void mife_mbp_init(
    mife_pp_t pp,
    int num_inputs,
    int (*paramfn)(int, int),
    void (*kilianfn)(struct _mife_pp_struct *, int *),
    void (*orderfn)(int, int *, int *),
    void (*setfn)(mife_mat_clr_t, fmpz_t, struct _mife_pp_struct *, mife_sk_t),
    int (*parsefn)(char **)
    ) {
  pp->num_inputs = num_inputs;
  pp->paramfn = paramfn;
  pp->kilianfn = kilianfn;
  pp->orderfn = orderfn;
  pp->setfn = setfn;
  pp->parsefn = parsefn;
} 

void mife_setup(mife_pp_t pp, mife_sk_t sk, int L, int lambda,
    gghlite_flag_t ggh_flags, char *shaseed) {
  flint_randinit_seed_crypto(sk->randstate, shaseed, 1);

  pp->n = malloc(pp->num_inputs * sizeof(int));
  for(int index = 0; index < pp->num_inputs; index++) { 
    pp->n[index] = pp->paramfn(pp->bitstr_len, index);
  }
  
  pp->kappa = pp->n[0] + pp->n[1];
  pp->L = L;

  pp->gamma = 0;
  pp->gammas = malloc(pp->num_inputs * sizeof(int));
  for(int i = 0 ; i < pp->num_inputs; i++) {
    pp->gammas[i] = 1 + (pp->n[i]-1) * (pp->L+1);
    pp->gamma += pp->gammas[i];
  }

  T = ggh_walltime(0);
  gghlite_jigsaw_init_gamma(sk->self,
                      lambda,
                      pp->kappa,
                      pp->gamma,
                      ggh_flags,
                      sk->randstate);

  pp->params_ref = &(sk->self->params);

  /*
  printf("Supporting at most 2^%d plaintexts, each in base %d,\n", pp->L,
      pp->d);
  printf("of length %d, with gamma = %d\n\n", pp->bitstr_len, pp->gamma);
*/
  fmpz_init(pp->p);
  fmpz_poly_oz_ideal_norm(pp->p, sk->self->g, sk->self->params->n, 0);

  /*
  printf("1. GGH InstGen wall time:                 %8.2f s\n",
      ggh_seconds(ggh_walltime(T)));
  */

  T = ggh_walltime(0);

  // set the kilian randomizers in sk
  pp->numR = pp->kappa - 1;
  sk->numR = pp->numR;
  int *dims = malloc(pp->numR * sizeof(int));
  pp->kilianfn(pp, dims);

  sk->R = malloc(sk->numR * sizeof(fmpz_mat_t));
  sk->R_inv = malloc(sk->numR * sizeof(fmpz_mat_t));

  for (int k = 0; k < pp->numR; k++) {
    fmpz_mat_init(sk->R[k], dims[k], dims[k]);
    for (int i = 0; i < dims[k]; i++) {
      for(int j = 0; j < dims[k]; j++) {
        fmpz_randm(fmpz_mat_entry(sk->R[k], i, j), sk->randstate, pp->p);
      }
    }
	
    fmpz_mat_init(sk->R_inv[k], dims[k], dims[k]);
    fmpz_modp_matrix_inverse(sk->R_inv[k], sk->R[k], dims[k], pp->p);
  }

  free(dims);
}

// message >= 0, d >= 2
void message_to_dary(ulong *dary, int bitstring_len, fmpz_t message, int d) {
  assert(d >= 2);
  fmpz_t message2;
  fmpz_init_set(message2, message);
  fmpz_t modresult;
  fmpz_init(modresult);
  fmpz_t modd;
  fmpz_init_set_ui(modd, d);

  int i;
  for (i = bitstring_len - 1; i >= 0; i--) {
    fmpz_tdiv_qr(message2, modresult, message2, modd);
    dary[i] = fmpz_get_ui(modresult);
  }
  
  fmpz_clear(message2);
  fmpz_clear(modd);
  fmpz_clear(modresult);
}

/**
 * Creates the first x-matrix in the matrix-compressed version of ORE.
 *
 * This matrix has a single row (so it's actually a vector), and d columns, 
 * which each represent the bit that is being read.
 *
 * @param m The matrix
 * @param input1 A number in [0,d-1]
 * @param input2 A number in [0,d-1]
 * @param d The base
 */
void ore_mc_clrmat_init_XFIRST(fmpz_mat_t m, int input, int d) {
  fmpz_mat_init(m, 1, d);
  fmpz_mat_zero(m);
  fmpz_set_ui(fmpz_mat_entry(m, 0, input), NONZERO_VAL);
}

/**
 * Creates the first y-matrix in the matrix-compressed version of ORE.
 *
 * This matrix is d x 3.
 *
 * @param m The matrix
 * @param input1 A number in [0,d-1]
 * @param input2 A number in [0,d-1]
 * @param d The base
 */
void ore_mc_clrmat_init_YFIRST(fmpz_mat_t m, int input, int d) {
  fmpz_mat_init(m, d, 3);
  fmpz_mat_zero(m);
 
  for(int i = 0; i < d; i++) {
    int j;
    if(i == input) {
      j = 0;
    } else if(i < input) {
      j = 1;
    } else { // i > input
      j = 2;
    }
    fmpz_set_ui(fmpz_mat_entry(m, i, j), NONZERO_VAL);
  }
}

/**
 * Creates the x matrices (not the first) in the matrix-compressed version of 
 * ORE.
 *
 * This matrix is 3 x (d+2).
 *
 * @param m The matrix
 * @param input1 A number in [0,d-1]
 * @param input2 A number in [0,d-1]
 * @param d The base
 */
void ore_mc_clrmat_init_XREST(fmpz_mat_t m, int input, int d) {
  fmpz_mat_init(m, 3, d+2);
  fmpz_mat_zero(m);
 
  for(int i = 0; i < 3; i++) {
    int j;
    if(i == 0) {
      j = input;
    } else if(i == 1) {
      j = d;
    } else { // i == 2
      j = d+1;
    }
    fmpz_set_ui(fmpz_mat_entry(m, i, j), NONZERO_VAL);
  }
}

/**
 * Creates the y matrices (not the first) in the matrix-compressed version of 
 * ORE.
 *
 * This matrix is (d+2) x 3.
 *
 * @param m The matrix
 * @param input1 A number in [0,d-1]
 * @param input2 A number in [0,d-1]
 * @param d The base
 */
void ore_mc_clrmat_init_YREST(fmpz_mat_t m, int input, int d) {
  fmpz_mat_init(m, d+2, 3);
  fmpz_mat_zero(m);
 
  for(int i = 0; i < d+2; i++) {
    int j;
    if(i == d) {
      j = 1;
    } else if(i == d+1) {
      j = 2;
    } else if(i == input) {
      j = 0;
    } else if(i < input) {
      j = 1;
    } else { // i > input
      j = 2;
    }
    fmpz_set_ui(fmpz_mat_entry(m, i, j), NONZERO_VAL);
  }
}



/**
 * Creates the first x-matrix in the degree-compressed version of ORE.
 *
 * This matrix has a single row (so it's actually a vector), and d columns, 
 * which each represent the bit that is being read.
 *
 * @param m The matrix
 * @param input1 A number in [0,d-1]
 * @param input2 A number in [0,d-1]
 * @param d The base
 */
void ore_dc_clrmat_init_FIRST(fmpz_mat_t m, int input, int d) {
  fmpz_mat_init(m, 1, d);
  fmpz_mat_zero(m);
  fmpz_set_ui(fmpz_mat_entry(m, 0, input), NONZERO_VAL);
}

/**
 * Creates the first y-matrix in the degree-compressed version of ORE.
 *
 * We use columns [0,d-1] to represent the bit being read, column d to represent 
 * '<', and d+1 to represent '>'.
 *
 * @param m The matrix
 * @param input1 A number in [0,d-1]
 * @param input2 A number in [0,d-1]
 * @param d The base
 */
void ore_dc_clrmat_init_SECOND(fmpz_mat_t m, int input1, int input2, int d) {
  fmpz_mat_init(m, d, d+2);
  fmpz_mat_zero(m);
  
  for(int i = 0; i < d; i++) {
    int j;
    if(i < input1) {
      j = d;
    } else if(i > input1) {
      j = d+1;
    } else { // i == input1
      j = input2;
    }
    fmpz_set_ui(fmpz_mat_entry(m, i, j), NONZERO_VAL);
  }
}

/**
 * Creates the "middle" matrices in the degree-compressed version of ORE.
 *
 * We use columns [0,d-1] to represent the bit being read, column d to represent 
 * '<', and d+1 to represent '>'. This is the same as the second matrix, except 
 * we have two more rows, and the last two rows just follow the identity matrix 
 * (since if we were in the '<' state, we stay in that state, and the same is 
 * true for the '>' state).
 *
 * @param m The matrix
 * @param input1 A number in [0,d-1]
 * @param input2 A number in [0,d-1]
 * @param d The base
 * @param type Either X_TYPE or Y_TYPE
 */
void ore_dc_clrmat_init_MIDDLE(fmpz_mat_t m, int input1, int input2, int d,
    int type) {
  fmpz_mat_init(m, d+2, d+2);
  fmpz_mat_zero(m);
  
  for(int i = 0; i < d; i++) {
    int j = -1;
    if(i < input1) {
      j = (type == X_TYPE) ? d+1 : d;
    } else if(i > input1) {
      j = (type == X_TYPE) ? d : d+1;
    } else { // i == input1
      j = input2;
    }
    fmpz_set_ui(fmpz_mat_entry(m, i, j), NONZERO_VAL);
  }
  fmpz_set_ui(fmpz_mat_entry(m, d, d), NONZERO_VAL);
  fmpz_set_ui(fmpz_mat_entry(m, d+1, d+1), NONZERO_VAL);
}

/**
 * Creates the last matrix in the degree-compressed version of ORE.
 *
 * We use column 0 to represent '=', column 1 for '<', and column 2 for '>'.
 *
 * @param m The matrix
 * @param input A number in [0,d-1]
 * @param d The base
 * @param type Either X_TYPE or Y_TYPE
 */
void ore_dc_clrmat_init_LAST(fmpz_mat_t m, int input, int d, int type) {
  fmpz_mat_init(m, d+2, 3);
  fmpz_mat_zero(m);
  
  for(int i = 0; i < d+2; i++) {
    int j;
    if(i == d) {
      j = 1;
    } else if(i == d+1) {
      j = 2;
    } else if(i == input) {
      j = 0;
    } else if(i < input) {
      j = (type == X_TYPE) ? 2 : 1;
    } else {
      assert(i > input);
      j = (type == X_TYPE) ? 1 : 2;
    }
    fmpz_set_ui(fmpz_mat_entry(m, i, j), NONZERO_VAL);
  }
}


/**
 * Creates the (special case) second matrix in the degree-compressed version of 
 * ORE.
 *
 * This only is used when the second matrix is also the last matrix, in which 
 * case the dimensions are d x 3.
 *
 * We use column 0 to represent '=', column 1 for '<', and column 2 for '>'.
 *
 * @param m The matrix
 * @param input A number in [0,d-1]
 * @param d The base
 */
void ore_dc_clrmat_init_SECONDANDLAST(fmpz_mat_t m, int input, int d) {
  fmpz_mat_init(m, d, 3);
  fmpz_mat_zero(m);
  
  for(int i = 0; i < d; i++) {
    int j = -1;
    if(i == input) {
      j = 0;
    } else if(i < input) {
      j = 1;
    } else if(i > input) {
      j = 2;
    }
    fmpz_set_ui(fmpz_mat_entry(m, i, j), NONZERO_VAL);
  }
}


/* input = the digit being read, (i,j) = coordinates of matrix, type = X or Y
 *
 * The DFA is defined as follows:
 * - state 0 is the equals state
 * - state 1 is the less than state
 * - state 2 is the greater than state
 *
 */
int ore_get_matrix_bit_normal_mbp(int input, int i, int j, int type) {
  if (type == X_TYPE) {
    if ((i == 1 && j == 1) || (i == 2 && j == 2))
      return NONZERO_VAL;
    if (i == 0 && j == input+3)
      return NONZERO_VAL;
    return 0;
  } else {
    if ((i == 1 && j == 1) || (i == 2 && j == 2))
      return NONZERO_VAL;
    int input_state = i-3; // we just read this digit from x
    if (input_state < 0)
      return 0;
	
    // 'input_state' is the bit from x
    // 'input' is the bit from y

    if (j == 0 && input_state == input)
      return NONZERO_VAL;
    if (j == 1 && input_state < input)
      return NONZERO_VAL;
    if (j == 2 && input_state > input)
      return NONZERO_VAL;
    return 0;
  }
}

void fmpz_mat_scalar_mul_modp(fmpz_mat_t m, fmpz_t scalar, fmpz_t modp) {
  for (int i = 0; i < m->r; i++) {
    for(int j = 0; j < m->c; j++) {
      fmpz_mul(fmpz_mat_entry(m, i, j), fmpz_mat_entry(m, i, j), scalar);
      fmpz_mod(fmpz_mat_entry(m, i, j), fmpz_mat_entry(m, i, j), modp);
    }
  }
}

int ore_mbp_param(int bitstr_len, int index) {
 
  if(ORE_GLOBAL_FLAGS & ORE_MBP_NORMAL) {
    return bitstr_len;
  }

  if(ORE_GLOBAL_FLAGS & ORE_MBP_DC) {
    if(index == 0) {
      return bitstr_len / 2 + 1;
    }
    if(index == 1) {
      return (bitstr_len + 1) / 2;
    }
  }
  
  if(ORE_GLOBAL_FLAGS & ORE_MBP_MC) {
    return bitstr_len;
  }

  return -1; // error
}

void ore_mbp_kilian(mife_pp_t pp, int *dims) {
  // set the kilian dimensions
  if(ORE_GLOBAL_FLAGS & ORE_MBP_NORMAL) {
    for(int i = 0; i < pp->numR; i++) {
      dims[i] = pp->d+3;
    }
  } else if(ORE_GLOBAL_FLAGS & ORE_MBP_DC) {
    dims[0] = pp->d;
    for(int i = 1; i < pp->numR; i++) {
      dims[i] = pp->d+2;
    }
  } else if(ORE_GLOBAL_FLAGS & ORE_MBP_MC) {
    dims[0] = pp->d;
    for(int i = 1; i < pp->numR; i++) {
      if(i % 2 == 1) {
        dims[i] = 3;
      } else {
        dims[i] = pp->d+2;
      }
    }
  } else {
    assert(0);
  }
}


/* sets the cleartext matrices */
void ore_mbp_set_matrices(mife_mat_clr_t met, fmpz_t message, mife_pp_t pp,
    mife_sk_t sk) {
  ulong *dary_repr = malloc(pp->bitstr_len * sizeof(ulong));
  message_to_dary(dary_repr, pp->bitstr_len, message, pp->d);

  assert(pp->num_inputs == 2);

  met->clr = malloc(pp->num_inputs * sizeof(fmpz_mat_t *));
  for(int i = 0; i < pp->num_inputs; i++) {
    met->clr[i] = malloc(pp->n[i] * sizeof(fmpz_mat_t));
  }

  if(ORE_GLOBAL_FLAGS & ORE_MBP_NORMAL) {
    assert(pp->n[0] == pp->n[1]);
    for(int k = 0; k < pp->n[0]; k++) {
      int dim = pp->d+3;
      for(int i = 0; i < pp->num_inputs; i++) {
        fmpz_mat_init(met->clr[i][k], dim, dim);
      }

      for(int i = 0; i < dim; i++) {
        for(int j = 0; j < dim; j++) {
          int x_digit = ore_get_matrix_bit_normal_mbp(dary_repr[k],
              i, j, X_TYPE);
          int y_digit = ore_get_matrix_bit_normal_mbp(dary_repr[k],
              i, j, Y_TYPE);
          fmpz_set_ui(fmpz_mat_entry(met->clr[0][k], i, j), x_digit);
          fmpz_set_ui(fmpz_mat_entry(met->clr[1][k], i, j), y_digit);
        }
      }
    }
  } else if(ORE_GLOBAL_FLAGS & ORE_MBP_DC) {
    for(int k = 0, bc = 0; k < pp->n[0]; k++, bc++) {
      if(bc == 0) {
        ore_dc_clrmat_init_FIRST(met->clr[0][k], dary_repr[bc], pp->d);
      } else if(bc == pp->bitstr_len - 1) {
        ore_dc_clrmat_init_LAST(met->clr[0][k], dary_repr[bc], pp->d,
            X_TYPE);
      } else {
        ore_dc_clrmat_init_MIDDLE(met->clr[0][k], dary_repr[bc],
            dary_repr[bc+1], pp->d, X_TYPE);
        bc++;
      }
    }
    
    for(int k = 0, bc = 0; k < pp->n[1]; k++, bc++) {
      if(k == 0 && pp->n[1] > 1) {
        ore_dc_clrmat_init_SECOND(met->clr[1][k], dary_repr[bc],
            dary_repr[bc+1], pp->d);
        bc++;
      } else if(k == 0 && pp->n[1] == 1) {
        ore_dc_clrmat_init_SECONDANDLAST(met->clr[1][k], dary_repr[bc],
            pp->d);
      } else if((pp->bitstr_len % 2 == 1) && (k == pp->n[1]-1)) {
        ore_dc_clrmat_init_LAST(met->clr[1][k], dary_repr[bc], pp->d,
            Y_TYPE);
      } else {
        ore_dc_clrmat_init_MIDDLE(met->clr[1][k], dary_repr[bc],
            dary_repr[bc+1], pp->d, Y_TYPE);
        bc++;
      }
    }
  } else if(ORE_GLOBAL_FLAGS & ORE_MBP_MC) {
    for(int k = 0; k < pp->n[0]; k++) {
      if(k == 0) {
        ore_mc_clrmat_init_XFIRST(met->clr[0][k], dary_repr[k], pp->d);
      } else {
        ore_mc_clrmat_init_XREST(met->clr[0][k], dary_repr[k], pp->d);
      }
    }
    for(int k = 0; k < pp->n[1]; k++) {
      if(k == 0) {
        ore_mc_clrmat_init_YFIRST(met->clr[1][k], dary_repr[k], pp->d);
      } else {
        ore_mc_clrmat_init_YREST(met->clr[1][k], dary_repr[k], pp->d);
      }
    }
  } else {
    assert(0);
  }
  free(dary_repr);    
}

void mife_apply_randomizers(mife_mat_clr_t met, mife_pp_t pp, mife_sk_t sk) {
  for(int i = 0; i < pp->num_inputs; i++) {
    for(int k = 0; k < pp->n[i]; k++) {
      fmpz_t rand;
      fmpz_init(rand);
      fmpz_randm(rand, sk->randstate, pp->p);
      fmpz_mat_scalar_mul_modp(met->clr[i][k], rand, pp->p);
      fmpz_clear(rand);
    }
  }
}

/**
 * Generates the i^th member of the exclusive partition family for the index 
 * sets.
 *
 * @param partitioning The description of the partitioning, each entry is in 
 * [0,d-1] and it is of length (1 + (d-1)(L+1)).
 * @param index The index being the i^th member of the partition family
 * @param L the log of the size of the partition family. So, i must be in the 
 * range [0,2^L-1]
 * @param nu The number of total elements to be multiplied. partitioning[] 
 * will describe a nu-partition of the universe set.
 */ 
void mife_gen_partitioning(int *partitioning, fmpz_t index, int L, int nu) {
  int j = 0;

  ulong *bitstring = malloc(L * sizeof(ulong));
  memset(bitstring, 0, L * sizeof(ulong));
  message_to_dary(bitstring, L, index, 2);

  for(; j < nu; j++) {
    partitioning[j] = j;
  }

  for(int k = 0; k < L; k++) {
    for(int j1 = 1; j1 < nu; j1++) {
      partitioning[j] = (bitstring[k] == 1) ? j1 : 0;
      j++;
    }
  }
  free(bitstring);
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

void gghlite_enc_mat_init(gghlite_params_t params, gghlite_enc_mat_t m,
    int nrows, int ncols) {
  m->nrows = nrows;
  m->ncols = ncols;
  m->m = malloc(nrows * sizeof(gghlite_enc_t *));
  assert(m->m);
  for(int i = 0; i < m->nrows; i++) {
    m->m[i] = malloc(m->ncols * sizeof(gghlite_enc_t));
    assert(m->m[i]);
    for(int j = 0; j < m->ncols; j++) {
      gghlite_enc_init(m->m[i][j], params);
    }
  }
}

void gghlite_enc_mat_clear(gghlite_enc_mat_t m) {
  for(int i = 0; i < m->nrows; i++) {
    for(int j = 0; j < m->ncols; j++) {
      gghlite_enc_clear(m->m[i][j]);
    }
    free(m->m[i]);
  }
  free(m->m);
}


void mife_mat_encode(mife_pp_t pp, mife_sk_t sk, gghlite_enc_mat_t enc,
    fmpz_mat_t m, int *group) {
  gghlite_clr_t e;
  gghlite_clr_init(e);
  for(int i = 0; i < enc->nrows; i++) {
    for(int j = 0; j < enc->ncols; j++) {
      fmpz_poly_set_coeff_fmpz(e, 0, fmpz_mat_entry(m, i, j));
      gghlite_enc_set_gghlite_clr(enc->m[i][j], sk->self, e, 1, group, 1,
          sk->randstate);
      NUM_ENCODINGS_GENERATED++;
      if(PRINT_ENCODING_PROGRESS) {
        printf("Generated encoding %d / %d (Time elapsed: %8.2f s)\n",
            NUM_ENCODINGS_GENERATED,
            pp->num_enc,
            ggh_seconds(ggh_walltime(T)));
      }
    }
  }
  gghlite_clr_clear(e);
}

void gghlite_enc_mat_zeros_print(mife_pp_t pp, gghlite_enc_mat_t m) {
  for(int i = 0; i < m->nrows; i++) {
    printf("[");
    for(int j = 0; j < m->ncols; j++) {
      printf(gghlite_enc_is_zero(*pp->params_ref, m->m[i][j]) ? "0 " : "x " );
    }
    printf("]\n");
  }
}


// index ranges from [0, kappa - 1]
void ore_mbp_ordering(int index, int *ip, int *jp) {
  *ip = index % 2;
  *jp = index / 2;
}

int ore_mbp_parse(char **m) {
  if(m[0][0] + m[0][1] + m[0][2] != 1)
    return -1;

  for(int i = 0; i < 3; i++) {
    if(m[0][i] == 1)
      return i;
  }

  return -1;
}

void mife_set_encodings(ore_ciphertext_t ct, mife_mat_clr_t met, fmpz_t index,
    mife_pp_t pp, mife_sk_t sk) {

  int **ptns = malloc(pp->num_inputs * sizeof(int *));
  for(int i = 0; i < pp->num_inputs; i++) {
    ptns[i] = malloc(pp->gammas[i] * sizeof(int));
    mife_gen_partitioning(ptns[i], index, pp->L, pp->n[i]);
  }

  /* construct the partitions in the group array form */
  int ***groups = malloc(pp->num_inputs * sizeof(int **));
  for(int i = 0; i < pp->num_inputs; i++) {
    int gamma_offset = 0;
    for(int h = 0; h < i; h++) {
      gamma_offset += pp->gammas[h];
    }

    groups[i] = malloc(pp->n[i] * sizeof(int *));
    for(int j = 0; j < pp->n[i]; j++) {
      groups[i][j] = malloc(pp->gamma * sizeof(int));
      memset(groups[i][j], 0, pp->gamma * sizeof(int));
      for(int k = 0; k < pp->gammas[i]; k++) {
        if(ptns[i][k] == j) {
          groups[i][j][k + i * pp->gammas[0]] = 1;
        }
      }
    }
  }

  for(int i = 0; i < pp->num_inputs; i++) {
    free(ptns[i]);
  }
  free(ptns);

  if(pp->flags & MIFE_SIMPLE_PARTITIONS) {
    // override group arrays with trivial partitioning
    for(int i = 0; i < pp->num_inputs; i++) {
      for(int j = 0; j < pp->n[i]; j++) {
        memset(groups[i][j], 0, pp->gamma * sizeof(int));
      }
    }
    
    for(int k = 0; k < pp->gamma; k++) {
      groups[0][0][k] = 1;
    }
  }

  if(! (pp->flags & MIFE_NO_KILIAN)) {
    // apply kilian to the cleartext matrices (overwriting them in the process)

    fmpz_mat_t tmp;

    for(int index = 0; index < pp->kappa; index++) {
      int i, j;
      pp->orderfn(index, &i, &j);

      // first one
      if(index == 0) {
        fmpz_mat_init(tmp, met->clr[i][j]->r, sk->R[0]->c);
        fmpz_mat_mul(tmp, met->clr[i][j], sk->R[0]);
        fmpz_mat_set(met->clr[i][j], tmp);
        fmpz_mat_clear(tmp);
        continue;
      }

      // last one
      if(index == pp->kappa - 1) {
        fmpz_mat_init(tmp, sk->R_inv[pp->numR-1]->r, met->clr[i][j]->c);
        fmpz_mat_mul(tmp, sk->R_inv[pp->numR-1], met->clr[i][j]);
        fmpz_mat_set(met->clr[i][j], tmp);
        fmpz_mat_clear(tmp);
        continue;
      }
      
      // all others
      fmpz_mat_init(tmp, sk->R_inv[index-1]->r, met->clr[i][j]->c);
      fmpz_mat_mul(tmp, sk->R_inv[index-1], met->clr[i][j]);
      fmpz_mat_mul(tmp, tmp, sk->R[index]);
      fmpz_mat_set(met->clr[i][j], tmp);
      fmpz_mat_clear(tmp);
    }
  }

  // encode
  ct->enc = malloc(pp->num_inputs * sizeof(gghlite_enc_mat_t *));
  for(int i = 0; i < pp->num_inputs; i++) {
    ct->enc[i] = malloc(pp->n[i] * sizeof(gghlite_enc_mat_t));
    for(int j = 0; j < pp->n[i]; j++) {
      gghlite_enc_mat_init(sk->self->params, ct->enc[i][j],
          met->clr[i][j]->r, met->clr[i][j]->c);
      mife_mat_encode(pp, sk, ct->enc[i][j], met->clr[i][j], groups[i][j]);
    }
  }
  
  // free group arrays
  for(int i = 0; i < pp->num_inputs; i++) {
    for(int j = 0; j < pp->n[i]; j++) {
      free(groups[i][j]);
    }
    free(groups[i]);
  }
  free(groups);
}

void gghlite_enc_mat_mul(gghlite_params_t params, gghlite_enc_mat_t r,
    gghlite_enc_mat_t m1, gghlite_enc_mat_t m2) {
  gghlite_enc_t tmp;
  gghlite_enc_init(tmp, params);

  gghlite_enc_mat_t tmp_mat;
  gghlite_enc_mat_init(params, tmp_mat, m1->nrows, m2->ncols);

  assert(m1->ncols == m2->nrows);

  for(int i = 0; i < m1->nrows; i++) {
    for(int j = 0; j < m2->ncols; j++) {
      for(int k = 0; k < m1->ncols; k++) {
        gghlite_enc_mul(tmp, params, m1->m[i][k], m2->m[k][j]);
        gghlite_enc_add(tmp_mat->m[i][j], params, tmp_mat->m[i][j], tmp);
      }
    }
  }

  gghlite_enc_mat_clear(r);
  gghlite_enc_mat_init(params, r, m1->nrows, m2->ncols);

  for(int i = 0; i < r->nrows; i++) {
    for(int j = 0; j < r->ncols; j++) {
      gghlite_enc_set(r->m[i][j], tmp_mat->m[i][j]);
    }
  }

  gghlite_enc_mat_clear(tmp_mat);
  gghlite_enc_clear(tmp);
}

int mife_evaluate(mife_pp_t pp, ore_ciphertext_t *cts) {
  gghlite_enc_mat_t tmp;

  for(int index = 1; index < pp->kappa; index++) {
    int i, j;
    pp->orderfn(index, &i, &j);
    
    if(index == 1) {
      // multiply the 0th index with the 1st index
      int i0, j0;
      pp->orderfn(0, &i0, &j0);
      gghlite_enc_mat_init(*pp->params_ref, tmp,
        cts[i0]->enc[i0][j0]->nrows, cts[i]->enc[i][j]->ncols);
      gghlite_enc_mat_mul(*pp->params_ref, tmp,
        cts[i0]->enc[i0][j0], cts[i]->enc[i][j]);
      continue;
    }

    gghlite_enc_mat_mul(*pp->params_ref, tmp, tmp, cts[i]->enc[i][j]);
  }

  char **result = malloc(tmp->nrows * sizeof(char *));
  for(int i = 0; i < tmp->nrows; i++) {
    result[i] = malloc(tmp->ncols * sizeof(char));
    for(int j = 0; j < tmp->ncols; j++) {
      result[i][j] = 1 - gghlite_enc_is_zero(*pp->params_ref, tmp->m[i][j]);
    }
  }
  gghlite_enc_mat_clear(tmp);

  int ret = pp->parsefn(result);

  for(int i = 0; i < tmp->nrows; i++) {
    free(result[i]);
  }
  free(result);

  return ret;
}

void fmpz_mat_modp(fmpz_mat_t m, int dim, fmpz_t p) {
  for(int i = 0; i < dim; i++) {
    for(int j = 0; j < dim; j++) {
      fmpz_mod(fmpz_mat_entry(m, i, j), fmpz_mat_entry(m, i, j), p);
    }
  }
}

/**
 * Test code
 */

int int_arrays_equal(ulong *arr1, ulong *arr2, int length) {
  for (int i = 0; i < length; i++) {
    if (arr1[i] != arr2[i])
      return 1;
  }
  return 0;
}

void test_dary_conversion() {
  printf("Testing d-ary conversion function...                          ");
  ulong dary1[4];
  ulong dary2[8];
  ulong dary3[4];
  ulong correct1[] = {1,0,1,0};
  ulong correct2[] = {0,0,0,0,5,4,1,4};
  ulong correct3[] = {0,0,0,2};
  fmpz_t num1, num2, num3;
  fmpz_init_set_ui(num1, 10);
  fmpz_init_set_ui(num2, 1234);
  fmpz_init_set_ui(num3, 2);

  message_to_dary(dary1, 4, num1, 2);
  message_to_dary(dary2, 8, num2, 6);
  message_to_dary(dary3, 4, num3, 11);


  int status = 0;
  status += int_arrays_equal(dary1, correct1, 4);
  status += int_arrays_equal(dary2, correct2, 8);
  status += int_arrays_equal(dary3, correct3, 4);



  if (status == 0)
    printf("SUCCESS\n");
  else
    printf("FAIL\n");	

}

int test_matrix_inv(int n, flint_rand_t randstate, fmpz_t modp) {
  printf("\nTesting matrix_inv function...                                ");
  fmpz_mat_t a;
  fmpz_mat_init(a, n, n);

  for(int i = 0; i < n; i++) {
    for(int j = 0; j < n; j++) {
      fmpz_randm(fmpz_mat_entry(a, i, j), randstate, modp);
    }
  }

  fmpz_mat_t inv;
  fmpz_mat_init(inv, n, n);

  fmpz_mat_t prod;
  fmpz_mat_init(prod, n, n);

  fmpz_modp_matrix_inverse(inv, a, n, modp);
  fmpz_mat_mul_modp(prod, a, inv, n, modp);

  fmpz_mat_t identity;
  fmpz_mat_init(identity, n, n);
  fmpz_mat_one(identity);

  int status = fmpz_mat_equal(prod, identity);
  if (status != 0)
    printf("SUCCESS\n");
  else
    printf("FAIL\n");

  fmpz_mat_clear(a);
  fmpz_mat_clear(inv);
  fmpz_mat_clear(prod);
  fmpz_mat_clear(identity);
  return status;
}

/**
 * Code to find the inverse of a matrix, adapted from:
 * https://www.cs.rochester.edu/~brown/Crypto/assts/projects/adj.html
 */

/*
   Recursive definition of determinate using expansion by minors.
*/
void fmpz_mat_det_modp(fmpz_t det, fmpz_mat_t a, int n, fmpz_t p) {
  assert(n >= 1);

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
  fmpz_mat_init(m, n-1, n-1);

  fmpz_set_ui(det, 0);
  for(int j1=0;j1<n;j1++) {
    for (int i=1;i<n;i++) {
      int j2 = 0;
      for (int j=0;j<n;j++) {
        if (j == j1)
          continue;
        fmpz_set(fmpz_mat_entry(m,i-1,j2), fmpz_mat_entry(a,i,j));
        j2++;
      }
    }
    fmpz_t det2;
    fmpz_init(det2);
    fmpz_mat_det_modp(det2, m, n-1, p);
    fmpz_mul(det2, det2, fmpz_mat_entry(a,0,j1));
    fmpz_mod(det2, det2, p);
    if(j1 % 2 == 1) {
      fmpz_negmod(det2, det2, p);
    }
    fmpz_add(det, det, det2);
    fmpz_clear(det2);
  }

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

void fmpz_modp_matrix_inverse(fmpz_mat_t inv, fmpz_mat_t a, int dim, fmpz_t p) {
  fmpz_t det;
  fmpz_init(det);
  fmpz_mat_det_modp(det, a, dim, p);
  fmpz_mat_t cofactor;
  fmpz_mat_init(cofactor, dim, dim);
  fmpz_mat_cofactor_modp(cofactor, a, dim, p);

  for(int i = 0; i < dim; i++) {
    for(int j = 0; j < dim; j++) {
      fmpz_t invmod;
      fmpz_init(invmod);
      fmpz_invmod(invmod, det, p);
      fmpz_t tmp;
      fmpz_init(tmp);
      fmpz_mod(tmp, fmpz_mat_entry(cofactor, j, i), p);
      fmpz_mul(tmp, tmp, invmod);
      fmpz_mod(tmp, tmp, p);
      fmpz_set(fmpz_mat_entry(inv,i,j), tmp);
      fmpz_clear(invmod);
      fmpz_clear(tmp);
    }
  }

  fmpz_clear(det);
  fmpz_mat_clear(cofactor);
}

/* copied from fmpz_mod_poly_fread, mostly (but fixed) */
int gghlite_enc_fread(FILE * f, fmpz_mod_poly_t poly)
{
    slong i, length;
    fmpz_t coeff;
    ulong res;

    fmpz_init(coeff);
    if (flint_fscanf(f, "%wd", &length) != 1) {
        fmpz_clear(coeff);
        return 0;
    }

    fmpz_fread(f,coeff);
    fmpz_mod_poly_clear(poly);
    fmpz_mod_poly_init(poly, coeff);
    fmpz_mod_poly_fit_length(poly, length);

    poly->length = length;
    flint_fscanf(f, " ");
  
    for (i = 0; i < length; i++)
    {
        flint_fscanf(f, " ");
        res = fmpz_fread(f, coeff);
        fmpz_mod_poly_set_coeff_fmpz(poly,i,coeff);

        if (!res)
        {
            poly->length = i;
            fmpz_clear(coeff);
            return 0;
        }
    }

    fmpz_clear(coeff);
    _fmpz_mod_poly_normalise(poly);

    return 1;
}

int test_ore(int lambda, int mspace_size, int num_messages, int d,
    int bitstr_len, ore_flag_t flags, int verbose) {
  printf("Testing ORE...                          ");
  ORE_GLOBAL_FLAGS = flags;
  int status = 0;
  int L = 80;
  mife_pp_t pp;
  mife_sk_t sk;
  mife_init_params(pp, d, bitstr_len, flags);

  mife_mbp_init(pp, 2, &ore_mbp_param, &ore_mbp_kilian, &ore_mbp_ordering,
     &ore_mbp_set_matrices, &ore_mbp_parse);

  gghlite_flag_t ggh_flags = GGHLITE_FLAGS_QUIET | GGHLITE_FLAGS_GOOD_G_INV;
  mife_setup(pp, sk, L, lambda, ggh_flags, DEFAULT_SHA_SEED);

  fmpz_t message_space_size;
  fmpz_init_set_ui(message_space_size, mspace_size);

  fmpz_t *messages = malloc(num_messages * sizeof(fmpz_t));
  for(int i = 0; i < num_messages; i++) {
    fmpz_init(messages[i]);
    fmpz_randm(messages[i], sk->randstate, message_space_size);
    if(verbose) {
      fmpz_print(messages[i]);
      printf("\n");
    }
  }

  ore_ciphertext_t *ciphertexts = malloc(num_messages * sizeof(ore_ciphertext_t));
  for(int i = 0; i < num_messages; i++) {
    mife_encrypt(ciphertexts[i], messages[i], pp, sk);
  }

  for(int i = 0; i < num_messages; i++) {
    for(int j = 0; j < num_messages; j++) {
      ore_ciphertext_t *cts = malloc(2 * sizeof(ore_ciphertext_t));
      memcpy(cts[0], ciphertexts[i], sizeof(ore_ciphertext_t));
      memcpy(cts[1], ciphertexts[j], sizeof(ore_ciphertext_t));

      int compare = mife_evaluate(pp, cts);
      int true_compare = fmpz_cmp(messages[i], messages[j]);
      if(true_compare < 0) {
        true_compare = 1;
      } else if(true_compare > 0) {
        true_compare = 2;
      }
      if(verbose) {
        printf(
          "messages #1: %lu, message #2: %lu, compare: %d, true_compare: %d\n",
          fmpz_get_ui(messages[i]), fmpz_get_ui(messages[j]),
          compare, true_compare
        );
      }
      if(compare != true_compare) {
        status += 1;
      }
    }
  }

  for(int i = 0; i < num_messages; i++) {
    ore_ciphertext_clear(pp, ciphertexts[i]);
  }
  mife_clear_pp(pp);
  mife_clear_sk(sk);
  
  free(messages);
  free(ciphertexts);

  if(status == 0) {
    printf("SUCCESS\n");
  } else {
    printf("FAIL\n");
  }
  return status;
}


void run_tests() {
  //test_matrix_inv(6, randstate, p);
  test_dary_conversion();
  test_ore(5, 16, 5, 2, 4, ORE_MBP_NORMAL, 0);
  test_ore(5, 16, 5, 2, 4, ORE_MBP_DC, 0);
  test_ore(5, 16, 5, 2, 4, ORE_MBP_MC, 0);
  test_ore(5, 1000, 10, 5, 5, ORE_MBP_NORMAL, 0);
  test_ore(5, 1000, 10, 5, 5, ORE_MBP_DC, 0);
  test_ore(5, 1000, 10, 5, 5, ORE_MBP_MC, 0);

  mpfr_free_cache();
  flint_cleanup();


}
