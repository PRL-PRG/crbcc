/*
 * crbcc: C R Bytecode Compiler
 * src/crbcc.c
 * 
 * Copyright (C) 2026 Josef Malý
 * Copyright (C) 2026 Faculty of Information Technology, CTU in Prague
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 */
#pragma once

/*
Turn this flag to true to enable a basic peephole optimizer over
the final bytecode object. The basic test suite will not pass
since structural compatibility will be ruined, however it should
work and it should introduce some minor speedups in the resulting code.
*/
#define OPTIMIZE_INCOMPATIBLE false

#ifndef R_NO_REMAP
# define R_NO_REMAP
#endif

#include <R.h>
#include <Rinternals.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>

#define END_OPCODES -1

static SEXP R_TrueValue;
static SEXP R_FalseValue;
static int BCVersion;

#define SEQ_ALONG( iter_name, along_var ) for ( SEXP iter_name = along_var; iter_name != R_NilValue; iter_name = CDR(iter_name) )
#define SEQ_ALONG_IX( iter_name, along_var, iter ) \
    int iter = 0; \
    for ( SEXP iter_name = along_var; iter_name != R_NilValue; iter_name = CDR(iter_name), iter++ )

// assuming "cb" varibale is a code buffer pointer
// in relevant context
#define PUTCONST(const) cb_putconst(cb, const)
#define L(x) (-(x + 1)) // When inserting a label identifier into instr. pool
#define PUTCODE(...) cb_putcode(cb, __VA_ARGS__, END_OPCODES)
#define PUTLABEL(x) cb_putlabel(cb, x)

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

#define MAX_OPCODE 128

