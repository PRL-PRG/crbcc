#  crbcc: C R Bytecode Compiler
#  Copyright (C) 2026 Josef Malý
#  Copyright (C) 2026 Faculty of Information Technology, CTU in Prague

#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 2 of the License, or (at
#  your option) any later version.

#  This program is distributed in the hope that it will be useful, but
#  WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
#  General Public License for more details.

# Synthetic corpus for compile-time warning/error coverage in src/cmpfun.c.
#
# Intended compile options when consuming this corpus:
#   list(suppressAll = FALSE, suppressUndefined = FALSE)

compiler_options_warnings <- list(
  suppressAll = FALSE,
  suppressUndefined = FALSE
)

# Helper used to trigger notify_bad_call() via argument matching failure.
.bad_call_helper <- function(alpha, beta = 0) {
  alpha + beta
}

.bad_call_helper3 <- function(alpha, beta, gamma = 0) {
  alpha + beta + gamma
}

warning_error_corpus <- list(
  wrong_dots_use = function() {
    ...
  },

  wrong_dots_use_dotdot1 = function() {
    ..1
  },

  wrong_dots_use_dotdot2 = function() {
    ..2
  },

  wrong_dots_use_dots_in_call = function() {
    list(...)
  },

  undef_var = function() {
    .definitely_missing_var_xyz123
  },

  undef_var_dotdotx = function() {
    ..x
  },

  undef_fun = function() {
    .definitely_missing_function_xyz123()
  },

  bad_assign_fun = function() {
    (foo())(x) <- 1
    x
  },

  no_super_assign_var = function() {
    .super_missing_var_xyz123 <<- 1
    invisible(NULL)
  },

  wrong_arg_count = function() {
    sqrt(1, 2)
  },

  wrong_arg_count_exp = function() {
    exp(1, 2)
  },

  wrong_arg_count_not = function() {
    `!`(1, 2)
  },

  wrong_arg_count_paren = function() {
    `(`(1, 2)
  },

  wrong_break_next_break = function() {
    break
  },

  wrong_break_next_next = function() {
    next
  },

  bad_call_argument_matching = function() {
    .bad_call_helper(1, alpha = 2)
  },

#  Known issue: match_call does not propagate
#  the error message from R_TryEval, because
#  R_TryEval returns just a bool.
#
#   bad_call_argument_matching_duplicate = function() {
#     .bad_call_helper(alpha = 1, alpha = 2)
#   },

  bad_call_argument_matching_partial = function() {
    .bad_call_helper3(1, 2, al = 3)
  },

  switch_no_alternatives = function() {
    switch(1)
  },

  switch_no_alternatives_char = function() {
    switch("a")
  },

  switch_multiple_defaults = function() {
    switch(1, a = 10, 20, 30)
  },

  switch_multiple_defaults_with_missing = function() {
    switch(2, a = 10, , 20, 30)
  },

  assign_syntactic_fun = function() {
    `for` <- function(...) NULL
    `for`
  },

  assign_syntactic_fun_plural = function() {
    `for` <- function(...) NULL
    `while` <- function(...) NULL
    list(`for`, `while`)
  }
)
