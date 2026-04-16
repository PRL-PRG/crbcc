#pragma region Headers

#include <R.h>
#include <Rdefines.h>
#include <R_ext/Rdynload.h>
#include <Rinternals.h>
#include <ctype.h>
#include <stdarg.h>

//#define DEBUG
#define OPTIMIZE_INCOMPATIBLE false
#define END_OPCODES -1

extern SEXP R_TrueValue;
extern SEXP R_FalseValue;
extern SEXP R_mkClosure(SEXP formals, SEXP body, SEXP env);

static int BCVersion;

#ifdef DEBUG
#define DEBUG_PRINT(...) Rprintf(__VA_ARGS__)
#else
#define DEBUG_PRINT(...) do {} while (0)
#endif

#define IDENTICAL(x,y) R_compute_identical(x, y, 0)

#define SEQ_ALONG( iter_name, along_var ) for ( SEXP iter_name = along_var; iter_name != R_NilValue; iter_name = CDR(iter_name) )
#define SEQ_ALONG_IX( iter_name, along_var, iter ) \
    int iter = 0; \
    for ( SEXP iter_name = along_var; iter_name != R_NilValue; iter_name = CDR(iter_name), iter++ )

#define PUTCONST(const) cb_putconst(cb, const, true)
#define PUTCONST_NODEDUP(const) cb_putconst(cb, const, false)
#define PUTCODES(...) cb_putcode(cb, __VA_ARGS__, END_OPCODES)
#define PUTCODE(code) cb_putcode(cb, code, END_OPCODES)

#pragma endregion

#pragma region Opcodes

// Opcode Definitions
#define RETURN_OP 1
#define LDCONST_OP 16
#define LDNULL_OP 17
#define LDTRUE_OP 18
#define LDFALSE_OP 19
#define GETVAR_OP 20
#define GETFUN_OP 23
#define CALL_OP 38
#define CHECKFUN_OP 28
#define PUSHCONSTARG_OP 34
#define PUSHTRUEARG_OP 36
#define PUSHFALSEARG_OP 37
#define PUSHNULLARG_OP 35
#define MAKEPROM_OP 29
#define SETTAG_OP 31
#define DOTSERR_OP 60
#define DDVAL_OP 21
#define DOMISSING_OP 30
#define DODOTS_OP 32
#define DDVAL_MISSOK_OP 93
#define SWITCH_OP 102
#define BASEGUARD_OP 123
#define POP_OP 4
#define MAKECLOSURE_OP 41
#define GETINTLBUILTIN_OP 27
#define GETBUILTIN_OP 26
#define CALLBUILTIN_OP 39
#define VISIBLE_OP 94
#define PUSHARG_OP 33
#define GETVAR_MISSOK_OP 92
#define RETURNJMP_OP 103
#define CALLSPECIAL_OP 40
#define SETVAR_OP 22
#define INVISIBLE_OP 15
#define BRIFNOT_OP 3
#define GOTO_OP 2
#define AND1ST_OP 88
#define AND2ND_OP 89
#define OR1ST_OP 90
#define OR2ND_OP 91
#define STARTLOOPCNTXT_OP 7
#define ENDLOOPCNTXT_OP 8
#define DOLOOPNEXT_OP 9
#define DOLOOPBREAK_OP 10
#define STARTFOR_OP 11
#define STEPFOR_OP 12
#define ENDFOR_OP 13
#define UMINUS_OP 42
#define UPLUS_OP 43
#define ADD_OP 44
#define SUB_OP 45
#define MUL_OP 46
#define DIV_OP 47
#define EXPT_OP 48
#define SQRT_OP 49
#define EXP_OP 50
#define LOG_OP 116
#define LOGBASE_OP 117
#define MATH1_OP 118
#define EQ_OP 51
#define NE_OP 52
#define LT_OP 53
#define LE_OP 54
#define GE_OP 55
#define GT_OP 56
#define AND_OP 57
#define OR_OP 58
#define NOT_OP 59
#define SETVAR2_OP 95
#define STARTASSIGN_OP 61
#define ENDASSIGN_OP 62
#define STARTASSIGN2_OP 96
#define ENDASSIGN2_OP 97
#define INCLNKSTK_OP 127
#define DECLNKSTK_OP 128
#define SETTER_CALL_OP 98
#define GETTER_CALL_OP 99
#define SWAP_OP 100
#define DUP2ND_OP 101
#define DOLLAR_OP 73
#define DOLLARGETS_OP 74
#define INCLNK_OP 124
#define DECLNK_OP 125
#define DECLNK_N_OP 126
#define INCLNKSTK_OP 127
#define DECLNKSTK_OP 128
#define STARTSUBASSIGN_OP 65
#define DFLTSUBASSIGN_OP 66
#define VECSUBASSIGN_OP 86
#define MATSUBASSIGN_OP 87
#define SUBASSIGN_N_OP 114
#define STARTSUBASSIGN_N_OP 105
#define STARTSUBASSIGN2_N_OP 111
#define DFLTSUBASSIGN2_OP 72
#define STARTSUBASSIGN2_OP 71
#define VECSUBASSIGN2_OP 108
#define MATSUBASSIGN2_OP 109
#define MATSUBSET2_OP 107
#define SUBASSIGN2_N_OP 115
#define STARTSUBSET_OP 63
#define DFLTSUBSET_OP 64
#define DFLTSUBSET2_OP 70
#define VECSUBSET_OP 84
#define MATSUBSET_OP 85
#define STARTSUBSET_N_OP 104
#define SUBSET2_N_OP 113
#define VECSUBSET2_OP 106
#define SUBSET_N_OP 112
#define STARTSUBSET2_OP 69
#define STARTSUBSET2_N_OP 110
#define COLON_OP 120
#define SEQALONG_OP 121
#define SEQLEN_OP 122
#define ISNULL_OP 75
#define ISLOGICAL_OP 76
#define ISINTEGER_OP 77
#define ISDOUBLE_OP 78
#define ISCOMPLEX_OP 79
#define ISCHARACTER_OP 80
#define ISSYMBOL_OP 81
#define ISOBJECT_OP 82
#define DOTCALL_OP 119

#pragma endregion

#pragma region Data Structures

typedef struct {

  int optimize_level;
  bool suppress_all;
  bool suppress_no_super_assign;
  bool suppress_all_undef;
  int num_suppress_vars;
  const char **suppress_undefined_vars; 

} CompilerOptions;

typedef struct LoopInfo {

  bool null;                      // Is this a valid loop context?
  int loop_label_id;              // Label ID for the start of the loop
  int end_label_id;               // Label ID for the end of the loop
  bool goto_ok;                   // Can a simple GOTO be used

} LoopInfo;

typedef struct CompilerContext {

  bool toplevel;                  // Is this a top-level expression?
  bool tailcall;                  // Is this in tail position?
  bool need_return_jmp;           // Does return() need a longjmp?

  CompilerOptions options;        

  // Other structures
  struct CompilerEnv *env;        // Current compilation environment
  struct LoopInfo loop;           // Current loop context (NULL if not in a loop)
  
  SEXP call;                      //Current R call being compiler (used for error messages)

} CompilerContext;

typedef enum {

  FRAME_LOCAL,
  FRAME_NAMESPACE,
  FRAME_GLOBAL

} FrameType;

typedef struct ExtraVars {

  const char ** vars;
  int count;

} ExtraVars;

typedef struct EnvFrame {

  FrameType type;         
  ExtraVars extra_vars;           // Vector of local variables discovered by the compiler
  struct EnvFrame *parent;        // Pointer to parent frame

} EnvFrame;

typedef struct CompilerEnv {

  struct EnvFrame *top_frame;     // Current frame being compiled
  SEXP r_env;                     // R environment object

} CompilerEnv;

typedef struct LabelTable {

  int *table;                      // Dynamic array of labels, ID corresponds to index
  int capacity;                    // Array capacity
  int labels_issued;               // Number of labels issued so far

} LabelTable;

typedef struct CodeBuffer {

  // Instruction stream
  int *code;                      // Dynamic array of integers
  int code_count;                 // Current size of the code array
  int code_capacity;              // Current capacity of the code array

  // Constant pool
  SEXP constant_pool_handle;      // Handle to preserve the constant pool, single item list
  SEXP constant_pool;             // R List object
  int const_count;                // Number of constants in the pool
  int const_capacity;             // Capacity of the constant pool

  // Label management
  struct LabelTable label_table;

  // Source tracking
  int * expr_buf;                  // Buffer for expressions
  int * srcref_buf;                // Buffer for source references

  SEXP current_expr;
  SEXP current_srcref;

  bool srcref_tracking_on;         // Is source reference tracking on
  bool expr_tracking_on;           // Is expression tracking on

  struct SwitchPatch *switch_patches;    // Linked list head of switch statement patches
  struct SwitchPatch *patch_tail;         // O(1) append

} CodeBuffer;

typedef struct SwitchPatch {

  bool has_char_labels;
  int char_code_offset;     // Where to patch the char INTSXP index
  int n_char;               // Length of char labels array
  int * char_labels;        // Array of label IDs (with default at the end)

  // Numeric Labels Data (Always used)
  int num_code_offset;      // Where to patch the num INTSXP index
  int n_num;                // Length of num labels array
  int * num_labels;         // Array of label IDs (with default at the end)

  struct SwitchPatch * next;

} SwitchPatch;

typedef struct {

  EnvFrame* defining_frame;
  SEXP env;
  SEXP value;
  bool found;

} VarInfo;

// in the original compiler loc = list(expr, srcref)
typedef struct Loc {
  bool is_null;
  SEXP expr;
  SEXP srcref;
} Loc;

typedef bool (*HandlerFn)(SEXP e, CodeBuffer *cb, CompilerContext *cntxt);
typedef bool (*SetterHandlerFn)(SEXP afun, SEXP place, SEXP origplace, SEXP call, CodeBuffer *cb, CompilerContext *cntxt);

typedef enum {
  BASE,
  STAT,
  NO
} Inline;

typedef struct InlineHandler {

  char func_name[256];            // Name of the function being inlined
  HandlerFn handler;              // Function pointer to the inlining handler

} InlineHandler;

typedef struct SetterInlineHandler {

  char func_name[256];            // Name of the function being inlined
  SetterHandlerFn handler;        // Function pointer to the inlining handler

} SetterInlineHandler;

typedef struct InlineInfo {

  SEXP env;                       // Environment where the function is defined
  
  bool can_inline;                // Can this function be inlined
  bool guard_needed;              // Is a type guard needed

  Inline in;

} InlineInfo;

#pragma endregion

#pragma region o0 Function Declarations

//  === CODE BUFFER FUNCTIONS ===
CodeBuffer *make_code_buffer(SEXP preseed, Loc loc);

// Source location tracking with helpers
Loc cb_savecurloc(CodeBuffer *cb);
void cb_restorecurloc(CodeBuffer *cb, Loc saved);
void cb_setcurloc(CodeBuffer *cb, SEXP expr, SEXP sref);
void cb_setcurexpr(CodeBuffer *cb, SEXP expr);
SEXP get_expr_srcref(SEXP expr);
SEXP extract_srcref(SEXP sref, int idx);
SEXP get_block_srcref(SEXP block_sref, int idx);

// Label management
int cb_makelabel(CodeBuffer *cb);
void cb_putlabel(CodeBuffer *cb, int label_id);
void cb_patchlabels(CodeBuffer *cb);
void ensure_label_capacity(LabelTable *lt, int needed_index);
void cb_putcodelabel(CodeBuffer * cb, int label_id);

// Code emission and constant pool management
void cb_putcode(CodeBuffer *cb, ...);
int cb_getcode(CodeBuffer *cb, int pos);
int cb_putconst(CodeBuffer *cb, SEXP item, bool check_dedup);
SEXP cb_getconst(CodeBuffer *cb, int idx);
void cb_putswitch(CodeBuffer *cb, int * int_labels, int int_count, int int_pos, int * char_labels, int char_count, int char_pos);

// === CONTEXT AND ENVIRONMENT FUNCTIONS ===

// Constructors for various compiler contexts etc
CompilerEnv *make_cenv(SEXP env);
CompilerEnv *make_fun_env(SEXP forms, SEXP body, CompilerContext *cntxt);
CompilerContext *make_toplevel_ctx(CompilerEnv *cenv, SEXP options);
CompilerContext *make_function_ctx(CompilerContext *cntxt, CompilerEnv *fenv, SEXP forms, SEXP body);
CompilerContext *make_call_ctx(CompilerContext *cntxt, SEXP call);
CompilerContext *make_non_tail_call_ctx(CompilerContext *cntxt);
CompilerContext *make_no_value_ctx(CompilerContext *cntxt);
CompilerContext *make_loop_ctx(CompilerContext *cntxt, int loop_label, int end_label);
CompilerContext *make_arg_ctx(CompilerContext *cntxt);
CompilerContext *make_promise_ctx(CompilerContext *cntxt);

// Environment manipulation
void add_cenv_vars(CompilerEnv *cenv, ExtraVars vars);
void add_cenv_frame(CompilerEnv *cenv, ExtraVars vars);

// === Compilation functions ===
void cmp(SEXP e, CodeBuffer *cb, CompilerContext *cntxt, bool missing_ok, bool setloc);
void cmp_const(SEXP val, CodeBuffer *cb, CompilerContext *cntxt);
void cmp_sym(SEXP sym, CodeBuffer *cb, CompilerContext *cntxt, bool missing_ok);
void cmp_call(SEXP call, CodeBuffer *cb, CompilerContext *cntxt, bool inline_ok);
void cmp_call_sym_fun(SEXP fun, SEXP args, SEXP call, CodeBuffer *cb, CompilerContext *cntxt);
void cmp_call_expr_fun(SEXP fun, SEXP args, SEXP call, CodeBuffer *cb, CompilerContext *cntxt);
void cmp_call_args(SEXP args, CodeBuffer *cb, CompilerContext *cntxt, bool nse);
void cmp_tag(SEXP tag, CodeBuffer *cb);
void cmp_const_arg(SEXP a, CodeBuffer *cb, CompilerContext *cntxt);

SEXP cmpfun(SEXP f, SEXP compiler_options);

// Weird ahh functions
bool find_var(SEXP var, CompilerContext *cntxt);
ExtraVars find_locals(SEXP expr, ExtraVars known_locals, CompilerContext *cntxt);
ExtraVars find_locals_list(SEXP elist, ExtraVars known_locals, CompilerContext *cntxt);
SEXP gen_code(SEXP e, CompilerContext *cntxt, SEXP gen, Loc loc);
const char * get_assigned_var( SEXP var, CompilerContext *cntxt );
bool may_call_browser(SEXP expr, CompilerContext *cntxt);
bool may_call_browser_list(SEXP exprlist, CompilerContext * cntxt);
SEXP is_compiled(SEXP fun);
void R_init_crbcc(DllInfo* dll);
VarInfo find_cenv_var( SEXP var, CompilerEnv * cenv );
bool find_loc_var( SEXP var, CompilerContext * cntxt );
SEXP code_buf_code( CodeBuffer * cb, CompilerContext * cntxt );
bool is_ddsym(SEXP sym);
SEXP find_fun_def( SEXP fun_sym, CompilerContext * cntxt );
bool check_call( SEXP def, SEXP call, bool *should_warn);
bool any_dots( SEXP args );
static bool is_base_var(SEXP sym, CompilerContext *cntxt);
static SEXP R_bcVersion();
static bool is_in_set(SEXP sym, SEXP set);
static ExtraVars union_sets(ExtraVars a, ExtraVars b);
SEXP constant_fold(SEXP e, CompilerContext* cntxt, Loc loc);

// Inlining
bool try_inline( SEXP e, CodeBuffer *cb, CompilerContext *cntxt );
InlineInfo get_inline_info( const char *name, CompilerContext * cntxt, bool guard_ok );
bool get_inline_handler( const char *name, Inline in, CompilerContext * cntxt, HandlerFn * found );

#pragma endregion

#pragma region Inline function declarations

// The actual inlining handlers
bool inline_left_brace( SEXP e, CodeBuffer *cb, CompilerContext *cntxt );
bool inline_function( SEXP e, CodeBuffer *cb, CompilerContext *cntxt );
bool inline_return( SEXP e, CodeBuffer *cb, CompilerContext *cntxt );
bool inline_if( SEXP e, CodeBuffer *cb, CompilerContext *cntxt );
bool inline_left_parenthesis( SEXP e, CodeBuffer *cb, CompilerContext *cntxt );
bool inline_and( SEXP e, CodeBuffer *cb, CompilerContext *cntxt );
bool inline_or( SEXP e, CodeBuffer *cb, CompilerContext *cntxt );
bool inline_repeat( SEXP e, CodeBuffer *cb, CompilerContext *cntxt );
bool inline_next( SEXP e, CodeBuffer *cb, CompilerContext *cntxt );
bool inline_break( SEXP e, CodeBuffer *cb, CompilerContext *cntxt );
bool inline_while( SEXP e, CodeBuffer *cb, CompilerContext *cntxt );
bool inline_for( SEXP e, CodeBuffer *cb, CompilerContext *cntxt );
bool inline_plus( SEXP e, CodeBuffer *cb, CompilerContext *cntxt );
bool inline_minus( SEXP e, CodeBuffer *cb, CompilerContext *cntxt );
bool inline_mul( SEXP e, CodeBuffer *cb, CompilerContext *cntxt );
bool inline_div( SEXP e, CodeBuffer *cb, CompilerContext *cntxt );
bool inline_pow( SEXP e, CodeBuffer *cb, CompilerContext *cntxt );
bool inline_exp( SEXP e, CodeBuffer *cb, CompilerContext *cntxt );
bool inline_sqrt( SEXP e, CodeBuffer *cb, CompilerContext *cntxt );
bool inline_log( SEXP e, CodeBuffer *cb, CompilerContext *cntxt );
bool cmp_math_1( SEXP e, CodeBuffer *cb, CompilerContext *cntxt );
bool inline_eq( SEXP e, CodeBuffer *cb, CompilerContext *cntxt );
bool inline_neq( SEXP e, CodeBuffer *cb, CompilerContext *cntxt );
bool inline_lt( SEXP e, CodeBuffer *cb, CompilerContext *cntxt );
bool inline_le( SEXP e, CodeBuffer *cb, CompilerContext *cntxt );
bool inline_ge( SEXP e, CodeBuffer *cb, CompilerContext *cntxt );
bool inline_gt( SEXP e, CodeBuffer *cb, CompilerContext *cntxt );
bool inline_and2( SEXP e, CodeBuffer *cb, CompilerContext *cntxt );
bool inline_or2( SEXP e, CodeBuffer *cb, CompilerContext *cntxt );
bool inline_not( SEXP e, CodeBuffer *cb, CompilerContext *cntxt );
bool inline_subset( SEXP e, CodeBuffer *cb, CompilerContext *cntxt );
bool inline_subset2( SEXP e, CodeBuffer *cb, CompilerContext *cntxt );
bool cmp_multi_colon( SEXP e, CodeBuffer *cb, CompilerContext *cntxt );
bool inline_required( SEXP e, CodeBuffer *cb, CompilerContext *cntxt );
bool inline_with( SEXP e, CodeBuffer *cb, CompilerContext *cntxt );
bool inline_colon( SEXP e, CodeBuffer *cb, CompilerContext *cntxt );
bool inline_seq_along( SEXP e, CodeBuffer *cb, CompilerContext *cntxt );
bool inline_seq_len( SEXP e, CodeBuffer *cb, CompilerContext *cntxt );
bool inline_dollar(SEXP e, CodeBuffer *cb, CompilerContext *cntxt);
bool cmp_simple_internal(SEXP e, CodeBuffer *cb, CompilerContext *cntxt);
bool cmp_dot_internal_call(SEXP e, CodeBuffer *cb, CompilerContext *cntxt);
bool inline_c_call(SEXP e, CodeBuffer *cb, CompilerContext *cntxt);
bool inline_switch(SEXP e, CodeBuffer *cb, CompilerContext *cntxt);

bool inline_is_character(SEXP e, CodeBuffer *cb, CompilerContext *cntxt);
bool inline_is_complex(SEXP e, CodeBuffer *cb, CompilerContext *cntxt);
bool inline_is_double(SEXP e, CodeBuffer *cb, CompilerContext *cntxt);
bool inline_is_integer(SEXP e, CodeBuffer *cb, CompilerContext *cntxt);
bool inline_is_logical(SEXP e, CodeBuffer *cb, CompilerContext *cntxt);
bool inline_is_name(SEXP e, CodeBuffer *cb, CompilerContext *cntxt);
bool inline_is_null(SEXP e, CodeBuffer *cb, CompilerContext *cntxt);
bool inline_is_object(SEXP e, CodeBuffer *cb, CompilerContext *cntxt);
bool inline_is_symbol(SEXP e, CodeBuffer *cb, CompilerContext *cntxt);

bool cmp_assign( SEXP e, CodeBuffer *cb, CompilerContext *cntxt );
bool dollar_setter_inline_handler(SEXP afun, SEXP place, SEXP orig, SEXP call, CodeBuffer *cb, CompilerContext *cntxt);
bool at_setter_inline_handler(SEXP afun, SEXP place, SEXP orig, SEXP call, CodeBuffer *cb, CompilerContext *cntxt);
bool dollar_getter_inline_handler(SEXP call, CodeBuffer *cb, CompilerContext *cntxt);
bool inline_subassign_setter(SEXP afun, SEXP place, SEXP orig, SEXP call, CodeBuffer *cb, CompilerContext *cntxt);
bool inline_subassign2_setter(SEXP afun, SEXP place, SEXP orig, SEXP call, CodeBuffer *cb, CompilerContext *cntxt);
bool inline_subset_getter( SEXP call, CodeBuffer *cb, CompilerContext *cntxt );
bool inline_subset2_getter( SEXP call, CodeBuffer *cb, CompilerContext *cntxt );
bool inline_local(SEXP e, CodeBuffer *cb, CompilerContext *cntxt);

SEXP inline_simple_internal_call(SEXP e, SEXP def);
bool simple_formals(SEXP def);
bool simple_args(SEXP icall, SEXP forms);
bool is_simple_internal(SEXP def);
SEXP simple_internals(SEXP pos);

