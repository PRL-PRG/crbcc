#ifndef R_NO_REMAP
#define R_NO_REMAP
#endif

#include <R.h>
#include <Rinternals.h>
#include <stdint.h>
#include <string.h>

#include "hash.h"

/*
 * Bounds to keep hashing fast
 */
#define CP_HASH_NODES 16   // Maximum number of objects that cp_hash reads
#define CP_HASH_ELTS 16    // Maximum number of elements of a vector that cp_hash reads

/*
 * Adds v to the hash h inspired by xxHash32.
 */
static inline unsigned int cp_hash_combine(unsigned int h, unsigned int v) {
  h ^= v;
  h *= 0x9E3779B1u;
  return h ^ (h >> 15);
}

static unsigned int cp_hash_double(double d) {
  if (R_IsNA(d))
    return 1;
  if (ISNAN(d))
    return 2;
  if (d == 0)
    return 0;

  uint64_t u;
  memcpy(&u, &d, sizeof(u));
  return (unsigned int) (u ^ (u >> 32));
}

static unsigned int cp_hash_char(SEXP c) {
  unsigned int h = 2166136261u; // FNV-1a offset basis

  for (const unsigned char *s = (const unsigned char *) CHAR(c); *s; s++) {
    if (*s > 127)
      return 4;
    h = (h ^ *s) * 16777619u; // FNV-1a prime
  }

  return h;
}

static unsigned int cp_hash_rec(SEXP x, int *budget) {

  unsigned int h = cp_hash_combine(0, TYPEOF(x));

  if (*budget <= 0)
    return h;
  (*budget)--;

  R_xlen_t n = 0;

  switch (TYPEOF(x)) {

    case SYMSXP:
    case ENVSXP: {
      uintptr_t p = (uintptr_t) x;
      return cp_hash_combine(h, (unsigned int) (p ^ (p >> 32)));
    }

    case CHARSXP:
      return cp_hash_combine(h, cp_hash_char(x));

    // identical() compares the constant pools of two code objects. The first
    // constant is the expression. Without the expression, all promise code
    // objects of a pool have the same hash, and the probe calls identical()
    // for each of them. The first nodes of the expression are sufficient.
    // More nodes make the hash slower than the calls of identical().
    case BCODESXP: {
      int expr_budget = 4;
      return cp_hash_combine(h, cp_hash_rec(R_BytecodeExpr(x), &expr_budget));
    }

    case LGLSXP:
    case INTSXP:
      n = XLENGTH(x);
      h = cp_hash_combine(h, (unsigned int) n);
      for (R_xlen_t i = 0; i < n && i < CP_HASH_ELTS; i++)
        h = cp_hash_combine(h, (unsigned int) (TYPEOF(x) == INTSXP ? INTEGER_ELT(x, i) : LOGICAL_ELT(x, i)));
      return h;

    case REALSXP:
      n = XLENGTH(x);
      h = cp_hash_combine(h, (unsigned int) n);
      for (R_xlen_t i = 0; i < n && i < CP_HASH_ELTS; i++)
        h = cp_hash_combine(h, cp_hash_double(REAL_ELT(x, i)));
      return h;

    case CPLXSXP:
      n = XLENGTH(x);
      h = cp_hash_combine(h, (unsigned int) n);
      for (R_xlen_t i = 0; i < n && i < CP_HASH_ELTS; i++) {
        Rcomplex z = COMPLEX_ELT(x, i);
        h = cp_hash_combine(h, cp_hash_double(z.r));
        h = cp_hash_combine(h, cp_hash_double(z.i));
      }
      return h;

    case RAWSXP:
      n = XLENGTH(x);
      h = cp_hash_combine(h, (unsigned int) n);
      for (R_xlen_t i = 0; i < n && i < CP_HASH_ELTS; i++)
        h = cp_hash_combine(h, RAW_ELT(x, i));
      return h;

    case STRSXP:
      n = XLENGTH(x);
      h = cp_hash_combine(h, (unsigned int) n);
      for (R_xlen_t i = 0; i < n && i < CP_HASH_ELTS; i++)
        h = cp_hash_combine(h, cp_hash_char(STRING_ELT(x, i)));
      return h;

    case VECSXP:
    case EXPRSXP:
      n = XLENGTH(x);
      h = cp_hash_combine(h, (unsigned int) n);
      for (R_xlen_t i = 0; i < n && i < CP_HASH_ELTS; i++)
        h = cp_hash_combine(h, cp_hash_rec(VECTOR_ELT(x, i), budget));
      return h;

    // Same walk as identical(): CAR and PRINTNAME(TAG) of each cell. Most
    // cells have no tag. The hash does not read an empty tag, thus the
    // budget is sufficient for more CARs.
    case LANGSXP:
    case LISTSXP:
    case DOTSXP:
      for (SEXP c = x; c != R_NilValue && *budget > 0; c = CDR(c)) {
        h = cp_hash_combine(h, cp_hash_rec(CAR(c), budget));
        if (TAG(c) != R_NilValue)
          h = cp_hash_combine(h, cp_hash_rec(PRINTNAME(TAG(c)), budget));
      }
      return h;

    default:
      return h;
  }
}

unsigned int cp_hash(SEXP x) {

  int budget = CP_HASH_NODES;
  return cp_hash_rec(x, &budget);
}
