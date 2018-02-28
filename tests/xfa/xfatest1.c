#include "xfa.h"
#include "xfatest.h"

extern void main(void);

const xfatest_t XFA(xfatest, 0) _xfatest1 = { .val = 1, .text = "xfatest1", .fptr = main };

int hack1;