#pragma endregion

#pragma region Constants

static const char *default_suppress_vars[] = {
    ".Generic", ".Method", ".Random.seed", ".self"
};

static const char *const_names[] = {
  "pi", "T", "F", 
  NULL
};

static const char *fold_funs[] = {
  "+", "-", "*", "/", "^", "(",
  ">", ">=", "==", "!=", "<", "<=", "||", "&&", "!",
  "|", "&", "%%",
  "c", "rep", ":",
  "abs", "acos", "acosh", "asin", "asinh", "atan", "atan2",
  "atanh", "ceiling", "choose", "cos", "cosh", "exp", "expm1",
  "floor", "gamma", "lbeta", "lchoose", "lgamma", "log", "log10",
  "log1p", "log2", "max", "min", "prod", "range", "round",
  "seq_along", "seq.int", "seq_len", "sign", "signif",
  "sin", "sinh", "sqrt", "sum", "tan", "tanh", "trunc",
  "baseenv", "emptyenv", "globalenv",
  "Arg", "Conj", "Im", "Mod", "Re",
  NULL
};

static const char *language_funs[] = {
  "^", "~", "<", "<<-", "<=", "<-", "=", "==", ">", ">=",
  "|", "||", "-", ":", "!", "!=", "/", "(", "[", "[<-", "[[",
  "[[<-", "{", "@", "$", "$<-", "*", "&", "&&", "%/%", "%*%",
  "%%", "+", "::", ":::", "@<-",
  "break", "for", "function", "if", "next", "repeat", "while",
  "local", "return", "switch",
  NULL
};

static const char * math1funs[] = {
  "floor", "ceiling", "sign",
  "expm1", "log1p",
  "cos", "sin", "tan", "acos", "asin", "atan",
  "cosh", "sinh", "tanh", "acosh", "asinh", "atanh",
  "lgamma", "gamma", "digamma", "trigamma",
  "cospi", "sinpi", "tanpi",
  NULL
};

static const char *safe_base_internals[] = {
  "atan2",  "besselY", "beta", "choose","drop", "inherits", "is.vector", "lbeta",
  "lchoose","nchar", "polyroot", "typeof", "vector", "which.max","which.min",
  "is.loaded", "identical","match", "rep.int", "rep_len",
  NULL
};

static const char *safe_stats_internals[] = {
  "dbinom",  "dcauchy",  "dgeom",  "dhyper",  "dlnorm", "dlogis",
  "dnorm",  "dpois",  "dunif",  "dweibull", "fft",  "mvfft", 
  "pbinom",  "pcauchy",  "pgeom", "phyper",  "plnorm",  "plogis", 
  "pnorm",  "punif",  "pweibull",  "qbinom",  "qcauchy",  "qgeom",
  "qhyper",  "qlnorm",  "qlogis",  "qnorm",  "qpois",  "qunif",
  "qweibull",  "rbinom",  "rcauchy",  "rgeom",  "rhyper",  "rlnorm",  "rlogis", 
  "rnorm",  "rpois",  "rsignrank",  "runif",  "rweibull",  "rwilcox",  "ptukey",  "qtukey",
  NULL
};

#pragma endregion

#pragma region Notifications

static void get_loc_file_line(Loc loc, const char **file, int *line) {
  *file = NULL;
  *line = -1;

  if (loc.is_null || loc.srcref == R_NilValue) {
    return;
  }

  SEXP utils_ns = R_FindNamespace(Rf_mkString("utils"));
  if (utils_ns == R_UnboundValue || TYPEOF(utils_ns) != ENVSXP) {
    return;
  }

  SEXP getSrcFilename = Rf_findVarInFrame(utils_ns, Rf_install("getSrcFilename"));
  SEXP getSrcLocation = Rf_findVarInFrame(utils_ns, Rf_install("getSrcLocation"));

  if (TYPEOF(getSrcFilename) == CLOSXP || TYPEOF(getSrcFilename) == BUILTINSXP || TYPEOF(getSrcFilename) == SPECIALSXP) {
    int err = 0;
    SEXP call_fn = PROTECT(Rf_lang2(getSrcFilename, loc.srcref));
    SEXP fn = PROTECT(R_tryEval(call_fn, utils_ns, &err));
    if (!err && TYPEOF(fn) == STRSXP && XLENGTH(fn) >= 1 && STRING_ELT(fn, 0) != NA_STRING) {
      *file = CHAR(STRING_ELT(fn, 0));
    }
    UNPROTECT(2);
  }

  if (TYPEOF(getSrcLocation) == CLOSXP || TYPEOF(getSrcLocation) == BUILTINSXP || TYPEOF(getSrcLocation) == SPECIALSXP) {
    int err = 0;
    SEXP what = PROTECT(Rf_mkString("line"));
    SEXP call_ln = PROTECT(Rf_lang3(getSrcLocation, loc.srcref, what));
    SEXP ln = PROTECT(R_tryEval(call_ln, utils_ns, &err));
    if (!err && TYPEOF(ln) == INTSXP && XLENGTH(ln) >= 1 && INTEGER(ln)[0] != NA_INTEGER) {
      *line = INTEGER(ln)[0];
    } else if (!err && TYPEOF(ln) == REALSXP && XLENGTH(ln) >= 1) {
      *line = (int) REAL(ln)[0];
    }
    UNPROTECT(3);
  }
}

static char *add_loc_string(const char *msg, Loc loc) {
  if (!msg) msg = "";

  const char *file = NULL;
  int line = -1;
  get_loc_file_line(loc, &file, &line);

  int needed = (int)strlen(msg) + 1;
  if (file && line > 0) needed += (int)strlen(file) + 32;

  char *full = (char *) R_alloc(needed, sizeof(char));
  if (file && line > 0) {
    snprintf(full, needed, "%s at %s:%d", msg, file, line);
  } else {
    snprintf(full, needed, "%s", msg);
  }

  return full;
}

static void cntxt_warn(const char *msg, CompilerContext *cntxt, Loc loc) {
  if (!msg) return;
  (void)cntxt;

  char *full = add_loc_string(msg, loc);
  Rprintf("Note: %s\n", full);
};

static void cntxt_stop(const char *msg, CompilerContext *cntxt, Loc loc) {
  char *full = add_loc_string(msg, loc);

  SEXP simple_error_sym = Rf_install("simpleError");
  SEXP stop_sym = Rf_install("stop");

  SEXP msg_sexp = PROTECT(Rf_mkString(full));
  SEXP call_obj = (cntxt && cntxt->call != R_NilValue) ? cntxt->call : R_NilValue;
  SEXP se_call = PROTECT(Rf_lang3(simple_error_sym, msg_sexp, call_obj));
  SEXP cond = PROTECT(Rf_eval(se_call, R_BaseEnv));
  SEXP stop_call = PROTECT(Rf_lang2(stop_sym, cond));

  Rf_eval(stop_call, R_BaseEnv);

  UNPROTECT(4);
}

static void notify_wrong_dots_use(SEXP var, CompilerContext *cntxt, Loc loc) {
  if (cntxt->options.suppress_all) return;

  const char * v = CHAR(PRINTNAME(var));

  char msg[256];
  snprintf(msg, sizeof(msg), "%s may be used in an incorrect context", v);
  cntxt_warn(msg, cntxt, loc);
};

static bool suppress_undef(SEXP name, CompilerContext *cntxt) {
  if (cntxt->options.suppress_all) return true;
  if (cntxt->options.suppress_all_undef) return true;

  if (TYPEOF(name) != SYMSXP) return false;
  const char *nm = CHAR(PRINTNAME(name));

  for (int i = 0; i < cntxt->options.num_suppress_vars; i++) {
    const char *s = cntxt->options.suppress_undefined_vars[i];
    if (s && strcmp(nm, s) == 0) return true;
  }
  return false;
}

static void notify_undef_var(SEXP var, CompilerContext *cntxt, Loc loc) {
  if (suppress_undef(var, cntxt)) return;

  const char *nm = (TYPEOF(var) == SYMSXP) ? CHAR(PRINTNAME(var)) : "<unknown>";
  char msg[512];
  snprintf(msg, sizeof(msg), "no visible binding for global variable '%s'", nm);
  cntxt_warn(msg, cntxt, loc);
}

static void notify_undef_fun(SEXP fun, CompilerContext *cntxt, Loc loc) {
  if (suppress_undef(fun, cntxt)) return;

  const char *nm = "<unknown>";
  if (TYPEOF(fun) == SYMSXP) nm = CHAR(PRINTNAME(fun));

  char msg[512];
  snprintf(msg, sizeof(msg), "no visible global function definition for '%s'", nm);
  cntxt_warn(msg, cntxt, loc);
}

static void notify_bad_assign_fun(CompilerContext *cntxt, Loc loc) {
  if (cntxt->options.suppress_all) return;
  cntxt_warn("invalid function in complex assignment", cntxt, loc);
}

static void notify_no_super_assign_var(SEXP symbol, CompilerContext *cntxt, Loc loc) {
  if (cntxt->options.suppress_all) return;
  if (cntxt->options.suppress_no_super_assign) return;

  const char *nm = (TYPEOF(symbol) == SYMSXP) ? CHAR(PRINTNAME(symbol)) : "<unknown>";

  char msg[512];
  snprintf(msg, sizeof(msg), "no visible binding for '<<-' assignment to '%s'", nm);
  cntxt_warn(msg, cntxt, loc);
}

static void notify_wrong_arg_count(SEXP fun, CompilerContext *cntxt, Loc loc) {
  if (cntxt->options.suppress_all) return;
  const char *name = CHAR(PRINTNAME(fun));
  char msg[512];
  snprintf(msg, sizeof(msg), "wrong number of arguments to '%s'", name);
  cntxt_warn(msg, cntxt, loc);
}

static void notify_wrong_break_next(SEXP fun, CompilerContext *cntxt, Loc loc) {
  if (cntxt->options.suppress_all) return;

  const char *name = CHAR(PRINTNAME(fun));
  char msg[512];
  snprintf(msg, sizeof(msg),
           "%s used in wrong context: no loop is visible",
           name);
  cntxt_warn(msg, cntxt, loc);
}

static void notify_local_fun(SEXP fun, CompilerContext *cntxt, Loc loc) {
  (void)fun;
  (void)cntxt;
  (void)loc;
}

static void notify_bad_call(SEXP call, CompilerContext *cntxt, Loc loc) {
  if (cntxt->options.suppress_all) return;

  const char *call_name = "<call>";
  if (TYPEOF(call) == LANGSXP && TYPEOF(CAR(call)) == SYMSXP) {
    call_name = CHAR(PRINTNAME(CAR(call)));
  }

  char msg[512];
  snprintf(msg, sizeof(msg), "possible error in '%s': argument matching failed", call_name);
  cntxt_warn(msg, cntxt, loc);
}

static void notify_multiple_switch_defaults(CompilerContext *cntxt, Loc loc) {
  if (cntxt->options.suppress_all) return;
  cntxt_warn("more than one default provided in switch() call", cntxt, loc);
}

static void notify_no_switch_cases(CompilerContext *cntxt, Loc loc) {
  if (cntxt->options.suppress_all) return;
  cntxt_warn("'switch' with no alternatives", cntxt, loc);
}

static void notify_assign_syntactic_fun(ExtraVars funs, CompilerContext *cntxt, Loc loc) {
  if (cntxt->options.suppress_all) return;
  if (funs.count <= 0) return;

  const char *prefix = (funs.count == 1)
      ? "local assignment to syntactic function: "
      : "local assignments to syntactic functions: ";

  int total_len = (int)strlen(prefix) + 1;
  for (int i = 0; i < funs.count; i++) {
    total_len += (int)strlen(funs.vars[i]);
    if (i + 1 < funs.count) total_len += 2;
  }

  char *msg = (char *) R_alloc(total_len, sizeof(char));
  msg[0] = '\0';
  strcat(msg, prefix);

  for (int i = 0; i < funs.count; i++) {
    strcat(msg, funs.vars[i]);
    if (i + 1 < funs.count) strcat(msg, ", ");
  }

  cntxt_warn(msg, cntxt, loc);
}

static void notify_compiler_error(const char *msg) {
  if (!msg) return;
  Rprintf("Error: compilation failed - %s\n", msg);
}


#pragma endregion

#pragma region o0

static bool is_in_c_set(const char *str, const char *set[]) {
  if (str == NULL) return false;
    
  for (int i = 0; set[i] != NULL; i++) {
    if (strcmp(str, set[i]) == 0) {
      return true;
    }
  }
  return false;
}

static bool is_const_mode(SEXP e) {

  SEXPTYPE t = TYPEOF(e);
  return (t == INTSXP || t == REALSXP || t == LGLSXP
          || t == NILSXP || t == CPLXSXP || t == STRSXP);

};

SEXP check_const(SEXP e) {

  const int MAX_CONST_SIZE = 10;

  if ( is_const_mode(e) && Rf_length(e) <= MAX_CONST_SIZE ) {

    SEXP res = PROTECT(Rf_allocVector( VECSXP, 1 ));
    SET_VECTOR_ELT( res, 0, e );

    UNPROTECT(1); // res
    return res;

  }

  return R_NilValue;

};

static SEXP constant_fold_sym(SEXP var, CompilerContext *cntxt) {

  if (is_in_c_set(CHAR(PRINTNAME(var)), const_names)) {
      DEBUG_PRINT("\t[@] Was in set\n");

    if ( is_base_var(var, cntxt) ) {
      DEBUG_PRINT("\t[@] Was based\n");

      SEXP val = Rf_findVar(var, R_BaseNamespace);

      if (TYPEOF(val) == PROMSXP) {
        int err = 0;
        val = R_tryEval(var, R_BaseNamespace, &err);
        if (err) return R_NilValue;
      }

      DEBUG_PRINT("\t[@] SYMBAHH\n");

      return check_const(val);
  
    }

  }

  return R_NilValue;

}

static SEXP get_fold_fun(SEXP var, CompilerContext *cntxt) {

  if ( is_in_c_set( CHAR(PRINTNAME(var)), fold_funs ) && is_base_var( var, cntxt ) ) {

    SEXP val = Rf_findVar( var, R_BaseNamespace );

    if ( Rf_isFunction(val) )
      return val;

  }

  return R_NilValue;

};

static SEXP constant_fold_call(SEXP e, CompilerContext* cntxt) {

  SEXP fun = CAR(e);

  if (TYPEOF(fun) == SYMSXP) {

    SEXP ffun = get_fold_fun(fun, cntxt);

    if (ffun != R_NilValue) {

      int n_args = 0;

      SEQ_ALONG( s, CDR(e) ) {
        n_args++;
      }

      // Allocate temporary storage for folded values and their names (tags)
      // We use VECSXP to hold them safely during processing
      SEXP folded_values = PROTECT(allocVector(VECSXP, n_args));
      SEXP arg_tags = PROTECT(allocVector(VECSXP, n_args));

      bool ok = true;

      // Iterate original arguments: fold and store
      SEQ_ALONG_IX( s, CDR(e), i ) {
        
        SEXP a = CAR(s);
        SEXP tag = TAG(s);

        if (a == R_MissingArg) {
          ok = false;
          break;
        }

        Loc null_loc = {0};
        SEXP val_wrapper = constant_fold(a, cntxt, null_loc);

        if (val_wrapper != R_NilValue) {

          SEXP val = VECTOR_ELT(val_wrapper, 0);

          if (!is_const_mode(val)) {
            ok = false;
            break;
          }

          SET_VECTOR_ELT(folded_values, i, val);
          SET_VECTOR_ELT(arg_tags, i, tag);  // original name

        } else {

          ok = false;
          break;
        
        }
      }

      if (ok) {

        SEXP arglist = PROTECT(allocList(n_args));
        SEXP node = arglist;

        // Build the argument list from back to front
        for (int j = 0; j < n_args; j++) {
          
          SEXP val = VECTOR_ELT(folded_values, j);
          SEXP tag = VECTOR_ELT(arg_tags, j);

          SETCAR(node, val);

          if (tag != R_NilValue)
            SET_TAG(node, tag);

          node = CDR(node);
        }

        SEXP new_call = PROTECT( LCONS(ffun, arglist) );

        int error = 0;
        SEXP result = R_tryEval(new_call, R_BaseEnv, &error);

        if (!error) {
          PROTECT(result);                                  // +1
          SEXP out = check_const(result);                   // allocates
          UNPROTECT(5);  // result, new_call, arglist, folded_values, arg_tags
          return out;
        }

      UNPROTECT(4);    // new_call, arglist, folded_values, arg_tags
      return R_NilValue;
    }
  }

  }

  return R_NilValue;

}

static bool dots_or_missing(SEXP args) {

  SEQ_ALONG( s, args ) {  
    SEXP val = CAR(s);
    if (val == R_DotsSymbol || val == R_MissingArg) {
      return true;
    }
  }

  return false;
}

SEXP constant_fold(SEXP e, CompilerContext* cntxt, Loc loc) {
  
  SEXPTYPE type = TYPEOF(e);

  switch (type) {
    case LANGSXP:
      return constant_fold_call(e, cntxt);

    case SYMSXP:
      return constant_fold_sym(e, cntxt);

    case PROMSXP:
      return R_NilValue;

    case BCODESXP:
      return R_NilValue;

    default:
      return check_const(e);
  }
};

bool cmp_special(SEXP e, CodeBuffer *cb, CompilerContext *cntxt) {

  SEXP fun = CAR(e);

  if ( TYPEOF(fun) == CHARSXP ) {
    fun = install( CHAR( fun ) );
  }

  int ci = PUTCONST( e );
  PUTCODES( CALLSPECIAL_OP, ci );

  if (  cntxt->tailcall )
    PUTCODE( RETURN_OP );

  return true;

};


void cmp_builtin_args(SEXP args, CodeBuffer *cb, CompilerContext *cntxt, bool missingOK) {
    
  CompilerContext * ncntxt = make_arg_ctx( cntxt );

  SEQ_ALONG( s, args ) {

    SEXP a = CAR(s);
    SEXP n = TAG(s);

    if (a == R_MissingArg) {

      if (missingOK) {
      
        PUTCODE( DOMISSING_OP );
        cmp_tag(n, cb);
      
      } else {
        cntxt_stop("missing arguments are not allowed", cntxt, cb_savecurloc(cb));

      }

    } else {

      if (TYPEOF(a) == SYMSXP) {

        Loc loc = cb_savecurloc(cb);
        SEXP ca = constant_fold(a, cntxt, loc);

        if (ca == R_NilValue) {
          cmp_sym(a, cb, ncntxt, missingOK);
          PUTCODE( PUSHARG_OP );
        } else {
          // TODO constant folded to a constant?
          SEXP ca_value = VECTOR_ELT(ca, 0); 
          cmp_const_arg(ca_value, cb, cntxt);
        }
      
        } else if (TYPEOF(a) == LANGSXP) {
        
          cmp(a, cb, ncntxt, false, true);        
          PUTCODE( PUSHARG_OP );
        
        } else {
          cmp_const_arg(a, cb, cntxt);
        }

      cmp_tag(n, cb);
    }

  }

}


bool cmp_builtin( SEXP e, CodeBuffer *cb, CompilerContext *cntxt, bool internal ) {

  SEXP fun = CAR( e );
  SEXP args = CDR( e );

  if ( dots_or_missing( args ) ) {
    
    return false;

  } else {

    int ci = PUTCONST( fun );

    if (internal)
      PUTCODE( GETINTLBUILTIN_OP );
    else
      PUTCODE( GETBUILTIN_OP );
    
    PUTCODE( ci );
  
    cmp_builtin_args( args, cb, cntxt, false );
    ci = PUTCONST( e );

    PUTCODES( CALLBUILTIN_OP, ci );

    if ( cntxt->tailcall )
      PUTCODE( RETURN_OP );
    
    return true;
  }

}


bool any_dots( SEXP args ) {

  SEQ_ALONG( s, args ) {
    SEXP arg = CAR( s );
    if ( arg != R_MissingArg && arg == R_DotsSymbol ) {
      return true;
    }
  }
  return false;

};


InlineInfo get_inline_info(const char *name, CompilerContext* cntxt, bool guard_ok) {
  
  InlineInfo ret;
  ret.can_inline = false;
  ret.guard_needed = false;
  ret.in = NO;

  if (cntxt->options.optimize_level == 0 || strcmp(name, "standardGeneric") == 0)
    return ret;

  VarInfo info = find_cenv_var(install(name), cntxt->env);

  if (!info.found || info.env == NULL)
    return ret;

  if (info.defining_frame != NULL && info.defining_frame->type == FRAME_LOCAL)
    return ret;

  SEXP from = info.env;

  bool is_namespace = R_IsNamespaceEnv(from);
  bool is_base = false;
  bool is_stat = false;

  if (is_namespace) {
    // It's an internal namespace hit
    is_base = (from == R_BaseNamespace);
    if (!is_base) {
      SEXP spec = R_NamespaceEnvSpec(from);
      if (TYPEOF(spec) == STRSXP && LENGTH(spec) > 0) {
        is_stat = (strcmp(CHAR(STRING_ELT(spec, 0)), "stats") == 0);
      }
    }
  } else {

    is_base = (from == R_BaseEnv);

    is_stat = false;
    if (R_IsPackageEnv(from)) {
      SEXP pkg_name = R_PackageEnvName(from);
      if (TYPEOF(pkg_name) == STRSXP && LENGTH(pkg_name) > 0) {
        is_stat = (strcmp(CHAR(STRING_ELT(pkg_name, 0)), "package:stats") == 0);
      }
    }

  }

  if (!is_base && !is_stat) {
    return ret;
  }

  if (is_namespace) {
    // Level 1+ allows namespace hits without guards
    ret.can_inline = true;
    ret.guard_needed = false;
    ret.in = is_base ? BASE : STAT;
    ret.env = from;
  } else {
    // It's a global hit. Check optimization levels 2 and 3.
    if (cntxt->options.optimize_level >= 3 ||
        (cntxt->options.optimize_level >= 2 && is_in_c_set(name, language_funs))) {
      // Level 3, or Level 2 + core language function: No guard needed
      ret.can_inline = true;
      ret.guard_needed = false;
      ret.in = is_base ? BASE : STAT;
      ret.env = from;
    } else if (guard_ok && is_base) {
      // Level 2 + normal base function + guard is allowed
      ret.can_inline = true;
      ret.guard_needed = true;
      ret.in = BASE;
      ret.env = from;
    }
  }

  return ret;
}

