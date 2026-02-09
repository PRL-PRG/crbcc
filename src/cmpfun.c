#pragma region Headers

#include <R.h>
#include <Rdefines.h>
#include <R_ext/Rdynload.h>
#include <Rinternals.h>
#include <ctype.h>

//#define DEBUG

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
#define DOMISSING_OP 40
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

#pragma endregion

#pragma region Data Structures

typedef struct CompilerContext {

  bool toplevel;                  // Is this a top-level expression?
  bool tailcall;                  // Is this in tail position?
  bool need_return_jmp;           // Does return() need a longjmp?

  short optimize_level;           // Integer (0-3) optimization level

  // Error flags
  int supress_all;
  int supress_no_super_assign;
  SEXP supress_undefined;         // Can be a boolean or char vector ()

  // Other structures
  struct CompilerEnv *env;        // Current compilation environment
  struct LoopInfo *loop;          // Current loop context (NULL if not in a loop)
  
  SEXP call;                      //Current R call being compiler (used for error messages)

} CompilerContext;


typedef struct LoopInfo {

  int loop_label_id;              // Label ID for the start of the loop
  int end_label_id;               // Label ID for the end of the loop
  bool goto_ok;                   // Can a simple GOTO be used
  struct LoopInfo *next;          // Linked list to previous loop (for nested loops)

} LoopInfo;

typedef enum {

  FRAME_LOCAL,
  FRAME_NAMESPACE,
  FRAME_GLOBAL

} FrameType;

typedef struct EnvFrame {

  FrameType type;         
  SEXP r_env;                     // R environment object
  SEXP extra_vars;                // Vector of local variables discovered by the compiler
  struct EnvFrame *parent;        // Pointer to parent frame

  SEXP shadow_node;               // For GC safety, holds r_env and extra_vars,
                                  // lives inside the CompilerEnv shadow stack
                                  // which is protected

} EnvFrame;

typedef struct CompilerEnv {

  struct EnvFrame *top_frame;     // Current frame being compiled
  SEXP shadow_stack;              // Handle for GC safety

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

} CodeBuffer;

typedef struct SwitchPatch {

  int cb_index;                   // Position in code buffer to patch
  int *label_ids;                 // Array of label IDs to jump to
  int count;                      // Number of label IDs
  struct SwitchPatch *next;       // Next patch in linked list

} SwitchPatch;

typedef struct VarInfo {

  EnvFrame * defining_frame;      // Frame where the variable is defined
  SEXP value;                     // var value
  bool found;                     // Was the variable found

} VarInfo;

// in the original compiler loc = list(expr, srcref)
typedef struct Loc {
  bool is_null;
  SEXP expr;
  SEXP srcref;
} Loc;

typedef bool (*HandlerFn)(SEXP e, CodeBuffer *cb, CompilerContext *cntxt);

typedef struct InlineHandler {

  char func_name[256];            // Name of the function being inlined
  HandlerFn handler;              // Function pointer to the inlining handler

} InlineHandler;

