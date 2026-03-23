#ifndef DEFS_H
#define DEFS_H

#include <stdint.h>
#include <stddef.h>
#include <assert.h>

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t  i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef float  f32;
typedef double f64;

typedef int32_t b32;

#define true 1
#define false 0

#define global_variable static
#define local_persist static
#define internal static

#define Kilobytes(Value) ((Value)*1024LL)
#define Megabytes(Value) (Kilobytes(Value)*1024LL)
#define Gigabytes(Value) (Megabytes(Value)*1024LL)
#define Terabytes(Value) (Gigabytes(Value)*1024LL)

#define ArrayCount(Array) (sizeof(Array) / sizeof((Array)[0]))

#define Assert(Expression) assert(Expression)

#define LONG_PATH 2048

static inline f32 Lerp(f32 A, f32 B, f32 T) {
    return A + (B - A) * T;
}

#endif // DEFS_H