bool try_inline(SEXP e, CodeBuffer* cb, CompilerContext* cntxt) {

  const char* name = CHAR(PRINTNAME(CAR(e))); 

  InlineInfo info = get_inline_info(name, cntxt, true);

  if (!info.can_inline) return false;

  HandlerFn handler = NULL;

  if (!get_inline_handler(name, info.in, cntxt, &handler)) return false;

  DEBUG_PRINT(">> Inlining function '%s' \n");

  if (info.guard_needed) {
    bool tailpos = cntxt->tailcall;
    if (tailpos) cntxt->tailcall = false;

    int expridx = PUTCONST(e);
    int endlabel = cb_makelabel(cb);

    PUTCODES(BASEGUARD_OP, expridx);
    cb_putcodelabel(cb, endlabel);

    if (!handler(e, cb, cntxt))
      cmp_call(e, cb, cntxt, false);

    cb_putlabel(cb, endlabel);

    if (tailpos) {
      PUTCODE(RETURN_OP);
      cntxt->tailcall = true;
    }

    return true;
  }

  return handler(e, cb, cntxt);
}

void cb_putswitch(CodeBuffer *cb, int * int_labels, int int_count, int int_pos, int * char_labels, int char_count, int char_pos) {

  SwitchPatch * patch = (SwitchPatch *) R_alloc(1, sizeof(SwitchPatch));
  
  patch->has_char_labels = (char_count > 0);
  
  patch->char_code_offset = char_pos;
  patch->num_code_offset = int_pos;
  
  patch->n_char = char_count;
  patch->n_num = int_count;

  patch->char_labels = char_labels;
  patch->num_labels = int_labels;
  patch->next = NULL;

  if ( cb->switch_patches == NULL ) {
    cb->switch_patches = patch;
    cb->patch_tail = patch; // Track the tail for O(1) appends
  } else {
    cb->patch_tail->next = patch;
    cb->patch_tail = patch;
  }

}

void ensure_label_capacity(LabelTable *lt, int needes_index) {

  if ( needes_index < lt->capacity ) {
    return; // Enough capacity
  }

  int old_cap = lt->capacity;
  int* old_table = lt->table;

  int new_cap = old_cap == 0 ? 16 : old_cap * 2;

  while ( needes_index >= new_cap ) {
    new_cap *= 2;
  }

  lt->table = (int *) R_alloc ( new_cap, sizeof( int ) );
  lt->capacity = new_cap;

  // Initialize new entries to -1 and copy old entries
  for ( int i = 0; i < new_cap; i++ ) {
    if ( i < old_cap ) {
      lt->table[i] = old_table[i];
    } else {
      lt->table[i] = -1; // Unset
    }
  }

};

// Set the jump target
void cb_putlabel(CodeBuffer * cb, int label_id) {

  int needed_index = label_id;
  ensure_label_capacity(&cb->label_table, needed_index);

  cb->label_table.table[needed_index] = (cb->code_count) + 1;

}

// Set up the jump source
void cb_putcodelabel(CodeBuffer * cb, int label_id) {
  PUTCODE(-(label_id + 1));
}

int cb_makelabel(CodeBuffer * cb) {

  cb->label_table.labels_issued++;
  int label_id = cb->label_table.labels_issued;

  return label_id;

}

void cb_patch_switches(CodeBuffer *cb) {

  SwitchPatch *patch = cb->switch_patches;

  while (patch != NULL) {
    
    if (patch->has_char_labels) {
      SEXP char_sexp = PROTECT(allocVector(INTSXP, patch->n_char));
      for (int i = 0; i < patch->n_char; i++) {
        INTEGER(char_sexp)[i] = cb->label_table.table[patch->char_labels[i]];
      }
      int char_idx = PUTCONST(char_sexp);
      cb->code[patch->char_code_offset] = char_idx; 
      UNPROTECT(1); 
    }

    SEXP num_sexp = PROTECT(allocVector(INTSXP, patch->n_num));
    for (int i = 0; i < patch->n_num; i++) {
      INTEGER(num_sexp)[i] = cb->label_table.table[patch->num_labels[i]];
    }
    int num_idx = PUTCONST(num_sexp);
    cb->code[patch->num_code_offset] = num_idx;
    UNPROTECT(1);

    patch = patch->next;
  }
  
  cb->switch_patches = NULL;
  if (cb->patch_tail) cb->patch_tail = NULL;
}

void cb_patchlabels(CodeBuffer * cb) {

  for ( int i = 0; i < cb->code_count; i++ ) {

    int code = cb->code[i];
    // Negative value indicates a label reference
    if ( code < 0 ) {
      // Convert to index
      int needed_index = (-(code + 1));
      
      if ( needed_index >= cb->label_table.capacity )
        Rf_error("Unresolved label reference: %d", code);
      
      int table_result = cb->label_table.table[needed_index];
    
      if ( table_result == -1 )
        Rf_error("Unresolved label reference: %d", code);

      
      cb->code[i] = table_result;
    
    }
  }

  cb_patch_switches(cb);

}

SEXP extract_srcref(SEXP sref, int idx) {

  if ( Rf_isList(sref) && Rf_length(sref) >= idx )
    return VECTOR_ELT(sref, idx - 1);

  if ( Rf_isInteger(sref) && Rf_length(sref) >= 6 )
    return sref;

  return R_NilValue;

}

SEXP get_expr_srcref(SEXP expr) {
  SEXP sattr = Rf_getAttrib(expr, Rf_install("srcref"));
  return extract_srcref(sattr, 1);
}

SEXP get_block_srcref(SEXP block_sref, int idx) {
  return extract_srcref(block_sref, idx);
}

Loc cb_savecurloc(CodeBuffer *cb) {
  Loc saved;
  saved.expr = cb->current_expr;
  saved.srcref = cb->current_srcref;
  return saved;
}

void cb_restorecurloc(CodeBuffer *cb, Loc saved) {
  if (cb->expr_tracking_on) cb->current_expr = saved.expr;
  if (cb->srcref_tracking_on) cb->current_srcref = saved.srcref;
}

void cb_setcurloc(CodeBuffer *cb, SEXP expr, SEXP sref) {
  if (cb->expr_tracking_on) cb->current_expr = expr;
  if (cb->srcref_tracking_on) cb->current_srcref = sref;
}

void cb_setcurexpr(CodeBuffer *cb, SEXP expr) {
  if (cb->expr_tracking_on) cb->current_expr = expr;

  if (cb->srcref_tracking_on) {
    SEXP sref = get_expr_srcref(expr);

    if ( sref != R_NilValue ) {
        cb->current_srcref = sref;
    }
  }
}

static bool is_base_var(SEXP sym, CompilerContext *cntxt) {

  InlineInfo info = get_inline_info( CHAR(PRINTNAME(sym)), cntxt, false );
  return ( info.can_inline &&
    ( info.env == R_BaseEnv || info.env == R_BaseNamespace) );

};

SEXP find_fun_def( SEXP fun_sym, CompilerContext * cntxt ) {

  CompilerEnv * cenv = cntxt->env;
  VarInfo var_info = find_cenv_var( fun_sym, cenv );

  if ( var_info.found ) {
    SEXP val = var_info.value;
    
    // TODO base R functions seem to be lazyloaded
    // as promises?
    if (TYPEOF(val) == PROMSXP) {
        
      int err = 0;
      
      // Evaluating the symbol in the top frame forces the promise safely
      val = R_tryEval(fun_sym, cenv->r_env, &err);
      if (err) return R_NilValue;
    }
    
    if ( Rf_isFunction( val ) ) {
      return val;
    }
  }

  return R_NilValue; // Not found
}

static SEXP eval_match_call(SEXP def, SEXP call, bool expand_dots, bool *had_error) {
  if (had_error) *had_error = false;

  SEXP quoted_call = PROTECT(Rf_lang2(install("quote"), call));
  SEXP match_call_expr;

  if (expand_dots) {
    match_call_expr = PROTECT(Rf_lang3(install("match.call"), def, quoted_call));
  } else {
    match_call_expr = PROTECT(Rf_lang4(install("match.call"), def, quoted_call, R_FalseValue));
  }

  int err_occurred = 0;
  SEXP matched_call = R_tryEval(match_call_expr, R_BaseEnv, &err_occurred);

  if (err_occurred) {
    if (had_error) *had_error = true;
    UNPROTECT(2);
    return R_NilValue;
  }

  UNPROTECT(2);
  return matched_call;
}

bool check_call(SEXP def, SEXP call, bool *should_warn) {

  int type = TYPEOF(def);
  int n_protect = 0;

  if (should_warn) *should_warn = false;

  if (type == BUILTINSXP || type == SPECIALSXP) {
    SEXP args_call = PROTECT(Rf_lang2(install("args"), def));
    def = PROTECT(Rf_eval(args_call, R_BaseEnv));
    n_protect += 2;
  }

  SEQ_ALONG( runner, CDR(call) ) {
    if (CAR(runner) == R_DotsSymbol) {
      UNPROTECT(n_protect);
      return false;
    }
  }

  if (def == R_NilValue || TYPEOF(def) != CLOSXP) {
    UNPROTECT(n_protect);
    return false;
  }

  bool err_occurred = false;
  SEXP matched = eval_match_call(def, call, true, &err_occurred);

  if (matched == R_NilValue && err_occurred) {
    if (should_warn) *should_warn = true;
    UNPROTECT(n_protect);
    return false;
  }

  UNPROTECT(n_protect);
  return true;
}

bool is_ddsym(SEXP sym) {

  if (TYPEOF(sym) != SYMSXP)
    return false;

  const char *name = CHAR(PRINTNAME(sym));

  if (name[0] != '.' || name[1] != '.')
    return false;

  if (name[2] == '\0')
    return false;

  for (int i = 2; name[i] != '\0'; i++) {
    if (!isdigit((unsigned char)name[i])) {
      return false;
    }
  }

  return true;
}

bool find_var( SEXP var, CompilerContext * cntxt ) {

  CompilerEnv * cenv = cntxt->env;
  VarInfo var_info = find_cenv_var( var, cenv );

  if (!var_info.found) {
    DEBUG_PRINT("?? find_var: Symbol '%s' NOT found in scope\n", CHAR(PRINTNAME(var)));
  }
  return var_info.found;

}

bool find_loc_var( SEXP var, CompilerContext * cntxt ) {

  CompilerEnv * cenv = cntxt->env;
  VarInfo var_info = find_cenv_var( var, cenv );

  if ( var_info.found && var_info.defining_frame != NULL && var_info.defining_frame->type == FRAME_LOCAL ) {
    DEBUG_PRINT("++ find_loc_var: Symbol '%s' found in LOCAL scope\n", CHAR(PRINTNAME(var)));
    return true;
  } else {
    DEBUG_PRINT("?? find_loc_var: Symbol '%s' NOT found in LOCAL scope\n", CHAR(PRINTNAME(var)));
    return false;
  }

}

VarInfo find_cenv_var(SEXP var, CompilerEnv* cenv) {
  
  VarInfo info = {NULL, R_NilValue, R_NilValue, false};
  const char* var_name = CHAR(PRINTNAME(var));
  EnvFrame* current = cenv->top_frame;

  while (current != NULL) {
    if (current->extra_vars.count != 0) {
      for (int i = 0; i < current->extra_vars.count; i++) {
        if (strcmp(var_name, current->extra_vars.vars[i]) == 0) {
          info.defining_frame = current;
          info.env = R_NilValue;         
          info.found = true;
          return info;
        }
      }
    }
    current = current->parent;
  }

  SEXP env = cenv->r_env;
  while (env != R_NilValue && env != R_EmptyEnv) {
    SEXP val = Rf_findVarInFrame3(env, var, FALSE); // TODO why did TRUE not work
    if (val != R_UnboundValue) {
      info.defining_frame = NULL;
      info.env = env;
      info.value = val;
      info.found = true;
      return info;
    }
    env = ENCLOS(env);
  }

  return info;  // Not found
}
const char * get_assigned_var( SEXP var, CompilerContext *cntxt ) {

  SEXP v = CADR( var );

  if ( v == R_MissingArg ) {
    Loc nloc = {true, R_NilValue, R_NilValue};
    cntxt_stop("bad assignment", cntxt, nloc);
    return NULL; 
  }

  // Handle strings and symbols
  if ( TYPEOF( v ) == SYMSXP ) return CHAR( PRINTNAME(v) );
  else if ( TYPEOF( v ) == STRSXP && Rf_length(v) > 0  ) return CHAR( STRING_ELT(v, 0) );
  else if ( TYPEOF( v ) == CHARSXP ) return CHAR( v );
  else {
    // Handle complex assignments names(x) <- 1
    while ( TYPEOF( v ) == LANGSXP ) {
      if ( Rf_length( v ) < 2 ) {
        Loc nloc = {true, R_NilValue, R_NilValue};
        cntxt_stop("bad assignment", cntxt, nloc);
      }
      v = CADR( v );
      if ( v == R_MissingArg ) {
        Loc nloc = {true, R_NilValue, R_NilValue};
        cntxt_stop("bad assignment", cntxt, nloc);
      }
    }

    if ( TYPEOF( v ) != SYMSXP ) {
      Loc nloc = {true, R_NilValue, R_NilValue};
      cntxt_stop("bad assignment", cntxt, nloc);
    }

    return CHAR( PRINTNAME(v) ); 
  }

};

static bool is_in_set(SEXP sym, SEXP set) {
  if (set == R_NilValue) return false;
  const char *name = CHAR(PRINTNAME(sym));
  
  for (int i = 0; i < LENGTH(set); i++) {
    const char *el = CHAR(STRING_ELT(set, i));
    if (strcmp(name, el) == 0) return true;
  }
  return false;
}

static ExtraVars union_sets(ExtraVars a, ExtraVars b) {

  if (a.count == 0) return b;
  if (b.count == 0) return a;

  int len_a = a.count;
  int len_b = b.count;
  int max_len = len_a + len_b;

  const char ** buffer = (const char **) R_alloc(max_len, sizeof(const char*));
  int count = 0;

  for (int i = 0; i < len_a; i++) {
    buffer[count++] = a.vars[i];
  }

  for (int i = 0; i < len_b; i++) {
    const char * val_b = b.vars[i];
    bool found = false;
    
    for (int j = 0; j < len_a; j++) {
      if (buffer[j] == val_b) { 
        found = true;
        break;
      }
    }
    
    if (!found) {
      buffer[count++] = val_b;
    }
  }

  ExtraVars ret;
  ret.count = count;
  ret.vars = buffer;

  return ret;

};

ExtraVars find_locals_list( SEXP elist, ExtraVars known_locals, CompilerContext *cntxt ) {

  // Initialize empty list of locals
  ExtraVars found;
  found.count = 0;
  found.vars = NULL;

  // Iterate over expressions in the list
  SEXP node = elist;
  while ( node != R_NilValue ) {
    
    // Get the first element
    SEXP expr = CAR( node );

    // Find locals in the expression
    ExtraVars new_vars = find_locals( expr, known_locals, cntxt );

    if ( new_vars.count != 0 ) {
      if (found.count == 0)
          found = new_vars;
      else
        found = union_sets(found, new_vars);
    }

    node = CDR( node );
  }
  return found;
};

ExtraVars find_locals( SEXP expr, ExtraVars known_locals, CompilerContext *cntxt ) {

  ExtraVars ret;
  ret.count = 0;
  ret.vars = NULL;

  // Base case: expression is not a function call
  if ( TYPEOF( expr ) != LANGSXP ) return ret;

  // here we know expr is a LANGSXP
  SEXP fun = CAR( expr );

  // Lambda or anonymous function call
  if ( TYPEOF( fun ) != SYMSXP )
    return find_locals_list( expr, known_locals, cntxt );

  // Its a function call with a symbol as function name
  const char* fname = CHAR( PRINTNAME( fun ) );

  // Its an assignment?
  if ( strcmp( fname, "<-" ) == 0 || strcmp( fname, "=" ) == 0 ) {
    
    // Assignment LHS (the variable being assigned to)
    const char * var =  get_assigned_var( expr, cntxt );

    ret.count = 1;
    ret.vars = (const char **) R_alloc(1, sizeof(const char *));
    ret.vars[0] = var;
    
    // Recurse into RHS
    ExtraVars rhs_locals = find_locals( CADDR( expr ), known_locals, cntxt );

    return union_sets( rhs_locals, ret );

  }

  // A for loop
  if ( strcmp( fname, "for" ) == 0 ) {

    const char * loop_var_raw = CHAR( PRINTNAME( CADR( expr ) ) );
    ExtraVars seq_locals = find_locals( CADDR( expr ), known_locals, cntxt );
    ExtraVars body_locals = find_locals( CADDDR( expr ), known_locals, cntxt );

    const char ** loop_var_arr = (const char **) R_alloc(1, sizeof(const char *));
    loop_var_arr[0] = loop_var_raw;
    ExtraVars loop_var = { loop_var_arr, 1 };

    return union_sets( union_sets( seq_locals, loop_var ), body_locals );
  
  }

  // Scope barriers / primitives
  if ( strcmp( fname, "function" ) == 0 ||
       strcmp( fname, "quote" ) == 0 ||
       strcmp( fname, "expression" ) == 0 ) {

      const char * cmpname = CHAR( PRINTNAME( fun ) ); 
      bool found = false;

      for ( int i = 0; i < known_locals.count; i++ ) {

        if ( strcmp( cmpname, known_locals.vars[i] ) == 0 ) {
          found = true;
          break;
        }

      }

      if ( ! found )
        return ret;
    
  }
    
  return find_locals_list( CDR( expr ), known_locals, cntxt );

};

void add_cenv_vars( CompilerEnv * cenv, ExtraVars vars ) {

  // Check if there is something to add
  if (vars.count == 0) return;

  #ifdef DEBUG
  if (TYPEOF(vars) == STRSXP && LENGTH(vars) > 0) {
      DEBUG_PRINT("vv add_cenv_vars: Adding locals to frame: ");
      for(int i=0; i < LENGTH(vars); i++) {
          DEBUG_PRINT("%s ", CHAR(STRING_ELT(vars, i)));
      }
      DEBUG_PRINT("\n");
  }
  #endif

  ExtraVars current_vars = cenv->top_frame->extra_vars;
  ExtraVars combined_vars = union_sets( current_vars, vars );
  
  cenv->top_frame->extra_vars = combined_vars;
  
};


void add_cenv_frame( CompilerEnv * cenv, ExtraVars vars ) {

  EnvFrame * new_frame = (EnvFrame *) R_alloc (1, sizeof( EnvFrame ));

  ExtraVars nil = {NULL,0};

  new_frame->type = FRAME_LOCAL; 
  new_frame->parent = cenv->top_frame;
  new_frame->extra_vars = nil; // Initialize to avoid garbage

  cenv->top_frame = new_frame;

  add_cenv_vars( cenv, vars );

};

// @manual 5.1
CompilerEnv * make_cenv( SEXP env ) {

  // Allocate the compilation environment entry point
  CompilerEnv *cenv = (CompilerEnv *) R_alloc (1, sizeof( CompilerEnv ));
  cenv->r_env = env;
  
  // Allocate the topmost frame
  cenv->top_frame = (EnvFrame *) R_alloc (1, sizeof( EnvFrame ));

  ExtraVars nil = {NULL,0};

  cenv->top_frame->parent = NULL;
  cenv->top_frame->extra_vars = nil;

  return cenv;

}

ExtraVars extract_names( SEXP forms ) {

  ExtraVars empty = { NULL, 0 };

  if ( forms == R_NilValue || length(forms) == 0 )
    return empty;

  ExtraVars vars = { (const char **) R_alloc( sizeof( const char * ), length( forms ) ), length( forms ) };

  int count = 0;
  SEQ_ALONG( iter, forms ) {
    if ( TAG(iter) != R_NilValue ) {
      vars.vars[count] = CHAR( PRINTNAME( TAG( iter ) ) );
      count++;
    }
  }

  vars.count = count;
  return vars;

}
// @manual 5.2
CompilerEnv * make_fun_env( SEXP forms, SEXP body, CompilerContext * cntxt ) {

  CompilerEnv *new_cenv = (CompilerEnv *) R_alloc (1, sizeof( CompilerEnv ));
  new_cenv->top_frame = cntxt->env->top_frame;
  new_cenv->r_env = cntxt->env->r_env;

  add_cenv_frame( new_cenv, extract_names(forms) );

  ExtraVars nullv = {NULL, 0};
  ExtraVars locals = find_locals_list( forms, nullv, cntxt );


  ExtraVars arg_names = extract_names(forms);
  ExtraVars tmp = union_sets( locals, arg_names );
  locals = tmp;

  while ( true ) {

    ExtraVars new_found = find_locals( body, locals, cntxt );
    ExtraVars combined = union_sets( locals, new_found );

    if ( combined.count == locals.count )
      break;

    locals = combined;

  }

  const char *special_syntax_funs[] = {"~", "<-", "=", "for", "function", NULL};
  const char **syntactic = (const char **) R_alloc(locals.count, sizeof(const char *));
  int syntactic_count = 0;

  for (int i = 0; i < locals.count; i++) {
    if (is_in_c_set(locals.vars[i], special_syntax_funs)) {
      syntactic[syntactic_count++] = locals.vars[i];
    }
  }

  if (syntactic_count > 0) {
    ExtraVars sf;
    sf.vars = syntactic;
    sf.count = syntactic_count;
    Loc nloc;
    nloc.is_null = true;
    nloc.expr = R_NilValue;
    nloc.srcref = R_NilValue;
    notify_assign_syntactic_fun(sf, cntxt, nloc);
  }

  add_cenv_vars( new_cenv, locals );  
  return new_cenv;
}

