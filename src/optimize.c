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
#include "crbcc_internals.h"

void optimize_peephole( CodeBuffer * cb ) {

    /*
    TODO: iterate over the instruction stream, check triplets at a time? since
    that is the longest subarray which could be affected atm.
    The crux is going to be recalculating the jump instruction positioning.
    */ 
    return;

};

/*
Redundant store/reload round-trip

Example:

f15 <- function(x) {
  result <- x + 1
  result
}

Produces:

GETVAR x
LDCONST 1
ADD
SETVAR result   --|
POP               |---> Could be optimized
GETVAR result   --|     to SETVAR result only
RETURN
*/

static int optimize_store_load_rt( int * inst_buff ) {

    return 3;
};

/*
Dead code after unconditional jump

Example:

f13 <- function(x) {
  for (i in 1:10) {
    if (i == 5) break
  }
  i
}

Produces:

... BRIFNOT -> L
GOTO 24        
GOTO 21     --> unreachable, remove
LDNULL
POP
STEPFOR ...

*/

static int optimize_dead_jmp( int * inst_buff ) {

    return 2;
};

/*
Dead constant push

Example:

f2 <- function(x) {
  5
  x + 1
}

Produces:

LDCONST 5  --|
POP        --|---> both can be removed
GETVAR x
LDCONST 1
ADD
RETURN
*/

static int optimize_dead_push( int * inst_buff ) {

    return 3;
};