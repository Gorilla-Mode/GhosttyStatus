#define UNITY_BUILD

#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <termios.h>

#include "global_typedefs.c"
#include "ds.c"
#include "state.c"
#include "cpu.c"
#include "bar.c"

static volatile sig_atomic_t running = 1;

static void stop(int signal_number)
{
    (void)signal_number;
    running = 0;
}

static void update_state(state* s)
{
    if (s->current_cpu.initialized)
        s->previous_cpu = s->current_cpu;
    s->current_cpu = sample_cpu();
    append_sample(s);

    s->cpu = cpu_usage(s);
    s->cpu_timeline = cpu_timeline(&s->history);
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

    state s = {0};
    rgb color_background = { .r = 40, .g = 40, .b = 40 };
    rgb color_text = { .r = 103, .g = 103, .b = 99 };

    printf("\033[?25l\033[2J\033[H"); //Hide cursor, clear, move to 1 row
    fflush(stdout);

    while (running)
    {
        update_state(&s);

        status_bar bar;
        if (bar_begin(&bar))
        {
            bar_append(&bar, s.cpu);
            bar_append(&bar, s.cpu_timeline);
            bar_draw(&bar, color_background, color_text);
            bar_end(&bar);
        }

        if (running)
            usleep(1000000);
    }

    f64_list_clear(&s.history);

    tcsetattr(STDIN_FILENO, TCSANOW, &old_termios);
    printf("\033[0m\n");  // Reset text color.
    printf("\033[?25h\n"); // Restore cursor.
    return 0;
}
