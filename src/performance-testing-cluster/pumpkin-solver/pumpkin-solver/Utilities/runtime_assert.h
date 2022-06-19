#pragma once

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#define runtime_assert(x) if (!(x)) { printf("Error, rip\n"); printf(#x); assert(#x); abort(); }