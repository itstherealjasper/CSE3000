#pragma once

/*
This header defines several custom assert macros.
Asserts takes as input an expression, and if the expression is false, the program crashes with an error message.
This makes asserts useful for debugging (in case crashing is acceptable).

A common use of asserts is to check preconditions and postconditions, for example:

int MyFunction(int x)
{
	pumpkin_assert_simple(x < 10, "The variable x must be less or equal to ten!");
	...
	[do some calculations]
	...
	pumpkin_assert_simple(return_value > 0, "The return value should be a positive integer!");
	return return_value;
}

The asserts defined in this header are tied to different 'levels', with the intended meaning that higher level asserts are more expensive to check.
The user is expected to define the identifier PUMPKIN_ASSERT_LEVEL_DEFINITION prior to including this header file.
The identifier PUMPKIN_ASSERT_LEVEL_DEFINITION determines the assertion level, which enables/disables asserts

For example, defining:

#define PUMPKIN_ASSERT_LEVEL_DEFINITION 1

will enable pumpkin_assert_simple, but disable pumpkin_assert_moderate and other asserts from higher levels. 

See below in the code for the constants that are meant to be used to define levels.

The only assert that cannot be disabled is pumpkin_assert_permanent, and should therefore be used with extra care.
*/

#include <cstdio>
#include <stdlib.h>

#define PUMPKIN_ASSERT_LEVEL_DEFINITION 1

/*#ifndef PUMPKIN_ASSERT_LEVEL_DEFINITION
	#error You need to define the identifier PUMPKIN_ASSERT_LEVEL before including pumpkin_assert.h. For example, define PUMPKIN_ASSERT_LEVEL at the start of your main.cpp.
#endif*/
	
//these are the values that should be used with PUMPKIN_ASSERT_LEVEL_DEFINITION
#define PUMPKIN_ASSERT_LEVEL_DISABLED 0 //disables all pumpkin asserts
#define PUMPKIN_ASSERT_LEVEL_SIMPLE 1 //simple asserts that have little to no impact on runtime
#define PUMPKIN_ASSERT_LEVEL_MODERATE 2
#define PUMPKIN_ASSERT_LEVEL_ADVANCED 3
#define PUMPKIN_ASSERT_LEVEL_EXTREME 4

#define pumpkin_assert_permanent(exp, message)\
\
	(!(exp)) ?\
		printf("ERROR: assert failed, aborting.\
			\n\tFailing expression: %s\
			\n\tError message: %s\
			\n\tFunction: %s\
			\n\tFile: %s\
			\n\tLine: %d\
			\n", #exp, #message, __FUNCTION__, __FILE__, __LINE__),\
		abort()\
	:\
	(void)0

#if (PUMPKIN_ASSERT_LEVEL_DEFINITION >= PUMPKIN_ASSERT_LEVEL_SIMPLE)
	#define pumpkin_assert_simple(expression, error_message) pumpkin_assert_permanent(expression, error_message)	
#else
	#define pumpkin_assert_simple(expression, error_message) ((void)0)
#endif

#if (PUMPKIN_ASSERT_LEVEL_DEFINITION >= PUMPKIN_ASSERT_LEVEL_MODERATE)
#define pumpkin_assert_moderate(expression, error_message) pumpkin_assert_permanent(expression, error_message)	
#else
#define pumpkin_assert_moderate(expression, error_message) ((void)0)
#endif

#if (PUMPKIN_ASSERT_LEVEL_DEFINITION >= PUMPKIN_ASSERT_LEVEL_ADVANCED)
#define pumpkin_assert_advanced(expression, error_message) pumpkin_assert_permanent(expression, error_message)	
#else
#define pumpkin_assert_advanced(expression, error_message) ((void)0)
#endif

#if (PUMPKIN_ASSERT_LEVEL_DEFINITION >= PUMPKIN_ASSERT_LEVEL_EXTREME)
#define pumpkin_assert_extreme(expression, error_message) pumpkin_assert_permanent(expression, error_message)	
#else
#define pumpkin_assert_extreme(expression, error_message) ((void)0)
#endif

//this is meant as a warning in case the user forgot to set the debug level for a release version
#if PUMPKIN_ASSERT_LEVEL_DEFINITION > 1

static int PumpkinDebugAssertPrintWarningMessage()
{
	printf("c program compiled with expensive pumpkin debug asserts, performance may be significantly slower. See PUMPKIN_ASSERT_LEVEL_DEFINITION in pumpkin_assert.h\n");
	return 0;
}
static int m = PumpkinDebugAssertPrintWarningMessage();

#endif // PUMPKIN_ASSERT_LEVEL_DEFINITION > 1


