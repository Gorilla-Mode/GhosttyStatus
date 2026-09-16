#define UNITY_BUILD

#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <termios.h>

#include "global_typedefs.c"
#include "cpu.c"
#include "bar.c"

static volatile sig_atomic_t running = 1;

static void stop(int signal_number)
{
    (void)signal_number;
    running = 0;
}

i32 main()
{
    signal(SIGTERM, stop);
    signal(SIGINT, stop);

    struct termios old_termios;
    struct termios new_termios;

    tcgetattr(STDIN_FILENO, &old_termios);

    new_termios = old_termios;
    new_termios.c_lflag &= ~(ICANON | ECHO);

    tcsetattr(STDIN_FILENO, TCSANOW, &new_termios);

    rgb color_background = { .r = 40, .g = 40, .b = 40 };
    rgb color_text = { .r = 103, .g = 103, .b = 99 };

    printf("\033[?25l\033[2J\033[H"); //Hide cursor, clear, move to 1 row
    fflush(stdout);

    while (running)
    {
        status_bar bar;
        if (bar_begin(&bar))
        {
            component cpu = cpu_usage();

            bar_append(&bar, cpu);
            bar_draw(&bar, color_background, color_text);
            bar_end(&bar);
        }

        if (running)
            usleep(1000000);
    }

    tcsetattr(STDIN_FILENO, TCSANOW, &old_termios);
    printf("\033[0m\n");  // Reset text color.
    printf("\033[?25h\n"); // Restore cursor.
    return 0;
}