CompilerOptions extract_options(SEXP optlist) {
  
  CompilerOptions ret;

  // Compiler defaults
  ret.optimize_level = 2;
  ret.suppress_all = true;
  ret.suppress_no_super_assign = false;
  ret.suppress_all_undef = false;
  ret.num_suppress_vars = 4;
  ret.suppress_undefined_vars = default_suppress_vars;

  if (optlist == R_NilValue || TYPEOF(optlist) != VECSXP) {
    return ret;
  }

  SEXP names = getAttrib(optlist, R_NamesSymbol);
  if (names == R_NilValue) {
    return ret;
  }

  const char* opt_names[4] = {"optimize", "suppressAll",
                              "suppressNoSuperAssignVar", "suppressUndefined"};

  SEXP opt_vals[4] = {R_NilValue, R_NilValue, R_NilValue, R_NilValue};

  int len = length(optlist);
  for (int i = 0; i < len; i++) {
    const char* key = CHAR(STRING_ELT(names, i));
    for (int j = 0; j < 4; j++) {
      if (strcmp(key, opt_names[j]) == 0) {
        opt_vals[j] = VECTOR_ELT(optlist, i);
        break;
      }
    }
  }

  // optimize
  if (opt_vals[0] != R_NilValue && isNumeric(opt_vals[0]))
    ret.optimize_level = asInteger(opt_vals[0]);

  // suppressAll
  if (opt_vals[1] != R_NilValue && isLogical(opt_vals[1]))
    ret.suppress_all = asLogical(opt_vals[1]);

  // suppressNoSuperAssignVar
  if (opt_vals[2] != R_NilValue && isLogical(opt_vals[2]))
    ret.suppress_no_super_assign = asLogical(opt_vals[2]);

  // suppressUndefined
  if (opt_vals[3] != R_NilValue) {
    if (isLogical(opt_vals[3])) {
      
      int val = asLogical(opt_vals[3]);
      
      if (val == 1) {
        ret.suppress_all_undef = true;
        ret.num_suppress_vars = 0;
        ret.suppress_undefined_vars = NULL;
      } else if (val == 0) {
        ret.suppress_all_undef = false;
        ret.num_suppress_vars = 0;
        ret.suppress_undefined_vars = NULL;
      }
    
    } else if (isString(opt_vals[3])) {
      ret.suppress_all_undef = false;
      int n = length(opt_vals[3]);
      ret.num_suppress_vars = n;

      if (n > 0) {
        ret.suppress_undefined_vars = (const char**) R_alloc (n, sizeof(const char*));
        for (int k = 0; k < n; k++) {
          ret.suppress_undefined_vars[k] = CHAR(STRING_ELT(opt_vals[3], k));
        }
      } else {
        ret.suppress_undefined_vars = NULL;
      }
    }
  }

  return ret;
}

// @manual 4.1
CompilerContext * make_toplevel_ctx( CompilerEnv *cenv, SEXP options ) {
  
  CompilerContext *ctx = (CompilerContext *) R_alloc (1, sizeof(CompilerContext) );

  ctx->toplevel = true;
  ctx->tailcall = true;
  ctx->need_return_jmp = false;

  ctx->options = extract_options( options );  
  ctx->env = cenv;
  ctx->loop.null = true;
  ctx->call = R_NilValue;

  return ctx;

}

// @manual 4.2
CompilerContext * make_function_ctx( CompilerContext * cntxt, CompilerEnv * fenv, SEXP forms, SEXP body ) {

  CompilerEnv * env = fenv;
  CompilerContext * ncntxt = make_toplevel_ctx( env, R_NilValue );
  ncntxt->options = cntxt->options;

  return ncntxt;
};

// @manual 4.2
CompilerContext * make_call_ctx( CompilerContext * cntxt, SEXP call ) {

  CompilerContext * ncntxt = (CompilerContext *) R_alloc (1, sizeof(CompilerContext) );
  
  // Copy over the parent
  *ncntxt = *cntxt;

  ncntxt->call = call;
  return ncntxt;

};

// manual 4.2
CompilerContext * make_non_tail_call_ctx( CompilerContext * cntxt ) {

  CompilerContext * ncntxt = (CompilerContext *) R_alloc (1, sizeof(CompilerContext) );

  *ncntxt = *cntxt;

  ncntxt->tailcall = false;
  return ncntxt;
  
};

// @manual 4.2
CompilerContext * make_no_value_ctx( CompilerContext * cntxt ) {

  CompilerContext * ncntxt = (CompilerContext *) R_alloc (1, sizeof(CompilerContext) );

  *ncntxt = *cntxt;

  ncntxt->tailcall = false;
  return ncntxt;
  
};

// @manual 4.2
CompilerContext * make_loop_ctx( CompilerContext * cntxt, int loop_label, int end_label ) {

  CompilerContext * ncntxt = make_no_value_ctx( cntxt );

  ncntxt->loop.loop_label_id = loop_label;
  ncntxt->loop.end_label_id = end_label;
  ncntxt->loop.goto_ok = true;
  ncntxt->loop.null = false;

  return ncntxt;

};

// @manual 4.2
CompilerContext * make_arg_ctx( CompilerContext * cntxt ) {

  // Alloc new instance
  CompilerContext * ncntxt = (CompilerContext *) R_alloc (1, sizeof(CompilerContext) );

  // Copy over the parent
  *ncntxt = *cntxt;
  
  ncntxt->tailcall = false;
  ncntxt->toplevel = false;
  
  if ( ! ncntxt->loop.null )
    ncntxt->loop.goto_ok = false;

  return ncntxt;

};

// @manual 4.2
CompilerContext * make_promise_ctx( CompilerContext * cntxt ) {

  CompilerContext * ncntxt = (CompilerContext *) R_alloc (1, sizeof(CompilerContext) );

  // Copy over the parent
  *ncntxt = *cntxt;

  ncntxt->tailcall = true;
  ncntxt->toplevel = false;
  ncntxt->need_return_jmp = true;

  if ( ! ncntxt->loop.null )
    ncntxt->loop.goto_ok = false;

  return ncntxt;

};

// @manual 3
CodeBuffer * make_code_buffer( SEXP preseed, Loc loc ) {

  CodeBuffer * cb = (CodeBuffer *) R_alloc (1, sizeof( CodeBuffer ) );

  cb->expr_tracking_on = true;
  cb->srcref_tracking_on = true;

  if ( loc.is_null ) {

    cb->current_expr = preseed;
    cb->current_srcref = get_expr_srcref( preseed );
  
  } else {
  
    cb->current_expr = loc.expr;
    cb->current_srcref = loc.srcref;
  
  }

  if ( Rf_isNull( cb->current_srcref ) )
    cb->srcref_tracking_on = false;

  cb->constant_pool_handle = Rf_allocVector(VECSXP, 1);
  PROTECT( cb->constant_pool_handle );

  // Initialize code buffer itself
  cb->code_capacity = 128; // Initial capacity
  cb->code_count = 0;
  cb->code = (int *) R_alloc ( cb->code_capacity, sizeof( int ) );
  
  // Initialize constant pool
  cb->constant_pool = R_NilValue; // Empty list
  cb->const_count = 0;

  cb->switch_patches = NULL;
  cb->patch_tail = NULL;

  // Initialize label table
  LabelTable lt;
  lt.capacity = 0;
  lt.labels_issued = 0;
  lt.table = NULL;

  cb->label_table = lt;

  // Initialize source tracking
  cb->expr_buf = (int *) R_alloc ( cb->code_capacity, sizeof( int ) );
  cb->srcref_buf = (int *) R_alloc ( cb->code_capacity, sizeof( int ) );

  PUTCONST(preseed);

  UNPROTECT(1); // constant_pool_handle
  return cb;

};

// @manual 2.4
void cmp_const( SEXP val, CodeBuffer * cb, CompilerContext * cntxt ) {

  DEBUG_PRINT("++ cmp_const: Compiling constant\n");

  if ( IDENTICAL( val, R_NilValue ) )
    PUTCODE( LDNULL_OP );
  else if ( IDENTICAL( val, R_TrueValue ) )
    PUTCODE( LDTRUE_OP );
  else if ( IDENTICAL( val, R_FalseValue ) )
    PUTCODE( LDFALSE_OP );
  else {
    int ci = PUTCONST( val ); 
    PUTCODES( LDCONST_OP, ci );
  }

  if ( cntxt->tailcall )
    PUTCODE( RETURN_OP );

};

// @manual 2.5
void cmp_sym( SEXP sym, CodeBuffer * cb, CompilerContext * cntxt, bool missing_ok ) {

  DEBUG_PRINT("++ cmp_sym: Compiling symbol '%s'\n", CHAR(PRINTNAME(sym)));

  if ( sym == R_DotsSymbol ) {
    notify_wrong_dots_use( sym, cntxt, cb_savecurloc(cb) );
    PUTCODE( DOTSERR_OP );
    return;
  }

  if ( is_ddsym( sym ) ) {
    if ( ! find_loc_var( sym, cntxt ) ) {
      notify_wrong_dots_use( sym, cntxt, cb_savecurloc(cb) );
    }
    
    int ci = PUTCONST( sym );

    if ( missing_ok ) {
      PUTCODE( DDVAL_MISSOK_OP );
    } else {
      PUTCODE( DDVAL_OP );
    }

    PUTCODE( ci );
    
    if ( cntxt->tailcall )
      PUTCODE( RETURN_OP );

    return;
  }

  if ( ! find_var( sym, cntxt ) ) {
    notify_undef_var(sym, cntxt, cb_savecurloc(cb));
  }
  
  int poolref = PUTCONST( sym );

  if ( missing_ok )
    PUTCODE( GETVAR_MISSOK_OP );
  else
    PUTCODE( GETVAR_OP );

  PUTCODE( poolref );

  if ( cntxt->tailcall )
    PUTCODE( RETURN_OP );

};

// @manual 2.6
void cmp_call( SEXP call, CodeBuffer * cb, CompilerContext * cntxt, bool inline_ok ) {

  DEBUG_PRINT("++ cmp_call: Compiling function call\n");
  
  Loc saved = cb_savecurloc( cb );
  cb_setcurexpr( cb, call );

  cntxt = make_call_ctx( cntxt, call );
  SEXP fun = CAR( call );
  SEXP args = CDR( call );

  if ( TYPEOF( fun ) == SYMSXP ) {
    
    if ( ! ( inline_ok && try_inline( call, cb, cntxt ) ) ) {

      // If not inlinable:
      DEBUG_PRINT("++ cmp_call: Calling symbol function '%s'\n", CHAR(PRINTNAME(fun)));

      if ( find_loc_var(fun,cntxt) ) {
        notify_local_fun(fun, cntxt, cb_savecurloc(cb));
      } else {

        SEXP def = find_fun_def( fun, cntxt );
        if ( Rf_isNull(def) ) {
          notify_undef_fun(fun, cntxt, cb_savecurloc(cb));
        } else {
          DEBUG_PRINT("++ cmp_call: Found function definition for symbol '%s'\n", CHAR(PRINTNAME(fun)));
          bool bad_call = false;
          check_call( def, call, &bad_call );
          if (bad_call) notify_bad_call(call, cntxt, cb_savecurloc(cb));
        }

      }

      cmp_call_sym_fun( fun, args, call, cb, cntxt );

    }

  } else {
    
    DEBUG_PRINT(".. cmp_call: Calling complex expression function\n");
    
    // Hack for handling break() and next() calls
    if ( TYPEOF( fun ) == LANGSXP && TYPEOF( CAR( fun )) == SYMSXP ) {
      const char* ch = CHAR( PRINTNAME( CAR( fun ) ) );
      if ( (strcmp( ch, "break" ) == 0) || (strcmp( ch, "next" ) == 0) ) {
        cmp( fun, cb, cntxt, false, true );
      } else {
        cmp_call_expr_fun( fun, args, call, cb, cntxt );
      }
    }
  
  }

  cb_restorecurloc( cb, saved );

};

// @manual 2.6
void cmp_call_sym_fun( SEXP fun, SEXP args, SEXP call, CodeBuffer * cb, CompilerContext * cntxt ) {

  DEBUG_PRINT("++ cmp_call_sym_fun: Compiling function call with symbol function '%s'\n", CHAR(PRINTNAME(fun)));

  const char* maybe_NSE_symbols[] = {"bquote", NULL}; // Null works as terminator
  
  int ci = PUTCONST( fun );
  PUTCODES( GETFUN_OP, ci );
  
  bool nse = is_in_c_set( CHAR(PRINTNAME(fun)), maybe_NSE_symbols );

  cmp_call_args( args, cb, cntxt, nse );

  ci = PUTCONST( call );
  PUTCODES( CALL_OP, ci );

  if ( cntxt->tailcall )
    PUTCODE( RETURN_OP );

};

void cmp_call_expr_fun( SEXP fun, SEXP args, SEXP call, CodeBuffer * cb, CompilerContext * cntxt ) {

  DEBUG_PRINT("++ cmp_call_expr_fun: Compiling function call with expression function\n");

  CompilerContext * ncntxt = make_non_tail_call_ctx( cntxt );
  cmp( fun, cb, ncntxt, false, true );
  PUTCODE( CHECKFUN_OP );
  bool nse = false;

  cmp_call_args( args, cb, cntxt, nse );
  int ci = PUTCONST( call );
  PUTCODES( CALL_OP, ci );

  if ( cntxt->tailcall )
    PUTCODE( RETURN_OP );

};

void cmp_call_args( SEXP args, CodeBuffer * cb, CompilerContext * cntxt, bool nse ) {

  DEBUG_PRINT("++ cmp_call_args: Compiling function call arguments\n");

  CompilerContext * pnctxt = make_promise_ctx( cntxt );

  while ( args != R_NilValue ) {

    SEXP a = CAR( args );
    SEXP n = TAG( args );

    // handle missing argument
    if ( a == R_MissingArg ) {
      PUTCODE( DOMISSING_OP );
      cmp_tag(n, cb);
      
      args = CDR( args ); 
      continue;           
    }

    // handle ... argument
    if ( (TYPEOF( a ) == SYMSXP) && (a == R_DotsSymbol) ) {

      if ( ! find_loc_var(R_DotsSymbol, cntxt) ) {
        notify_wrong_dots_use( a, cntxt, cb_savecurloc(cb) );
      }

      PUTCODE(DODOTS_OP);
      
      args = CDR( args ); 
      continue;
    }

    if ( TYPEOF(a) == BCODESXP ) {
      cntxt_stop("cannot compile byte code literals in code", cntxt, cb_savecurloc(cb));
    }

    if ( TYPEOF(a) == PROMSXP ) {
      cntxt_stop("cannot compile promise literals in code", cntxt, cb_savecurloc(cb));
    }


    // compile general argument
    if ( TYPEOF( a ) == SYMSXP || TYPEOF( a ) == LANGSXP ) {
      int ci;
      if ( nse )
        ci = PUTCONST( a );
      else {
        SEXP c = PROTECT( gen_code( a, pnctxt, R_NilValue, cb_savecurloc( cb ) ) );
        ci = PUTCONST( c );
        UNPROTECT(1); //c
      }
      
      PUTCODES( MAKEPROM_OP, ci );
    
    } else {
      cmp_const_arg( a, cb, pnctxt );
    }

    cmp_tag( n, cb );
  
    args = CDR( args );

  }

};

void cmp_tag( SEXP tag, CodeBuffer * cb ) {

  DEBUG_PRINT("++ cmp_tag: Compiling argument tag\n");

  if ( tag == R_NilValue || tag == R_MissingArg )
    return;

  const char *name_str = CHAR(PRINTNAME(tag));
  
  if (name_str[0] == '\0')
      return;

  int ci = PUTCONST( tag );
  PUTCODES( SETTAG_OP, ci );

};

void cmp_const_arg( SEXP a, CodeBuffer * cb, CompilerContext * cntxt ) {

  DEBUG_PRINT("++ cmp_const_arg: Compiling constant argument\n");

  if ( isNull(a) )
    PUTCODES( PUSHNULLARG_OP );
  else if ( IDENTICAL(a, R_TrueValue) )
    PUTCODES( PUSHTRUEARG_OP );
  else if ( IDENTICAL(a, R_FalseValue) )
    PUTCODES( PUSHFALSEARG_OP );
  else {
    int ci = PUTCONST( a );
    PUTCODES( PUSHCONSTARG_OP, ci );
  }
  
};

void cb_putcode( CodeBuffer * cb, ... ) {

  va_list args;
  va_start(args, cb);

  int srcrefpatchstart = cb->code_count;

  while (1) {
    int opcode = va_arg(args, int);

    // Terminate processing when the sentinel is encountered
    if (opcode == END_OPCODES)
      break;

    // Resize if needed
    if ( cb->code_count >= cb->code_capacity ) {
    
      // Resize code array
      cb->code_capacity *= 2;

      // realloc logic !! TOSTYLE duplicate code
      int *new_code = (int *) R_alloc(cb->code_capacity, sizeof(int));
      memcpy(new_code, cb->code, (cb->code_capacity / 2) * sizeof(int));
      cb->code = new_code;
    
      // Update the source tracking buffers
      int *new_expr_buf = (int *) R_alloc(cb->code_capacity, sizeof(int));
      memcpy(new_expr_buf, cb->expr_buf, (cb->code_capacity / 2) * sizeof(int));
      cb->expr_buf = new_expr_buf;

      int *new_srcref_buf = (int *) R_alloc(cb->code_capacity, sizeof(int));
      memcpy(new_srcref_buf, cb->srcref_buf, (cb->code_capacity / 2) * sizeof(int));
      cb->srcref_buf = new_srcref_buf;

    }

    cb->code[ cb->code_count ] = opcode;
    cb->code_count += 1;

  }

  va_end(args);

  int srcrefpatchend = cb->code_count;

  // handle source tracking
  int expression_idx = cb->expr_tracking_on ? PUTCONST_NODEDUP( cb->current_expr ) : 0;
  int srcref_idx = cb->srcref_tracking_on ? PUTCONST_NODEDUP( cb->current_srcref ) : 0;
  
  for (int i = srcrefpatchstart; i < srcrefpatchend; i++) {

    if ( cb->expr_tracking_on ) {
      cb->expr_buf[ i ] = expression_idx;
    }

    if ( cb->srcref_tracking_on ) {
      if ( cb->current_srcref != R_NilValue ) {
        cb->srcref_buf[ i ] = srcref_idx;
      } else {
        cb->srcref_buf[ i ] = -1; // No srcref
      }
    }

  }
  
};

int cb_getcode( CodeBuffer * cb, int pos ) {

  if ( pos < 0 || pos >= cb->code_count ) {
    error("CodeBuffer: Invalid code position");
  }

  return cb->code[ pos ];

};


int cb_putconst( CodeBuffer * cb, SEXP item, bool check_dedup ) {

DEBUG_PRINT("++ putconst: Adding constant to pool\n");

  // Initialize constant pool if it doesn't exist
  if ( cb->constant_pool == R_NilValue ) {
    cb->const_capacity = 16;
    cb->const_count = 0;

    cb->constant_pool = PROTECT( Rf_allocVector( VECSXP, cb->const_capacity ) );

    // Link the constant pool to its handle (protects agains GC)
    SET_VECTOR_ELT( cb->constant_pool_handle, 0, cb->constant_pool );

    UNPROTECT(1);
  }

  // Check if item already exists in pool
  for (int j = 0; j < cb->const_count; j++) {
    SEXP compare = VECTOR_ELT( cb->constant_pool, j);
    
    if (item == compare)
      return j;

  }

  // Deep check only for check_dedup = TRUE
  if (!OPTIMIZE_INCOMPATIBLE || check_dedup) {

    for (int j = cb->const_count - 1; j >= 0; j--) {
      SEXP compare = VECTOR_ELT( cb->constant_pool, j);
      
      /* 16 - take closure environments into account  */
      if (R_compute_identical(item, compare, 16)) {
        DEBUG_PRINT("++ putconst: Found existing constant in pool at index %d\n", j);
        
        // Found so return the existing index immediately,
        // Do not increment const_count
        return j;
      }
    }

  }

  // Resize constant pool if full
  if ( cb->const_count == cb->const_capacity ) {
    
    int new_cap = cb->const_capacity * 2;
    SEXP new_pool = Rf_allocVector( VECSXP, new_cap );
    PROTECT( new_pool );
    
    // Copy existing items to the new pool
    for ( int i = 0; i < cb->const_count; i++ ) {
      SET_VECTOR_ELT( new_pool, i, VECTOR_ELT( cb->constant_pool, i ) );
    }

    // Swap the old pool from handler, old one will be garbage collected
    SET_VECTOR_ELT( cb->constant_pool_handle, 0, new_pool );
    
    cb->constant_pool = new_pool;
    cb->const_capacity = new_cap;
    UNPROTECT(1); // new_pool
  }

  // Add the new item to the pool
  SET_VECTOR_ELT( cb->constant_pool, cb->const_count, item );
  
  // Capture the current index
  int new_idx = cb->const_count;
  
  // Increment the count
  cb->const_count++;
  
  return new_idx;

};

SEXP cb_getconst( CodeBuffer * cb, int idx ) {

  if ( idx < 0 || idx >= cb->const_count ) {
    error("CodeBuffer: Invalid constant index");
  }

  return VECTOR_ELT( cb->constant_pool, idx );

};

// @manual 15.1
bool may_call_browser( SEXP expr, CompilerContext * cntxt ) {

  if ( TYPEOF( expr ) == LANGSXP ) {

    SEXP fun = CAR( expr );
    
    if ( TYPEOF( fun ) == SYMSXP ) {

      const char* fname = CHAR( PRINTNAME( fun ) );
      
      if ( strcmp(fname, "browser") == 0 ) {
        return true;
      } else if ( (strcmp(fname, "function") == 0) && is_base_var(fun, cntxt) ) {
        return false;
      } else {
        return may_call_browser_list( CDR( expr ), cntxt );
      }

    } else {
      return may_call_browser_list(expr, cntxt);
    }
  } else {
    return false;
  }

}

