#include "xfa.h"
#include "xfatest.h"

extern void printf(void);

XFA(xfatest, 0) const xfatest_t _xfatest2 = { .val = 2, .text = "xfatest2", .fptr = printf };

int hack2;
