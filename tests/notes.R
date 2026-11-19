## The compiler notes of crbcc must be the same as the notes of GNU R. The
## test compares the text that each compiler writes to stdout and stderr,
## and the result: the byte code or the error. crbcc writes the notes in
## English only.
invisible(Sys.setLanguage("en"))

run <- function(cmp, f, options) {
  con <- textConnection("out", "w", local = TRUE)
  sink(con)
  sink(con, type = "message")
  code <- tryCatch(cmp(f, options = options), error = function(e) e,
                   finally = { sink(type = "message"); sink() })
  close(con)
  list(code = code, out = out)
}
same <- function(f, options = list(suppressAll = FALSE)) {
  gnur <- run(compiler::cmpfun, f, options)
  crbcc <- run(crbcc::cmpfun, f, options)
  if (!identical(gnur$out, crbcc$out)) {
    print(f)
    message("GNU R:\n", paste(gnur$out, collapse = "\n"))
    message("crbcc:\n", paste(crbcc$out, collapse = "\n"))
    stop("the notes are not the same")
  }
  stopifnot(crbcc:::identical_strict(gnur$code, crbcc$code))
}
src <- function(text) eval(parse(text = text, keep.source = TRUE))

## Calls with arguments that do not match (issue #8).
same(function() paste0(1, collapse = 1, collapse = 2))
same(src("function() {\n  paste0(1, collapse = 1, collapse = 2)\n}"))
same(function(x) nchar(x, foo = 1, bar = 2))
same(function(x) rep_len(x, 1, 2, 3))
## If suppressAll is TRUE, there is no output.
same(function() paste0(1, collapse = 1, collapse = 2), list())

## Local variables.
same(function() { assign("v1", 1); delayedAssign("v2", 2); v1 + v2 })
same(function(e) { assign("v1", w <- 1, envir = e); v1 + w })
same(function(x) { x[i <- 1] <- 2; i })
same(function() { local({ v3 <- 1 }); v3 })
same(function(local) { local({ v3 <- 1 }); v3 })
same(function() { y ~ (v4 <- 1); v4 })
same(function() { quote <- identity; quote(v5 <- 1); v5 })
same(function() { assign("c", function(...) 1); c(1, 2) })
same(function() { `~` <- 1; `function` <- 2; `<-` <- 3; `for` <- 4; `=` <- 5; 6 })
## The frames below topenv() of the closure environment are local, as in
## GNU R. The compiler does not examine a call to a local function.
same(local({ g <- function(a) NULL; function() g(1, 2) }))
same(local({ sum <- function(a) a; function() sum(1, 2) }))
same((function() { g <- function(a) NULL; function() g(1, 2) })())
same((function(...) function() list(..1, ...))(1))
## A string in latin1 as a name. GNU R translates it to the native encoding.
s <- iconv("\u00e9", "UTF-8", "latin1")
f <- function(x) NULL
body(f) <- call("{", call("<-", s, 1), as.name("\u00e9"))
same(f)
body(f) <- call("<<-", s, 1)
same(f)
body(f) <- call("{", call("<-", call("names", call("$", quote(x), s)), 1), call("$", quote(x), s))
same(f)

## The other notes.
same(src("function() {\n  zzz()\n}"))
same(function() zzz)
same(function() zzz <<- 1)
same(function() break)
same(function() ..1)
same(function(x) switch(x, a = 1, 2, 3))
same(function(x) switch(x))

## GNU R cannot compile a call to `if` without a then-expression.
same(function(x) `if`(x))
same(function() `if`())
same(function(x) if (x) 1 else 2 + `if`(x))

## The compiler must not evaluate the code of the user before an error.
bc <- compiler::compile(quote(1))
f <- function() NULL
body(f) <- as.call(list(quote(message), "EVALUATED", bc))
same(f)
body(f) <- as.call(list(quote(g), quote(message("EVALUATED")), bc))
same(f)
## A bad assignment. This test examines only the output.
body(f) <- call("<-", quote(g(1)), quote(message("EVALUATED")))
stopifnot(length(run(crbcc::cmpfun, f, NULL)$out) == 0)
## A byte code literal that the compiler folds. GNU R stops in constantFold().
for (level in 0:3) {
  for (b in list(call("{", quote(x), bc), call("<-", quote(x), bc), call("-", bc))) {
    body(f) <- b
    same(f, list(optimize = level, suppressAll = FALSE))
  }
}

