#ifndef _ORE_H_
#define _ORE_H_

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <gmp.h>
#include <gghlite/gghlite.h>
#include <gghlite/gghlite-defs.h>
#include "common.h"

int NUM_ENCODINGS_GENERATED;
int PRINT_ENCODING_PROGRESS;
uint64_t T;

/* these are used for get_matrix_bit() */
#define X_TYPE 0
#define Y_TYPE 1
#define NONZERO_VAL 1

typedef enum {
  //!< default behaviour
  MIFE_DEFAULT = 0x00,
  
  //!< do not multiply kilian randomizers into the encodings
  MIFE_NO_KILIAN    = 0x01, 
  
  //!< do not multiply the scalar randomizers into the encodings
  MIFE_NO_RANDOMIZERS    = 0x02,

  //!< pick a simple partitioning (x[0] is encoded at the universe, all others 
  //are encoded at the empty set.)
  MIFE_SIMPLE_PARTITIONS  = 0x04,
} mife_flag_t;

typedef enum {
   //!< the MBP produced is a series of 2n (d+3) x (d+3) matrices
  ORE_MBP_NORMAL = 0x08,

  //!< the MBP produced is degree-compressed by reading the input as: x0 (y0 y1) 
  //(x1 x2) (y2 y3) ...
  ORE_MBP_DC = 0x10,

  //!< the MBP produced is matrix-compressed, with dimensions 3 x d. This is not 
  //compatible with ORE_MBP_DC, though.
  ORE_MBP_MC = 0x20,

  ORE_DEFAULT = ORE_MBP_DC,
} ore_flag_t;

ore_flag_t ORE_GLOBAL_FLAGS = ORE_DEFAULT;

struct _gghlite_enc_mat_struct {
  int nrows; // number of rows in the matrix
  int ncols; // number of columns in the matrix
  gghlite_enc_t **m;
};

typedef struct _gghlite_enc_mat_struct gghlite_enc_mat_t[1];

struct _mife_mat_clr_struct {
  fmpz_mat_t **clr;
};

typedef struct _mife_mat_clr_struct mife_mat_clr_t[1];

struct _ore_ciphertext_struct {
  gghlite_enc_mat_t **enc;
};

typedef struct _ore_ciphertext_struct ore_ciphertext_t[1];

struct _mife_sk_struct {
  int numR;
  gghlite_sk_t self;
  fmpz_mat_t *R;
  fmpz_mat_t *R_inv;
  flint_rand_t randstate;
};

typedef struct _mife_sk_struct mife_sk_t[1];

struct _mife_pp_struct {
  int num_inputs; // the arity of the MBP (for comparisons, this is 2).
  int *n; // of length num_inputs
  int *gammas; // gamma for each input
  long ct_size; // estimated size of a ciphertext based on # of encodings
  int bitstr_len; // length of the plaintexts in d-ary
  int d; // the base
  int L; // log # of plaintexts we can support
  int gamma; // should be sum of gammas[i] 
  int kappa; // the kappa for gghlite (degree of multilinearity)
  int numR; // number of kilian matrices. should be kappa-1
  int num_enc; // number of encodings per ciphertext
  mife_flag_t flags;
  fmpz_t p; // the prime, the order of the field
  gghlite_params_t *params_ref; // gghlite's public parameters, by reference

  // MBP function pointers
  int (*paramfn)(int, int); // for determining number of matrices per input
  void (*kilianfn)(struct _mife_pp_struct *, int *); // set kilian dimensions
  void (*orderfn)(int, int *, int *); // function pointer for MBP ordering
  void (*setfn)(mife_mat_clr_t, fmpz_t, struct _mife_pp_struct *, mife_sk_t);
  int (*parsefn)(char **); // function pointer for parsing output 
};

typedef struct _mife_pp_struct mife_pp_t[1];


/* MIFE interface */
void mife_init_params(mife_pp_t pp, int d, int bitstr_len, mife_flag_t flags);
void mife_mbp_init(
    mife_pp_t pp,
    int num_inputs,
    int (*paramfn)(int, int),
    void (*kilianfn)(struct _mife_pp_struct *, int *),
    void (*orderfn)(int, int *, int *),
    void (*setfn)(mife_mat_clr_t, fmpz_t, struct _mife_pp_struct *, mife_sk_t),
    int (*parsefn)(char **)
    );
void mife_setup(mife_pp_t pp, mife_sk_t sk, int L, int lambda,
    gghlite_flag_t ggh_flags, char *shaseed);
void mife_encrypt(ore_ciphertext_t ct, fmpz_t message, mife_pp_t pp,
    mife_sk_t sk);
int mife_evaluate(mife_pp_t pp, ore_ciphertext_t *cts);

/* MIFE internal functions */
void mife_challenge_gen(int argc, char *argv[]);
void mife_apply_randomizers(mife_mat_clr_t met, mife_pp_t pp, mife_sk_t sk);
void mife_set_encodings(ore_ciphertext_t ct, mife_mat_clr_t met, fmpz_t index,
    mife_pp_t pp, mife_sk_t sk);
