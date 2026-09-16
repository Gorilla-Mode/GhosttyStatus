#include <stdint.h>
#include <stddef.h>

//sane names
typedef int8_t   i8;
typedef int16_t  i16;
typedef int32_t  i32;
typedef int64_t  i64;

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef float f32;
typedef double f64;

typedef struct
{
    u8 r;
    u8 g;
    u8 b;
}rgb;

typedef struct
{
    const char* header;
    char* text;
    size_t width;
} component;

typedef struct
{
    char* header;
    char* text;
    size_t width;
    size_t position;
} status_bar;
