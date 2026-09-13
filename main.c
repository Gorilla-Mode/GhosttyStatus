#include <stdio.h>
#include <stdint.h>
#include <signal.h>
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

#include <stdio.h>
#include <termios.h>

static volatile sig_atomic_t running = 1;

static void stop(int signal_number)
{
    (void)signal_number;
    running = 0;
}

static void cpu_usage()
{
    typedef struct
    {
        u64 user;
        u64 sys;
        u64 idle;
        u64 nice;
        int initialized;
    } cpu_sample;

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
    {
        return;
    }

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
        return;
    }

    u64 delta_user = current.user - previous.user;
    u64 delta_sys = current.sys - previous.sys;
    u64 delta_idle = current.idle - previous.idle;
    u64 delta_nice = current.nice - previous.nice;
    previous = current;

    u64 total = delta_user + delta_sys + delta_idle + delta_nice;
    if (total == 0)
        return;

    u64 busy_total = total - delta_idle;
    u64 busy_user = busy_total - delta_sys;
    u64 busy_sys = busy_total - delta_user;

    printf("\rTotal: %.1lf%% System: %.1lf%% User: %.1lf%%\033[K",
        (double)busy_total / (double)total * 100.0,
        (double)busy_sys / (double)total * 100.0,
        (double)busy_user / (double)total * 100.0);

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

    printf("\033[?25l\033[2J\033[H"); //Hide cursor, clear, move to 1 row
    printf("Cpu Usage:\n");
    fflush(stdout);

    while (running)
    {
        cpu_usage();

        if (running)
            usleep(1000000);
    }

    tcsetattr(STDIN_FILENO, TCSANOW, &old_termios);
    printf("\033[?25h\n"); // Restore cursor.
    return 0;
}