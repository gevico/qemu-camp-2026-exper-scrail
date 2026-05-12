#pragma once
#include <stdint.h>

// *********** TYPES ******************
typedef uint32_t word_t;
typedef int32_t sword_t;
typedef word_t reg_t;

typedef word_t vaddr_t;



// *********** MACROS ******************
// macro concatenation
#define concat_temp(x, y) x ## y
#define concat(x, y) concat_temp(x, y)
#define concat3(x, y, z) concat(concat(x, y), z)
#define concat4(x, y, z, w) concat3(concat(x, y), z, w)
#define concat5(x, y, z, v, w) concat4(concat(x, y), z, v, w)

// functional-programming-like macro (X-macro)
// apply the function `f` to each element in the contain `c`
// NOTE1: `c` should be defined as a list like:
//   f(a0) f(a1) f(a2) ...
// NOTE2: each element in the contain can be a tuple
#define MAP(c, f) c(f)