typedef struct InlineInfo {

  SEXP env;                       // Environment where the function is defined
  
  bool can_inline;                // Can this function be inlined
  bool guard_needed;              // Is a type guard needed

  char package[256];              // Package name

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

// Code emission and constant pool management
void cb_putcode(CodeBuffer *cb, int opcode);
int cb_getcode(CodeBuffer *cb, int pos);
int cb_putconst(CodeBuffer *cb, SEXP item);
SEXP cb_getconst(CodeBuffer *cb, int idx);
void cb_putswitch(CodeBuffer *cb, int *label_ids, int count);

// === CONTEXT AND ENVIRONMENT FUNCTIONS ===

// Constructors for various compiler contexts etc
CompilerEnv *make_cenv(SEXP env);
CompilerEnv *make_fun_env(SEXP forms, SEXP body, CompilerContext *cntxt);
CompilerContext *make_toplevel_ctx(CompilerEnv *cenv);
CompilerContext *make_function_ctx(CompilerContext *cntxt, CompilerEnv *fenv, SEXP forms, SEXP body);
CompilerContext *make_call_ctx(CompilerContext *cntxt, SEXP call);
CompilerContext *make_non_tail_call_ctx(CompilerContext *cntxt);
CompilerContext *make_no_value_ctx(CompilerContext *cntxt);
CompilerContext *make_loop_ctx(CompilerContext *cntxt, int loop_label, int end_label);
CompilerContext *make_arg_ctx(CompilerContext *cntxt);
CompilerContext *make_promise_ctx(CompilerContext *cntxt);

// Environment manipulation
void add_cenv_vars(CompilerEnv *cenv, SEXP vars);
void add_cenv_frame(CompilerEnv *cenv, SEXP vars);

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

SEXP cmpfun(SEXP f, void* __placeholder__);

// Weird ahh functions
bool find_var(SEXP var, CompilerContext *cntxt);
SEXP find_locals(SEXP expr, SEXP known_locals);
SEXP find_locals_list(SEXP elist, SEXP known_locals);
SEXP gen_code(SEXP e, CompilerContext *cntxt, SEXP gen, Loc loc);
SEXP get_assigned_var(SEXP var);
bool may_call_browser(SEXP expr, CompilerContext *cntxt);
bool may_call_browser_list(SEXP exprlist, CompilerContext * cntxt);
SEXP is_compiled(SEXP fun);
void R_init_crbcc(DllInfo* dll);
VarInfo find_cenv_var( SEXP var, CompilerEnv * cenv );
bool find_loc_var( SEXP var, CompilerContext * cntxt );
SEXP code_buf_code( CodeBuffer * cb, CompilerContext * cntxt );
bool is_ddsym(SEXP sym);
SEXP find_fun_def( SEXP fun_sym, CompilerContext * cntxt );
SEXP check_call( SEXP def, SEXP call );
bool any_dots( SEXP args );

// Inlining
bool try_inline( SEXP e, CodeBuffer *cb, CompilerContext *cntxt );
InlineInfo get_inline_info( char name[256], CompilerContext * cntxt, bool guard_ok );
bool get_inline_handler( char name[256], char package[256], HandlerFn * found );
void pack_frame_name(EnvFrame * frame, char out[256] );

#pragma endregion

#pragma region Inline function declarations

// The actual inlining handlers
bool inline_left_brace( SEXP e, CodeBuffer *cb, CompilerContext *cntxt );
bool inline_function( SEXP e, CodeBuffer *cb, CompilerContext *cntxt );
bool inline_return( SEXP e, CodeBuffer *cb, CompilerContext *cntxt );
bool fake_inline_assign( SEXP e, CodeBuffer *cb, CompilerContext *cntxt );
bool inline_if( SEXP e, CodeBuffer *cb, CompilerContext *cntxt );
bool inline_left_parenthesis( SEXP e, CodeBuffer *cb, CompilerContext *cntxt );
bool inline_and( SEXP e, CodeBuffer *cb, CompilerContext *cntxt );
bool inline_or( SEXP e, CodeBuffer *cb, CompilerContext *cntxt );
bool inline_repeat( SEXP e, CodeBuffer *cb, CompilerContext *cntxt );
bool inline_next( SEXP e, CodeBuffer *cb, CompilerContext *cntxt );
bool inline_break( SEXP e, CodeBuffer *cb, CompilerContext *cntxt );

#pragma endregion

#pragma region o0

static bool is_base_var(SEXP sym, CompilerContext *cntxt);

static SEXP R_bcVersion();
static bool is_in_set(SEXP sym, SEXP set);
static SEXP union_sets(SEXP a, SEXP b);

SEXP constant_fold(SEXP e, CompilerContext* cntxt, Loc loc);

static bool is_in_c_set(const char *str, const char *set[]) {
  if (str == NULL) return false;
    
  for (int i = 0; set[i] != NULL; i++) {
    if (strcmp(str, set[i]) == 0) {
      return true;
    }
  }
  return false;
}

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

static SEXP constant_fold_sym( SEXP var, CompilerContext * cntxt ) {

  if ( is_in_c_set(CHAR(PRINTNAME(var)), const_names) && is_base_var(var, cntxt) ) {

    SEXP val = Rf_findVar( var, R_BaseNamespace );

    if ( Rf_isFunction( val ) ) {
      return val;
    }

  }

  return R_NilValue;

};

static SEXP get_fold_fun(SEXP var, CompilerContext *cntxt) {

  if ( is_in_c_set( CHAR(PRINTNAME(var)), fold_funs ) && is_base_var( var, cntxt ) ) {

    SEXP val = Rf_findVar( var, R_BaseNamespace );

    if ( Rf_isFunction(val) )
      return val;

  }

  return R_NilValue;

};


//  !!!!!!!!! TODO dangerous
static SEXP constant_fold_call(SEXP e, CompilerContext *cntxt) {
    
  SEXP fun = CAR(e);
    
    // if (typeof(fun) == "symbol")
    if (TYPEOF(fun) == SYMSXP) {
        
        // ffun <- getFoldFun(fun, cntxt)
        SEXP ffun = get_fold_fun(fun, cntxt);
        
        // if (! is.null(ffun))
        if (ffun != R_NilValue) {
            
            // First pass: Count arguments to allocate storage
            int n_args = 0;
            for (SEXP s = CDR(e); s != R_NilValue; s = CDR(s)) {
                n_args++;
            }

            // Allocate temporary storage for folded values and their names (tags)
            // We use VECSXP to hold them safely during processing
            SEXP folded_values = PROTECT(allocVector(VECSXP, n_args));
            SEXP arg_tags = PROTECT(allocVector(VECSXP, n_args));
            
            int i = 0;
            bool ok = true;
            
            // Iterate original arguments: fold and store
            for (SEXP s = CDR(e); s != R_NilValue; s = CDR(s), i++) {
                SEXP a = CAR(s);
                SEXP tag = TAG(s);
                
                // if (missing(a)) ...
                if (a == R_MissingArg) {
                    ok = false; 
                    break;
                }
                
                // val <- constantFold(a, cntxt)
                Loc null_loc = {0}; 
                SEXP val_wrapper = constant_fold(a, cntxt, null_loc);
                
                if (val_wrapper != R_NilValue) {
                    // Extract value from list(value=...) wrapper
                    SEXP val = VECTOR_ELT(val_wrapper, 0);
                    
                    // Check mode immediately to save work
                    if (!is_const_mode(val)) {
                        ok = false;
                        break;
                    }
                    
                    SET_VECTOR_ELT(folded_values, i, val);
                    SET_VECTOR_ELT(arg_tags, i, tag); // Store original name
                } else {
                    ok = false;
                    break;
                }
            }
            
            if (ok) {
                // Construct the new call (ffun val1 val2 ...)
                // We must build it backwards: (val_last) -> (val_n-1, val_last) ...
                
                SEXP new_call = R_NilValue;
                
                // 1. Build the argument list from back to front
                for (int j = n_args - 1; j >= 0; j--) {
                    SEXP val = VECTOR_ELT(folded_values, j);
                    SEXP tag = VECTOR_ELT(arg_tags, j);
                    
                    // LCONS creates a new LANGSXP node with CAR=val, CDR=new_call
                    new_call = LCONS(val, new_call);
                    
                    // Restore the argument name if it existed
                    if (tag != R_NilValue) {
                        SET_TAG(new_call, tag);
                    }
                }
                
                // 2. Prepend the function itself
                new_call = LCONS(ffun, new_call);
                
                // Protect the constructed call
                PROTECT(new_call);
                
                // Execute
                int error = 0;
                SEXP result = R_tryEval(new_call, R_BaseEnv, &error);
                
                UNPROTECT(1); // new_call
                UNPROTECT(2); // folded_values, arg_tags
                
                if (!error) {
                    return check_const(result);
                } else {
                    return R_NilValue;
                }
            }
            
            UNPROTECT(2); // folded_values, arg_tags
            return R_NilValue;
        }
    }
    
    return R_NilValue;
}


static bool dots_or_missing(SEXP args) {
  for (SEXP s = args; s != R_NilValue; s = CDR(s)) {
    SEXP val = CAR(s);
    if (val == R_DotsSymbol || val == R_MissingArg) {
      return true;
    }
  }
  return false;
}


SEXP constant_fold(SEXP e, CompilerContext* cntxt, Loc loc) {

  SEXPTYPE type = TYPEOF(e);

  switch (type)
  {
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


void cmp_special(SEXP e, CodeBuffer *cb, CompilerContext *cntxt) {

  SEXP fun = CAR(e);

  if ( TYPEOF(fun) == CHARSXP ) {
    fun = Rf_install( CHAR( fun ) ); //TODO protect?
  }

  int ci = cb_putconst( cb, fun );
  cb_putcode( cb, CALLSPECIAL_OP );
  cb_putcode( cb, ci );

  if (  cntxt->tailcall )
    cb_putcode( cb, RETURN_OP );

};


void cmp_builtin_args(SEXP args, CodeBuffer *cb, CompilerContext *cntxt, bool missingOK) {
    
  CompilerContext * ncntxt = make_arg_ctx( cntxt );

  for (SEXP s = args; s != R_NilValue; s = CDR(s)) {
        
    SEXP a = CAR(s);  // The argument value (args[[i]])
    SEXP n = TAG(s);  // The argument name  (names[[i]]) TODO check

    if (a == R_MissingArg) {
      if (missingOK) {
        cb_putcode(cb, DOMISSING_OP);
        cmp_tag(n, cb);
      } 
      else {
        // cntxt$stop(gettext("missing arguments are not allowed"), ...)
        Loc loc = cb_savecurloc(cb);
        exit(1); //TODO correct kill
        // compiler_stop(cntxt, "missing arguments are not allowed", loc);
      }
  } else {

    if (TYPEOF(a) == SYMSXP) {
        // ca <- constantFold(a, cntxt, loc = cb$savecurloc())
        Loc loc = cb_savecurloc(cb);
        SEXP ca = constant_fold(a, cntxt, loc);

        if (ca == R_NilValue) {
          cmp_sym(a, cb, ncntxt, missingOK);
          cb_putcode(cb, PUSHARG_OP);
        } else {
          // TODO constant folded to a constant?
          SEXP ca_value = VECTOR_ELT(ca, 0); 
          cmp_const_arg(ca_value, cb, cntxt);
        }
    }

    else if (TYPEOF(a) == LANGSXP) {
      cmp(a, cb, ncntxt, false, true);        
      cb_putcode(cb, PUSHARG_OP);
    }

    else {
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

    int ci = cb_putconst( cb, fun );

    if (internal)
      cb_putcode( cb, GETINTLBUILTIN_OP );
    else
      cb_putcode( cb, GETBUILTIN_OP );
    
    cb_putcode( cb, ci );
  
    cmp_builtin_args( args, cb, cntxt, false );
    ci = cb_putconst( cb, e );

    cb_putcode( cb, CALLBUILTIN_OP );
    cb_putcode( cb, ci );

    if ( cntxt->tailcall )
      cb_putcode( cb, RETURN_OP );
    
    return true;
  }

}


bool any_dots( SEXP args ) {

  for ( SEXP s = args; s != R_NilValue; s = CDR( s ) ) {
    SEXP arg = CAR( s );
    if ( arg != R_MissingArg && arg == R_DotsSymbol ) {
      return true;
    }
  }
  return false;

};


InlineInfo get_inline_info( char name[256], CompilerContext * cntxt, bool guard_ok ) {
/*
  InlineInfo ret;
  ret.can_inline = false;

  short optimize = cntxt->optimize_level;
  SEXP name_wrapped = PROTECT(Rf_mkChar( name ));

  if ( optimize > 0 && ! ( is_in_set( name_wrapped, R_NilValue ) ) ) {

    VarInfo info = find_cenv_var( name_wrapped, cntxt->env );
    if ( ! info.found )
      return ret;

    EnvFrame * defining_frame = info.defining_frame;
    FrameType ftype = defining_frame->type;

    //if ( ftype == FRAME_NAMESPACE )
    if ( R_IsNamespaceEnv( defining_frame->r_env ) ) {

      SEXP top_level_env = cntxt->env->top_frame->r_env;
      if ( ! R_IsNamespaceEnv( top_level_env ) ||
           ! IDENTICAL( defining_frame->r_env, R_ParentEnv( top_level_env ) )) {
             //context stop idk

           }

      defining_frame = cntxt->env->top_frame;

    }

    ret.package = ///// LEFT OF HERE

  }

  UNPROTECT(1); //name_wrapped
  return ret;*/

  InlineInfo ret;
  ret.can_inline = true;
  ret.guard_needed = false;
  strncpy(ret.package, "base\0", 5);

  return ret;
};


bool try_inline( SEXP e, CodeBuffer *cb, CompilerContext *cntxt ) {

  char name[256];
  strncpy( name, CHAR(PRINTNAME(CAR(e))), 255 );
  
  InlineInfo info = get_inline_info( name, cntxt, true );

  if ( ! info.can_inline )
    return false;

  HandlerFn handler = NULL;

  if( ! get_inline_handler( name, info.package, &handler ) )
    return false;

  DEBUG_PRINT(">> Inlining function '%s' from package '%s'\n", name, info.package);

  if ( info.guard_needed ) {

    bool tailpos = cntxt->tailcall;
    if ( tailpos ) cntxt->tailcall = false;

    int expridx = cb_putconst( cb, e );
    int endlabel = cb_makelabel( cb );
    cb_putcode( cb, BASEGUARD_OP );
    cb_putcode( cb, expridx );
    cb_putcode( cb, endlabel );
    if (! handler(e, cb, cntxt))
      cmp_call(e, cb, cntxt, false);

    cb_putlabel(cb, endlabel);
    if ( tailpos ) cb_putcode(cb, RETURN_OP);
    return true;
  
  }
  
  return handler( e, cb, cntxt );

};


// TODO check heavy
void cb_putswitch(CodeBuffer *cb, int *label_ids, int count) {

  cb_putcode(cb, SWITCH_OP);

  int patch_pos = cb->code_count; // Position to patch later, placing into SwitchPatch struct
  cb_putcode(cb, 0); // Placeholder

  SwitchPatch * patch = (SwitchPatch *) R_alloc (1, sizeof( SwitchPatch ));
  patch->cb_index = patch_pos;
  patch->count = count;

  patch->label_ids = (int *) R_alloc ( count, sizeof( int ) );
  memcpy( patch->label_ids, label_ids, count * sizeof( int ) );

  if ( cb->switch_patches == NULL ) {
    cb->switch_patches = patch;
    patch->next = NULL;
  } else {
    patch->next = cb->switch_patches;
    cb->switch_patches = patch;
  }

};

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
  cb_putcode(cb, -(label_id + 1));
}

int cb_makelabel(CodeBuffer * cb) {

  cb->label_table.labels_issued++;
  int label_id = cb->label_table.labels_issued;

  return label_id;

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

  // Patch switch statement patches
  SwitchPatch * patch = cb->switch_patches;
  while ( patch ) {

    SEXP offset_table = PROTECT(Rf_allocVector( INTSXP, patch->count ));

    for (int i = 0; i < patch->count; i++) {
        
      int label_index = patch->label_ids[i];
      INTEGER(offset_table)[i] = cb->label_table.table[label_index];
      // NOTE: here, the conversion from negative doesnt happen. It only happens
      // when extracting from code buffer, because there it would clash with actual opcodes.
      // Here its a separate structure.

      // TODO check for unresolved labels

    }
    
    int idx = cb_putconst(cb, offset_table);
    cb->code[patch->cb_index] = idx;
    patch = patch->next;

    UNPROTECT(1); // offset_table

  }

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

  if (find_loc_var(sym, cntxt)) {
    return false;
  }

  SEXP env = cntxt->env->top_frame->r_env;
  SEXP val = Rf_findVar(sym, env);

  if (val != R_UnboundValue) {
    int t = TYPEOF(val);
    return (t == SPECIALSXP || t == BUILTINSXP);
  }

  return false;

};

SEXP find_fun_def( SEXP fun_sym, CompilerContext * cntxt ) {

  CompilerEnv * cenv = cntxt->env;
  VarInfo var_info = find_cenv_var( fun_sym, cenv );

  if ( var_info.found ) {
    SEXP val = var_info.value;
    if ( Rf_isFunction( val ) ) {
      return val;
    }
  }

  return R_NilValue; // Not found

};

SEXP check_call(SEXP def, SEXP call) {

  int type = TYPEOF(def);

  if (type == BUILTINSXP || type == SPECIALSXP) {

    // Construct call: args(def)
    SEXP args_call = PROTECT(lang2(install("args"), def));
    int error = 0;

    // Evaluate args(def)
    def = R_tryEval(args_call, R_NilValue, &error);
    UNPROTECT(1); // args_call
        
    if (error) return ScalarLogical(NA_LOGICAL);

  }

  // Check if call has any '...' arguments
  int has_dots = 0;

  for (SEXP runner = CDR(call); runner != R_NilValue; runner = CDR(runner)) {
    SEXP arg = CAR(runner);
    if (TYPEOF(arg) == SYMSXP && arg == R_DotsSymbol) {
      has_dots = 1;
      break;
    }
  }

  // if (typeof(def) != "closure" || anyDots(call)) NA
  if (TYPEOF(def) != CLOSXP || has_dots) {
    return ScalarLogical(NA_LOGICAL);
  }

  // Construct call: match.call(def, call)
  SEXP match_call_expr = PROTECT( lang3(install("match.call"), def, call) );
  int errorOccurred = 0;
  
  // R_tryEvalSilent suppresses error messages
  R_tryEvalSilent(match_call_expr, R_NilValue, &errorOccurred);
  
  UNPROTECT(1); // match_call_expr

  if (errorOccurred) {

    // Get deparsed call string for the warning message
    SEXP dep_call = PROTECT(lang3(install("deparse"), call, ScalarInteger(20)));
    SEXP dep_res = R_tryEval(dep_call, R_NilValue, NULL);
    const char *call_str = "call";
    if (TYPEOF(dep_res) == STRSXP && LENGTH(dep_res) > 0) {
        call_str = CHAR(STRING_ELT(dep_res, 0));
    }

    // Signal the warning (equivalent to 'signal(emsg)')
    warning("possible error in '%s'", call_str);

    UNPROTECT(1); // dep_call
    return ScalarLogical(0); // FALSE
  
  }

  return ScalarLogical(1);
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

  if ( var_info.found && var_info.defining_frame->type == FRAME_LOCAL ) {
      DEBUG_PRINT("++ find_loc_var: Symbol '%s' found in LOCAL scope\n", CHAR(PRINTNAME(var)));
      return true;
  } else {
      DEBUG_PRINT("?? find_loc_var: Symbol '%s' NOT found in LOCAL scope\n", CHAR(PRINTNAME(var)));
      return false;
  }

}

VarInfo find_cenv_var( SEXP var, CompilerEnv * cenv ) {

  VarInfo info = { NULL, R_NilValue, false };

  const char* var_name = CHAR( PRINTNAME( var ) );

  EnvFrame * current = cenv->top_frame;

  // Walk up the environment frames
  while ( current != NULL ) {

    // Check if its in extra_vars
    if ( (current->extra_vars != R_NilValue) && (TYPEOF(current->extra_vars) == STRSXP) ) {

      int n = Rf_length( current->extra_vars );
      for ( int i = 0; i < n; i++ ) {
        
        const char* extra = CHAR( STRING_ELT( current->extra_vars, i ) );

        if ( strcmp( var_name, extra ) == 0 ) {
          info.defining_frame = current;
          info.found = true;
          return info;
        }

      }
     
    }

    // If not check if its in runtime environment,
    // using Rf_findVarInFrame3
    SEXP val = Rf_findVarInFrame3( current->r_env, var, TRUE );
    if ( val != R_UnboundValue ) {
      info.defining_frame = current;
      info.value = val;
      info.found = true;
      return info;
    }

    current = current->parent;

  }

  return info; // Not found

};

SEXP get_assigned_var( SEXP var ) {
  SEXP v = CADR( var );

  if ( v == R_NilValue ) {
    Rf_error("Bad assignment");
    return R_NilValue; 
  }

  // Handle strings and symbols
  if ( TYPEOF( v ) == SYMSXP ) {
    return Rf_ScalarString( PRINTNAME(v) ); // Return STRSXP
  } 
  else if ( TYPEOF( v ) == CHARSXP ) {
    return Rf_ScalarString( v ); // Return STRSXP
  } 
  else {
    // Handle complex assignments names(x) <- 1
    while ( TYPEOF( v ) == LANGSXP ) {
      if ( Rf_length( v ) < 2 ) Rf_error("Bad assignment");
      v = CADR( v );
      if ( v == R_NilValue ) Rf_error("Bad assignment");
    }

    if ( TYPEOF( v ) != SYMSXP ) Rf_error("Bad assignment");

    return Rf_ScalarString( PRINTNAME(v) ); 
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

static SEXP union_sets(SEXP a, SEXP b) {

  if (a == R_NilValue) return b;
  if (b == R_NilValue) return a;

  SEXP call = PROTECT(Rf_lang3(Rf_install("union"), a, b));
  SEXP res = Rf_eval(call, R_BaseEnv);
  UNPROTECT(1);
    
    return res;
}

SEXP find_locals_list( SEXP elist, SEXP known_locals ) {

  // Initialize empty list of locals
  SEXP found = R_NilValue;

  // Iterate over expressions in the list
  SEXP node = elist;
  while ( node != R_NilValue ) {
    
    // Get the first element
    SEXP expr = CAR( node );

    // Find locals in the expression
    SEXP new_vars = find_locals( expr, known_locals );
    PROTECT( new_vars );

    if ( new_vars != R_NilValue ) {
      if (found == R_NilValue)
          found = new_vars;
      else
        found = union_sets(found, new_vars);
    }

    UNPROTECT(1); // new_vars
    node = CDR( node );
  }
  return found;
};

/*
TODO: refactor find_locals to avoid deep recursion on large expressions
      use a todo list like in the reference
*/

SEXP find_locals( SEXP expr, SEXP known_locals ) {


  // Base case: expression is not a function call
  if ( TYPEOF( expr ) != LANGSXP ) return R_NilValue;

  // here we know expr is a LANGSXP
  SEXP fun = CAR( expr );

  // Lambda or anonymous function call
  if ( TYPEOF( fun ) != SYMSXP )
    return find_locals_list( expr, known_locals ); // weird thingy (yes)

  // Its a function call with a symbol as function name
  const char* fname = CHAR( PRINTNAME( fun ) );

  // Its an assignment?
  if ( strcmp( fname, "<-" ) == 0 || strcmp( fname, "=" ) == 0 ) {
    
    // Assignment LHS (the variable being assigned to)
    SEXP var =  get_assigned_var( expr );

    // Recurse into RHS
    SEXP rhs_locals = find_locals( CADDR( expr ), known_locals );

    return union_sets( rhs_locals, var );

  }

  // A for loop
  if ( strcmp( fname, "for" ) == 0 ) {

    SEXP loop_var = Rf_ScalarString( PRINTNAME( CADR( expr ) ) );
    SEXP seq_locals = find_locals( CADDR( expr ), known_locals );
    SEXP body_locals = find_locals( CADDDR( expr ), known_locals );
    
    return union_sets( union_sets( seq_locals, loop_var ), body_locals );
  
  }

  // Scope barriers / primitives
  if ( strcmp( fname, "function" ) == 0 ||
       strcmp( fname, "quote" ) == 0 ||
       strcmp( fname, "expression" ) == 0 ) {

      if (!is_in_set( fun, known_locals )) {
        return R_NilValue;
      }
    
  }
    
  // General case: recurse into all arguments
  return find_locals_list( CDR( expr ), known_locals );

};

void add_cenv_vars( CompilerEnv * cenv, SEXP vars ) {

  // Check if there is something to add
  if (vars == R_NilValue) return;

  #ifdef DEBUG
  if (TYPEOF(vars) == STRSXP && LENGTH(vars) > 0) {
      DEBUG_PRINT("vv add_cenv_vars: Adding locals to frame: ");
      for(int i=0; i < LENGTH(vars); i++) {
          DEBUG_PRINT("%s ", CHAR(STRING_ELT(vars, i)));
      }
      DEBUG_PRINT("\n");
  }
  #endif

  SEXP current_vars = cenv->top_frame->extra_vars;
  SEXP combined_vars = union_sets( current_vars, vars );
  
  // TODO  ?
  cenv->top_frame->extra_vars = combined_vars;

  SET_VECTOR_ELT( cenv->top_frame->shadow_node, 1, combined_vars );
  
};


void add_cenv_frame( CompilerEnv * cenv, SEXP vars ) {

  EnvFrame * new_frame = (EnvFrame *) R_alloc (1, sizeof( EnvFrame ));
  
  new_frame->r_env = Rf_allocSExp( ENVSXP );
  SEXP shadow_node = Rf_allocVector( VECSXP, 2 );
  SET_VECTOR_ELT( shadow_node, 0, new_frame->r_env );
  SET_VECTOR_ELT( shadow_node, 1, R_NilValue ); // extra_vars placeholder

  new_frame->shadow_node = shadow_node;

  SET_ENCLOS(new_frame->r_env, cenv->top_frame->r_env);
  
  new_frame->type = FRAME_LOCAL; 
  new_frame->parent = cenv->top_frame;
  new_frame->extra_vars = R_NilValue; // Initialize to avoid garbage

  cenv->top_frame = new_frame;

  // Update shadow stack
  SEXP current_stack = VECTOR_ELT( cenv->shadow_stack, 0 );
  SEXP new_stack = CONS( shadow_node, current_stack );

  SET_VECTOR_ELT( cenv->shadow_stack, 0, new_stack );

  add_cenv_vars( cenv, vars );
};

// @manual 5.1
CompilerEnv * make_cenv( SEXP env ) {

  // Allocate the compilation environment entry point
  CompilerEnv *cenv = (CompilerEnv *) R_alloc (1, sizeof( CompilerEnv ));
  
  // Allocate the topmost frame
  cenv->top_frame = (EnvFrame *) R_alloc (1, sizeof( EnvFrame ));

  cenv->top_frame->parent = NULL;
  cenv->top_frame->r_env = env;
  cenv->top_frame->extra_vars = R_NilValue;

  // Initialize shadow stack for GC safety for r_env and extra_vars
  cenv->shadow_stack = Rf_allocVector( VECSXP, 1 );
  SET_VECTOR_ELT( cenv->shadow_stack, 0, R_NilValue ); 

  return cenv;

}

// @manual 5.2
CompilerEnv * make_fun_env( SEXP forms, SEXP body, CompilerContext * cntxt ) {

  CompilerEnv *new_cenv = (CompilerEnv *) R_alloc (1, sizeof( CompilerEnv ));

  new_cenv->shadow_stack = Rf_allocVector( VECSXP, 1 );
  PROTECT( new_cenv->shadow_stack );
  SET_VECTOR_ELT( new_cenv->shadow_stack, 0, R_NilValue ); 

  new_cenv->top_frame = cntxt->env->top_frame;

  add_cenv_frame( new_cenv, getAttrib( forms, R_NamesSymbol ) );

  SEXP locals = find_locals_list( forms, R_NilValue );
  PROTECT(locals);

  SEXP arg_names = getAttrib( forms, R_NamesSymbol );
  
  SEXP tmp = union_sets( locals, arg_names );
  UNPROTECT(1); // locals
  locals = tmp;
  PROTECT(locals);

  while ( true ) {

    SEXP new_found = find_locals( body, locals );
    SEXP combined = union_sets( locals, new_found );

    if ( Rf_length( combined ) == Rf_length( locals ) )
      break;

    UNPROTECT(1); // locals
    locals = combined;
    PROTECT(locals);

  }


  add_cenv_frame( new_cenv, arg_names );
  add_cenv_vars( new_cenv, locals );  

  UNPROTECT(2); // locals, shadow_stack
  return new_cenv;
}

// @manual 4.1
CompilerContext * make_toplevel_ctx( CompilerEnv *cenv ) {
  
  CompilerContext *ctx = (CompilerContext *) R_alloc (1, sizeof(CompilerContext) );

  ctx->toplevel = true;
  ctx->tailcall = true;
  ctx->need_return_jmp = false;
  ctx->optimize_level = 0; // Defaulting to 0 because no inlining yet

  // TODO compiler options later to be passed as argument in this fn
  ctx->supress_all = false;
  ctx->supress_no_super_assign = false;
  ctx->supress_undefined = R_NilValue;
  
  ctx->env = cenv;
  ctx->loop = NULL;
  ctx->call = R_NilValue;

  return ctx;

}

// @manual 4.2
CompilerContext * make_function_ctx( CompilerContext * cntxt, CompilerEnv * fenv, SEXP forms, SEXP body ) {

  CompilerEnv * env = fenv;
  CompilerContext * ncntxt = make_toplevel_ctx( env );

  ncntxt->optimize_level = cntxt->optimize_level;
  ncntxt->supress_all = cntxt->supress_all;
  ncntxt->supress_no_super_assign = cntxt->supress_no_super_assign;
  ncntxt->supress_undefined = cntxt->supress_undefined;
  
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

  LoopInfo * li = (LoopInfo *) R_alloc (1, sizeof( LoopInfo ) );
  li->loop_label_id = loop_label;
  li->end_label_id = end_label;
  li->goto_ok = true;

  ncntxt->loop = li;
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
  
  if ( ncntxt->loop != NULL )
    ncntxt->loop->goto_ok = false;

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

  if ( ncntxt->loop != NULL )
    ncntxt->loop->goto_ok = false;

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

  // Initialize label table
  LabelTable lt;
  lt.capacity = 0;
  lt.labels_issued = 0;
  lt.table = NULL;

  cb->label_table = lt;

  // Initialize source tracking
  cb->expr_buf = (int *) R_alloc ( cb->code_capacity, sizeof( int ) );
  cb->srcref_buf = (int *) R_alloc ( cb->code_capacity, sizeof( int ) );

  cb_putconst(cb, preseed);

  UNPROTECT(1); // constant_pool_handle
  return cb;

};

// @manual 2.4
void cmp_const( SEXP val, CodeBuffer * cb, CompilerContext * cntxt ) {

  DEBUG_PRINT("++ cmp_const: Compiling constant\n");

  if ( IDENTICAL( val, R_NilValue ) )
    cb_putcode( cb, LDNULL_OP );
  else if ( IDENTICAL( val, R_TrueValue ) )
    cb_putcode( cb, LDTRUE_OP );
  else if ( IDENTICAL( val, R_FalseValue ) )
    cb_putcode( cb, LDFALSE_OP );
  else {
    int ci = cb_putconst( cb, val ); 
    cb_putcode( cb, LDCONST_OP );
    cb_putcode( cb, ci );
  }

  if ( cntxt->tailcall )
    cb_putcode( cb, RETURN_OP );

};

// @manual 2.5
void cmp_sym( SEXP sym, CodeBuffer * cb, CompilerContext * cntxt, bool missing_ok ) {

  DEBUG_PRINT("++ cmp_sym: Compiling symbol '%s'\n", CHAR(PRINTNAME(sym)));

  if ( sym == R_DotsSymbol ) {
    // TODO notify_wrong_dots_use()
    cb_putcode( cb, DOTSERR_OP );
    return;
  }

  if ( is_ddsym( sym ) ) {
    if ( ! find_loc_var( sym, cntxt ) ) {
      // TODO notify_wrong_dots_use()
    }
    
    int ci = cb_putconst( cb, sym );

    if ( missing_ok ) {
      cb_putcode( cb, DDVAL_MISSOK_OP );
    } else {
      cb_putcode( cb, DDVAL_OP );
    }

    cb_putcode( cb, ci );
    
    if ( cntxt->tailcall )
      cb_putcode( cb, RETURN_OP );

    return;
  }

  if ( ! find_var( sym, cntxt ) ) {
    DEBUG_PRINT("?? cmp_sym: Undefined symbol '%s'\n", CHAR(PRINTNAME(sym)));
    warning("Undefined symbol: %s", CHAR(PRINTNAME(sym)));
  }
  
  int poolref = cb_putconst( cb, sym );

  if ( missing_ok )
    cb_putcode( cb, GETVAR_MISSOK_OP );
  else
    cb_putcode( cb, GETVAR_OP );

  cb_putcode( cb, poolref );

  if ( cntxt->tailcall )
    cb_putcode( cb, RETURN_OP );

};

// @manual 2.6
// TODO add inlineOK argument
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
      // Notify about something?
      } else {

        SEXP def = find_fun_def( fun, cntxt );
        if ( Rf_isNull(def) ) {
          DEBUG_PRINT("?? cmp_call: Undefined function symbol '%s'\n", CHAR(PRINTNAME(fun)));
          // warning("Undefined function: %s", CHAR(PRINTNAME(fun)));
        } else {
          DEBUG_PRINT("++ cmp_call: Found function definition for symbol '%s'\n", CHAR(PRINTNAME(fun)));
          check_call( def, call ); // todo handle exceptions
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
        return cmp( fun, cb, cntxt, false, true );
      }

      cmp_call_expr_fun( fun, args, call, cb, cntxt );
    
    }
  
  }

  cb_restorecurloc( cb, saved );

};

// @manual 2.6
void cmp_call_sym_fun( SEXP fun, SEXP args, SEXP call, CodeBuffer * cb, CompilerContext * cntxt ) {

  DEBUG_PRINT("++ cmp_call_sym_fun: Compiling function call with symbol function '%s'\n", CHAR(PRINTNAME(fun)));

  const char* maybe_NSE_symbols[] = {"bquote", NULL}; // Null works as terminator
  
  int ci = cb_putconst( cb, fun );
  cb_putcode( cb, GETFUN_OP );
  cb_putcode( cb, ci );
  
  bool nse = false;
  const char *fun_name = CHAR(PRINTNAME(fun));

  for (const char** symbol = maybe_NSE_symbols; *symbol != NULL; ++symbol) {
    if ( strcmp( fun_name, *symbol ) == 0 ) nse = true;
  }

  cmp_call_args( args, cb, cntxt, nse );
  ci = cb_putconst( cb, call );
  cb_putcode( cb, CALL_OP );
  cb_putcode( cb, ci );

  if ( cntxt->tailcall )
    cb_putcode( cb, RETURN_OP );

};

void cmp_call_expr_fun( SEXP fun, SEXP args, SEXP call, CodeBuffer * cb, CompilerContext * cntxt ) {

  DEBUG_PRINT("++ cmp_call_expr_fun: Compiling function call with expression function\n");

  CompilerContext * ncntxt = make_non_tail_call_ctx( cntxt );
  cmp( fun, cb, ncntxt, false, true );
  cb_putcode( cb, CHECKFUN_OP );
  bool nse = false;

  cmp_call_args( args, cb, cntxt, nse );
  int ci = cb_putconst( cb, call );
  cb_putcode( cb, CALL_OP );
  cb_putcode( cb, ci );

  if ( cntxt->tailcall ) {
    cb_putcode( cb, RETURN_OP );
  }

};

void cmp_call_args( SEXP args, CodeBuffer * cb, CompilerContext * cntxt, bool nse ) {

  DEBUG_PRINT("++ cmp_call_args: Compiling function call arguments\n");

  SEXP names = getAttrib( args, R_NamesSymbol );
  CompilerContext * pnctxt = make_promise_ctx( cntxt );

  while ( args != R_NilValue ) {

    SEXP a = CAR( args );
    SEXP n = TAG( args );

    // handle missing argument
    if ( a == R_MissingArg ) {
      cb_putcode( cb, DOMISSING_OP );
      cmp_tag(n, cb);
      return;
    }

    // handle ... argument TODO check whether correct
    if ( (TYPEOF( a ) == SYMSXP) && (strcmp( CHAR(PRINTNAME(a)), "..." ) == 0) ) {

      if ( ! find_loc_var(R_DotsSymbol, cntxt) ) {
        warning("'...' used in an incorrect context");
      }

      cb_putcode(cb, DODOTS_OP);
      return;
    }

    if ( TYPEOF(a) == BCODESXP ) {
      //TODO swap this for ctx_stop
      error( "cannot compile byte code literals in code" );
    }

    if ( TYPEOF(a) == PROMSXP ) {
      error( "cannot compiler promise literals in code" );
    }


    // compile general argument
    if ( TYPEOF( a ) == SYMSXP || TYPEOF( a ) == LANGSXP ) {
      int ci;
      if ( nse )
        ci = cb_putconst( cb, a );
      else
        ci = cb_putconst( cb, gen_code( a, pnctxt, R_NilValue, cb_savecurloc( cb ) ) );
      
      cb_putcode( cb, MAKEPROM_OP );
      cb_putcode( cb, ci );
    
    } else {
      cmp_const_arg( a, cb, pnctxt );
    }

    cmp_tag( n, cb );
  
    //##
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

  int ci = cb_putconst( cb, tag );
  cb_putcode( cb, SETTAG_OP );

};

void cmp_const_arg( SEXP a, CodeBuffer * cb, CompilerContext * cntxt ) {

  DEBUG_PRINT("++ cmp_const_arg: Compiling constant argument\n");

  if ( isNull(a) )
    cb_putcode( cb, PUSHNULLARG_OP );
  else if ( IDENTICAL(a, R_TrueValue) )
    cb_putcode( cb, PUSHTRUEARG_OP );
  else if ( IDENTICAL(a, R_FalseValue) )
    cb_putcode( cb, PUSHFALSEARG_OP );
  else {
    int ci = cb_putconst( cb, a );
    cb_putcode( cb, PUSHCONSTARG_OP );
    cb_putcode( cb, ci );
  }
  
};

void cb_putcode( CodeBuffer * cb, int opcode ) {
  DEBUG_PRINT("++ putcode: Emitting Opcode %d\n", opcode);

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
  
  // handle source tracking
  int expression_idx = cb_putconst( cb, cb->current_expr );
  cb->expr_buf[ cb->code_count ] = expression_idx;

  if (  cb->current_srcref != R_NilValue ) {
    int srcref_idx = cb_putconst( cb, cb->current_srcref );
    cb->srcref_buf[ cb->code_count ] = srcref_idx;
  } else {
    cb->srcref_buf[ cb->code_count ] = -1; // No srcref
  }

  cb->code_count += 1;

};

int cb_getcode( CodeBuffer * cb, int pos ) {

  if ( pos < 0 || pos >= cb->code_count ) {
    error("CodeBuffer: Invalid code position");
  }

  return cb->code[ pos ];

};


int cb_putconst( CodeBuffer * cb, SEXP item ) {

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
    
    /* 16 - take closure environments into account  */
    if (item == compare || R_compute_identical(item, compare, 16)) {
      DEBUG_PRINT("++ putconst: Found existing constant in pool at index %d\n", j);
      
      // Found so return the existing index immediately,
      // Do not increment const_count
      return j;
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
        if ( is_base_var(fun, cntxt) )
          return true;
      }

      if ( strcmp(fname, "function") == 0 ) {
        if ( is_base_var(fun, cntxt) )
          return false;
      }

      else {
        may_call_browser_list( CDR( expr ), cntxt );
      }

    } else {
      if (may_call_browser_list(expr, cntxt)) return true;
    }
  } else {
    return false;
  }

}

bool may_call_browser_list(SEXP exprlist, CompilerContext * cntxt) {

  SEXP node = exprlist;
  while ( node != R_NilValue ) {

    SEXP expr = CAR( node );

    if ( (expr == R_MissingArg) && may_call_browser( expr, cntxt ) )
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
  
  if ( Rf_isNull( ce ) ) {

    DEBUG_PRINT(".. cmp: Expression not folded\n");
    // Not foldable, generate code normally

    SEXPTYPE type = TYPEOF( e );
    switch ( type )
    {
    
    case LANGSXP:
      DEBUG_PRINT(".. cmp: Compiling Call (LANGSXP)\n");
      cmp_call( e, cb, cntxt, true );
      break;
    
    case SYMSXP:
      DEBUG_PRINT(".. cmp: Compiling Symbol (SYMSXP): %s\n", CHAR(PRINTNAME(e)));
      cmp_sym( e, cb, cntxt, missing_ok );
      break;
    
    case BCODESXP:
      DEBUG_PRINT("!! cmp: Error - Literal Bytecode found\n");
      Rf_error("Cannot compile bytecode");
      // TODO add context and location info
      break;
    
    case PROMSXP:
      DEBUG_PRINT("!! cmp: Error - Literal Promise found\n");
      Rf_error("Cannot compile promise");
      break;

    default:
      DEBUG_PRINT(".. cmp: Compiling Constant (Type: %d)\n", type);
      cmp_const( e, cb, cntxt );
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
SEXP cmpfun(SEXP f, void* __placeholder__) {

  SEXPTYPE type = TYPEOF( f );

  switch (type)
  {
  case CLOSXP:
    DEBUG_PRINT(">> cmpfun: Compiling CLOSXP (Function)\n");
    
    CompilerEnv * cenv = make_cenv( CLOENV(f) );

    /* TODO / TOASK, RE: protecting variables returned from functions
      is it okay to unprotect then return SEXP then protect outside,
      or should the function end with an unbalanced stack, caller unprotects it
      and reprotects the returned SEXP?
    */
    PROTECT( cenv->shadow_stack );

    CompilerContext * cntxt = make_toplevel_ctx( cenv );

    CompilerEnv * fenv = make_fun_env( FORMALS(f), BODY(f), cntxt );
    PROTECT( fenv->shadow_stack );

    CompilerContext * ncntxt = make_function_ctx(cntxt, fenv, FORMALS(f), BODY(f));
    
    if ( may_call_browser( BODY(f), ncntxt ) ) {
      DEBUG_PRINT("!! cmpfun: Function may call browser, skipping compilation\n");
      return f;
    }
    
    bool is_block = false;
    if ( TYPEOF( BODY(f) ) == LANGSXP ) {
      SEXP sym = CAR( BODY(f) );
      if ( TYPEOF(sym) == SYMSXP ) {
          const char *name = CHAR( PRINTNAME(sym) );
        if ( strcmp( name, "{" ) == 0 ) {
          is_block = true;
        }
      }
    }

    Loc loc;
    loc.is_null = false;

    if ( !is_block ) {
      loc.expr = BODY(f);
      loc.srcref = get_expr_srcref( f );
    }
    else
      loc.is_null = true;
    
    SEXP b = PROTECT( gen_code( BODY(f), ncntxt, R_NilValue, loc ) );

    DEBUG_PRINT("<< cmpfun: Compilation finished, creating closure\n");

    SEXP val = R_mkClosure(FORMALS(f), b, CLOENV(f));

    SEXP attrs = ATTRIB(f);
    if (!Rf_isNull(attrs)) {
        SET_ATTRIB(val, attrs);
    }

    if (isS4(f))
        val = Rf_asS4(val, FALSE, 0);
    
    UNPROTECT(3); // b, cenv->shadow_stack, fenv->shadow_stack
    DEBUG_PRINT("<< cmpfun done, returning\n");

    return val;

  case BUILTINSXP:
  case SPECIALSXP:
     DEBUG_PRINT(">> cmpfun: Primitive function (BUILTIN/SPECIAL), skipping\n");
     return f;

  default:
    DEBUG_PRINT("!! cmpfun: Error - Argument is not a function type: %d\n", type);
    Rf_error("Argument must be a closure, builtin, or special function");

  }

};

// @manual 2.1
SEXP gen_code( SEXP e, CompilerContext * cntxt, SEXP gen, Loc loc ) {

  CodeBuffer * cb = make_code_buffer(e, loc);
  PROTECT( cb->constant_pool_handle ); // Protect constant pool handle

  if ( Rf_isNull( gen ) )
    cmp( e, cb, cntxt, false, false );
  //else
  //  gen(cb,cntxt) <- This will have to be a function pointer passed in gen

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
    cb_putconst( cb, srcref_vec ); // Ensure srcref_buf is in constant pool

    UNPROTECT(1); // srcref_vec

  }

  if ( cb->expr_tracking_on ) {

    SEXP expr_vec = PROTECT( Rf_allocVector( INTSXP, cb->code_count + 1) );
    int * expr_ptr = INTEGER( expr_vec );

    expr_ptr[0] = NA_INTEGER; 

    for (int i = 0; i < cb->code_count; i++)
      expr_ptr[i+1] = cb->expr_buf[i];

    setAttrib(expr_vec, R_ClassSymbol, Rf_mkString("expressionsIndex"));
    cb_putconst( cb, expr_vec ); // Ensure expr_buf is in constant pool

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
  int res = 0;
  if (TYPEOF(fun) == CLOSXP) {
    res = (TYPEOF(BODY(fun)) == BCODESXP);
  }
  return ScalarLogical(res);
}

// Registration of C functions
static const R_CallMethodDef CallEntries[] = {
    {"cmpfun", (DL_FUNC) &cmpfun, 2},
    {"is_compiled", (DL_FUNC) &is_compiled, 1},
    {NULL, NULL, 0}
};

void R_init_crbcc(DllInfo* dll) {
  R_registerRoutines(dll, NULL, CallEntries, NULL, NULL);
  R_useDynamicSymbols(dll, FALSE);

  BCVersion = Rf_asInteger(R_bcVersion());
}

#pragma endregion

bool get_inline_handler( char name[256], char package[256], HandlerFn * found ) {

  // Macro to reduce boilerplate for handler assignment
#define INLINE_HANDLER_CASE(NAMESTR, FN) \
    if (strcmp(name, NAMESTR) == 0) { \
      *found = &FN; \
      return true; \
    }

  if (strcmp(package, "base") == 0) {
    INLINE_HANDLER_CASE("{", inline_left_brace)
    INLINE_HANDLER_CASE("function", inline_function)
    INLINE_HANDLER_CASE("return", inline_return)
    INLINE_HANDLER_CASE("<-", fake_inline_assign)
    INLINE_HANDLER_CASE("if", inline_if)
    INLINE_HANDLER_CASE("(", inline_left_parenthesis)
    INLINE_HANDLER_CASE("&&", inline_and)
    INLINE_HANDLER_CASE("||", inline_or)
    INLINE_HANDLER_CASE("repeat", inline_repeat)
    INLINE_HANDLER_CASE("next", inline_next)
    INLINE_HANDLER_CASE("break", inline_break)
  }

  return false;

#undef INLINE_HANDLER_CASE
}

#pragma region Inline Handlers

// Inline brusle
bool inline_left_brace( SEXP e, CodeBuffer *cb, CompilerContext *cntxt ) {

  DEBUG_PRINT("[_] Inlining left brace function");

  int n = Rf_length( e );
  if ( n == 1 ) {
    cmp( R_NilValue, cb, cntxt, false, true );
  } else {
    Loc sloc = cb_savecurloc(cb);
    SEXP bsrefs = Rf_getAttrib( e , Rf_install("srcref") );

    SEXP runner = CDR( e );

    if ( n > 2 ) {

      CompilerContext * ncntxt = make_no_value_ctx(cntxt);

      for (int i = 2; i < n; i++) {

        SEXP subexp = CAR( runner );

        cb_setcurloc(cb, subexp, get_block_srcref(bsrefs, i));
        cmp( subexp, cb, ncntxt, false, false );
        cb_putcode( cb, POP_OP );

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

  DEBUG_PRINT("[_] Inlining function definition");

  SEXP formals = CADR( e );
  SEXP body = CDDR( e );

  SEXP sref = R_NilValue;

  if ( length(e) > 3)
    sref = CADDDR( e );

  CompilerContext * ncntxt = make_function_ctx( cntxt, cntxt->env, formals, body );

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

  int ci = cb_putconst( cb, const_list );

  cb_putcode( cb, MAKECLOSURE_OP );
  cb_putcode( cb, ci );

  if ( cntxt->tailcall )
    cb_putcode( cb, RETURN_OP );

  UNPROTECT(2); // body, const_list
  return true;

};

bool inline_left_parenthesis( SEXP e, CodeBuffer *cb, CompilerContext *cntxt ) {

  DEBUG_PRINT("[_] Inlining left parenthesis function");

  // TODO when these functions are implemented
  if ( any_dots(e) )
    return cmp_builtin( e, cb, cntxt, false );

  if ( length(e) != 2 ) {

    Loc loc = cb_savecurloc(cb);
    //TODO notify_wrong_arg_count( "(", cntxt, loc );
    return cmp_builtin( e, cb, cntxt, false );
  }

  if ( cntxt->tailcall ) {

    CompilerContext * ncntxt = make_non_tail_call_ctx( cntxt );
    cmp( CADR( e ), cb, ncntxt, false, true );
    cb_putcode( cb, VISIBLE_OP );
    cb_putcode( cb, RETURN_OP );
    
    return true;
  }

  cmp( CADR( e ), cb, cntxt, false, true );
  return true;

};

bool inline_return( SEXP e, CodeBuffer *cb, CompilerContext *cntxt ) {

  DEBUG_PRINT("[_] Inlining return");

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

  if ( cntxt->need_return_jmp ) {
    cb_putcode( cb, RETURNJMP_OP );
  } else {
    cb_putcode( cb, RETURN_OP );
  }

  return true;

};

bool fake_inline_assign( SEXP e, CodeBuffer *cb, CompilerContext *cntxt ) {

  DEBUG_PRINT("[_] Inlining assignment (beta)");

  SEXP lhs = CADR( e );
  SEXP rhs = CADDR( e );

  if ( TYPEOF( lhs ) != SYMSXP ) {
    // TODO notify_invalid_lhs_assign( cntxt, cb_savecurloc( cb ) );
    return false;
  }

  CompilerContext * ncntxt = make_non_tail_call_ctx( cntxt );
  cmp( rhs, cb, ncntxt, false, true );

  int sym_idx = cb_putconst( cb, lhs );
  cb_putcode( cb, SETVAR_OP );
  cb_putcode( cb, sym_idx );

  if ( cntxt->tailcall ) {
    cb_putcode( cb, INVISIBLE_OP );
    cb_putcode( cb, RETURN_OP );
  }


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

      if ( asLogical( value ) ) {
        cmp( then, cb, cntxt, false, true );
      }
      else if ( has_else ) {
        cmp( eelse, cb, cntxt, false, true );
      } else if (cntxt->tailcall) {

        cb_putcode(cb, LDNULL_OP);
        cb_putcode(cb, INVISIBLE_OP);
        cb_putcode(cb, RETURN_OP);
      
      } else {
        cb_putcode(cb, LDNULL_OP);
      }

      return true;

    }

  }

  CompilerContext * ncntxt = make_non_tail_call_ctx( cntxt );
  cmp( test, cb, ncntxt, false, true );

  int callidx = cb_putconst( cb, e );
  int else_label = cb_makelabel( cb );

  cb_putcode(cb, BRIFNOT_OP);
  cb_putcode(cb, callidx);
  cb_putcodelabel(cb, else_label);

  cmp( then, cb, cntxt, false, true );

  if ( cntxt->tailcall ) {

    cb_putlabel( cb, else_label);

    if ( has_else )
      cmp( eelse, cb, cntxt, false, true);
    else {
      cb_putcode(cb, LDNULL_OP);
      cb_putcode(cb, INVISIBLE_OP);
      cb_putcode(cb, RETURN_OP);
    }

  } else {

    int end_label = cb_makelabel(cb);
    cb_putcode( cb, GOTO_OP );
    cb_putcodelabel( cb, end_label );

    cb_putlabel(cb, else_label);

    if ( has_else )
      cmp( eelse, cb, cntxt, false, true);
    else
      cb_putcode( cb, LDNULL_OP );

    cb_putlabel( cb, end_label );

  }

  return true;

};

bool inline_and( SEXP e, CodeBuffer *cb, CompilerContext *cntxt ) {

  CompilerContext * ncntxt = make_arg_ctx( cntxt );
  int callidx = cb_putconst(cb, e);
  int label = cb_makelabel(cb);

  cmp(CADR(e), cb, ncntxt, false, true);

  cb_putcode(cb, AND1ST_OP);
  cb_putcode(cb, callidx);
  cb_putcodelabel(cb, label);

  cmp(CADDR(e), cb, ncntxt, false, true);

  cb_putcode(cb, AND2ND_OP);
  cb_putcode(cb, callidx);
  cb_putlabel(cb, label);

  if (cntxt->tailcall)
      cb_putcode(cb, RETURN_OP);

  return true;

}

bool inline_or( SEXP e, CodeBuffer *cb, CompilerContext *cntxt ) {

  CompilerContext * ncntxt = make_arg_ctx( cntxt );
  int callidx = cb_putconst(cb, e);
  int label = cb_makelabel(cb);

  cmp(CADR(e), cb, ncntxt, false, true);

  cb_putcode(cb, OR1ST_OP);
  cb_putcode(cb, callidx);
  cb_putcodelabel(cb, label);

  cmp(CADDR(e), cb, ncntxt, false, true);

  cb_putcode(cb, OR2ND_OP);
  cb_putcode(cb, callidx);
  cb_putlabel(cb, label);

  if (cntxt->tailcall)
      cb_putcode(cb, RETURN_OP);

  return true;

};

// forward declaration - circular
bool check_skip_loop_cntxt(SEXP e, CompilerContext *cntxt, bool breakOK);

bool is_loop_stop_fun(const char *fname, CompilerContext *cntxt) {
  const char *stop_set[] = {"function", "for", "while", "repeat", NULL};
  SEXP sym = PROTECT( Rf_install(fname) );
  bool result = is_in_c_set(fname, stop_set) && is_base_var(sym, cntxt);
  UNPROTECT(1);
  return result;
}

bool is_loop_top_fun(const char *fname, CompilerContext *cntxt) {
  const char *top_set[] = {"(", "{", "if", NULL};    
  SEXP sym = PROTECT( Rf_install(fname) );
  bool result = is_in_c_set(fname, top_set) && is_base_var(sym, cntxt);
  UNPROTECT(1);
  return result;
}

bool check_skip_loop_cntxt_list(SEXP elist, CompilerContext *cntxt, bool breakOK) {
  for (SEXP s = elist; s != R_NilValue; s = CDR(s)) {
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

      // Check if break/next is allowed in current branch
      if (!breakOK && (strcmp(fname, "break") == 0 || strcmp(fname, "next") == 0)) {
        return false;
      }
      
      // Functions that definitely stop loop analysis (e.g., nested loops)
      if (is_loop_stop_fun(fname, cntxt)) {
        return true;
      }
      
      // Functions that preserve the loop "top" (e.g., '{', 'if')
      if (is_loop_top_fun(fname, cntxt)) {
        return check_skip_loop_cntxt_list(CDR(e), cntxt, breakOK);
      }
      
      // Evaluators that might execute arbitrary code
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
  cb_putcode(cb, POP_OP);
  cb_putcode(cb, GOTO_OP);
  cb_putcodelabel(cb, loop_label);
  cb_putlabel(cb, end_label);
}

bool inline_repeat(SEXP e, CodeBuffer *cb, CompilerContext *cntxt) {
  
  SEXP body = CADR(e);

  if (check_skip_loop_cntxt(body, cntxt, true)) {
    
    // Optimization: context is simple enough to skip heavy setup
    cmp_repeat_body(body, cb, cntxt);

  } else {

    // The original compiler says this is bad
    // and I agree TODO check if this is real
    CompilerContext ncntxt = *cntxt;
    ncntxt.need_return_jmp = true;
    
    int ljmpend_label = cb_makelabel(cb);

    cb_putcode(cb, STARTLOOPCNTXT_OP);
    cb_putcode(cb, 0);
    cb_putcodelabel(cb, ljmpend_label);

    cmp_repeat_body(body, cb, &ncntxt);

    cb_putlabel(cb, ljmpend_label);
    cb_putcode(cb, ENDLOOPCNTXT_OP);
    cb_putcode(cb, 0);
  }

  cb_putcode(cb, LDNULL_OP);

  if (cntxt->tailcall) {
    cb_putcode(cb, INVISIBLE_OP);
    cb_putcode(cb, RETURN_OP);
  }

  return true;
}

bool inline_break(SEXP e, CodeBuffer *cb, CompilerContext *cntxt) {

  if (cntxt->loop == NULL) {
      Loc loc = cb_savecurloc(cb);
      // TODO notifyWrongBreakNext("break", cntxt, loc);
      cmp_special(e, cb, cntxt);
      return true;
    }
    else if (cntxt->loop->goto_ok) {
        cb_putcode(cb, GOTO_OP);
        cb_putcodelabel(cb, cntxt->loop->end_label_id);
        return true;
    } 
    
    cmp_special(e, cb, cntxt);
    return true;
}

bool inline_next(SEXP e, CodeBuffer *cb, CompilerContext *cntxt) {

  if (cntxt->loop == NULL) {
    Loc loc = cb_savecurloc(cb);
    // TODO notifyWrongBreakNext("next", cntxt, loc);
    cmp_special(e, cb, cntxt);
    return true;
  } 

  else if (cntxt->loop->goto_ok) {
    cb_putcode(cb, GOTO_OP);
    cb_putcodelabel(cb, cntxt->loop->loop_label_id);
    return true;
  }
  
  cmp_special(e, cb, cntxt);
  return true;

}


#pragma endregion