bool may_call_browser_list(SEXP exprlist, CompilerContext * cntxt) {

  SEXP node = exprlist;
  while ( node != R_NilValue ) {

    SEXP expr = CAR( node );

    if ( (expr != R_MissingArg) && may_call_browser( expr, cntxt ) )
      return true;

    node = CDR( node );
  }

  return false;

};

// @manual 2.3
void cmp( SEXP e, CodeBuffer * cb, CompilerContext * cntxt, bool missing_ok, bool setloc ) {

  Loc sloc;

  if ( setloc ) {
    sloc = cb_savecurloc(cb);
    cb_setcurexpr(cb, e);
  }

  SEXP ce = constant_fold( e, cntxt, cb_savecurloc( cb ) );
  
  if ( ce == R_NilValue ) {

    // Not foldable, generate code normally
    DEBUG_PRINT(".. cmp: Expression not folded\n");

    SEXPTYPE type = TYPEOF( e );
    switch (type) {
      case LANGSXP:
        DEBUG_PRINT(".. cmp: Compiling Call (LANGSXP)\n");
        cmp_call(e, cb, cntxt, true);
        break;

      case SYMSXP:
        DEBUG_PRINT(".. cmp: Compiling Symbol (SYMSXP): %s\n", CHAR(PRINTNAME(e)));
        cmp_sym(e, cb, cntxt, missing_ok);
        break;

      case BCODESXP:
        DEBUG_PRINT("!! cmp: Error - Literal Bytecode found\n");
        cntxt_stop("cannot compile byte code literals in code", cntxt, cb_savecurloc(cb));
        break;

      case PROMSXP:
        DEBUG_PRINT("!! cmp: Error - Literal Promise found\n");
        cntxt_stop("cannot compile promise literals in code", cntxt, cb_savecurloc(cb));
        break;

      default:
        DEBUG_PRINT(".. cmp: Compiling Constant (Type: %d)\n", type);
        cmp_const(e, cb, cntxt);
        break;
    }

  } else {
    cmp_const( VECTOR_ELT(ce, 0), cb, cntxt );
  }

  // Restore previous location
  if ( setloc )
    cb_restorecurloc(cb, sloc);

};

// @manual 15.1 
SEXP cmpfun(SEXP f, SEXP compiler_options) {

  SEXPTYPE type = TYPEOF( f );

  switch (type) {
    case CLOSXP:
      DEBUG_PRINT(">> cmpfun: Compiling CLOSXP (Function)\n");

      CompilerEnv* cenv = make_cenv(CLOENV(f));
      CompilerContext* cntxt = make_toplevel_ctx(cenv, compiler_options);
      CompilerEnv* fenv = make_fun_env(FORMALS(f), BODY(f), cntxt);
      CompilerContext* ncntxt =
          make_function_ctx(cntxt, fenv, FORMALS(f), BODY(f));

      if (may_call_browser(BODY(f), ncntxt)) {
        DEBUG_PRINT("!! cmpfun: Function may call browser, skipping compilation\n");
        return f;
      }

      bool is_block = false;
      if (TYPEOF(BODY(f)) == LANGSXP) {
        SEXP sym = CAR(BODY(f));
        if (TYPEOF(sym) == SYMSXP) {
          const char* name = CHAR(PRINTNAME(sym));
          if (strcmp(name, "{") == 0) {
            is_block = true;
          }
        }
      }

      Loc loc;
      loc.is_null = false;

      if (!is_block) {
        loc.expr = BODY(f);
        loc.srcref = get_expr_srcref(f);
      } else
        loc.is_null = true;

      SEXP b = PROTECT(gen_code(BODY(f), ncntxt, R_NilValue, loc));

      DEBUG_PRINT("<< cmpfun: Compilation finished, creating closure\n");

      SEXP val = PROTECT( R_mkClosure(FORMALS(f), b, CLOENV(f)) );

      SEXP attrs = ATTRIB(f);
      if (!Rf_isNull(attrs)) {
        SET_ATTRIB(val, attrs);
      }

      if (isS4(f)) {
        UNPROTECT(1); // val
        val = Rf_asS4(val, FALSE, 0);
        PROTECT(val); // Not sure about this, but whatever
      }

      UNPROTECT(2);  // b, val
      DEBUG_PRINT("<< cmpfun done, returning\n");

      return val;

    case BUILTINSXP:
    case SPECIALSXP:
      DEBUG_PRINT( ">> cmpfun: Primitive function (BUILTIN/SPECIAL), skipping\n");
      return f;

    default:
      DEBUG_PRINT("!! cmpfun: Error - Argument is not a function type: %d\n", type);
      Rf_error("Argument must be a closure, builtin, or special function");
  }

};

// @manual 2.1
// TODO what is gen for? Compiler reference says its used when inlining loops
// but gen_code is not being called from anywhere in there? Maybe wrong?
SEXP gen_code( SEXP e, CompilerContext * cntxt, SEXP gen, Loc loc ) {

  CodeBuffer * cb = make_code_buffer(e, loc);
  PROTECT( cb->constant_pool_handle ); // Protect constant pool handle

  if ( Rf_isNull( gen ) )
    cmp( e, cb, cntxt, false, false );
  //else
  //  gen(cb,cntxt) <- This will have to be a function pointer passed in gen TODO ?

  SEXP result = PROTECT(code_buf_code(cb, cntxt));
  
  UNPROTECT(2); // constant_pool_handle, result
  return result;

}

SEXP code_buf_code( CodeBuffer * cb, CompilerContext * cntxt ) {

  // TODO patch labels once inlining is real
  DEBUG_PRINT("++ code_buf_code: Generating final bytecode object\n");
  DEBUG_PRINT("   Code size: %d\n", cb->code_count);
  DEBUG_PRINT("   Constant pool size: %d\n", cb->const_count);

  cb_patchlabels(cb);

  SEXP code_vec = PROTECT( Rf_allocVector( INTSXP, cb->code_count + 1 ) );

  if ( cb->srcref_tracking_on ) {

    SEXP srcref_vec = PROTECT( Rf_allocVector( INTSXP, cb->code_count + 1) );
    int * srcref_ptr = INTEGER( srcref_vec );
  
    srcref_ptr[0] = NA_INTEGER; 

    for (int i = 0; i < cb->code_count; i++)
      srcref_ptr[i+1] = cb->srcref_buf[i];

    setAttrib(srcref_vec, R_ClassSymbol, Rf_mkString("srcrefsIndex"));
    PUTCONST( srcref_vec ); // Ensure srcref_buf is in constant pool

    UNPROTECT(1); // srcref_vec

  }

  if ( cb->expr_tracking_on ) {

    SEXP expr_vec = PROTECT( Rf_allocVector( INTSXP, cb->code_count + 1) );
    int * expr_ptr = INTEGER( expr_vec );

    expr_ptr[0] = NA_INTEGER; 

    for (int i = 0; i < cb->code_count; i++)
      expr_ptr[i+1] = cb->expr_buf[i];

    setAttrib(expr_vec, R_ClassSymbol, Rf_mkString("expressionsIndex"));
    PUTCONST( expr_vec ); // Ensure expr_buf is in constant pool

    UNPROTECT(1); // expr_vec

  }

  // Put bytecode version as first instruction
  INTEGER(code_vec)[0] = asInteger(R_bcVersion());
  memcpy(INTEGER(code_vec) + 1, cb->code, cb->code_count * sizeof(int));

  SEXP const_pool;

  if (cb->constant_pool == R_NilValue || cb->const_count == 0) {
    // Create empty constant pool if no contants
    const_pool = PROTECT(Rf_allocVector(VECSXP, 0));

  } else {
    // Resize constant pool to actual size
    const_pool = PROTECT(Rf_allocVector(VECSXP, cb->const_count));
    
    for (int i = 0; i < cb->const_count; i++)
      SET_VECTOR_ELT(const_pool, i, VECTOR_ELT(cb->constant_pool, i));
    
  }

  // .Internal(mkCode(code, consts))
  SEXP mkCode_sym = Rf_install("mkCode");
  SEXP inner_call = PROTECT( Rf_lang3(mkCode_sym, code_vec, const_pool) );
    
  SEXP internal_sym = Rf_install(".Internal");
  SEXP full_call = PROTECT(Rf_lang2(internal_sym, inner_call));
    
  SEXP final_bcode = Rf_eval(full_call, R_BaseEnv);

  UNPROTECT(4); // code_vec, const_pool, inner_call, full_call
  return final_bcode;

}

// ============================================================ //

static SEXP R_bcVersion() {
  SEXP call, res;

  // .Internal(bcVersion())
  call = PROTECT(lang1(install("bcVersion")));
  call = PROTECT(lang2(install(".Internal"), call));

  res = PROTECT(eval(call, R_BaseEnv));

  UNPROTECT(3);
  return res;
}

SEXP is_compiled(SEXP fun) {
  bool res = 0;
  if (TYPEOF(fun) == CLOSXP) {
    res = (TYPEOF(BODY(fun)) == BCODESXP);
  }
  return res ? R_TrueValue : R_FalseValue;
}

SEXP compile( SEXP e, SEXP env, SEXP options, SEXP srcref ) {

  Loc loc = { false, e, srcref };
  CompilerEnv * cenv = make_cenv(env);
  CompilerContext * cntxt = make_toplevel_ctx( cenv, options );
  
  ExtraVars empty = { NULL, 0 };
  add_cenv_vars( cenv, find_locals( e, empty, cntxt ) );

  if ( may_call_browser( e, cntxt ) )
    return e;
  else if (srcref == R_NilValue)
    return gen_code( e, cntxt, R_NilValue, loc );
  else
    return gen_code( e, cntxt, R_NilValue, loc );

}

// Registration of C functions
static const R_CallMethodDef CallEntries[] = {
    {"cmpfun", (DL_FUNC) &cmpfun, 2},
    {"is_compiled", (DL_FUNC) &is_compiled, 1},
    {"compile", (DL_FUNC) &compile, 4},
    {"cmpfile", (DL_FUNC) NULL, 0},
    {NULL, NULL, 0}
};

void R_init_crbcc(DllInfo* dll) {
  R_registerRoutines(dll, NULL, CallEntries, NULL, NULL);
  R_useDynamicSymbols(dll, FALSE);

  BCVersion = Rf_asInteger(R_bcVersion());
}

#pragma endregion

#pragma region Inlining mechanisms

// Macro to reduce boilerplate for handler assignment
#define INLINE_HANDLER_CASE(NAMESTR, FN) \
    if (strcmp(name, NAMESTR) == 0) { \
      *found = &FN; \
      return true; \
    }

bool cmp_builtin_default( SEXP e, CodeBuffer *cb, CompilerContext *cntxt ) {
  return cmp_builtin( e,cb,cntxt,false );
}

bool get_inline_handler( const char *name, Inline in, CompilerContext * cntxt, HandlerFn * found ) {

  if ( in == BASE ) {
    INLINE_HANDLER_CASE("{", inline_left_brace)
    INLINE_HANDLER_CASE("function", inline_function)
    INLINE_HANDLER_CASE("return", inline_return)
    INLINE_HANDLER_CASE("if", inline_if)
    INLINE_HANDLER_CASE("(", inline_left_parenthesis)
    INLINE_HANDLER_CASE("&&", inline_and)
    INLINE_HANDLER_CASE("||", inline_or)
    INLINE_HANDLER_CASE("repeat", inline_repeat)
    INLINE_HANDLER_CASE("next", inline_next)
    INLINE_HANDLER_CASE("break", inline_break)
    INLINE_HANDLER_CASE("while", inline_while)
    INLINE_HANDLER_CASE("for", inline_for)
    INLINE_HANDLER_CASE("+", inline_plus)
    INLINE_HANDLER_CASE("-", inline_minus)
    INLINE_HANDLER_CASE("*", inline_mul)
    INLINE_HANDLER_CASE("/", inline_div)
    INLINE_HANDLER_CASE("^", inline_pow)
    INLINE_HANDLER_CASE("exp", inline_exp)
    INLINE_HANDLER_CASE("sqrt", inline_sqrt)
    INLINE_HANDLER_CASE("log", inline_log)
    INLINE_HANDLER_CASE("==", inline_eq)
    INLINE_HANDLER_CASE("!=", inline_neq)
    INLINE_HANDLER_CASE("<", inline_lt)
    INLINE_HANDLER_CASE("<=", inline_le)
    INLINE_HANDLER_CASE(">", inline_gt)
    INLINE_HANDLER_CASE(">=", inline_ge)
    INLINE_HANDLER_CASE("&", inline_and2)
    INLINE_HANDLER_CASE("|", inline_or2)
    INLINE_HANDLER_CASE("!", inline_not)
    INLINE_HANDLER_CASE("=", cmp_assign)
    INLINE_HANDLER_CASE("<-", cmp_assign)
    INLINE_HANDLER_CASE("<<-", cmp_assign)
    INLINE_HANDLER_CASE("[", inline_subset)
    INLINE_HANDLER_CASE("[[", inline_subset2)
    INLINE_HANDLER_CASE("::", cmp_multi_colon)
    INLINE_HANDLER_CASE(":::", cmp_multi_colon)
    INLINE_HANDLER_CASE("with", inline_with)
    INLINE_HANDLER_CASE("require", inline_required)
    INLINE_HANDLER_CASE(":", inline_colon)
    INLINE_HANDLER_CASE("seq_along", inline_seq_along)
    INLINE_HANDLER_CASE("seq_len", inline_seq_len)
    INLINE_HANDLER_CASE(".Internal", cmp_dot_internal_call)
    INLINE_HANDLER_CASE("local", inline_local)
    INLINE_HANDLER_CASE("is.character", inline_is_character)
    INLINE_HANDLER_CASE("is.complex", inline_is_complex)
    INLINE_HANDLER_CASE("is.double", inline_is_double)
    INLINE_HANDLER_CASE("is.integer", inline_is_integer)
    INLINE_HANDLER_CASE("is.logical", inline_is_logical)
    INLINE_HANDLER_CASE("is.name", inline_is_name)
    INLINE_HANDLER_CASE("is.null", inline_is_null)
    INLINE_HANDLER_CASE("is.object", inline_is_object)
    INLINE_HANDLER_CASE("is.symbol", inline_is_symbol)
    INLINE_HANDLER_CASE("$", inline_dollar)
    INLINE_HANDLER_CASE(".Call", inline_c_call)
    INLINE_HANDLER_CASE("switch", inline_switch)

    for (size_t i = 0; math1funs[i] != NULL; i++)
    {
      if (strcmp(name, math1funs[i]) == 0) {
        *found = &cmp_math_1;
        return true;
      }
    }
    
    for (size_t i = 0; safe_base_internals[i] != NULL; i++) {
      if (strcmp(name, safe_base_internals[i]) == 0) {
        *found = &cmp_simple_internal;
        return true;
      }
    }
  
  }

  if ( in == STAT ) {

    for (size_t i = 0; safe_stats_internals[i] != NULL; i++) {
      if (strcmp(name, safe_stats_internals[i]) == 0) {
        *found = &cmp_simple_internal;
        return true;
      }
    }
  

  }

  // Check if is SPECIAL or BUILTIN
  SEXP def = find_fun_def( install(name) , cntxt );

  if (TYPEOF(def) == BUILTINSXP) {

    *found = &cmp_builtin_default;
    return true;

  } else if (TYPEOF(def) == SPECIALSXP) {

    *found = &cmp_special;
    return true;

  }

  return false;

}

bool get_setter_inline_handler( const char *name, Inline in, SetterHandlerFn * found ) {

  if (in == BASE) {
    INLINE_HANDLER_CASE("$<-", dollar_setter_inline_handler)
    INLINE_HANDLER_CASE("@<-", at_setter_inline_handler)
    INLINE_HANDLER_CASE("[<-", inline_subassign_setter)
    INLINE_HANDLER_CASE("[[<-", inline_subassign2_setter)
  }

  return false;

}

bool get_getter_inline_handler( const char *name, Inline in, HandlerFn * found ) {

  if (in == BASE) {
    INLINE_HANDLER_CASE("$", dollar_getter_inline_handler)
    INLINE_HANDLER_CASE("[", inline_subset_getter)
    INLINE_HANDLER_CASE("[[", inline_subset2_getter)
  }
  
  return false;

}

#undef INLINE_HANDLER_CASE
#pragma endregion

#pragma region Inline handlers

bool inline_left_brace( SEXP e, CodeBuffer *cb, CompilerContext *cntxt ) {

  int n = length( e );
  if ( n == 1 ) {
    
    cmp( R_NilValue, cb, cntxt, false, true );
  
  } else {
  
    Loc sloc = cb_savecurloc(cb);
    SEXP bsrefs = Rf_getAttrib( e , install("srcref") );

    SEXP runner = CDR( e );

    if ( n > 2 ) {

      CompilerContext * ncntxt = make_no_value_ctx(cntxt);

      for (int i = 2; i < n; i++) {

        SEXP subexp = CAR( runner );

        cb_setcurloc(cb, subexp, get_block_srcref(bsrefs, i));
        cmp( subexp, cb, ncntxt, false, false );
        PUTCODE( POP_OP );

        runner = CDR( runner );

      }

    }

    SEXP subexp = CAR( runner );
    cb_setcurloc( cb, subexp, get_block_srcref( bsrefs, n ) );
    cmp( subexp, cb, cntxt, false, false );
    cb_restorecurloc(cb,sloc);

  }


  return true;

};

bool inline_function( SEXP e, CodeBuffer *cb, CompilerContext *cntxt ) {

  SEXP formals = CADR( e );
  SEXP body = CADDR( e );

  SEXP sref = R_NilValue;

  if ( length(e) > 3)
    sref = CADDDR( e );

  CompilerEnv * fenv = make_fun_env(formals, body, cntxt);
  CompilerContext * ncntxt = make_function_ctx( cntxt, fenv, formals, body );

  if ( may_call_browser( body, ncntxt ) ) {
    DEBUG_PRINT("!! inline_function_handler: Function may call browser, skipping inlining\n");
    return false;
  }

  Loc loc = cb_savecurloc( cb );
  SEXP cbody = gen_code( body, ncntxt, R_NilValue, loc );
  PROTECT( cbody );


  SEXP const_list = PROTECT( Rf_allocVector( VECSXP, 3 ) );
  SET_VECTOR_ELT( const_list, 0, formals );
  SET_VECTOR_ELT( const_list, 1, cbody );
  SET_VECTOR_ELT( const_list, 2, sref );

  int ci = PUTCONST( const_list );
  PUTCODES( MAKECLOSURE_OP, ci );

  if ( cntxt->tailcall )
    PUTCODE( RETURN_OP );

  UNPROTECT(2); // body, const_list
  return true;

};

bool inline_left_parenthesis( SEXP e, CodeBuffer *cb, CompilerContext *cntxt ) {

  DEBUG_PRINT("[_] Inlining left parenthesis function");

  if ( any_dots(e) )
    return cmp_builtin( e, cb, cntxt, false );

  if ( length(e) != 2 ) {

    notify_wrong_arg_count(CAR(e), cntxt, cb_savecurloc(cb));
    return cmp_builtin( e, cb, cntxt, false );
  }

  if ( cntxt->tailcall ) {

    CompilerContext * ncntxt = make_non_tail_call_ctx( cntxt );
    cmp( CADR( e ), cb, ncntxt, false, true );

    PUTCODES( VISIBLE_OP, RETURN_OP );
    return true;
  
  }

  cmp( CADR( e ), cb, cntxt, false, true );
  return true;

};

bool inline_return( SEXP e, CodeBuffer *cb, CompilerContext *cntxt ) {

  if ( dots_or_missing(e) || length(e) > 2 ) {
    cmp_special( e, cb, cntxt );
    return true;
  }

  SEXP ret_expr;

  if ( length(e) == 1 ) {
    ret_expr = R_NilValue;
  } else {
    ret_expr = CADR( e );
  }

  CompilerContext * ncntxt = make_non_tail_call_ctx( cntxt );
  cmp( ret_expr, cb, ncntxt, false, true );

  if ( cntxt->need_return_jmp )
    PUTCODE( RETURNJMP_OP );
  else
    PUTCODE( RETURN_OP );

  return true;

};

bool inline_if( SEXP e, CodeBuffer *cb, CompilerContext *cntxt ) {

  DEBUG_PRINT("[_] Inlining if statement");
  // **** TEST FOR MISSING **** //
  SEXP test = CADR( e );
  SEXP then = CADDR( e );
  SEXP eelse = R_NilValue;

  bool has_else = ( length(e) == 4 );
  
  if ( has_else ) {
    eelse = CADDDR( e );
  }

  SEXP ct = constant_fold( test, cntxt, cb_savecurloc( cb ) );

  if ( ! Rf_isNull(ct) ) {

    SEXP value = VECTOR_ELT( ct, 0 );
    // TODO ! isNA function
    if (Rf_isLogical( value ) && length( value ) == 1) {

      if ( asLogical( value ) )
        cmp( then, cb, cntxt, false, true );
      else if ( has_else )
        cmp( eelse, cb, cntxt, false, true );
      else if (cntxt->tailcall)
        PUTCODES( LDNULL_OP, INVISIBLE_OP, RETURN_OP );
      else
        PUTCODE( LDNULL_OP);

      return true;

    }

  }

  CompilerContext * ncntxt = make_non_tail_call_ctx( cntxt );
  cmp( test, cb, ncntxt, false, true );

  int callidx = PUTCONST( e );
  int else_label = cb_makelabel( cb );

  PUTCODES( BRIFNOT_OP, callidx );
  cb_putcodelabel(cb, else_label);

  cmp( then, cb, cntxt, false, true );

  if ( cntxt->tailcall ) {

    cb_putlabel( cb, else_label);

    if ( has_else )
      cmp( eelse, cb, cntxt, false, true);
    else {
      PUTCODES( LDNULL_OP, INVISIBLE_OP, RETURN_OP );
    }

  } else {

    int end_label = cb_makelabel(cb);
    PUTCODE( GOTO_OP );
    cb_putcodelabel( cb, end_label );
    cb_putlabel(cb, else_label);

    if ( has_else )
      cmp( eelse, cb, cntxt, false, true);
    else
      PUTCODE( LDNULL_OP );

    cb_putlabel( cb, end_label );

  }

  return true;

};

