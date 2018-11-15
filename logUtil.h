#ifndef LOGUTIL_H_INCLUDED
#define LOGUTIL_H_INCLUDED

#include "logUtil.h"

void logBeginningFunction(const char *function);
void logEndingFunction(const char *function);
void logExceptionInFunction(const char *function, const char *message);

#endif
