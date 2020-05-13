#pragma once

#define VRUTIL_POWER_OF_TWO(xExponent) (1 << xExponent)

#define NULLSTR 0
#define NULLENT world

#ifndef NDEBUG

#define ASSERT(x) \
    do { \
        if(!(x)) \
        { \
            bprint("Assertion failure!\n'" __FUNC__ "' at " __LINESTR__ "\nExpression is false: '" #x "'\n"); \
        } \
    } while(FALSE)

#else

#define ASSERT(x)

#endif