void mife_clear_pp_read(mife_pp_t pp);
void mife_clear_pp(mife_pp_t pp);
void mife_clear_sk(mife_sk_t sk);
void mife_mat_clr_clear(mife_pp_t pp, mife_mat_clr_t met);
void mife_gen_partitioning(int *partitioning, fmpz_t index, int L, int nu);
void mife_mat_encode(mife_pp_t pp, mife_sk_t sk, gghlite_enc_mat_t enc,
    fmpz_mat_t m, int *group);
void gghlite_enc_mat_zeros_print(mife_pp_t pp, gghlite_enc_mat_t m);
void gghlite_enc_mat_mul(gghlite_params_t params, gghlite_enc_mat_t r,
    gghlite_enc_mat_t m1, gghlite_enc_mat_t m2);
void flint_randinit_seed_crypto(flint_rand_t randstate,
    char *seed, int gmp);

/* functions dealing with ORE challenge generation */
void ore_set_best_params(mife_pp_t pp, int lambda, fmpz_t message_space_size);
void ore_ciphertext_clear(mife_pp_t pp, ore_ciphertext_t ct);
void message_to_dary(ulong *dary, int bitstring_len, fmpz_t message, int d);
int ore_get_matrix_bit_normal_mbp(int input, int i, int j, int type);
int ore_mbp_param(int bitstr_len, int index);
void ore_mbp_kilian(mife_pp_t pp, int *dims);
void ore_mbp_set_matrices(mife_mat_clr_t met, fmpz_t message, mife_pp_t pp,
    mife_sk_t sk);
void ore_mbp_ordering(int index, int *ip, int *jp);
int ore_mbp_parse(char **m);

/* functions dealing with file reading and writing for encodings */
#define gghlite_enc_fprint fmpz_mod_poly_fprint 
int gghlite_enc_fread(FILE * f, gghlite_enc_t poly);
void gghlite_params_clear_read(gghlite_params_t self);
void fwrite_ore_pp(mife_pp_t pp, char *filepath);
void fread_ore_pp(mife_pp_t pp, char *filepath);
void fwrite_ore_ciphertext(mife_pp_t pp, ore_ciphertext_t ct, char *filepath);
void fwrite_gghlite_enc_mat(mife_pp_t pp, gghlite_enc_mat_t m, FILE *fp);
void fread_ore_ciphertext(mife_pp_t pp, ore_ciphertext_t ct, char *filepath);
void fread_gghlite_enc_mat(mife_pp_t pp, gghlite_enc_mat_t m, FILE *fp);

/* functions dealing with fmpz types and matrix multiplications mod fmpz_t */
void fmpz_modp_matrix_inverse(fmpz_mat_t inv, fmpz_mat_t a, int dim, fmpz_t p);
void fmpz_mat_modp(fmpz_mat_t m, int dim, fmpz_t p);
void fmpz_mat_scalar_mul_modp(fmpz_mat_t m, fmpz_t scalar, fmpz_t modp);
void fmpz_mat_mul_modp(fmpz_mat_t a, fmpz_mat_t b, fmpz_mat_t c, int n,
    fmpz_t p);
void gghlite_enc_mat_init(gghlite_params_t params, gghlite_enc_mat_t m,
    int nrows, int ncols);
void gghlite_enc_mat_clear(gghlite_enc_mat_t m);
void fmpz_init_exp(fmpz_t exp, int base, int n);

/* functions for computing matrix inverse mod fmpz_t */
void fmpz_mat_modp(fmpz_mat_t m, int dim, fmpz_t p);
void fmpz_mat_det_modp(fmpz_t det, fmpz_mat_t a, int n, fmpz_t p);
void fmpz_mat_cofactor_modp(fmpz_mat_t b, fmpz_mat_t a, int n, fmpz_t p);
void fmpz_modp_matrix_inverse(fmpz_mat_t inv, fmpz_mat_t a, int dim, fmpz_t p);

/* test functions */
void run_tests();
int test_matrix_inv(int n, flint_rand_t randstate, fmpz_t modp);
int int_arrays_equal(ulong *arr1, ulong *arr2, int length);
void test_dary_conversion();
int test_matrix_inv(int n, flint_rand_t randstate, fmpz_t modp);
int test_ore(int lambda, int mspace_size, int num_messages, int d,
    int bitstr_len, ore_flag_t flags, int verbose);


/* benchmarking info for choosing best params */

int MAX_KAPPA_BENCH = 28;
long KAPPA_BENCH[] = {
-1,
-1,
13743796,
39428602,
50011646,
127578831,
149971402,
172363974,
194756546,
217149117,
504072750,
551305371,
598537993,
645770614,
693003236,
740235857,
787468479,
834701100,
881933722,
929166343,
2051532015,
2150882456,
2250232897,
2349583338,
2448933779,
2548284220,
2647634661,
2746985102,
2846335543,
2945685984,
3045036425,
3144386867,
3243737308,
3343087749,
3442438190,
3541788631,
3641139072,
3740489513,
3839839954,
};


#endif /* _ORE_H_ */
