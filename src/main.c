#define UNITY_BUILD

#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <termios.h>

#include "global_typedefs.c"
#include "cpu.c"

static volatile sig_atomic_t running = 1;

static void stop(int signal_number)
{
    (void)signal_number;
    running = 0;
}

static void bar_end(status_bar* bar)
{
    free(bar->header);
    free(bar->text);
    *bar = (status_bar){0};
}

static int bar_begin(status_bar* bar)
{
    struct winsize size = {0};
    *bar = (status_bar){ .width = 80, .position = 0 };

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &size) == 0 && size.ws_col > 0)
        bar->width = size.ws_col;

    bar->header = malloc(bar->width + 1);
    bar->text = malloc(bar->width + 1);
    if (!bar->header || !bar->text)
    {
        bar_end(bar);
        return 0;
    }

    memset(bar->header, ' ', bar->width);
    memset(bar->text, ' ', bar->width);
    bar->header[bar->width] = '\0';
    bar->text[bar->width] = '\0';
    return 1;
}

static void bar_copy(char* row, size_t position, size_t width, const char* text)
{
    if (!text)
        return;

    size_t length = strlen(text);
    if (length > width)
        length = width;
    memcpy(row + position, text, length);
}

// Appending consumes the allocated text, even when the component is clipped.
static void bar_append(status_bar* bar, component item)
{
    if (item.width > 0 && bar->position < bar->width)
    {
        if (bar->position > 0)
        {
            size_t gap = bar->width - bar->position;
            if (gap > 3)
                gap = 3;
            bar_copy(bar->header, bar->position, gap, " | ");
            bar_copy(bar->text, bar->position, gap, " | ");
            bar->position += gap;
        }

        size_t width = bar->width - bar->position;
        if (width > item.width)
            width = item.width;
        bar_copy(bar->header, bar->position, width, item.header);
        bar_copy(bar->text, bar->position, width, item.text);
        bar->position += width;
    }

    free(item.text);
}

static void bar_draw(const status_bar* bar, rgb color_background, rgb color_text)
{
    printf("\033[38;2;%d;%d;%dm"
           "\033[48;2;%d;%d;%dm"
           "\033[1;1H\r%s"
           "\033[2;1H\r%s"
           "\033[2;1H\r",
           color_text.r, color_text.g, color_text.b,
           color_background.r, color_background.g, color_background.b,
           bar->header, bar->text);
    fflush(stdout);
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