bool inline_and( SEXP e, CodeBuffer *cb, CompilerContext *cntxt ) {

  CompilerContext * ncntxt = make_arg_ctx( cntxt );
  int callidx = PUTCONST(e);
  int label = cb_makelabel(cb);

  cmp(CADR(e), cb, ncntxt, false, true);

  PUTCODES( AND1ST_OP, callidx );
  cb_putcodelabel(cb, label);

  cmp(CADDR(e), cb, ncntxt, false, true);

  PUTCODES( AND2ND_OP, callidx );
  cb_putlabel(cb, label);

  if (cntxt->tailcall)
    PUTCODE( RETURN_OP);

  return true;

}

bool inline_or( SEXP e, CodeBuffer *cb, CompilerContext *cntxt ) {

  CompilerContext * ncntxt = make_arg_ctx( cntxt );
  int callidx = PUTCONST(e);
  int label = cb_makelabel(cb);

  cmp(CADR(e), cb, ncntxt, false, true);

  PUTCODES( OR1ST_OP, callidx );
  cb_putcodelabel(cb, label);

  cmp(CADDR(e), cb, ncntxt, false, true);

  PUTCODES( OR2ND_OP, callidx );
  cb_putlabel(cb, label);

  if (cntxt->tailcall)
    PUTCODE( RETURN_OP );

  return true;

};

// forward declaration - circular
bool check_skip_loop_cntxt(SEXP e, CompilerContext *cntxt, bool breakOK);

bool is_loop_stop_fun(const char *fname, CompilerContext *cntxt) {

  const char *stop_set[] = {"function", "for", "while", "repeat", NULL};
  return is_in_c_set(fname, stop_set) && is_base_var(install(fname), cntxt);

}

bool is_loop_top_fun(const char *fname, CompilerContext *cntxt) {

  const char *top_set[] = {"(", "{", "if", NULL};    
  return is_in_c_set(fname, top_set) && is_base_var(install(fname), cntxt);

}

bool check_skip_loop_cntxt_list(SEXP elist, CompilerContext *cntxt, bool breakOK) {
  SEQ_ALONG( s, elist ) {
    SEXP a = CAR(s);
    if (a != R_MissingArg) {
      if (!check_skip_loop_cntxt(a, cntxt, breakOK)) {
        return false;
      }
    }
  }
  return true;
}

bool check_skip_loop_cntxt(SEXP e, CompilerContext *cntxt, bool breakOK) {

  if (TYPEOF(e) == LANGSXP) {

    SEXP fun = CAR(e);
      
    if (TYPEOF(fun) == SYMSXP) {

      const char* fname = CHAR(PRINTNAME(fun));

      if (!breakOK && (strcmp(fname, "break") == 0 || strcmp(fname, "next") == 0)) {
        return false;
      }
      
      if (is_loop_stop_fun(fname, cntxt)) {
        return true;
      }
      
      if (is_loop_top_fun(fname, cntxt)) {
        return check_skip_loop_cntxt_list(CDR(e), cntxt, breakOK);
      }
      
      if (strcmp(fname, "eval") == 0 || 
        strcmp(fname, "evalq") == 0 || 
        strcmp(fname, "source") == 0) {
        return false;
      }
          
      return check_skip_loop_cntxt_list(CDR(e), cntxt, false);
    } else {
      return check_skip_loop_cntxt_list(e, cntxt, false);
    }
  }
  
  return true;
}

void cmp_repeat_body(SEXP body, CodeBuffer *cb, CompilerContext *cntxt) {

  int loop_label = cb_makelabel(cb);
  int end_label = cb_makelabel(cb);

  cb_putlabel(cb, loop_label);

  CompilerContext * lcntxt = make_loop_ctx(cntxt, loop_label, end_label);

  cmp(body, cb, lcntxt, false, true);
  PUTCODES( POP_OP, GOTO_OP );
  cb_putcodelabel(cb, loop_label);
  cb_putlabel(cb, end_label);

}

bool inline_repeat(SEXP e, CodeBuffer *cb, CompilerContext *cntxt) {
  
  SEXP body = CADR(e);

  if (check_skip_loop_cntxt(body, cntxt, true)) {
    
    // Optimization: context is simple enough to skip heavy setup
    cmp_repeat_body(body, cb, cntxt);

  } else {

    cntxt->need_return_jmp = true;
    
    int ljmpend_label = cb_makelabel(cb);

    PUTCODES( STARTLOOPCNTXT_OP, 0 );
    cb_putcodelabel(cb, ljmpend_label);

    cmp_repeat_body(body, cb, cntxt);

    cb_putlabel(cb, ljmpend_label);
    PUTCODES( ENDLOOPCNTXT_OP, 0 );
  
  }

  PUTCODE(LDNULL_OP);

  if (cntxt->tailcall)
    PUTCODES( INVISIBLE_OP, RETURN_OP );

  return true;
}

bool inline_break(SEXP e, CodeBuffer *cb, CompilerContext *cntxt) {

  if (cntxt->loop.null) {
    
    Loc loc = cb_savecurloc(cb);
    notify_wrong_break_next(CAR(e), cntxt, cb_savecurloc(cb));
    return cmp_special(e, cb, cntxt);

  } else if (cntxt->loop.goto_ok) {

    PUTCODE( GOTO_OP);
    cb_putcodelabel(cb, cntxt->loop.end_label_id);
    return true;
  
  }
    
  return cmp_special(e, cb, cntxt);
}

bool inline_next(SEXP e, CodeBuffer *cb, CompilerContext *cntxt) {

  if (cntxt->loop.null) {
    Loc loc = cb_savecurloc(cb);
    notify_wrong_break_next(CAR(e), cntxt, cb_savecurloc(cb));
    cmp_special(e, cb, cntxt);
    return true;
  } 

  if (cntxt->loop.goto_ok) {
    PUTCODE(GOTO_OP);
    cb_putcodelabel(cb, cntxt->loop.loop_label_id);
    return true;
  }
  
  cmp_special(e, cb, cntxt);
  return true;

}

bool cmp_while_body( SEXP call, SEXP condition, SEXP body, CodeBuffer * cb, CompilerContext * cntxt ) {

  int loop_label = cb_makelabel(cb);
  int end_label = cb_makelabel(cb);

  cb_putlabel( cb, loop_label );

  CompilerContext * lcntxt = make_loop_ctx( cntxt, loop_label, end_label );
  // compile condition
  cmp( condition, cb, lcntxt, false, true );

  int callidx = PUTCONST( call );
  
  // if condition evaluated to false jump to end 
  PUTCODES( BRIFNOT_OP, callidx );
  cb_putcodelabel(cb, end_label);

  // compiled body
  cmp( body, cb, lcntxt, false, true );

  // pop body result, go up the loop again
  PUTCODES( POP_OP, GOTO_OP );
  cb_putcodelabel(cb, loop_label);
  cb_putlabel(cb, end_label);

  return true;

}

bool inline_while(SEXP e, CodeBuffer *cb, CompilerContext *cntxt) {

  SEXP condition = CADR(e);
  SEXP body = CADDR(e);

  if ( check_skip_loop_cntxt(condition, cntxt, true)
  && check_skip_loop_cntxt(body, cntxt, true) ) {

    cmp_while_body( e, condition, body, cb, cntxt );

  } else {

    cntxt->need_return_jmp = true;
    int ljmpend = cb_makelabel( cb );

    PUTCODES( STARTLOOPCNTXT_OP, 0 );
    cb_putcodelabel( cb, ljmpend );

    cmp_while_body( e, condition, body, cb, cntxt );

    cb_putlabel( cb, ljmpend );

    PUTCODES(ENDLOOPCNTXT_OP, 0);
  }

  PUTCODE( LDNULL_OP );

  if ( cntxt->tailcall )
    PUTCODES( INVISIBLE_OP, RETURN_OP );

  return true;

}

bool cmp_for_body( int callidx, SEXP body, int ci, CodeBuffer * cb, CompilerContext * cntxt ) {

  int body_label = cb_makelabel(cb); 
  int loop_label = cb_makelabel(cb); 
  int end_label = cb_makelabel(cb); 

  if ( ci < 0 ) {
  
    PUTCODE(GOTO_OP);
    cb_putcodelabel(cb, loop_label);
  
  } else {

    PUTCODES( STARTFOR_OP, callidx, ci );
    cb_putcodelabel(cb, loop_label);

  }

  cb_putlabel(cb, body_label);

  CompilerContext * lcntxt = make_loop_ctx( cntxt, loop_label, end_label );
  cmp( body, cb, lcntxt, false, true );

  PUTCODE(POP_OP);
  cb_putlabel( cb, loop_label );
  PUTCODE(STEPFOR_OP);
  cb_putcodelabel( cb, body_label );
  cb_putlabel( cb, end_label );

  return true;

}

bool inline_for(SEXP e, CodeBuffer *cb, CompilerContext *cntxt) {

  SEXP sym = CADR( e );
  SEXP seq = CADDR( e );
  SEXP body = CADDDR( e );

  if ( ! isSymbol( sym ) )
    return false;

  CompilerContext * ncntxt = make_non_tail_call_ctx( cntxt );
  cmp( seq, cb, ncntxt, false, true );

  int ci = PUTCONST( sym );
  int callidx = PUTCONST( e );

  if ( check_skip_loop_cntxt( body, cntxt, true ) ) {

    cmp_for_body( callidx, body, ci, cb, cntxt );

  } else {

    cntxt->need_return_jmp = true;
    int ctxt_label = cb_makelabel( cb );
    
    PUTCODES( STARTFOR_OP, callidx, ci );
    cb_putcodelabel( cb, ctxt_label);
    cb_putlabel( cb, ctxt_label );

    int ljmpend_label = cb_makelabel( cb );
    PUTCODES( STARTLOOPCNTXT_OP, 1 );
    cb_putcodelabel( cb, ljmpend_label );

    cmp_for_body( -1, body, -1, cb, cntxt );

    cb_putlabel(cb,ljmpend_label);
    PUTCODES( ENDLOOPCNTXT_OP, 1 );

  }

  PUTCODE(ENDFOR_OP);

  if ( cntxt->tailcall )
    PUTCODES( INVISIBLE_OP, RETURN_OP );

  return true;

}

bool cmp_prim_1( SEXP e, CodeBuffer * cb, int op, CompilerContext * cntxt ) {

  if ( dots_or_missing( CDR(e) ) ) {
    cmp_builtin(e, cb, cntxt, false);
    return true;
  }

  if ( length(e) != 2 ) {
    notify_wrong_arg_count(CAR(e), cntxt, cb_savecurloc(cb));
    cmp_builtin(e, cb, cntxt, false);
    return true;
  }

  CompilerContext * ncntxt = make_non_tail_call_ctx( cntxt );
  cmp( CADR(e), cb, ncntxt, false, true );
  int ci = PUTCONST( e );
  PUTCODES( op, ci );

  if ( cntxt->tailcall )
    PUTCODE( RETURN_OP );

  return true;

}

bool cmp_prim_2( SEXP e, CodeBuffer * cb, int op, CompilerContext * cntxt ) {

  if ( dots_or_missing( CDR(e) ) ) {
    return cmp_builtin(e, cb, cntxt, false);
  }

  if ( length(e) != 3 ) {
    notify_wrong_arg_count(CAR(e), cntxt, cb_savecurloc(cb));
    return cmp_builtin(e, cb, cntxt, false);
  }

  CompilerContext * ncntxt = make_non_tail_call_ctx( cntxt );
  cmp( CADR(e), cb, ncntxt, false, true );

  ncntxt = make_arg_ctx( cntxt );
  cmp( CADDR(e), cb, ncntxt, false, true );
  int ci = PUTCONST(e);
  PUTCODES( op, ci );

  if ( cntxt->tailcall )
    PUTCODE( RETURN_OP );

  return true;

}

bool inline_plus(SEXP e, CodeBuffer * cb, CompilerContext * cntxt ) {
  if ( length(e) == 3 )
    return cmp_prim_2(e, cb, ADD_OP, cntxt);
  else
    return cmp_prim_1(e, cb, UPLUS_OP, cntxt);
}

bool inline_minus(SEXP e, CodeBuffer * cb, CompilerContext * cntxt ) {
  if ( length(e) == 3 )
    return cmp_prim_2(e, cb, SUB_OP, cntxt);
  else
    return cmp_prim_1(e, cb, UMINUS_OP, cntxt);
}

bool inline_mul(SEXP e, CodeBuffer * cb, CompilerContext * cntxt ) {
  return cmp_prim_2(e, cb, MUL_OP, cntxt);
}

bool inline_div(SEXP e, CodeBuffer * cb, CompilerContext * cntxt ) {
  return cmp_prim_2(e, cb, DIV_OP, cntxt);
}

bool inline_pow(SEXP e, CodeBuffer * cb, CompilerContext * cntxt ) {
  return cmp_prim_2(e, cb, EXPT_OP, cntxt);
}

bool inline_exp(SEXP e, CodeBuffer * cb, CompilerContext * cntxt ) {
  return cmp_prim_1(e, cb, EXP_OP, cntxt);
}

bool inline_sqrt(SEXP e, CodeBuffer * cb, CompilerContext * cntxt ) {
  return cmp_prim_1(e, cb, SQRT_OP, cntxt);
}

bool inline_log(SEXP e, CodeBuffer * cb, CompilerContext * cntxt) {
  
  if (dots_or_missing(e) || !isNull(getAttrib(e, R_NamesSymbol)) || length(e) < 2 || length(e) > 3) {

    cmp_special(e, cb, cntxt);

  } else {

    int ci = PUTCONST(e);
    CompilerContext * ncntxt = make_non_tail_call_ctx(cntxt);
    cmp( CADR(e), cb, ncntxt, false, true );

    if ( length(e) == 2 ) {
      PUTCODES( LOG_OP, ci );
    } else {
      ncntxt = make_arg_ctx(cntxt);
      cmp(CADDR(e), cb, ncntxt, false, true);
      PUTCODES( LOGBASE_OP, ci );
    }

  }

  if ( cntxt->tailcall )
    PUTCODE(RETURN_OP);

  return true;

}

bool cmp_math_1(SEXP e, CodeBuffer * cb, CompilerContext * cntxt) {

  if ( dots_or_missing( CDR(e) ) ) {
    return cmp_builtin(e, cb, cntxt, false);
  }
  if ( length(e) != 2 ) {
    notify_wrong_arg_count(CAR(e), cntxt, cb_savecurloc(cb));
    cmp_builtin(e, cb, cntxt, false);
  }

  const char * name = CHAR( PRINTNAME( CAR(e) ) );

  int idx = -1;

  for (size_t i = 0; math1funs[i] != NULL; i++)
  {
    if (strcmp(name, math1funs[i]) == 0) {
      idx = i;
      break;
    }
  }

  if ( idx == -1 ) {
    cntxt_stop("cannot compile this expression", cntxt, cb_savecurloc(cb));
  }

  Loc loc = cb_savecurloc(cb);
  CompilerContext * ncntxt = make_non_tail_call_ctx(cntxt);
  cmp( CADR(e), cb, ncntxt, false, true );
  int ci = PUTCONST(e);
  PUTCODES( MATH1_OP, ci, idx );

  if ( cntxt->tailcall )
    PUTCODE(RETURN_OP);

  return true;

}

bool inline_eq(SEXP e, CodeBuffer * cb, CompilerContext * cntxt) {
  return cmp_prim_2(e, cb, EQ_OP, cntxt);
}

bool inline_neq(SEXP e, CodeBuffer * cb, CompilerContext * cntxt) {
  return cmp_prim_2(e, cb, NE_OP, cntxt);
}

bool inline_lt(SEXP e, CodeBuffer * cb, CompilerContext * cntxt) {
  return cmp_prim_2(e, cb, LT_OP, cntxt);
}

bool inline_le(SEXP e, CodeBuffer * cb, CompilerContext * cntxt) {
  return cmp_prim_2(e, cb, LE_OP, cntxt);
}

bool inline_ge(SEXP e, CodeBuffer * cb, CompilerContext * cntxt) {
  return cmp_prim_2(e, cb, GE_OP, cntxt);
}

bool inline_gt(SEXP e, CodeBuffer * cb, CompilerContext * cntxt) {
  return cmp_prim_2(e, cb, GT_OP, cntxt);
}

bool inline_and2(SEXP e, CodeBuffer * cb, CompilerContext * cntxt) {
  return cmp_prim_2(e, cb, AND_OP, cntxt);
}

bool inline_or2(SEXP e, CodeBuffer * cb, CompilerContext * cntxt) {
  return cmp_prim_2(e, cb, OR_OP, cntxt);
}

bool inline_not(SEXP e, CodeBuffer * cb, CompilerContext * cntxt) {
  return cmp_prim_1(e, cb, NOT_OP, cntxt);
}

//
// Assignment inlining
//

typedef struct FlattenedPlace {
  
  SEXP origplaces;
  SEXP places;

} FlattenedPlace;

SEXP get_assign_fun(SEXP fun) {
  if (TYPEOF(fun) == SYMSXP) {
    const char *base = CHAR(PRINTNAME(fun));
    size_t n = strlen(base);

    char *name = (char *) R_alloc(n + 3, sizeof(char));
    memcpy(name, base, n);
    name[n] = '<';
    name[n + 1] = '-';
    name[n + 2] = '\0';

    return install(name);
  }

  if (TYPEOF(fun) == LANGSXP && length(fun) == 3) {
    SEXP op = CAR(fun);

    if (op == R_DoubleColonSymbol || op == R_TripleColonSymbol) {
      SEXP member = CADDR(fun);

      if (TYPEOF(member) == SYMSXP) {
        const char *base = CHAR(PRINTNAME(member));
        size_t n = strlen(base);

        char *name = (char *) R_alloc(n + 3, sizeof(char));
        memcpy(name, base, n);
        name[n] = '<';
        name[n + 1] = '-';
        name[n + 2] = '\0';

        return Rf_lang3(op, CADR(fun), install(name));
      }
    }
  }

  return R_NilValue;
}

bool try_getter_inline(SEXP call, CodeBuffer *cb, CompilerContext *cntxt) {

  SEXP fun_sym = CAR(call);
  if (TYPEOF(fun_sym) != SYMSXP)
      return false;

  const char* name = CHAR(PRINTNAME(fun_sym));
  InlineInfo info = get_inline_info(name, cntxt, false);

  if (!info.can_inline) {
      return false;
  } else {

    HandlerFn h;
    bool found = get_getter_inline_handler(name, info.in, &h);

    if (found) {
      return h(call, cb, cntxt);
    } else {
      return false;
    }
  
  }
}

bool try_setter_inline(SEXP afun, SEXP place, SEXP origplace, SEXP call, 
                     CodeBuffer *cb, CompilerContext *cntxt) {

  if (TYPEOF(afun) != SYMSXP)
    return false;

  const char* name = CHAR(PRINTNAME(afun));
  InlineInfo info = get_inline_info(name, cntxt, false);

  if (!info.can_inline) {
    return false;
  } else {

    SetterHandlerFn h;
    bool found = get_setter_inline_handler(name, info.in, &h);

    if (found) {
      return h(afun, place, origplace, call, cb, cntxt);
    } else {
      return false;
    }

  }
}

void cmp_getter_call(SEXP place, SEXP origplace, CodeBuffer *cb, CompilerContext *cntxt) {

  CompilerContext * ncntxt = make_call_ctx(cntxt, place);
  Loc sloc = cb_savecurloc(cb);
  cb_setcurexpr(cb, origplace);
  SEXP fun = CAR(place);
  
  if (TYPEOF(fun) == SYMSXP) {
    if (!try_getter_inline(place, cb, ncntxt)) {

      int ci = PUTCONST(fun);
      PUTCODES( GETFUN_OP, ci, PUSHNULLARG_OP );
      
      cmp_call_args(CDDR(place), cb, ncntxt, false);
      
      int cci = PUTCONST( place);
      PUTCODES( GETTER_CALL_OP, cci, SWAP_OP );

    }
  } 
  else {

    cmp(fun, cb, ncntxt, false, true);
    PUTCODES( CHECKFUN_OP, PUSHNULLARG_OP );
    cmp_call_args(CDDR(place), cb, ncntxt, false);

    int cci = PUTCONST( place);
    PUTCODES( GETTER_CALL_OP, cci, SWAP_OP );

  }
  
  cb_restorecurloc(cb, sloc);
}

SEXP copy_spine_and_append_value(SEXP args, SEXP vexpr) {
  
  SEXP dummy = PROTECT(Rf_allocList(1));
  SEXP tail = dummy;

  for (SEXP s = args; s != R_NilValue; s = CDR(s)) {

    SEXP node = PROTECT(Rf_allocList(1)); 
    SETCAR(node, CAR(s));
    SET_TAG(node, TAG(s));
    
    SETCDR(tail, node);
    tail = node;
    
    UNPROTECT(1); 
  }

  SEXP val_node = PROTECT(Rf_allocList(1));
  SETCAR(val_node, vexpr);
  SET_TAG(val_node, install("value"));
  
  SETCDR(tail, val_node);
  UNPROTECT(1); // val_node

  SEXP result = CDR(dummy);
  UNPROTECT(1); // dummy
  
  return result;
}

