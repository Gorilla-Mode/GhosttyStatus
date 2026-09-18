// Keep assertions enabled in Release test builds too.
#undef NDEBUG
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <util.h>

static size_t live_allocations;
static size_t allocation_calls;
static size_t fail_at;
static size_t updates;

static int allocation_fails()
{
    return ++allocation_calls == fail_at;
}

static void* checked_malloc(size_t size)
{
    if (allocation_fails())
        return nullptr;
    void* result = malloc(size);
    assert(result);
    live_allocations++;
    return result;
}

static int checked_asprintf(char** text, const char* format, ...)
{
    if (allocation_fails())
    {
        *text = nullptr;
        return -1;
    }
    va_list args = nullptr;
    va_start(args, format);
    int length = vasprintf(text, format, args);
    va_end(args);
    assert(length >= 0);
    live_allocations++;
    return length;
}

static void checked_free(void* pointer)
{
    if (pointer)
    {
        assert(live_allocations > 0);
        live_allocations--;
    }
    free(pointer);
}

static int next_update(useconds_t delay);

#define malloc checked_malloc
#define asprintf checked_asprintf
#define free checked_free
#define usleep next_update
#define main status_main
#include "../src/main.c"
#undef main
#undef usleep
#undef free
#undef asprintf
#undef malloc

static int next_update(useconds_t delay)
{
    assert(delay == 1000000);
    if (++updates == 25)
        running = 0;
    return 0;
}

static void reset_allocations(size_t failure)
{
    assert(live_allocations == 0);
    allocation_calls = 0;
    fail_at = failure;
}

static state sample_state()
{
    return (state){
        .previous_cpu = { .initialized = 1 },
        .current_cpu = { .user = 25, .sys = 15, .idle = 60, .initialized = 1 },
        .current_mem = { .used_bytes = 8ULL << 30, .total_bytes = 16ULL << 30,
                         .initialized = 1 }
    };
}

static void assert_cleared(component item)
{
    assert(!item.text && !item.header && item.width == 0);
}

static size_t component_cycle(size_t failure, size_t width, int sampled)
{
    reset_allocations(failure);
    state s = sampled ? sample_state() : (state){0};
    append_sample(&s);
    s.cpu = cpu_usage(&s);
    s.mem = mem_usage(&s);
    s.cpu_timeline = cpu_timeline(&s.history);

    if (!failure && sampled)
    {
        assert(strcmp(s.cpu.text, "Total: 40.0%  System: 15.0%  User: 25.0% ") == 0);
        assert(strcmp(s.mem.text, "50.0% 8.0/16.0 GiB") == 0);
        assert(strcmp(s.cpu_timeline.text,
                      "40.0% 0.0% 0.0% 0.0% 0.0% 0.0% 0.0% 0.0% 0.0% 0.0%") == 0);
    }

    status_bar bar;
    if (bar_begin(&bar))
    {
        assert(bar.width >= width);
        bar.width = width;
        bar.header[width] = bar.text[width] = '\0';
        bar_append(&bar, s.cpu);
        bar_append(&bar, s.mem);
        // Timeline deliberately stays hidden, as in the application.
        if (!failure && sampled && width == 80)
        {
            assert(strncmp(bar.header, "CPU", 3) == 0);
            assert(strncmp(bar.header + 41, " | Memory", 9) == 0);
            assert(strncmp(bar.text, s.cpu.text, strlen(s.cpu.text)) == 0);
            assert(strncmp(bar.text + 44, s.mem.text, strlen(s.mem.text)) == 0);
        }
        // Copies must survive releasing their source, including clipped text.
        state_clear_components(&s);
        assert(strlen(bar.header) == width && strlen(bar.text) == width);
        bar_end(&bar);
    }
    state_clear_components(&s);
    assert_cleared(s.cpu);
    assert_cleared(s.mem);
    assert_cleared(s.cpu_timeline);
    assert(s.history.count == (sampled && failure != 1 ? 1 : 0));
    f64_list_clear(&s.history);
    assert(live_allocations == 0);
    return allocation_calls;
}

static void test_history()
{
    reset_allocations(0);
    state s = sample_state();
    for (size_t i = 0; i < 30; i++)
    {
        assert(f64_list_append(&s.history, (f64)i, CPU_HISTORY_LIMIT));
        assert(s.history.count == (i < 10 ? i + 1 : 10));
        assert(s.history.head->value == (i < 10 ? 0 : (f64)i - 9));
        assert(s.history.tail->value == (f64)i);
        s.cpu_timeline = cpu_timeline(&s.history);
        state_clear_components(&s);
        assert(live_allocations == s.history.count);
    }
    fail_at = allocation_calls + 1;
    assert(!f64_list_append(&s.history, 30, CPU_HISTORY_LIMIT));
    assert(s.history.count == 10 && s.history.tail->value == 29);
    assert(!f64_list_append(&s.history, 30, 0));
    f64_list_clear(&s.history);
    f64_list_clear(&s.history);
    assert(!s.history.head && !s.history.tail && s.history.count == 0);
    assert(live_allocations == 0);
}

static void test_main_loop()
{
    // Supply a real terminal so main's terminal setup and restoration run too.
    int master, slave;
    assert(openpty(&master, &slave, nullptr, nullptr, nullptr) == 0);
    int saved_input = dup(STDIN_FILENO);
    int saved_output = dup(STDOUT_FILENO);
    FILE* output = tmpfile();
    assert(saved_input >= 0 && saved_output >= 0 && output);
    assert(dup2(slave, STDIN_FILENO) >= 0);
    assert(dup2(fileno(output), STDOUT_FILENO) >= 0);
    struct termios before, after;
    assert(tcgetattr(STDIN_FILENO, &before) == 0);

    // The first update has 13 formatting allocations, followed by both bar rows.
    // Fail each one in turn, then run enough updates to exercise shutdown cleanup.
    for (size_t failure = 0; failure <= 15; failure++)
    {
        reset_allocations(failure);
        updates = 0;
        running = 1;
        assert(status_main() == 0);
        assert(updates == 25 && live_allocations == 0);
        assert(tcgetattr(STDIN_FILENO, &after) == 0);
        // macOS sets PENDIN when canonical mode is restored; it is kernel state.
        assert((before.c_lflag & ~PENDIN) == (after.c_lflag & ~PENDIN));
        assert(before.c_iflag == after.c_iflag && before.c_oflag == after.c_oflag);
        assert(before.c_cflag == after.c_cflag);
        assert(memcmp(before.c_cc, after.c_cc, sizeof(before.c_cc)) == 0);
        assert(cfgetispeed(&before) == cfgetispeed(&after));
        assert(cfgetospeed(&before) == cfgetospeed(&after));
    }
    fflush(stdout);
    assert(dup2(saved_input, STDIN_FILENO) >= 0);
    assert(dup2(saved_output, STDOUT_FILENO) >= 0);
    close(saved_input);
    close(saved_output);
    close(master);
    close(slave);
    fclose(output);
}

int main()
{
    const size_t widths[] = {0, 1, 41, 42, 44, 45, 80};
    for (int sampled = 0; sampled <= 1; sampled++)
        for (size_t i = 0; i < sizeof(widths) / sizeof(*widths); i++)
        {
            size_t calls = component_cycle(0, widths[i], sampled);
            for (size_t failure = 1; failure <= calls; failure++)
                component_cycle(failure, widths[i], sampled);
        }
    test_history();
    test_main_loop();
    puts("Component ownership, allocation failures, clipping, history, and main loop passed.");
    return 0;
}
