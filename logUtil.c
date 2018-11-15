#include "logUtil.h"
#include <stdio.h>
#include <stdlib.h>

static void logDashedLine(){
    fprintf(stderr, "------------------------------------\n");
}

void logBeginningFunction(const char *function){
    logDashedLine();
    fprintf(stderr, "Function %s is starting\n", function);
}

void logEndingFunction(const char *function){
    fprintf(stderr, "Function %s is ending\n", function);
    logDashedLine();
}

void logExceptionInFunction(const char *function, const char *message){
    fprintf(stderr, "Exception in %s: %s\n", function, message);
    logDashedLine();
}