#include <R.h>
#include <R_ext/Rdynload.h>
#include <Rinternals.h>

extern SEXP R_TrueValue;
extern SEXP R_FalseValue;

#define RETURN_OP 1
#define LDCONST_OP 16
#define LDNULL_OP 17
#define LDTRUE_OP 18
#define LDFALSE_OP 19

static int BCVersion;

extern SEXP R_mkClosure(SEXP formals, SEXP body, SEXP env);

static SEXP make_bc_code(SEXP code, SEXP consts) {
  // .Internal(mkCode(code, consts))
  SEXP call;
  call = PROTECT(Rf_lang3(Rf_install("mkCode"), code, consts));
  call = PROTECT(Rf_lang2(Rf_install(".Internal"), call));

  SEXP res = PROTECT(Rf_eval(call, R_BaseEnv));

  UNPROTECT(3);
  return res; /* BCODESXP */
}

static SEXP R_bcVersion() {
  SEXP call, res;

  // .Internal(bcVersion())
  call = PROTECT(lang1(install("bcVersion")));
  call = PROTECT(lang2(install(".Internal"), call));

  res = PROTECT(eval(call, R_BaseEnv));

  UNPROTECT(4);
  return res;
}

/**
 * Create a compiled function with bytecode structure
 * Creates a function that has a bytecode-like body that R can recognize
 *
 * @param original_fun The original function
 * @param constant The constant value
 * @return A compiled function
 */
static SEXP compiled_constant(SEXP constant) {
  SEXP code, consts, bc;
  int i = 1;

  if (constant == R_NilValue) {
    PROTECT(code = allocVector(INTSXP, 3));
    INTEGER(code)[i++] = LDNULL_OP;
    PROTECT(consts = allocVector(VECSXP, 0));
  } else if (constant == R_TrueValue) {
    PROTECT(code = allocVector(INTSXP, 3));
    INTEGER(code)[i++] = LDTRUE_OP;
    PROTECT(consts = allocVector(VECSXP, 0));
  } else if (constant == R_FalseValue) {
    PROTECT(code = allocVector(INTSXP, 3));
    INTEGER(code)[i++] = LDFALSE_OP;
    PROTECT(consts = allocVector(VECSXP, 0));
  } else {
    /* LDCONST takes an immediate pool index operand */
    PROTECT(code = allocVector(INTSXP, 4));
    INTEGER(code)[i++] = LDCONST_OP;
    INTEGER(code)[i++] = 0; /* const pool index */
    PROTECT(consts = allocVector(VECSXP, 1));
    SET_VECTOR_ELT(consts, 0, constant);
  }

  INTEGER(code)[0] = BCVersion;
  INTEGER(code)[i++] = RETURN_OP;
  PROTECT(bc = make_bc_code(code, consts));
  UNPROTECT(3);
  return bc;
}

SEXP cmpfun(SEXP fun, SEXP options) {
  if (TYPEOF(fun) != CLOSXP) error("Argument must be a closure");

  SEXP body = BODY(fun);

  switch (TYPEOF(body)) {
    case NILSXP:
    case REALSXP:
    case INTSXP:
    case LGLSXP:
    case STRSXP:
    case CPLXSXP: {
      SEXP compiled_body = PROTECT(compiled_constant(body));
      SEXP out = PROTECT(R_mkClosure(FORMALS(fun), compiled_body, CLOENV(fun)));
      UNPROTECT(2);
      return out;
    }
    default:
      Rprintf(
          "Function body is not a constant - returning original function\n");
      return fun;
  }
}

SEXP is_compiled(SEXP fun) {
  int res = 0;
  if (TYPEOF(fun) == CLOSXP) {
    res = (TYPEOF(BODY(fun)) == BCODESXP);
  }
  return ScalarLogical(res);
}

// Registration of C functions
static const R_CallMethodDef CallEntries[] = {
    {"cmpfun", (DL_FUNC)&cmpfun, 2},
    {"is_compiled", (DL_FUNC)&is_compiled, 1},
    {NULL, NULL, 0}};

void R_init_crbcc(DllInfo* dll) {
  R_registerRoutines(dll, NULL, CallEntries, NULL, NULL);
  R_useDynamicSymbols(dll, FALSE);

  BCVersion = Rf_asInteger(R_bcVersion());
}