## The warnings of the compile must be the same as in GNU R, for each
## optimize level.
warnings_of <- function(cmp, f, options) {
  w <- character()
  withCallingHandlers(cmp(f, options = options), warning = function(c) {
    w <<- c(w, conditionMessage(c))
    invokeRestart("muffleWarning")
  })
  w
}
same_warnings <- function(f) {
  for (level in 0:3) {
    options <- list(optimize = level)
    stopifnot(identical(warnings_of(compiler::cmpfun, f, options),
                        warnings_of(crbcc::cmpfun, f, options)))
  }
}
## A partial argument match. GNU R calls match.call() twice for an inlined
## simple .Internal call.
op <- options(warnPartialMatchArgs = TRUE)
same_warnings(function(x) nchar(x, ty = "chars"))
same_warnings(function(x) rep_len(x, length = 3) + dnorm(x, me = 0))
options(op)

## Bodies that the parser cannot make. The error messages must be the same
## as in GNU R, for each optimize level.
error_of <- function(cmp, f, options)
  tryCatch({ cmp(f, options = options); "" }, error = conditionMessage)
same_error <- function(f) {
  for (level in 0:3) {
    options <- list(optimize = level)
    stopifnot(identical(error_of(compiler::cmpfun, f, options),
                        error_of(crbcc::cmpfun, f, options)))
  }
}
f <- function() NULL
body(f) <- call("<-", 1, 1)
same_error(f)
body(f) <- call("<-", call("f", 1:30 + 0.5), 1)
same_error(f)
body(f) <- call("<-", c("a", "b"), 1)
same_warnings(f)
## The conditions are the same as in GNU R, also their call.
cond_of <- function(cmp, f) tryCatch(cmp(f), warning = identity, error = identity)
stopifnot(identical(cond_of(compiler::cmpfun, f), cond_of(crbcc::cmpfun, f)))
body(f) <- call("<-", character(0), 1)
stopifnot(identical(cond_of(compiler::cmpfun, f), cond_of(crbcc::cmpfun, f)))
## A call to the empty symbol.
body(f) <- as.call(list(quote(expr = ), 1))
same_error(f)
body(f) <- call("<-", call("names", as.call(list(quote(expr = ), quote(x)))), 1)
same_error(f)

## A constant fold that gives an error or a warning. GNU R does not fold the
## call, and it writes nothing.
same(function() -"a")
same(function() 1:3 + 1:2)
same(function() sqrt(-1), list(optimize = 3, suppressAll = FALSE))
same(function() sqrt(4) + log(-1L) + exp(1) - 1i, list(optimize = 3, suppressAll = FALSE))
same(function() (-Inf)^1e308)
same(function() exp(1e309i), list(optimize = 3, suppressAll = FALSE))

## compile() gives the variables of the code the frame type of env, as GNU R
## does. For the global environment, the type is "global".
same_compile <- function(e) {
  options <- list(suppressAll = FALSE)
  gnur <- run(compiler::compile, e, options)
  crbcc <- run(crbcc::compile, e, options)
  stopifnot(identical(gnur$out, crbcc$out),
            crbcc:::identical_strict(gnur$code, crbcc$code))
}
same_compile(quote({ zzz <- function(a) NULL; zzz(1, 2) }))
g <- function(a) NULL
same_compile(quote({ g <- function(a) NULL; g(1, 2) }))

## The arguments of cmpfun are the same as in GNU R.
stopifnot(identical(formals(crbcc::cmpfun), formals(compiler::cmpfun)))

## NULL options are the default options, as in GNU R.
same(function(x) zzz(x), NULL)
## Options that are not a list.
same(function() zzz, c(suppressAll = FALSE))
same(function() zzz, list2env(list(suppressAll = FALSE)))
same(function() zzz, FALSE)

## Arguments that are not closures.
same(sum)
same(`if`)
same(1)
