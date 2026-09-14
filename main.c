#include <stdio.h>
#include <stdint.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <mach/mach.h>

//sane names
typedef int8_t   i8;
typedef int16_t  i16;
typedef int32_t  i32;
typedef int64_t  i64;
typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

#include <termios.h>

static volatile sig_atomic_t running = 1;

static void stop(int signal_number)
{
    (void)signal_number;
    running = 0;
}

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

static component cpu_usage()
{
    typedef struct
    {
        u64 user;
        u64 sys;
        u64 idle;
        u64 nice;
        int initialized;
    } cpu_sample;

    component output = { .header = "CPU Usage Monitor", .width = 41 };
    if (0 > asprintf(&output.text, "Total: %-6s System: %-6s User: %-6s", "--", "--", "--"))
        output.text = nullptr;

    static cpu_sample previous = {0};
    const host_t host = mach_host_self();
    processor_info_array_t info;
    mach_msg_type_number_t count;
    natural_t cpu_count;

    kern_return_t result = host_processor_info(
        host,
        PROCESSOR_CPU_LOAD_INFO,
        &cpu_count,
        &info,
        &count);

    mach_port_deallocate(mach_task_self(), host);

    if (result != KERN_SUCCESS)
        return output;

    cpu_sample current = { .initialized = 1 };

    for (natural_t cpu = 0; cpu < cpu_count; cpu++)
    {
        current.user += info[cpu * CPU_STATE_MAX + CPU_STATE_USER];
        current.sys  += info[cpu * CPU_STATE_MAX + CPU_STATE_SYSTEM];
        current.idle += info[cpu * CPU_STATE_MAX + CPU_STATE_IDLE];
        current.nice += info[cpu * CPU_STATE_MAX + CPU_STATE_NICE];
    }

    vm_deallocate(mach_task_self(), (vm_address_t)info,
                  (vm_size_t)count * sizeof(*info));

    if (!previous.initialized)
    {
        previous = current;

        return output;
    }

    u64 delta_user = current.user - previous.user;
    u64 delta_sys = current.sys - previous.sys;
    u64 delta_idle = current.idle - previous.idle;
    u64 delta_nice = current.nice - previous.nice;
    previous = current;

    u64 total = delta_user + delta_sys + delta_idle + delta_nice;
    if (total == 0)
        return output;

    u64 busy_total = total - delta_idle;
    u64 busy_user = busy_total - delta_sys;
    u64 busy_sys = busy_total - delta_user;

    char* total_text = nullptr;
    char* system_text = nullptr;
    char* user_text = nullptr;

    if (0 > asprintf(&total_text, "%.1lf%%",
        (double)busy_total / (double)total * 100.0))
        return output;
    if (0 > asprintf(&system_text, "%.1lf%%",
        (double)busy_sys / (double)total * 100.0))
    {
        free(total_text);

        return output;
    }
    if (0 > asprintf(&user_text, "%.1lf%%",
        (double)busy_user / (double)total * 100.0))
    {
        free(total_text);
        free(system_text);

        return output;
    }

    char* text = nullptr;

    int length = asprintf(&text, "Total: %-6s System: %-6s User: %-6s",
        total_text, system_text, user_text);

    free(total_text);
    free(system_text);
    free(user_text);

    if (length < 0)
        return output;

    free(output.text);
    output.text = text;
    return output;
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

    rgb color_background = { .r = 57, .g = 57, .b = 60 };
    rgb color_text = { .r = 20, .g = 20, .b = 20 };

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