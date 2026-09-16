#ifndef UNITY_BUILD
#include "global_typedefs.c"
#endif

#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdio.h>

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