static const int op_argc[MAX_OPCODE + 1] = {
  [RETURN_OP]             = 0,
  [GOTO_OP]               = 1,
  [BRIFNOT_OP]            = 2,
  [POP_OP]                = 0,
  [STARTLOOPCNTXT_OP]     = 2,
  [ENDLOOPCNTXT_OP]       = 1,
  [DOLOOPNEXT_OP]         = 0,
  [DOLOOPBREAK_OP]        = 0,
  [STARTFOR_OP]           = 3,
  [STEPFOR_OP]            = 1,
  [ENDFOR_OP]             = 0,
  [INVISIBLE_OP]          = 0,
  [LDCONST_OP]            = 1,
  [LDNULL_OP]             = 0,
  [LDTRUE_OP]             = 0,
  [LDFALSE_OP]            = 0,
  [GETVAR_OP]             = 1,
  [DDVAL_OP]              = 1,
  [SETVAR_OP]             = 1,
  [GETFUN_OP]             = 1,
  [GETBUILTIN_OP]         = 1,
  [GETINTLBUILTIN_OP]     = 1,
  [CHECKFUN_OP]           = 0,
  [MAKEPROM_OP]           = 1,
  [DOMISSING_OP]          = 0,
  [SETTAG_OP]             = 1,
  [DODOTS_OP]             = 0,
  [PUSHARG_OP]            = 0,
  [PUSHCONSTARG_OP]       = 1,
  [PUSHNULLARG_OP]        = 0,
  [PUSHTRUEARG_OP]        = 0,
  [PUSHFALSEARG_OP]       = 0,
  [CALL_OP]               = 1,
  [CALLBUILTIN_OP]        = 1,
  [CALLSPECIAL_OP]        = 1,
  [MAKECLOSURE_OP]        = 1,
  [UMINUS_OP]             = 1,
  [UPLUS_OP]              = 1,
  [ADD_OP]                = 1,
  [SUB_OP]                = 1,
  [MUL_OP]                = 1,
  [DIV_OP]                = 1,
  [EXPT_OP]               = 1,
  [SQRT_OP]               = 1,
  [EXP_OP]                = 1,
  [EQ_OP]                 = 1,
  [NE_OP]                 = 1,
  [LT_OP]                 = 1,
  [LE_OP]                 = 1,
  [GE_OP]                 = 1,
  [GT_OP]                 = 1,
  [AND_OP]                = 1,
  [OR_OP]                 = 1,
  [NOT_OP]                = 1,
  [DOTSERR_OP]            = 0,
  [STARTASSIGN_OP]        = 1,
  [ENDASSIGN_OP]          = 1,
  [STARTSUBSET_OP]        = 2,
  [DFLTSUBSET_OP]         = 0,
  [STARTSUBASSIGN_OP]     = 2,
  [DFLTSUBASSIGN_OP]      = 0,
  [STARTSUBSET2_OP]       = 2,
  [DFLTSUBSET2_OP]        = 0,
  [STARTSUBASSIGN2_OP]    = 2,
  [DFLTSUBASSIGN2_OP]     = 0,
  [DOLLAR_OP]             = 2,
  [DOLLARGETS_OP]         = 2,
  [ISNULL_OP]             = 0,
  [ISLOGICAL_OP]          = 0,
  [ISINTEGER_OP]          = 0,
  [ISDOUBLE_OP]           = 0,
  [ISCOMPLEX_OP]          = 0,
  [ISCHARACTER_OP]        = 0,
  [ISSYMBOL_OP]           = 0,
  [ISOBJECT_OP]           = 0,
  [VECSUBSET_OP]          = 1,
  [MATSUBSET_OP]          = 1,
  [VECSUBASSIGN_OP]       = 1,
  [MATSUBASSIGN_OP]       = 1,
  [AND1ST_OP]             = 2,
  [AND2ND_OP]             = 1,
  [OR1ST_OP]              = 2,
  [OR2ND_OP]              = 1,
  [GETVAR_MISSOK_OP]      = 1,
  [DDVAL_MISSOK_OP]       = 1,
  [VISIBLE_OP]            = 0,
  [SETVAR2_OP]            = 1,
  [STARTASSIGN2_OP]       = 1,
  [ENDASSIGN2_OP]         = 1,
  [SETTER_CALL_OP]        = 2,
  [GETTER_CALL_OP]        = 1,
  [SWAP_OP]               = 0,
  [DUP2ND_OP]             = 0,
  [SWITCH_OP]             = 4,
  [RETURNJMP_OP]          = 0,
  [STARTSUBSET_N_OP]      = 2,
  [STARTSUBASSIGN_N_OP]   = 2,
  [VECSUBSET2_OP]         = 1,
  [MATSUBSET2_OP]         = 1,
  [VECSUBASSIGN2_OP]      = 1,
  [MATSUBASSIGN2_OP]      = 1,
  [STARTSUBSET2_N_OP]     = 2,
  [STARTSUBASSIGN2_N_OP]  = 2,
  [SUBSET_N_OP]           = 2,
  [SUBSET2_N_OP]          = 2,
  [SUBASSIGN_N_OP]        = 2,
  [SUBASSIGN2_N_OP]       = 2,
  [LOG_OP]                = 1,
  [LOGBASE_OP]            = 1,
  [MATH1_OP]              = 2,
  [DOTCALL_OP]            = 2,
  [COLON_OP]              = 1,
  [SEQALONG_OP]           = 1,
  [SEQLEN_OP]             = 1,
  [BASEGUARD_OP]          = 2,
  [INCLNK_OP]             = 0,
  [DECLNK_OP]             = 0,
  [DECLNK_N_OP]           = 1,
  [INCLNKSTK_OP]          = 0,
  [DECLNKSTK_OP]          = 0,
};


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

typedef struct NamesList {        // List of discovered variable names

  const char ** vars;             // List of strings, the actual names
  int count;

} NamesList;

typedef struct EnvFrame {

  NamesList extra_vars;           // Vector of local variables discovered by the compiler
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
  int char_code_offset;            // Where to patch the char INTSXP index
  int n_char;                      // Length of char labels array
  int * char_labels;               // Array of label IDs (with default at the end)

  // Numeric Labels Data (Always used)
  int num_code_offset;             // Where to patch the num INTSXP index
  int n_num;                       // Length of num labels array
  int * num_labels;                // Array of label IDs (with default at the end)

  struct SwitchPatch * next;

} SwitchPatch;

// Information about a variable
typedef struct VarInfo {

  EnvFrame* defining_frame;        // The frame in which the variable is defined if local
  SEXP env;                        // The env in which the variable is defined if not local
  SEXP value;                      // The variable itself
  bool found;

} VarInfo;

typedef struct Loc {               // Location tracking utility
  bool is_null;                    // expr & srcref are populated or not
  SEXP expr;
  SEXP srcref;
} Loc;

typedef struct Folded {            // Constant folding result

  bool folded;                     // Folding success flag
  SEXP value;                      // Result if fold successful

} Folded;

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