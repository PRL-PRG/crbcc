x <- function() {
    print(y)
    return(5)
}

crbcc::cmpfun(x)
print("-----------")
compiler::cmpfun(x)