void cmp_setter_call(SEXP place, SEXP origplace, SEXP vexpr, CodeBuffer *cb, CompilerContext *cntxt) {

  SEXP afun = get_assign_fun(CAR(place));
  PROTECT(afun);

  SEXP tmp_name = install("*tmp*");

  SEXP acall_args = PROTECT(copy_spine_and_append_value(CDDR(place), vexpr));

  SEXP inner = PROTECT(LCONS(tmp_name, acall_args));
  SEXP outer = PROTECT(LCONS(afun, inner));

  SEXP another_inner = PROTECT(LCONS(tmp_name, acall_args));
  SEXP acall = PROTECT(LCONS(afun, another_inner));

  CompilerContext * ncntxt = make_call_ctx(cntxt, acall);
  Loc sloc = cb_savecurloc(cb);

  SEXP cexpr_args = PROTECT(copy_spine_and_append_value(CDDR(origplace), vexpr));

  SEXP third_inner = PROTECT(LCONS(CADR(origplace), cexpr_args));
  SEXP cexpr = PROTECT(LCONS(afun, third_inner));
  
  cb_setcurexpr(cb, cexpr);

  if (afun == R_NilValue) {
    cntxt_stop("invalid function in complex assignment", cntxt, cb_savecurloc(cb));
  }
  else if (TYPEOF(afun) == SYMSXP) {
    if (!try_setter_inline(afun, place, origplace, acall, cb, ncntxt)) {

      int ci = PUTCONST(afun);
      PUTCODES(GETFUN_OP, ci, PUSHNULLARG_OP);

      cmp_call_args(CDDR(place), cb, ncntxt, false);
      
      int cci = PUTCONST(acall);
      int cvi = PUTCONST(vexpr);
      PUTCODES( SETTER_CALL_OP, cci, cvi );
    }
  } else {
    cmp(afun, cb, ncntxt, false, true);
    PUTCODES( CHECKFUN_OP, PUSHNULLARG_OP );
    cmp_call_args(CDDR(place), cb, ncntxt, false);

    int cci = PUTCONST(acall);
    int cvi = PUTCONST(vexpr);
    PUTCODES( SETTER_CALL_OP, cci, cvi );

  }

  cb_restorecurloc(cb, sloc);
  UNPROTECT(9); // afun, acall_args, acall, cexpr_args, cexpr, inner, outer, another_inner, third_inner

}

FlattenedPlace flatten_place(SEXP place, CompilerContext *cntxt, Loc loc) {

  int count = 0;
  SEXP p = place;

  while (TYPEOF(p) == LANGSXP) {
    if (length(p) < 2) {
      cntxt_stop("bad assignment 1", cntxt, loc); 
    }
    count++;
    p = CADR(p);
  }

  if (TYPEOF(p) != SYMSXP) {
    cntxt_stop("bad assignment 2", cntxt, loc);
  }

  SEXP places = PROTECT(allocVector(VECSXP, count));
  SEXP origplaces = PROTECT(allocVector(VECSXP, count));

  SEXP tmp_name = install("*tmp*");

  p = place;
  
  for (int i = 0; i < count; i++) {
    SET_VECTOR_ELT(origplaces, i, p);

    //TODO comment on this
    SEXP new_args = PROTECT(Rf_allocList(1));
    SETCAR(new_args, tmp_name);
    SET_TAG(new_args, TAG(CDR(p)));
    SETCDR(new_args, CDDR(p));

    SEXP tplace = PROTECT(Rf_allocList(1));
    SETCAR(tplace, CAR(p)); 
    SET_TAG(tplace, TAG(p));
    SETCDR(tplace, new_args);   
    SET_TYPEOF(tplace, LANGSXP);

    SET_VECTOR_ELT(places, i, tplace);
    UNPROTECT(2); // tplace, new_args

    p = CADR(p);
  }

  FlattenedPlace result;
  result.places = places;
  result.origplaces = origplaces;

  UNPROTECT(2); // places, origplaces
  return result;
}

bool cmp_complex_assign(SEXP symbol, SEXP lhs, SEXP value, bool superAssign, CodeBuffer *cb, CompilerContext *cntxt) {
    
  int start_op, end_op;

  if (superAssign) {
    start_op = STARTASSIGN2_OP;
    end_op = ENDASSIGN2_OP;
  } else {
    if (!find_var(symbol, cntxt)) {
      notify_undef_var(symbol, cntxt, cb_savecurloc(cb));
    }
    start_op = STARTASSIGN_OP;
    end_op = ENDASSIGN_OP;
  }

  if (!cntxt->toplevel)
    PUTCODE(INCLNKSTK_OP);

  // Compile the value to be assigned
  CompilerContext * ncntxt = make_non_tail_call_ctx(cntxt);
  cmp(value, cb, ncntxt, false, true);

  // Prepare constant for the symbol and emit start of assignment
  int csi = PUTCONST(symbol);
  PUTCODES(start_op, csi);

  // Prepare context for arguments/indices
  ncntxt = make_arg_ctx(cntxt);
  FlattenedPlace flat = flatten_place(lhs, cntxt, cb_savecurloc(cb));
  PROTECT( flat.origplaces );
  PROTECT( flat.places );
  
  int n_places = length(flat.places);

  // TODO check if this is GC safe
  // flatPlaceIdxs <- seq_along(flatPlace)[-1]
  for (int i = n_places - 1; i >= 1; i--) {
    cmp_getter_call(VECTOR_ELT(flat.places, i), 
                  VECTOR_ELT(flat.origplaces, i), 
                  cb, ncntxt);
  }

  // Emit the primary setter (the actual replacement)
  cmp_setter_call(VECTOR_ELT(flat.places, 0), 
                  VECTOR_ELT(flat.origplaces, 0), 
                  value, cb, ncntxt);

  // Emit the remaining setters in forward order to rebuild the object
  SEXP vtmp_name = install("*vtmp*");
  for (int i = 1; i < n_places; i++) {
    cmp_setter_call(VECTOR_ELT(flat.places, i), 
                  VECTOR_ELT(flat.origplaces, i), 
                  vtmp_name, cb, ncntxt);
  }

  PUTCODES( end_op, csi );

  if (!cntxt->toplevel)
    PUTCODE(DECLNKSTK_OP);

  if (cntxt->tailcall)
    PUTCODES( INVISIBLE_OP, RETURN_OP );

  UNPROTECT(2); // flat.origplaces, flat.places
  return true;
}

bool check_assign(SEXP e, CompilerContext *cntxt, Loc loc) {

  if (length(e) != 3)
    return false;

  SEXP place = CADR(e);

  if (TYPEOF(place) == SYMSXP || (TYPEOF(place) == STRSXP && length(place) == 1)) {
    return true;
  }

  while (TYPEOF(place) == LANGSXP) {
    SEXP fun = CAR(place);

    bool is_valid_fun = (TYPEOF(fun) == SYMSXP);
      
    if (!is_valid_fun && TYPEOF(fun) == LANGSXP && length(fun) == 3) {
      SEXP fun_head = CAR(fun);
      if (TYPEOF(fun_head) == SYMSXP) {
        if (fun_head == R_DoubleColonSymbol || fun_head == R_TripleColonSymbol) {
          is_valid_fun = true;
        }
      }
    }

    if (!is_valid_fun) {
      notify_bad_assign_fun(cntxt, loc);
      return false;
    }

    place = CADR(place);
  }

  return (TYPEOF(place) == SYMSXP);

}

bool cmp_symbol_assign( SEXP symbol, SEXP value, bool super_assign, CodeBuffer * cb, CompilerContext * cntxt ) {

  CompilerContext * ncntxt = make_non_tail_call_ctx(cntxt);
  cmp(value, cb, ncntxt, false, true);

  int ci = PUTCONST(symbol);
  if (super_assign)
    PUTCODE( SETVAR2_OP );
  else
    PUTCODE( SETVAR_OP );

  PUTCODE(ci);

  if (cntxt->tailcall)
    PUTCODES(INVISIBLE_OP, RETURN_OP);

  return true;

}

bool cmp_assign(SEXP e, CodeBuffer *cb, CompilerContext *cntxt) {

  if (!check_assign(e, cntxt, cb_savecurloc(cb))) {
    return cmp_special(e, cb, cntxt);
  }

  // new kind of comparasion, lets think
  bool super_assign = (strcmp( CHAR(PRINTNAME(CAR(e))), "<<-" ) == 0);

  SEXP lhs = CADR(e);
  SEXP value = CADDR(e);

  SEXP symbol = install( get_assigned_var(e, cntxt) );

  if (super_assign && !find_var(symbol, cntxt)) {
    notify_no_super_assign_var(symbol, cntxt, cb_savecurloc(cb));
  }

  if (isSymbol(lhs) || TYPEOF(lhs) == STRSXP) {
    return cmp_symbol_assign(symbol, value, super_assign, cb, cntxt);
  } 
  else if (TYPEOF(lhs) == LANGSXP) {
    return cmp_complex_assign(symbol, lhs, value, super_assign, cb, cntxt);
  } 
  else {
    return cmp_special(e, cb, cntxt);
  }

  return true;

}

bool dollar_setter_inline_handler(SEXP afun, SEXP place, SEXP orig, SEXP call, CodeBuffer *cb, CompilerContext *cntxt) {

  DEBUG_PRINT("[_] Trying to inline $<-\n");

  if (any_dots(place) || length(place) != 3) {
    return false;
  }

  SEXP sym = CADDR(place);

  if (TYPEOF(sym) == STRSXP && LENGTH(sym) > 0) {
      sym = install(CHAR(STRING_ELT(sym, 0)));
  }

  if (isSymbol(sym)) {

    int ci = PUTCONST(call);
    int csi = PUTCONST(sym);
    
    PUTCODES( DOLLARGETS_OP, ci, csi );    
    return true;

  }

  return false;
}

bool dollar_getter_inline_handler(SEXP call, CodeBuffer *cb, CompilerContext *cntxt) {
  
  if (any_dots(call) || length(call) != 3) {
    return false;
  }

  SEXP sym = CADDR(call);

  if (TYPEOF(sym) == STRSXP && LENGTH(sym) > 0) {
    sym = install(CHAR(STRING_ELT(sym, 0)));
  }

  if (isSymbol(sym)) {

    int ci = PUTCONST(call);
    int csi = PUTCONST(sym);

    PUTCODES( DUP2ND_OP, DOLLAR_OP, ci, csi, SWAP_OP );
    return true;
  }

  return false;
}

bool at_setter_inline_handler(SEXP afun, SEXP place, SEXP orig, SEXP call, CodeBuffer *cb, CompilerContext *cntxt) {

  if ( ! dots_or_missing(place) && length(place) == 3 && TYPEOF(CADDR(place)) == SYMSXP ) {

    SEXP place_cp = PROTECT( duplicate(place) );

    SETCADDR( place_cp, ScalarString(PRINTNAME(CADDR(place_cp))) );
    
    SEXP i = call;
    for ( ; CDR(i) != R_NilValue; i = CDR(i) );
    
    cmp_setter_call( place_cp, orig, CAR(i), cb, cntxt );

    UNPROTECT(1); // place_cp
    return true;

  }

  return false;

}

typedef struct dlftop {
  int code;
  bool rank;
} dlftop;

bool has_names(SEXP place) {
  for (SEXP s = place; s != R_NilValue; s = CDR(s)) {
    if (TAG(s) != R_NilValue) {
      return true; 
    }
  }

  return false;
}

void cmp_indices(SEXP indices, CodeBuffer * cb, CompilerContext * cntxt) {

  for ( SEXP i = indices; i != R_NilValue; i = CDR(i) ) {
    cmp( CAR(i), cb, cntxt, true, true );
  }

};

bool cmp_subset_dispatch( int start_op, dlftop dlftop, SEXP e, CodeBuffer * cb, CompilerContext * cntxt ) {

  if ( dots_or_missing(e) || has_names(e) || length(e) < 3 ) {
    return false;
  } else {
    
    SEXP oe = CADR(e);
    if ( oe == R_MissingArg ) {
      //PASS
      return false;
    }

    CompilerContext * ncntxt = make_arg_ctx(cntxt);
    int ci = PUTCONST(e);
    int label = cb_makelabel(cb);

    cmp(oe, cb, ncntxt, false, true);

    PUTCODES( start_op, ci );
    cb_putcodelabel(cb, label);

    SEXP indices = CDDR(e);
    cmp_indices(indices, cb, ncntxt);

    if (dlftop.rank)
      PUTCODES( dlftop.code, ci, length(indices) );
    else
      PUTCODES( dlftop.code, ci );

    cb_putlabel(cb, label);

    if ( cntxt->tailcall )
      PUTCODE( RETURN_OP );

    return true;

  }

  return false;

};

bool cmp_dispatch(int start_op, int dflt_op, SEXP e, CodeBuffer * cb, CompilerContext * cntxt, bool missing_ok) {

  if ((missing_ok && any_dots(e)) ||
      (!missing_ok && dots_or_missing(e)) ||
      length(e) == 1) {
        cmp_special(e,cb,cntxt);
  } else {

    int ne = length(e);
    SEXP oe = CADR(e);

    if ( oe == R_MissingArg )
      cmp_special(e,cb,cntxt);
    else {

      CompilerContext * ncntxt = make_arg_ctx(cntxt);
      cmp( oe, cb, ncntxt, false, true );
      int ci = PUTCONST(e);
      int end_label = cb_makelabel(cb);

      PUTCODES( start_op, ci );
      cb_putcodelabel(cb, end_label);

      if ( ne > 2 )
        cmp_builtin_args(CDDR(e), cb, cntxt, missing_ok);

      PUTCODE(dflt_op);
      cb_putlabel(cb, end_label);

      if ( cntxt->tailcall )
        PUTCODE(RETURN_OP);

    }

  }

  return true;

};

bool cmp_setter_dispatch(int start_op, int dflt_op, SEXP afun, SEXP place, SEXP call, CodeBuffer * cb, CompilerContext * cntxt) {

  if ( any_dots(place) )
    return false;
  else {

    int ci = PUTCONST(call);
    int end_label = cb_makelabel(cb);

    PUTCODES(start_op, ci);
    cb_putcodelabel(cb,end_label);

    if (length(place) > 2) {
      cmp_builtin_args(CDDR(place), cb, cntxt, true);
    }

    PUTCODE(dflt_op);
    cb_putlabel(cb, end_label);
    return true;

  }

}

bool inline_subset( SEXP e, CodeBuffer *cb, CompilerContext *cntxt ) {

  if ( dots_or_missing(e) || has_names(e) || length(e) < 3 ) {
    return cmp_dispatch(STARTSUBSET_OP, DFLTSUBSET_OP, e, cb, cntxt, true);
  } else {

    int nidx = length(e) - 2;
    dlftop dlftop;

    if ( nidx == 1 ) {
      dlftop.code = VECSUBSET_OP;
      dlftop.rank = false;
    } else if (nidx == 2) {
      dlftop.code = MATSUBSET_OP;
      dlftop.rank = false;
    } else {
      dlftop.code = SUBSET_N_OP;
      dlftop.rank = true;
    }

    return cmp_subset_dispatch( STARTSUBSET_N_OP, dlftop, e, cb, cntxt );

  }


};

bool inline_subset2( SEXP e, CodeBuffer *cb, CompilerContext *cntxt ) {

  if ( dots_or_missing(e) || has_names(e) || length(e) < 3 ) {
    return cmp_dispatch( STARTSUBSET2_OP, DFLTSUBSET2_OP, e, cb, cntxt, true );
  } else {

    int nidx = length(e) - 2;
    dlftop dlftop;
    
    if ( nidx == 1 ) {
      dlftop.code = VECSUBSET2_OP;
      dlftop.rank = false;
    } else if (nidx == 2) {
      dlftop.code = MATSUBSET2_OP;
      dlftop.rank = false;
    } else {
      dlftop.code = SUBSET2_N_OP;
      dlftop.rank = true;
    }

    return cmp_subset_dispatch(STARTSUBSET2_N_OP, dlftop, e, cb, cntxt);

  }

};

bool cmp_subassign_dispatch(int start_op, dlftop dfltop, SEXP afun, SEXP place, SEXP call, CodeBuffer * cb, CompilerContext * cntxt) {

  if ( dots_or_missing(place) || has_names(place) || length(place) < 3 ) {
    cntxt_stop("cannot compile this expression", cntxt, cb_savecurloc(cb));
    return false;
  } else {
    int ci = PUTCONST(call);
    int label = cb_makelabel(cb);

    PUTCODES(start_op, ci);
    cb_putcodelabel(cb,label);

    SEXP indices = CDDR(place);
    cmp_indices(indices, cb, cntxt);

    PUTCODES( dfltop.code, ci );

    if ( dfltop.rank )
      PUTCODE(length(indices));

    cb_putlabel(cb,label);
    return true;
    
  }

}

bool inline_subassign_setter(SEXP afun, SEXP place, SEXP orig, SEXP call, CodeBuffer *cb, CompilerContext *cntxt) {

  bool any_names = has_names(place);

  if ( dots_or_missing(place) || any_names || length(place) < 3 ) {
    return cmp_setter_dispatch(STARTSUBASSIGN_OP, DFLTSUBASSIGN_OP, afun,place,call,cb,cntxt);
  } else {

    int nidx = length(place) - 2;
    dlftop dfltop;
    
    if ( nidx == 1 ) {
      dfltop.code = VECSUBASSIGN_OP;
      dfltop.rank = false;
    } else if (nidx == 2) {
      dfltop.code = MATSUBASSIGN_OP;
      dfltop.rank = false;
    } else {
      dfltop.code = SUBASSIGN_N_OP;
      dfltop.rank = true;
    }

    return cmp_subassign_dispatch(STARTSUBASSIGN_N_OP, dfltop, afun, place, call, cb, cntxt);

  }

}

bool inline_subassign2_setter(SEXP afun, SEXP place, SEXP orig, SEXP call, CodeBuffer *cb, CompilerContext *cntxt) {

  bool any_names = has_names(place);

  if ( dots_or_missing(place) || any_names || length(place) < 3 ) {
    return cmp_setter_dispatch(STARTSUBASSIGN2_OP, DFLTSUBASSIGN2_OP, afun,place,call,cb,cntxt);
  } else {

    int nidx = length(place) - 2;
    dlftop dlftop;
    
    if ( nidx == 1 ) {
      dlftop.code = VECSUBASSIGN2_OP;
      dlftop.rank = false;
    } else if (nidx == 2) {
      dlftop.code = MATSUBASSIGN2_OP;
      dlftop.rank = false;
    } else {
      dlftop.code = SUBASSIGN2_N_OP;
      dlftop.rank = true;
    }

    return cmp_subassign_dispatch(STARTSUBASSIGN2_N_OP, dlftop, afun, place, call, cb, cntxt);

  }

}

bool cmp_getter_dispatch(int start_op, int dflt_op, SEXP call, CodeBuffer * cb, CompilerContext * cntxt) {

  if ( any_dots(call) ) {
    return false; // punt
  } else {

    int ci = PUTCONST(call);
    int end_label = cb_makelabel(cb);

    PUTCODES( DUP2ND_OP, start_op, ci );
    cb_putcodelabel(cb, end_label);

    if (length(call) > 2) {
      cmp_builtin_args(CDDR(call), cb, cntxt, true);
    }

    PUTCODE(dflt_op);
    cb_putlabel(cb, end_label);
    PUTCODE(SWAP_OP);

    return true;

  }

}

bool cmp_subset_getter_dispatch(int start_op, dlftop dfltop, SEXP call, CodeBuffer * cb, CompilerContext * cntxt) {

  // Fallback if missing args, named args, or insufficient length
  if ( dots_or_missing(call) || has_names(call) || length(call) < 3 ) {
    cntxt_stop("cannot compile this expression", cntxt, cb_savecurloc(cb));
    return false;
  } else {
    
    int ci = PUTCONST(call);
    int end_label = cb_makelabel(cb);

    PUTCODES( DUP2ND_OP, start_op, ci );
    cb_putcodelabel(cb, end_label);

    SEXP indices = CDDR(call);
    cmp_indices(indices, cb, cntxt);

    if ( dfltop.rank ) {
      PUTCODES( dfltop.code, ci, length(indices) );
    } else {
      PUTCODES( dfltop.code, ci );
    }

    cb_putlabel(cb, end_label);
    PUTCODE(SWAP_OP);
    
    return true;
  }

}

bool inline_subset_getter( SEXP call, CodeBuffer *cb, CompilerContext *cntxt ) {

  if ( dots_or_missing(call) || has_names(call) || length(call) < 3 ) {
    return cmp_getter_dispatch(STARTSUBSET_OP, DFLTSUBSET_OP, call, cb, cntxt);
  } else {

    int nidx = length(call) - 2;
    dlftop dfltop;

    if ( nidx == 1 ) {
      dfltop.code = VECSUBSET_OP;
      dfltop.rank = false;
    } else if (nidx == 2) {
      dfltop.code = MATSUBSET_OP;
      dfltop.rank = false;
    } else {
      dfltop.code = SUBSET_N_OP;
      dfltop.rank = true;
    }

    return cmp_subset_getter_dispatch( STARTSUBSET_N_OP, dfltop, call, cb, cntxt );
  }

}

bool inline_subset2_getter( SEXP call, CodeBuffer *cb, CompilerContext *cntxt ) {

  if ( dots_or_missing(call) || has_names(call) || length(call) < 3 ) {
    return cmp_getter_dispatch(STARTSUBSET2_OP, DFLTSUBSET2_OP, call, cb, cntxt);
  } else {

    int nidx = length(call) - 2;
    dlftop dfltop;
    
    if ( nidx == 1 ) {
      dfltop.code = VECSUBSET2_OP;
      dfltop.rank = false;
    } else if (nidx == 2) {
      dfltop.code = MATSUBSET2_OP;
      dfltop.rank = false;
    } else {
      dfltop.code = SUBSET2_N_OP;
      dfltop.rank = true;
    }

    return cmp_subset_getter_dispatch(STARTSUBSET2_N_OP, dfltop, call, cb, cntxt);
  }

}

bool cmp_multi_colon(SEXP e, CodeBuffer *cb, CompilerContext *cntxt) {

  if ( !dots_or_missing(e) && length(e) == 3 ) {

    #define good_type(a) ((TYPEOF(a) == SYMSXP) || (TYPEOF(a) == STRSXP && length(a) == 1))

    SEXP fun = CAR(e);
    SEXP x = CADR(e);
    SEXP y = CADDR(e);

    if ( good_type(x) && good_type(y) ) {
      
      SEXP x_str = PROTECT(coerceVector(x, STRSXP));
      SEXP y_str = PROTECT(coerceVector(y, STRSXP));
      SEXP inner = PROTECT(CONS( y_str, R_NilValue ));
      SEXP args  = PROTECT( CONS( x_str, inner ) );

      cmp_call_sym_fun(fun, args, e, cb, cntxt);
      UNPROTECT(4); // x_str, y_str, inner, args
      return true;
    
    } else return false;

  } else {
    return false;
  }

};

