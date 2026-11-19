/*
 * Hash for the constant pool.
 *
 * In this file, "x is identical to y" means 
 * x == y || R_compute_identical(x, y, 16)
 *
 * similarly to what do_putconst in GNU-R (eval.c) does.
 *
 * Contract: if x is identical to y, then cp_hash(x) == cp_hash(y).
 *
 * cp_hash obeys these rules of identical.c with flags 16:
 *  - Attributes: identical.c compares them as a set. The hash ignores all
 *    attributes.
 *  - Doubles: -0 and +0 are identical. NA_real_ and NaN are not identical.
 *    All other NaN values are identical.
 *  - Strings (Seql): two non-ASCII strings in different encodings can be
 *    identical. All non-ASCII strings have one hash value. Two identical
 *    ASCII strings have the same bytes.
 *  - Symbols and environments: two objects are identical only if they are
 *    the same object. The hash uses the pointer.
 *  - Byte code: identical byte code has identical constant pools. The hash
 *    reads the first nodes of the first constant, the expression.
 *  - Closures, builtins, promises and other types: the hash uses only the
 *    type.
 *
 * The hash reads at most CP_HASH_NODES objects and CP_HASH_ELTS elements of
 * a vector (hash.c). Identical objects have the same structure, thus the hash
 * stops at the same point for each of them.
 *
 * Note: -0 and 0 use the same pool entry. GNU R does this, and crbcc does the
 * same to make the same byte code. The byte code can then give a different
 * result than the interpreter. For example:
 *
 *   f <- function(x) {
 *     b <- -0
 *     c <- 0
 *     g(x, c)
 *   }
 *   g <- function(x, c) 1 / c
 *
 * The compiler folds -0 into a constant and puts it in the pool first. The
 * literal 0 is identical to -0, thus it gets the pool entry of -0. In the
 * interpreter, f(1) gives Inf. In the byte code of GNU R and crbcc, f(1)
 * gives -Inf.
 */

#ifndef CRBCC_HASH_H
#define CRBCC_HASH_H

#include <Rinternals.h>

// Returns the hash of x for the constant pool.
unsigned int cp_hash(SEXP x);

#endif
