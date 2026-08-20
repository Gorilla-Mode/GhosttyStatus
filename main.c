#include <stdio.h>
#include <stdint.h>
#include <unistd.h>

//sane names
typedef int8_t   i8;
typedef int16_t  i16;
typedef int32_t  i32;
typedef int64_t  i64;
typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

#include <stdio.h>
#include <termios.h>

i32 main() {
    struct termios old_termios;
    struct termios new_termios;

    tcgetattr(STDIN_FILENO, &old_termios);

    new_termios = old_termios;
    new_termios.c_lflag &= ~(ICANON | ECHO);

    tcsetattr(STDIN_FILENO, TCSANOW, &new_termios);

    printf("\033[?25l"); //Hide cursor
    printf("\033[2J"); // clear
    fflush(stdout);

    while (true)
    {
        printf("\rCPU: 12%% | RAM: 8.2 GB | 22:35\033[K");
        fflush(stdout);

        sleep(1);
    }

    tcsetattr(STDIN_FILENO, TCSANOW, &old_termios);
}