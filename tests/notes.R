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

## Arguments that are not closures.
same(sum)
same(`if`)
same(1)
