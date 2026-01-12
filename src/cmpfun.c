#include <R.h>
#include <Rdefines.h>
#include <R_ext/Rdynload.h>
#include <Rinternals.h>
#include <ctype.h>

extern SEXP R_TrueValue;
extern SEXP R_FalseValue;

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

// #define DEBUG 

#ifdef DEBUG
#define DEBUG_PRINT(...) Rprintf(__VA_ARGS__)
#else
#define DEBUG_PRINT(...) do {} while (0)
#endif


#define IDENTICAL(x,y) R_compute_identical(x, y, 0)

static int BCVersion;

extern SEXP R_mkClosure(SEXP formals, SEXP body, SEXP env);


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

  // TODO Label management (maps label names (strings) to code offsets (ints))
  // This needs to be a hashmap
  void *label_table;
  int label_generator_id;         // For generating unique labels

  // Source tracking
  int * expr_buf;                  // Buffer for source expressions
  int * srcref_buf;                // Buffer for source references

  SEXP current_expr;   
  SEXP current_srcref;

  bool srcref_tracking_on;         // Is source reference tracking on
  bool expr_tracking_on;           // Is expression tracking on

} CodeBuffer;

typedef struct VarInfo {

  EnvFrame * defining_frame;      // Frame where the variable is defined
  SEXP value;                     // var value
  bool found;                     // Was the variable found

} VarInfo;

typedef struct SavedLoc {
    SEXP expr;
    SEXP srcref;
} SavedLoc;

// TODO here maybe should add InlineHandler structs for managing inlining of functions
// TODO label table
// TODO compile() function

// Forward declarations for all defined functions
// because the compiler keeps yelling at me

SEXP get_expr_srcref(SEXP expr);
SEXP extract_srcref(SEXP sref, int idx);
SEXP get_block_srcref(SEXP block_sref, int idx);

SavedLoc cb_savecurloc(CodeBuffer *cb);
void cb_restorecurloc(CodeBuffer *cb, SavedLoc saved);
void cb_setcurloc(CodeBuffer *cb, SEXP expr, SEXP sref);
void cb_setcurexpr(CodeBuffer *cb, SEXP expr);

bool find_var(SEXP var, CompilerContext *cntxt);
SEXP find_locals(SEXP expr, SEXP known_locals);
SEXP find_locals_list(SEXP elist, SEXP known_locals);
SEXP gen_code(SEXP e, CompilerContext *cntxt, SEXP gen, SEXP loc);
SEXP get_assigned_var(SEXP var);
void add_cenv_vars(CompilerEnv *cenv, SEXP vars);
void add_cenv_frame(CompilerEnv *cenv, SEXP vars);
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
CodeBuffer *make_code_buffer(SEXP preseed, SEXP loc);
void cmp_const(SEXP val, CodeBuffer *cb, CompilerContext *cntxt);
void cmp_sym(SEXP sym, CodeBuffer *cb, CompilerContext *cntxt, bool missing_ok);
void cmp_call(SEXP call, CodeBuffer *cb, CompilerContext *cntxt);
void cmp_call_sym_fun(SEXP fun, SEXP args, SEXP call, CodeBuffer *cb, CompilerContext *cntxt);
void cmp_call_expr_fun(SEXP fun, SEXP args, SEXP call, CodeBuffer *cb, CompilerContext *cntxt);
void cmp_call_args(SEXP args, CodeBuffer *cb, CompilerContext *cntxt, bool nse);
void cmp_tag(SEXP tag, CodeBuffer *cb);
void cmp_const_arg(SEXP a, CodeBuffer *cb, CompilerContext *cntxt);
void cb_putcode(CodeBuffer *cb, int opcode);
int cb_getcode(CodeBuffer *cb, int pos);
int cb_putconst(CodeBuffer *cb, SEXP item);
SEXP cb_getconst(CodeBuffer *cb, int idx);
bool may_call_browser(SEXP expr, CompilerContext *cntxt);
bool may_call_browser_list(SEXP exprlist, CompilerContext * cntxt);
void cmp(SEXP e, CodeBuffer *cb, CompilerContext *cntxt, bool missing_ok, bool setloc);
SEXP cmpfun(SEXP f, void* __placeholder__);
SEXP is_compiled(SEXP fun);
void R_init_crbcc(DllInfo* dll);
VarInfo find_cenv_var( SEXP var, CompilerEnv * cenv );
bool find_loc_var( SEXP var, CompilerContext * cntxt );
SEXP code_buf_code( CodeBuffer * cb, CompilerContext * cntxt );
bool is_ddsym(SEXP sym);
SEXP find_fun_def( SEXP fun_sym, CompilerContext * cntxt );
SEXP check_call( SEXP def, SEXP call );

static bool is_base_var(SEXP sym, CompilerContext *cntxt);

static SEXP R_bcVersion();
static bool is_in_set(SEXP sym, SEXP set);
static SEXP union_sets(SEXP a, SEXP b);


SEXP get_expr_srcref(SEXP expr) {
  return Rf_getAttrib(expr, Rf_install("srcref"));
}

SEXP extract_srcref(SEXP sref, int idx) {

  if ( TYPEOF(sref) == VECSXP && Rf_length(sref) >= idx )
    return VECTOR_ELT(sref, idx - 1);

  if ( TYPEOF(sref) == INTSXP && Rf_length(sref) >= 6 )
    return sref;

  return R_NilValue;

}

// TODO why
SEXP get_block_srcref(SEXP block_sref, int idx) {
  return extract_srcref(block_sref, idx);
}

SavedLoc cb_savecurloc(CodeBuffer *cb) {
  SavedLoc saved;
  saved.expr = cb->current_expr;
  saved.srcref = cb->current_srcref;
  return saved;
}

