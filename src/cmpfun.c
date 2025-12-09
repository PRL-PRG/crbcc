#include <R.h>
#include <Rdefines.h>
#include <R_ext/Rdynload.h>
#include <Rinternals.h>

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

#define DEBUG 

#ifdef DEBUG
#define DEBUG_PRINT(...) Rprintf(__VA_ARGS__)
#else
#define DEBUG_PRINT(...) do {} while (0)
#endif

static int BCVersion;

extern SEXP R_mkClosure(SEXP formals, SEXP body, SEXP env);


typedef struct CompilerContext {

  bool toplevel;                  // Is this a top-level expression?
  bool tailcall;                  // Is this in tail position?
  bool need_return_jmp;           // Does return() need a longjmp?

  short optimize_level;           // Integer (0-3) optimization level TODO explain

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

} EnvFrame;

typedef struct CompilerEnv {

  struct EnvFrame *top_frame;     // Current frame being compiled

} CompilerEnv;

typedef struct CodeBuffer {

  // Instruction stream
  int *code;                      // Dynamic array of integers
  int code_count;                 // Current size of the code array
  int code_capacity;              // Current capacity of the code array

  // Constant pool
  SEXP constant_pool;             // R List object
  int const_count;                // Number of constants in the pool
  int const_capacity;             // Capacity of the constant pool

  // Label management (maps label names (strings) to code offsets (ints))
  // This needs to be a hashmap
  void *label_table;
  int label_generator_id;         // For generating unique labels

  // Source tracking
  SEXP expr_buf;                  // Buffer for source expressions
  SEXP srcref_buf;                // Buffer for source references

} CodeBuffer;

typedef struct VarInfo {

  EnvFrame * defining_frame;      // Frame where the variable is defined
  SEXP value;                     // var value
  bool found;                     // Was the variable found

} VarInfo;

// TODO here maybe should add InlineHandler structs for managing inlining of functions
// TODO compile() function
// TODO I think the context make. fns should be returning new instances instead of modifying in place
// ASK Do I have to wrap R_NilValue in PROTECT/UNPROTECT when assigning to struct fields?
// TODO I think I should not use malloc directly but Rapi allocations instead?
// TODO cleanup memory allocations

// Forward declarations for all defined functions
// because the compiler keeps yelling at me

bool find_var(SEXP var, CompilerContext *cntxt);
SEXP find_locals(SEXP expr);
SEXP find_locals_list(SEXP elist);
SEXP gen_code(SEXP e, CompilerContext *cntxt, SEXP gen, SEXP loc);
SEXP get_assigned_var(SEXP var);
void add_cenv_vars(CompilerEnv *cenv, SEXP vars);
void add_cenv_frame(CompilerEnv *cenv, SEXP vars);
CompilerEnv *make_cenv(SEXP env);
CompilerEnv *fun_env(SEXP forms, SEXP body, CompilerContext *cntxt);
CompilerContext *make_toplevel_ctx(CompilerEnv *cenv);
CompilerContext *make_function_ctx(CompilerContext *cntxt, SEXP forms, SEXP body);
CompilerContext *make_call_ctx(CompilerContext *cntxt, SEXP call);
CompilerContext *make_non_tail_call_ctx(CompilerContext *cntxt);
CompilerContext *make_no_value_ctx(CompilerContext *cntxt);
CompilerContext *make_loop_ctx(CompilerContext *cntxt, int loop_label, int end_label);
CompilerContext *make_arg_ctx(CompilerContext *cntxt);
CompilerContext *make_promise_ctx(CompilerContext *cntxt);
CodeBuffer *make_code_buffer();
void free_code_buffer(CodeBuffer *cb);
void cmp_const(SEXP val, CodeBuffer *cb, CompilerContext *cntxt);
void cmp_sym(SEXP sym, CodeBuffer *cb, CompilerContext *cntxt);
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
void cmp(SEXP e, CodeBuffer *cb, CompilerContext *cntxt, bool setloc);
SEXP cmpfun(SEXP f, void* __placeholder__);
static SEXP make_bc_code(SEXP code, SEXP consts);
static SEXP R_bcVersion();
SEXP is_compiled(SEXP fun);
void R_init_crbcc(DllInfo* dll);
VarInfo find_cenv_var( SEXP var, CompilerEnv * cenv );
SEXP code_buf_code( CodeBuffer * cb, CompilerContext * cntxt );