bool inline_with(SEXP e, CodeBuffer *cb, CompilerContext *cntxt) {

  cntxt->options.suppress_all_undef = true;
  cmp_call_sym_fun( CAR(e), CDR(e), e, cb, cntxt );
  return true;

};

bool inline_required(SEXP e, CodeBuffer *cb, CompilerContext *cntxt) {

  cntxt->options.suppress_all_undef = true;
  cmp_call_sym_fun( CAR(e), CDR(e), e, cb, cntxt );
  return true;

}

bool inline_colon(SEXP e, CodeBuffer *cb, CompilerContext *cntxt) {
  return cmp_prim_2( e, cb, COLON_OP, cntxt );
}

bool inline_seq_len(SEXP e, CodeBuffer *cb, CompilerContext *cntxt) {
  return cmp_prim_1( e, cb, SEQLEN_OP, cntxt );
}

bool inline_seq_along(SEXP e, CodeBuffer *cb, CompilerContext *cntxt) {
  return cmp_prim_1( e, cb, SEQALONG_OP, cntxt );
}

bool inline_dollar(SEXP e, CodeBuffer *cb, CompilerContext *cntxt) {

  if (any_dots(e) || length(e) != 3) {
    cmp_special(e, cb, cntxt);
    return true; 
  }

  SEXP arg3 = CADDR(e);
  SEXP sym = arg3;

  // if (is.character(e[[3]]) && length(e[[3]]) == 1 && e[[3]] != "")
  if (TYPEOF(arg3) == STRSXP && length(arg3) == 1) {
    const char *str = CHAR(STRING_ELT(arg3, 0));
    if (str[0] != '\0') {
      sym = install(str);
    }
  }

  if (TYPEOF(sym) == SYMSXP) {

    CompilerContext * ncntxt = make_arg_ctx(cntxt);
    
    cmp(CADR(e), cb, ncntxt, false, true);
    
    int ci = PUTCONST(e);
    int csi = PUTCONST(sym);

    PUTCODES( DOLLAR_OP, ci, csi );

    if (cntxt->tailcall)
      PUTCODE(RETURN_OP);

    return true;

  } else {
    cmp_special(e, cb, cntxt);
    return true;
  }
}

SEXP inline_simple_internal_call(SEXP e, SEXP def) {

  if (!dots_or_missing(e) && is_simple_internal(def)) {
    
    SEXP forms = FORMALS(def);
    
    SEXP b = BODY(def);
    
    if (TYPEOF(b) == BCODESXP) {
      b = R_BytecodeExpr(b);
    }
    
    if (TYPEOF(b) == LANGSXP && length(b) == 2 && CAR(b) == install("{")) {
      b = CADR(b);
    }
    
    SEXP icall = CADR(b);
    
    bool match_err = false;
    SEXP matched_call_raw = eval_match_call(def, e, false, &match_err);
    if (matched_call_raw == R_NilValue && match_err) {
      return R_NilValue;
    }

    SEXP matched_call = PROTECT(matched_call_raw);
    
    SEXP args_head = R_NilValue;
    SEXP args_tail = R_NilValue;
    
    SEQ_ALONG( node, CDR(icall) ) {
      SEXP n = CAR(node);
      SEXP subst_val = n;
      
      if (TYPEOF(n) == SYMSXP) {
        bool found = false;
        
        SEQ_ALONG( m_node, CDR(matched_call) ) {
          if (TAG(m_node) == n) {
            subst_val = CAR(m_node);
            found = true;
            break;
          }
        }
        
        if (!found) {
          SEQ_ALONG( f_node, forms ) {
            if (TAG(f_node) == n) {
              subst_val = CAR(f_node);
              break;
            }
          }
        }
      }
      
      SEXP new_node = PROTECT(CONS(subst_val, R_NilValue));
      if (args_head == R_NilValue) {
        args_head = new_node;
      } else {
        SETCDR(args_tail, new_node);
      }
      args_tail = new_node;
      UNPROTECT(1); // new_node 
    }
    
    PROTECT(args_head);
    
    SEXP inner_call = PROTECT(Rf_lcons(CAR(icall), args_head));
    SEXP dot_internal_sym = install(".Internal");
    SEXP final_call = PROTECT(Rf_lang2(dot_internal_sym, inner_call));
    
    UNPROTECT(4); // matched_call, args_head, inner_call, final_call 
    
    return final_call;
    
  } else {
    return R_NilValue;
  }
}

bool simple_formals(SEXP def) {
  SEXP forms = FORMALS(def);
  
  // False if "..." is involved
  SEQ_ALONG( node, forms ) {
    
    if (TAG(node) == R_DotsSymbol)
      return false;
  
  }

  SEQ_ALONG( node, forms ) {

    SEXP d = CAR(node);

    if (d != R_MissingArg) {
      int type = TYPEOF(d);

      if (type == SYMSXP || type == LANGSXP || type == PROMSXP || type == BCODESXP) {
        return false;
      }
    }
  }
  return true;
}


bool is_simple_internal(SEXP def) {

  if (TYPEOF(def) == CLOSXP && simple_formals(def)) {
    
    SEXP b = BODY(def);
    
    // since R 3.5.0 packages are precompiled
    if (TYPEOF(b) == BCODESXP) {
      b = R_BytecodeExpr(b);
    }
    
    if (TYPEOF(b) == LANGSXP && length(b) == 2 && CAR(b) == install("{")) {
      b = CADR(b);
    }
    
    if (TYPEOF(b) == LANGSXP && CAR(b) == install(".Internal")) {
      SEXP icall = CADR(b);
      SEXP ifun = CAR(icall);
      
      if (TYPEOF(ifun) == SYMSXP) {
        SEXP is_builtin_sym = install("is.builtin.internal");
        SEXP dot_internal_sym = install(".Internal");
        SEXP quote_sym = install("quote");
        
        SEXP quoted_ifun = PROTECT(Rf_lang2(quote_sym, ifun));
        SEXP inner_call = PROTECT(Rf_lang2(is_builtin_sym, quoted_ifun));
        SEXP outer_call = PROTECT(Rf_lang2(dot_internal_sym, inner_call));
        
        SEXP res = PROTECT(Rf_eval(outer_call, R_BaseEnv));
        bool is_builtin = LOGICAL(res)[0];
        
        UNPROTECT(4); // res, outer_call, inner_call, quoted_ifun
        
        if (is_builtin && simple_args(icall, FORMALS(def))) {
          return true;
        }
      }
    }
  }
  return false;
}

// Note: forms is passed directly instead of names(forms) to do pointer matching on tags.
bool simple_args(SEXP icall, SEXP forms) {

  SEQ_ALONG( node, CDR(icall) ) {

    SEXP a = CAR(node);
    
    if (a == R_MissingArg) {
      return false;
    } else if (TYPEOF(a) == SYMSXP) {

      bool found = false;

      SEQ_ALONG( f_node, forms ) {
        if (TAG(f_node) == a) {
          found = true;
          break;
        }

      }

      if (!found)
        return false;

    } else {
      
      int type = TYPEOF(a);
      if (type == LANGSXP || type == PROMSXP || type == BCODESXP) {
        return false;
      }
    
    }
  }
  return true;
}


SEXP simple_internals(SEXP pos) {
  
  // names <- ls(pos = pos, all.names = TRUE)
  SEXP ls_sym = install("ls");
  SEXP all_names_sym = install("all.names");
  
  SEXP ls_call = PROTECT(Rf_lang3(ls_sym, pos, R_TrueValue));
  SET_TAG(CDDR(ls_call), all_names_sym);
  
  SEXP names_vec = PROTECT(Rf_eval(ls_call, R_GlobalEnv));
  
  if (TYPEOF(names_vec) != STRSXP || length(names_vec) == 0) {
    UNPROTECT(2); // ls_call, names_vec
    return Rf_allocVector(STRSXP, 0);
  }

  SEXP as_env_sym = install("as.environment");
  SEXP env_call = PROTECT(Rf_lang2(as_env_sym, pos));
  SEXP target_env = PROTECT(Rf_eval(env_call, R_GlobalEnv));
  
  int n = length(names_vec);
  int match_count = 0;
  
  // Temporary array to flag which names are simple internals
  int *keep = (int*)R_alloc(n, sizeof(int));
  
  for (int i = 0; i < n; i++) {
    SEXP sym = installChar(STRING_ELT(names_vec, i));
    SEXP def = Rf_findVarInFrame(target_env, sym);
    
    if (def != R_UnboundValue && is_simple_internal(def)) {
      keep[i] = 1;
      match_count++;
    } else {
      keep[i] = 0;
    }
  }
  
  SEXP res = PROTECT(Rf_allocVector(STRSXP, match_count));
  int idx = 0;
  for (int i = 0; i < n; i++) {
    if (keep[i]) {
      SET_STRING_ELT(res, idx++, STRING_ELT(names_vec, i));
    }
  }
  
  UNPROTECT(5);
  return res;
};

bool cmp_simple_internal(SEXP e, CodeBuffer *cb, CompilerContext *cntxt) {
  
  if (any_dots(e))
    return false;
 
  SEXP fun_sym = CAR(e);
  SEXP def = find_fun_def(fun_sym, cntxt);
  
  bool should_warn = false;
  if (!check_call(def, e, &should_warn)) {
    if (should_warn) notify_bad_call(e, cntxt, cb_savecurloc(cb));
    return false;
  }
  
  SEXP call = inline_simple_internal_call(e, def);
  
  if (call == R_NilValue) {
    return false;
  } else {
    return cmp_dot_internal_call(call, cb, cntxt);
  }
}


bool cmp_dot_internal_call(SEXP e, CodeBuffer *cb, CompilerContext *cntxt) {
  
  SEXP ee = CADR(e);
  SEXP sym = CAR(ee);
  
  bool is_builtin = false;
  
  if (TYPEOF(sym) == SYMSXP) {

    SEXP is_builtin_sym = install("is.builtin.internal");
    SEXP dot_internal_sym = install(".Internal");
    
    SEXP quoted_sym = PROTECT(Rf_lang2(install("quote"), sym));
    SEXP inner_call = PROTECT(Rf_lang2(is_builtin_sym, quoted_sym));
    SEXP outer_call = PROTECT(Rf_lang2(dot_internal_sym, inner_call));
    
    SEXP res = PROTECT(Rf_eval(outer_call, R_BaseEnv));
    
    if (TYPEOF(res) == LGLSXP && LENGTH(res) > 0) {
      is_builtin = LOGICAL(res)[0];
    }
    
    UNPROTECT(4);
  }
  
  if (is_builtin) {
    return cmp_builtin(ee, cb, cntxt, true);
  } else {
    return cmp_special(e, cb, cntxt);
  }
  
}

bool inline_local(SEXP e, CodeBuffer *cb, CompilerContext *cntxt) {
  
  if (length(e) == 2) {
    
    SEXP expr = CADR(e); 
    
    SEXP fun_sym = install("function");
    SEXP inner_call = PROTECT(Rf_lang4(fun_sym, R_NilValue, expr, R_NilValue));
    
    SEXP outer_call = PROTECT(Rf_lang1(inner_call));
    
    cmp(outer_call, cb, cntxt, false, true);
    
    UNPROTECT(2); // outer_call, inner_call
    return true;
    
  } else return false;
}

bool cmp_is( int op, SEXP e, CodeBuffer *cb, CompilerContext * cntxt ) {

  if ( any_dots(e) || length(e) != 2 )
    return cmp_builtin(e, cb, cntxt, false);
  else {

    CompilerContext * s = make_arg_ctx(cntxt);
    cmp(CADR(e), cb, s, false, true);
    PUTCODE(op);

    if ( cntxt->tailcall )
      PUTCODE(RETURN_OP);
    
    return true;
  }

};

bool inline_is_character(SEXP e, CodeBuffer *cb, CompilerContext *cntxt) {
  return cmp_is(ISCHARACTER_OP, e, cb, cntxt);
}

bool inline_is_complex(SEXP e, CodeBuffer *cb, CompilerContext *cntxt) {
  return cmp_is(ISCOMPLEX_OP, e, cb, cntxt);
}

bool inline_is_double(SEXP e, CodeBuffer *cb, CompilerContext *cntxt) {
  return cmp_is(ISDOUBLE_OP, e, cb, cntxt);
}

bool inline_is_integer(SEXP e, CodeBuffer *cb, CompilerContext *cntxt) {
  return cmp_is(ISINTEGER_OP, e, cb, cntxt);
}

bool inline_is_logical(SEXP e, CodeBuffer *cb, CompilerContext *cntxt) {
  return cmp_is(ISLOGICAL_OP, e, cb, cntxt);
}

bool inline_is_name(SEXP e, CodeBuffer *cb, CompilerContext *cntxt) {
  // is.name uses the same instruction as is.symbol
  return cmp_is(ISSYMBOL_OP, e, cb, cntxt);
}

bool inline_is_null(SEXP e, CodeBuffer *cb, CompilerContext *cntxt) {
  return cmp_is(ISNULL_OP, e, cb, cntxt);
}

bool inline_is_object(SEXP e, CodeBuffer *cb, CompilerContext *cntxt) {
  return cmp_is(ISOBJECT_OP, e, cb, cntxt);
}

bool inline_is_symbol(SEXP e, CodeBuffer *cb, CompilerContext *cntxt) {
  return cmp_is(ISSYMBOL_OP, e, cb, cntxt);
}

bool inline_c_call(SEXP e, CodeBuffer *cb, CompilerContext *cntxt) {

  const int nargsmax = 16;

  if ( dots_or_missing( CDR(e) ) || has_names(e)
      || length(e) < 2 || length(e) > nargsmax + 2 )
      return cmp_builtin(e, cb, cntxt, false);
  else {

    CompilerContext * ncntxt = make_non_tail_call_ctx(cntxt);
    cmp(CADR(e), cb, ncntxt, false, true);
    int nargs = length(e) - 2;
    if ( nargs > 0 ) {
      ncntxt = make_arg_ctx( cntxt );
      
      for ( SEXP iter = CDDR(e); iter != R_NilValue ; iter = CDR(iter) ) {
        cmp( CAR(iter), cb, ncntxt, false, true );
      }
    }

    int ci = PUTCONST(e);
    PUTCODES( DOTCALL_OP, ci, nargs );

    if (cntxt->tailcall)
      PUTCODE(RETURN_OP);

    return true;

  }

}

typedef struct {

  SEXP expr;
  const char * name;
  int label;
  bool is_missing;
  bool is_default;
  int next_non_missing;

} SwitchCase;

// returns false if there are multiple defaults
bool preprocess_cases(SEXP cases, SwitchCase * processed, CodeBuffer * cb, bool * has_names) {

  int n_unnamed = 0;
  int n_named = 0;

  SEQ_ALONG( iter, cases ) {
    SEXP name = TAG(iter);
    if (name == R_NilValue || strlen(CHAR(PRINTNAME(name))) == 0) n_unnamed++;
    else n_named++;
  }

  if (n_named == 0 && length(cases) == 1) {
    n_named = 1; 
    *has_names = true;
  } else {
    *has_names = (n_named > 0);
  }

  if (n_unnamed > 1 && n_named > 0) {
      return false; 
  }

  // When a missing case is met, a pointer to it in the processed array is stored.
  // Once another non-missing case occurs, the unresolved cases get its next_non_missing
  // set to it and the unresolved array is cleared.
  int n_unresolved = 0;
  SwitchCase ** unresolved = (SwitchCase**) R_alloc( length(cases), sizeof(SwitchCase*) );


  int idx = 0;
  for ( SEXP iter = cases; iter != R_NilValue; iter = CDR(iter) ) {

    SEXP expr = CAR(iter);
    SEXP name = TAG(iter);

    SwitchCase * current = &(processed[idx]);

    current->expr = expr;

    current->is_missing = (expr == R_MissingArg);
    current->is_default = ( (name == R_NilValue) || (strlen(CHAR(PRINTNAME(name))) == 0) );


    if ( !current->is_missing ) {
      current->label = cb_makelabel(cb);      
      current->next_non_missing = current->label;
    }

    if ( current->is_missing ) {
      current->label = -1;
      current->next_non_missing = -1;
      unresolved[n_unresolved] = current;
      n_unresolved++;

    } else if ( n_unresolved > 0 ) {
      
      for ( int i = 0; i < n_unresolved; i++ ) {
        unresolved[i]->next_non_missing = current->label;
      }

      n_unresolved = 0;

    }

    if ( current->is_default ) {
      n_unnamed++;
    } else {
      current->name = CHAR(PRINTNAME(name));
      n_named++;
      *has_names = true;
    }

    idx++;
  }

  return true;

}

SEXP resolve_char_labels(SEXP cases, SwitchCase * processed, int default_label, int ** out_labels, int * out_count) {
  int total = length(cases);
  int n_used = 0;
  
  const char ** used_names = (const char **) R_alloc( total + 1, sizeof(const char*) );
  int * jumps_arr = (int *) R_alloc( total + 1, sizeof(int) );

  int char_default_target = default_label;

  for ( int i = 0; i < total; i++ ) {
    if ( processed[i].is_default ) {
      if (char_default_target == default_label) {
        char_default_target = (processed[i].next_non_missing == -1) ? default_label : processed[i].next_non_missing;
      }
    } else {
      bool matches = false; 
      for ( int j = 0; j < n_used; j++ ) {
        if (strcmp( processed[i].name, used_names[j] ) == 0) {
          matches = true;
          break;
        }
      }
      if ( !matches ) {
        used_names[n_used] = processed[i].name;
        jumps_arr[n_used] = (processed[i].next_non_missing == -1) ? default_label : processed[i].next_non_missing;
        n_used++;
      }
    }
  }

  used_names[n_used] = "";
  jumps_arr[n_used] = char_default_target;
  n_used++;

  SEXP names_sexp = PROTECT( allocVector( STRSXP, n_used ) );
  for ( int i = 0; i < n_used; i++ ) {
    SET_STRING_ELT( names_sexp, i, mkChar(used_names[i]) );
  }

  *out_labels = jumps_arr;
  *out_count = n_used;

  return names_sexp; // Caller must UNPROTECT(1)
}

int * resolve_num_labels(SEXP cases, SwitchCase * processed, int miss_label, int default_label, int * out_count) {
  int total = length(cases);
  *out_count = total + 1;
  
  int * jumps_arr = (int *) R_alloc( *out_count, sizeof(int) );

  for ( int i = 0; i < total; i++ ) {
    jumps_arr[i] = processed[i].is_missing ? miss_label : processed[i].label;
  }
  
  jumps_arr[total] = default_label;

  return jumps_arr;
}

bool inline_switch(SEXP e, CodeBuffer *cb, CompilerContext *cntxt) {

  if ( length(e) < 2 || any_dots(e) )
    return cmp_special(e, cb, cntxt);
  else {

    SEXP expr = CADR(e);
    SEXP cases = CDDR(e);
    int end_label = -1;

    if ( !cntxt->tailcall )
      end_label = cb_makelabel(cb);

    if ( length(cases) < 1 ) {
      notify_no_switch_cases(cntxt, cb_savecurloc(cb));
    }

    SwitchCase * processed_cases = (SwitchCase*) R_alloc( length(cases), sizeof(SwitchCase) );

    bool has_names = false;
    bool okay = preprocess_cases( cases, processed_cases, cb, &has_names );

    if ( ! okay ) {
      notify_multiple_switch_defaults(cntxt, cb_savecurloc(cb));
      cmp_special(e, cb, cntxt);
      return true;
    }

    CompilerContext * ncntxt = make_non_tail_call_ctx(cntxt);
    cmp( expr, cb, ncntxt, false, true );

    int call_idx = PUTCONST(e);

    int names_idx = -1;
    int null_idx = -1;
    int char_count = 0;
    int * char_labels_c = NULL; 

    int default_label = cb_makelabel(cb);

    if (has_names) {
        SEXP names_sexp = resolve_char_labels(cases, processed_cases, default_label, &char_labels_c, &char_count);
        names_idx = PUTCONST(names_sexp);
        UNPROTECT(1);
    } else {
        null_idx = PUTCONST(R_NilValue);
    }

    bool any_miss = false;
    int miss_label = -1;

    for ( int i = 0; i < length(cases); i++ )
      if ( processed_cases[i].is_missing ) any_miss = true;

    if ( any_miss )
      miss_label = cb_makelabel(cb);

    int int_count = 0;
    int * int_labels_c = resolve_num_labels(cases, processed_cases, miss_label, default_label, &int_count);

    PUTCODES( SWITCH_OP, call_idx );

    int char_pos = -1;

    if ( has_names ) {
      PUTCODE(names_idx);
      char_pos = cb->code_count;
      PUTCODE(0);         
    } else {
      PUTCODE(null_idx); 
      char_pos = cb->code_count;
      PUTCODE(null_idx);  
    }

    int int_pos = cb->code_count;  
    
    PUTCODE(0);             
    cb_putswitch(cb, int_labels_c, int_count, int_pos, char_labels_c, char_count, char_pos);

    if ( any_miss ) {

      cb_putlabel(cb, miss_label);
      SEXP inner = PROTECT(mkString("empty alternative in numeric switch"));
      SEXP stop_call = PROTECT(Rf_lang2(install("stop"), inner));
      cmp(stop_call, cb, cntxt, false, true); 
      UNPROTECT(2); //inner, stop_call
    
    }
    
    cb_putlabel(cb, default_label);
    PUTCODE(LDNULL_OP);

    if (cntxt->tailcall) {
      PUTCODES( INVISIBLE_OP, RETURN_OP );
    } else {
      PUTCODE( GOTO_OP );
      cb_putcodelabel(cb, end_label); 
    }

    for ( int i = 0; i < length(cases); i++ ) {
      if ( ! processed_cases[i].is_missing ) {
      
        cb_putlabel(cb, processed_cases[i].label);
        cmp( processed_cases[i].expr, cb, cntxt, false, true );

        if ( ! cntxt->tailcall ) {
          PUTCODE(GOTO_OP);
          cb_putcodelabel(cb, end_label);
        }
      
      }
    }

    if ( ! cntxt->tailcall )
      cb_putlabel(cb, end_label);


    return true;
  }

}

#pragma endregion