void cb_restorecurloc(CodeBuffer *cb, SavedLoc saved) {
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
      if ( LENGTH( v ) < 2 ) Rf_error("Bad assignment");
      v = CADDR( v );
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

  UNPROTECT(1); // locals
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
    return ncntxt;// 1. The State (What file/line are we in RIGHT NOW?)
  SEXP current_expr;   
  SEXP current_srcref;
  
};

// @manual 4.2
CompilerContext * make_loop_ctx( CompilerContext * cntxt, int loop_label, int end_label ) {

  CompilerContext * ncntxt = make_no_value_ctx( cntxt );

  LoopInfo * li = (LoopInfo *) R_alloc (1, sizeof( LoopInfo ) );
  li->loop_label_id = loop_label;
  li->end_label_id = end_label;
  li->goto_ok = true;

  cntxt->loop = li;
  return cntxt;

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
CodeBuffer * make_code_buffer( SEXP preseed, SEXP loc ) {

  CodeBuffer * cb = (CodeBuffer *) R_alloc (1, sizeof( CodeBuffer ) );

  cb->expr_tracking_on = true;
  cb->srcref_tracking_on = true;

  if ( loc == R_NilValue ) {
    cb->current_expr = preseed;
    cb->current_srcref = get_expr_srcref( preseed );
  } else {
    cb->current_expr = preseed;
    cb->current_srcref = R_NilValue;
  }

  if ( cb->current_srcref == R_NilValue ) {
    cb->srcref_tracking_on = false;
  }

  cb->constant_pool_handle = Rf_allocVector(VECSXP, 1);
  PROTECT( cb->constant_pool_handle );

  // Initialize code buffer itself
  cb->code_capacity = 128; // Initial capacity
  cb->code_count = 0;
  cb->code = (int *) R_alloc ( cb->code_capacity, sizeof( int ) );
  
  // Initialize constant pool
  cb->constant_pool = R_NilValue; // Empty list
  cb->const_count = 0;

  // TODO initialize label table
  cb->label_table = NULL;
  cb->const_capacity = 0; 
  cb->label_generator_id = 0;

  // Initialize source tracking
  cb->expr_buf = (int *) R_alloc ( cb->code_capacity, sizeof( int ) );
  cb->srcref_buf = (int *) R_alloc ( cb->code_capacity, sizeof( int ) );

  cb_putconst(cb, preseed);

  UNPROTECT(1); // constant_pool_handle
  return cb;

};

void dloc(SEXP e, CodeBuffer * cb) {

  SEXP srcref = Rf_getAttrib(e, Rf_install("srcref"));
  cb->current_expr = e;

  // TODO
  cb->current_srcref = srcref;

}

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
    cb_putcode( cb, LDCONST_OP );
    int ci = cb_putconst( cb, val ); 
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
  // TODO if missing ok
  cb_putcode( cb, GETVAR_OP );
  cb_putcode( cb, poolref );

  if ( cntxt->tailcall )
    cb_putcode( cb, RETURN_OP );

};

// @manual 2.6
// TODO add inlineOK argument
void cmp_call( SEXP call, CodeBuffer * cb, CompilerContext * cntxt ) {

  // TODO handle locs
  DEBUG_PRINT("++ cmp_call: Compiling function call\n");

  cntxt = make_call_ctx( cntxt, call );
  SEXP fun = CAR( call );
  SEXP args = CDR( call );

  if ( TYPEOF( fun ) == SYMSXP ) {
    
    if ( false ) {

      // TODO inline function here

    } else {

      // If not inlinable:
      DEBUG_PRINT("++ cmp_call: Calling symbol function '%s'\n", CHAR(PRINTNAME(fun)));

      if ( find_loc_var(fun,cntxt) ) {
      // Notify about something?
      } else {

        SEXP def = find_fun_def( fun, cntxt );
        if ( Rf_isNull(def) ) {
          DEBUG_PRINT("?? cmp_call: Undefined function symbol '%s'\n", CHAR(PRINTNAME(fun)));
          warning("Undefined function: %s", CHAR(PRINTNAME(fun)));
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

    // TODO restore curloc
  
  }

  // if ( cntxt->tailcall )
  //   cb_putcode( cb, RETURN_OP );

  // TODO restore context

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
        ci = cb_putconst( cb, gen_code( a, pnctxt, R_NilValue, R_NilValue ) );
      
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
    cb->srcref_buf[ cb->code_count ] = 0; // No srcref
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

  SavedLoc sloc;

  if ( setloc ) {
    sloc = cb_savecurloc(cb);
    cb_setcurexpr(cb, e);
  }

  // TODO constant fold here (ce means constant expression i guess)
  SEXP ce = R_NilValue;
  
  if ( Rf_isNull( ce ) ) {

    DEBUG_PRINT(".. cmp: Expression not folded\n");
    // Not foldable, generate code normally

    SEXPTYPE type = TYPEOF( e );
    switch ( type )
    {
    
    case LANGSXP:
      DEBUG_PRINT(".. cmp: Compiling Call (LANGSXP)\n");
      cmp_call( e, cb, cntxt );
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

  } else
    cmp_const( ce, cb, cntxt );

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

    SEXP loc; 
    
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

    if ( !is_block )
      loc = PROTECT( Rf_list2( R_NilValue, R_NilValue ) );
    else
      loc = R_NilValue;
    
    SEXP b = PROTECT( gen_code( BODY(f), ncntxt, R_NilValue, loc ) );

    if ( !is_block )
      UNPROTECT(1); // loc

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
SEXP gen_code( SEXP e, CompilerContext * cntxt, SEXP gen, SEXP loc ) {

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