bool find_var( SEXP var, CompilerContext * cntxt ) {

  CompilerEnv * cenv = cntxt->env;
  VarInfo var_info = find_cenv_var( var, cenv );

  if (!var_info.found) {
     DEBUG_PRINT("?? find_var: Symbol '%s' NOT found in scope\n", CHAR(PRINTNAME(var)));
  }
  return var_info.found;

}

VarInfo find_cenv_var( SEXP var, CompilerEnv * cenv ) {

  VarInfo info = { NULL, R_NilValue, false };

  const char* var_name = CHAR( PRINTNAME( var ) );

  EnvFrame * current = cenv->top_frame;

  // Walk up the environment frames
  while ( current != NULL ) {

    // Check if its in extra_vars
    if ( current->extra_vars != R_NilValue ) {

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
  SEXP v = CADDR( var );

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


SEXP find_locals_list( SEXP elist ) {

  // Initialize empty list of locals
  SEXP found = R_NilValue;

  // Iterate over expressions in the list
  SEXP node = elist;
  while ( node != R_NilValue ) {
    
    // Get the first element
    SEXP expr = CAR( node );

    // Find locals in the expression (this is a recursive call)
    SEXP new_vars = find_locals( expr );

    if ( new_vars != R_NilValue ) {

      if (found == R_NilValue) {
          found = new_vars;
      } else {
          SEXP union_symbol = Rf_install("union");
          SEXP call = PROTECT( Rf_lang3( union_symbol, found, new_vars ) );
          found = Rf_eval( call, R_BaseEnv ); 
          UNPROTECT(1); 
      }
    }

    // Strip tjhe first element and continue
    node = CDR( node );

  }
  return found;
};


SEXP find_locals( SEXP expr ) {


  // Base case: expression is not a function call
  if ( TYPEOF( expr ) != LANGSXP ) return R_NilValue;
  

  // Okay so here we know expr is a LANGSXP
  SEXP fun = CAR( expr );

  if ( TYPEOF( fun ) != SYMSXP )
    return find_locals_list( expr ); // weird thingy

  // Its a function call with a symbol as function name
  const char* fname = CHAR( PRINTNAME( fun ) );

  // Its an assignment?
  if ( strcmp( fname, "<-" ) == 0 || strcmp( fname, "=" ) == 0 ) {
    
    // Assignment LHS (the variable being assigned to)
    SEXP var =  get_assigned_var( expr );

    // Recurse into RHS
    SEXP rhs_locals = find_locals( CADDR( expr ) );

    // Union of variable and rhs_locals
    SEXP union_symbol = Rf_install("union");
    SEXP call = PROTECT( Rf_lang3( union_symbol, rhs_locals, var ) );
    SEXP combined = Rf_eval( call, R_BaseEnv ); 
    UNPROTECT(1);
    return combined;

  }

  // A for loop
  if ( strcmp( fname, "for" ) == 0 ) {
    SEXP loop_var_sym = CADR( expr ); 
    SEXP loop_var_str = Rf_ScalarString( PRINTNAME( loop_var_sym ) );

    SEXP seq_locals = find_locals( CADDR( expr ) );
    SEXP body_locals = find_locals( CADDDR( expr ) );

    SEXP union_symbol = Rf_install("union");
    SEXP call1 = PROTECT( Rf_lang3( union_symbol, body_locals, loop_var_str ) );
    SEXP temp = PROTECT( Rf_eval( call1, R_BaseEnv ) );
    SEXP call2 = PROTECT( Rf_lang3( union_symbol, temp, seq_locals ) );
    SEXP combined = Rf_eval( call2, R_BaseEnv );

    UNPROTECT(2); // Unprotect call1, call2 (and temp implicit)
    return combined;
  }

  // Scope barriers
  if ( strcmp( fname, "function" ) == 0 ||
       strcmp( fname, "quote" ) == 0 ||
       strcmp( fname, "expression" ) == 0 )
    return R_NilValue;


  // General case: recurse into all arguments
  return find_locals_list( CDR( expr ) );

};

void add_cenv_vars( CompilerEnv * cenv, SEXP vars ) {

  // Check if there is something to add
  if (vars == R_NilValue) return;
  
  // Combine current vars with new vars using R union function,
  // is this efficient? Or should I do this in plain C

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
  
  SEXP union_symbol = Rf_install("union");
  SEXP call = PROTECT( Rf_lang3( union_symbol, current_vars, vars ) );
  SEXP combined_vars = Rf_eval( call, R_BaseEnv ); 
  
  cenv->top_frame->extra_vars = combined_vars;
  R_PreserveObject(combined_vars);
  
  UNPROTECT(2);
  // Release old object if it existed
  if (cenv->top_frame->extra_vars != R_NilValue) {
      R_ReleaseObject(cenv->top_frame->extra_vars);
  }

  cenv->top_frame->extra_vars = combined_vars;
  
  UNPROTECT(1);
};


void add_cenv_frame( CompilerEnv * cenv, SEXP vars ) {

  EnvFrame * new_frame = (EnvFrame *) malloc ( 1 * sizeof( EnvFrame ));
  
  new_frame->r_env = Rf_allocSExp( ENVSXP );
  // Preserve the environment stored in C struct
  R_PreserveObject(new_frame->r_env);

  SET_ENCLOS(new_frame->r_env, cenv->top_frame->r_env);
  new_frame->type = FRAME_LOCAL; 
  new_frame->parent = cenv->top_frame;
  new_frame->extra_vars = R_NilValue; // Initialize to avoid garbage

  cenv->top_frame = new_frame;

  add_cenv_vars( cenv, vars );
};

// @manual 5.1
CompilerEnv * make_cenv( SEXP env ) {

  // Allocate the compilation environment entry point
  CompilerEnv *cenv = (CompilerEnv *) malloc (1 * sizeof( CompilerEnv ));
  
  // Allocate the topmost frame
  cenv->top_frame = (EnvFrame *) malloc ( 1 * sizeof( EnvFrame ));

  // TODO what should the initial values be
  cenv->top_frame->parent = NULL;
  cenv->top_frame->r_env = env;
  cenv->top_frame->extra_vars = R_NilValue;

  return cenv;

}

// @manual 5.2
CompilerEnv * fun_env( SEXP forms, SEXP body, CompilerContext * cntxt ) {
                                          //TODO
                                          // this apparently should access names?

  add_cenv_frame( cntxt->env, getAttrib( forms, R_NamesSymbol ) );
  SEXP locals = find_locals_list( body /*TODO concat with forms, also pass cntxnt for errors*/ );
  add_cenv_vars( cntxt->env, locals );
  
  return cntxt->env;

}

// @manual 4.1
CompilerContext * make_toplevel_ctx( CompilerEnv *cenv ) {
  
  CompilerContext *ctx = (CompilerContext *) malloc ( 1 * sizeof(CompilerContext) );

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
CompilerContext * make_function_ctx( CompilerContext * cntxt, SEXP forms, SEXP body ) {

  CompilerEnv * nenv = fun_env(forms, body, cntxt);

  CompilerContext * ncntxt = (CompilerContext *) malloc ( 1 * sizeof(CompilerContext) );
  ncntxt->env = nenv;

  // Copy options from parent context
  ncntxt->optimize_level = cntxt->optimize_level;
  ncntxt->supress_all = cntxt->supress_all;
  ncntxt->supress_no_super_assign = cntxt->supress_no_super_assign;
  ncntxt->supress_undefined = cntxt->supress_undefined;
  
  return ncntxt;
};

// @manual 4.2
CompilerContext * make_call_ctx( CompilerContext * cntxt, SEXP call ) {

  cntxt->call = call;
  return cntxt;

};

// manual 4.2
CompilerContext * make_non_tail_call_ctx( CompilerContext * cntxt ) {

    cntxt->tailcall = false;
    return cntxt;
  
};

// @manual 4.2
CompilerContext * make_no_value_ctx( CompilerContext * cntxt ) {

    cntxt->tailcall = false;
    return cntxt;
  
};

// @manual 4.2
CompilerContext * make_loop_ctx( CompilerContext * cntxt, int loop_label, int end_label ) {

  LoopInfo * li = (LoopInfo *) malloc ( 1 * sizeof( LoopInfo ) );
  li->loop_label_id = loop_label;
  li->end_label_id = end_label;
  li->goto_ok = true;

  cntxt->loop = li;
  return cntxt;

};

// @manual 4.2
CompilerContext * make_arg_ctx( CompilerContext * cntxt ) {

  cntxt->tailcall = false;
  cntxt->toplevel = false;

  if ( cntxt->loop != NULL )
    cntxt->loop->goto_ok = false;

  return cntxt;

};

// @manual 4.2
CompilerContext * make_promise_ctx( CompilerContext * cntxt ) {

  cntxt->tailcall = false;
  cntxt->toplevel = true;
  cntxt->need_return_jmp = true;

  if ( cntxt->loop != NULL )
    cntxt->loop->goto_ok = false;

  return cntxt;

};

// @manual 3
CodeBuffer * make_code_buffer() {

  CodeBuffer * cb = (CodeBuffer *) malloc ( 1 * sizeof( CodeBuffer ) );

  cb->code_capacity = 128; // Initial capacity
  cb->code_count = 0;
  cb->code = (int *) malloc ( cb->code_capacity * sizeof( int ) );

  cb->constant_pool = R_NilValue; // Empty list
  cb->const_count = 0;

  cb->label_table = NULL; // TODO initialize label table
  cb->const_capacity = 0; 
  cb->label_generator_id = 0;

  cb->expr_buf = R_NilValue; // TODO initialize expression buffer
  cb->srcref_buf = R_NilValue; // TODO initialize srcref buffer

  return cb;

};

void free_code_buffer(CodeBuffer *cb) {
    if (cb) {
        if (cb->code) free(cb->code);
        if (cb->constant_pool != R_NilValue) R_ReleaseObject(cb->constant_pool);
        free(cb);
    }
}

// @manual 2.4
void cmp_const( SEXP val, CodeBuffer * cb, CompilerContext * cntxt ) {

  if ( val == R_NilValue )
    cb_putcode( cb, LDNULL_OP );
  else if ( val == R_TrueValue )
    cb_putcode( cb, LDTRUE_OP );
  else if ( val == R_FalseValue )
    cb_putcode( cb, LDFALSE_OP );
  else {
    cb_putcode( cb, LDCONST_OP );
    cb_putconst( cb, val );
  }

};

// @manual 2.5
void cmp_sym( SEXP sym, CodeBuffer * cb, CompilerContext * cntxt ) {

  if ( ! find_var( sym, cntxt ) ) {
      DEBUG_PRINT("?? cmp_sym: Undefined symbol '%s'\n", CHAR(PRINTNAME(sym)));
      // error("Undefined symbol: %s", CHAR(PRINTNAME(sym)));
      // Warning instead of error for basic version to avoid crashes on globals
      warning("Undefined symbol: %s", CHAR(PRINTNAME(sym)));
  }
  
  int poolref = cb_putconst( cb, sym );
  // TODO if missing ok
  cb_putcode( cb, GETVAR_OP );
  cb_putcode( cb, poolref );

  if ( cntxt->tailcall ) {
    cb_putcode( cb, RETURN_OP );
  }

};

// @manual 2.6
void cmp_call( SEXP call, CodeBuffer * cb, CompilerContext * cntxt ) {

  // TODO handle contexts
  cntxt = make_call_ctx( cntxt, call );
  SEXP fun = CAR( call );
  SEXP args = CDR( call );

  if ( TYPEOF( fun ) == SYMSXP ) {
    
    // TODO do inlining here
    DEBUG_PRINT(".. cmp_call: Calling symbol function '%s'\n", CHAR(PRINTNAME(fun)));
    cmp_call_sym_fun( fun, args, call, cb, cntxt );

  } else {
    
    DEBUG_PRINT(".. cmp_call: Calling complex expression function\n");
    // Hack for handling break() and next() calls
    if ( TYPEOF( fun ) == LANGSXP && TYPEOF( CAR( fun )) == SYMSXP ) {
      const char* ch = CHAR( PRINTNAME( CAR( fun ) ) );
      if ( (strcmp( ch, "break" ) == 0) || (strcmp( ch, "next" ) == 0) ) {
        return cmp( fun, cb, cntxt, true );
      }

    }

    cmp_call_expr_fun( fun, args, call, cb, cntxt );
  
  }

  // TODO restore context

};

// @manual 2.6
void cmp_call_sym_fun( SEXP fun, SEXP args, SEXP call, CodeBuffer * cb, CompilerContext * cntxt ) {

  const char* maybe_NSE_symbols[] = {"bquote", NULL}; 
  
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

  if ( cntxt->tailcall ) {
    cb_putcode( cb, RETURN_OP );
  }

};

void cmp_call_expr_fun( SEXP fun, SEXP args, SEXP call, CodeBuffer * cb, CompilerContext * cntxt ) {

  CompilerContext * ncntxt = make_non_tail_call_ctx( cntxt );
  cmp( fun, cb, ncntxt, true );
  cb_putcode( cb, CHECKFUN_OP );
  bool nse = false;
  
  cmp_call_args( args, cb, ncntxt, nse );
  int ci = cb_putconst( cb, call );
  cb_putcode( cb, CALL_OP );
  cb_putcode( cb, ci );

  if ( ncntxt->tailcall ) {
    cb_putcode( cb, RETURN_OP );
  }
};

void cmp_call_args( SEXP args, CodeBuffer * cb, CompilerContext * cntxt, bool nse ) {

  SEXP names = getAttrib( args, R_NamesSymbol );
  CompilerContext * pnctxt = make_promise_ctx( cntxt );

  while ( args != R_NilValue ) {

    SEXP a = CAR( args );
    SEXP n = TAG( args );

    // TODO handle weird stuff with ... and missing arguments

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

  const char *name_str = CHAR(PRINTNAME(tag));
  
  if (name_str[0] == '\0')
      return;

  int ci = cb_putconst( cb, tag );
  cb_putcode( cb, SETTAG_OP );

};

void cmp_const_arg( SEXP a, CodeBuffer * cb, CompilerContext * cntxt ) {

  if ( a == R_NilValue )
    cb_putcode( cb, PUSHNULLARG_OP );
  else if ( a == R_TrueValue )
    cb_putcode( cb, PUSHTRUEARG_OP );
  else if ( a == R_FalseValue )
    cb_putcode( cb, PUSHFALSEARG_OP );
  else {
    int ci = cb_putconst( cb, gen_code( a, cntxt, R_NilValue, R_NilValue ) );
    cb_putcode( cb, PUSHCONSTARG_OP );
    cb_putcode( cb, ci );
  }
  
};

void cb_putcode( CodeBuffer * cb, int opcode ) {
  DEBUG_PRINT("++ putcode: Emitting Opcode %d\n", opcode);

  if ( cb->code_count >= cb->code_capacity ) {
    // Resize code array
    cb->code_capacity *= 2;
    cb->code = (int *) realloc( cb->code, cb->code_capacity * sizeof( int ) );
  }

  cb->code[ cb->code_count ] = opcode;
  cb->code_count += 1;
  // TODO handle source tracking here

};

int cb_getcode( CodeBuffer * cb, int pos ) {

  if ( pos < 0 || pos >= cb->code_count ) {
    error("CodeBuffer: Invalid code position");
  }

  return cb->code[ pos ];

};


int cb_putconst( CodeBuffer * cb, SEXP item ) {
  if ( cb->constant_pool == R_NilValue ) {
       cb->const_capacity = 16;
       cb->constant_pool = Rf_allocVector( VECSXP, cb->const_capacity );
       R_PreserveObject(cb->constant_pool);
  }

  if ( cb->const_count == cb->const_capacity ) {
    int new_cap = cb->const_capacity * 2;
    SEXP new_pool = Rf_allocVector( VECSXP, new_cap );
    R_PreserveObject(new_pool);
    
    for ( int i = 0; i < cb->const_count; i++ ) {
      SET_VECTOR_ELT( new_pool, i, VECTOR_ELT( cb->constant_pool, i ) );
    }

    R_ReleaseObject(cb->constant_pool);
    cb->constant_pool = new_pool;
    cb->const_capacity = new_cap;

  }

  int i = cb->const_count;

  SET_VECTOR_ELT( cb->constant_pool, cb->const_count, item );
  cb->const_count++;

  return i;

};

SEXP cb_getconst( CodeBuffer * cb, int idx ) {

  if ( idx < 0 || idx >= cb->const_count ) {
    error("CodeBuffer: Invalid constant index");
  }

  return VECTOR_ELT( cb->constant_pool, idx );

};


bool may_call_browser( SEXP expr, CompilerContext * cntxt ) {
  // TODO
  return false;
}


// @manual 2.3
void cmp( SEXP e, CodeBuffer * cb, CompilerContext * cntxt, bool setloc ) {

  if ( setloc ) {
    //TODO set source location
    // I aint doing this now
  }

  // TODO constant fold here (ce means constant expression i guess)
  SEXP ce = R_NilValue;
  
  if ( isNull( ce ) ) {

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
      cmp_sym( e, cb, cntxt );
      break;
    
    case BCODESXP:
      DEBUG_PRINT("!! cmp: Error - Literal Bytecode found\n");
      Rf_error("Cannot compile bytecode");
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

  //TODO Restore curloc if setloc is true

};

// @manual 15.1 
SEXP cmpfun(SEXP f, void* __placeholder__) {

  SEXPTYPE type = TYPEOF( f );

  switch (type)
  {
  case CLOSXP:

    DEBUG_PRINT(">> cmpfun: Compiling CLOSXP (Function)\n");
    
    CompilerContext * cntxt = make_toplevel_ctx( make_cenv( CLOENV(f) ) );
    CompilerContext * ncntxt = make_function_ctx(cntxt, FORMALS(f), BODY(f));
    
    if ( may_call_browser( BODY(f), ncntxt ) ) {
      DEBUG_PRINT("!! cmpfun: Function may call browser, skipping compilation\n");
      return f;
    }

    SEXP loc; // Of type LISTSXP

    if ( TYPEOF( BODY(f) ) != LANGSXP || strcmp(CHAR( STRING_ELT(BODY(f), 1) ), "{" ) != 0 )
      loc = PROTECT( Rf_list2( R_NilValue, R_NilValue ) );
    else
      loc = PROTECT( R_NilValue ); //TODO doesnt this have a macro ?
    
    SEXP b = gen_code( BODY(f), ncntxt, R_NilValue, loc ); //PROTECT?
    
    // val <- .Internal(bcClose(formals(f), b, environment(f)))
    // TODO make a function for calling .Internal functions
    SEXP bcClose_sym = Rf_install("bcClose");
    SEXP internal_sym = Rf_install(".Internal");
    SEXP inner_call = PROTECT( Rf_lang4(bcClose_sym, FORMALS(f), b, CLOENV(f)) );
    SEXP full_call = PROTECT( Rf_lang2(internal_sym, inner_call) );
    SEXP val = PROTECT( Rf_eval(full_call, R_BaseEnv) );

    SEXP attrs = ATTRIB( f ); //PROTECT? Or is it ok to not protect since f is protected?

    if ( ! isNull( attrs ) )
      SET_ATTRIB( val, attrs );
    
    if ( isS4( f ) )
      // TODO I have no idea what FALSE and 0 mean here
      val = asS4( val, FALSE, 0 );

    UNPROTECT(4); 
    
    // TODO: Free C structs here
    return val;

  case BUILTINSXP:
  case SPECIALSXP:
     DEBUG_PRINT(">> cmpfun: Primitive function (BUILTIN/SPECIAL), skipping\n");
    return f;

  default:
    DEBUG_PRINT("!! cmpfun: Error - Argument is not a function type: %d\n", type);
    Rf_error("Argument must be a closure, builtin, or special function");
    return R_NilValue; 
  }

}


// @manual 2.1
SEXP gen_code( SEXP e, CompilerContext * cntxt, SEXP gen, SEXP loc ) {

  CodeBuffer * cb = make_code_buffer();

  if ( isNull( gen ) )
  if ( Rf_isNull( gen ) )
    cmp( e, cb, cntxt, false );

  SEXP result = code_buf_code(cb, cntxt);
  
  free_code_buffer(cb);
  
  return result;
};

SEXP code_buf_code( CodeBuffer * cb, CompilerContext * cntxt ) {

  // TODO patch labels once inlining is real

  SEXP code_vec = PROTECT( Rf_allocVector( INTSXP, cb->code_count ) );
  memcpy(INTEGER(code_vec), cb->code, cb->code_count * sizeof(int));
  SEXP const_pool = cb->constant_pool;
  if (const_pool == R_NilValue) const_pool = PROTECT(Rf_allocVector(VECSXP, 0));
  else PROTECT(const_pool); // Balance the unprotect below

  // .Internal(mkCode(code, consts))
  SEXP mkCode_sym = Rf_install("mkCode");
  SEXP inner_call = Rf_lang3(mkCode_sym, code_vec, const_pool);
    
  SEXP internal_sym = Rf_install(".Internal");
  SEXP full_call = PROTECT(Rf_lang2(internal_sym, inner_call));
    
  SEXP final_bcode = Rf_eval(full_call, R_BaseEnv);

  UNPROTECT(3); 
  return final_bcode;

};

// ============================================================ //

// static SEXP make_bc_code(SEXP code, SEXP consts) {
//   // .Internal(mkCode(code, consts))
//   SEXP call;
//   call = PROTECT(Rf_lang3(Rf_install("mkCode"), code, consts));
//   call = PROTECT(Rf_lang2(Rf_install(".Internal"), call));

//   SEXP res = PROTECT(Rf_eval(call, R_BaseEnv));

//   UNPROTECT(3);
//   return res; /* BCODESXP */
// }

static SEXP R_bcVersion() {
  SEXP call, res;

  // .Internal(bcVersion())
  call = PROTECT(lang1(install("bcVersion")));
  call = PROTECT(lang2(install(".Internal"), call));

  res = PROTECT(eval(call, R_BaseEnv));

  UNPROTECT(4);
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
