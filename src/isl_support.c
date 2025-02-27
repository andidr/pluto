/*
 * PLUTO: An automatic parallelizer and locality optimizer
 *
 * Copyright (C) 2007-2012 Uday Bondhugula
 *
 * This file is part of Pluto.
 *
 * Pluto is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * A copy of the GNU General Public Licence can be found in the file
 * `LICENSE' in the top-level directory of this distribution.
 *
 */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "constraints.h"

#include "isl/aff.h"
#include "isl/val.h"
#include "isl/val_gmp.h"

/*
 * Construct a PlutoMatrix with the same content as the given isl_mat.
 */
PlutoMatrix *pluto_matrix_from_isl_mat(__isl_keep isl_mat *mat) {
  int i, j;
  int rows, cols;
  PlutoMatrix *pluto;

  rows = isl_mat_rows(mat);
  cols = isl_mat_cols(mat);
  pluto = pluto_matrix_alloc(rows, cols);

  for (i = 0; i < rows; ++i)
    for (j = 0; j < cols; ++j) {
      isl_val *v = isl_mat_get_element_val(mat, i, j);
      pluto->val[i][j] = isl_val_get_num_si(v);
      isl_val_free(v);
    }

  return pluto;
}

/*
 * Convert an isl affine expression to Pluto function
 */
isl_stat isl_aff_to_pluto_func(__isl_take isl_set *set, __isl_take isl_aff *aff,
                               void *user) {
  int i, j, npar;

  npar = isl_aff_dim(aff, isl_dim_param);

  PlutoMatrix **mat_p = (PlutoMatrix **)user;
  if (*mat_p != NULL)
    pluto_matrix_free(*mat_p);
  *mat_p = pluto_matrix_alloc(1, isl_aff_dim(aff, isl_dim_in) + npar + 1);
  PlutoMatrix *mat = *mat_p;

  if (isl_aff_dim(aff, isl_dim_div) >= 1) {
    isl_aff *div = isl_aff_get_div(aff, 0);
    isl_val *v = isl_aff_get_denominator_val(div);
    isl_aff_free(div);
    if (!isl_val_is_one(v)) {
      pluto_matrix_zero_row(mat, 0);
      isl_val_free(v);
      isl_set_free(set);
      isl_aff_free(aff);
      return isl_stat_ok;
    }
    isl_val_free(v);
  }

  for (i = 0; i < isl_aff_dim(aff, isl_dim_in); i++) {
    isl_val *v = isl_aff_get_coefficient_val(aff, isl_dim_in, i);
    mat->val[0][i] = isl_val_get_num_si(v);
    isl_val_free(v);
  }
  for (j = 0; j < npar; i++, j++) {
    isl_val *v = isl_aff_get_coefficient_val(aff, isl_dim_param, j);
    mat->val[0][i] = isl_val_get_num_si(v);
    isl_val_free(v);
  }
  isl_val *v = isl_aff_get_constant_val(aff);
  mat->val[0][i] = isl_val_get_num_si(v);
  isl_val_free(v);

  isl_set_free(set);
  isl_aff_free(aff);

  return isl_stat_ok;
}

/* Does the mpz value fit in a long long? */
static int mpz_fits_ll(mpz_t z) {
  int sign;

  mpz_t tmp;
  mpz_init(tmp);
  /* tmp = (upper bits of r) */
  if (mpz_sgn(z) > 0) {
    mpz_tdiv_q_2exp(tmp, z, 63);
  } else {
    mpz_add_ui(tmp, z, 1);
    mpz_tdiv_q_2exp(tmp, tmp, 63);
  }
  sign = mpz_sgn(tmp);
  mpz_clear(tmp);
  return sign == 0;
}

/* Extract the numerator of a rational value "v" as a long long int.
 *
 * If "v" is not a rational value, then the result is undefined.
 */
long long isl_val_get_num_ll(__isl_keep isl_val *v) {
  unsigned lo, hi;
  mpz_t z, tmp;
  int sign;
  long long result;

  mpz_init(tmp);

  if (!v)
    return 0;
  if (!isl_val_is_rat(v))
    isl_die(isl_val_get_ctx(v), isl_error_invalid, "expecting rational value",
            return 0);

  mpz_init(z);
  isl_val_get_num_gmp(v, z);

  if (!mpz_fits_ll(z)) {
    printf("[pluto_math_support] numerator too large; returning "
           "largest/smallest signed 64-bit number\n");
    sign = mpz_sgn(z);
    mpz_clear(z);

    /* 2^63-1 is the largest positive signed number for 64-bit */
    /* -2^63 is the largest negative signed number for 64-bit */
    return sign ? (long long)((1ULL << 63) - 1) : (long long)((1ULL << 63));
  }

  /* tmp = (lower 64 bits of r) */
  mpz_mod_2exp(tmp, z, 64);
  mpz_clear(z);

  /* lo = tmp & 0xffffffff */
  lo = mpz_get_ui(tmp);

  /* tmp >>= 32 */
  mpz_div_2exp(tmp, tmp, 32);

  /* hi = tmp & 0xffffffff */
  hi = mpz_get_ui(tmp);

  mpz_clear(tmp);
  result = (long long)((((unsigned long long)hi) << 32) + lo);

  return result;
}
