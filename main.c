#include <stdio.h>
#include <stdint.h>

//sane names
typedef int8_t   i8;
typedef int16_t  i16;
typedef int32_t  i32;
typedef int64_t  i64;
typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

i32 main()
{
    while (true)
    {
        printf("\rCPU: 12%% | RAM: 8.2G | main | 23:41");
        fflush(stdout);
    }
}
