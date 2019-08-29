#pragma once
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
#include <time.h>
#include <stdarg.h>
#include <io.h>
#include <math.h>

#ifndef __BYTEBOOL__
#define __BYTEBOOL__
//typedef enum {} qboolean;
typedef unsigned char byte;
#endif

#include <algorithm>
#include <atomic>

class qboolean final {
public:
    qboolean() = default;
    qboolean(const qboolean&) = default;
    qboolean(bool b) { m_value = (int)b; }
    qboolean(int i) { m_value = i; }
    operator int() { return m_value; }
private:
    int m_value;
};
static_assert(sizeof(qboolean) == 4, "");
typedef unsigned char byte;

struct vec3_t {
    union {
        struct { float x, y, z; };
        float data[3];
    };
};

typedef float vec_t;
