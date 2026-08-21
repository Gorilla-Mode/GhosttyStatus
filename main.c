#include <stdio.h>
#include <stdint.h>
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

        processor_info_array_t info;
        mach_msg_type_number_t count;
        natural_t cpu_count;

        host_processor_info(
            mach_host_self(),
            PROCESSOR_CPU_LOAD_INFO,
            &cpu_count,
            &info,
            &count);

        u64 user = 0;
        u64 sys  = 0;
        u64 idle = 0;
        u64 nice = 0;

        for (natural_t cpu = 0; cpu < cpu_count; cpu++) {
            user += info[cpu * CPU_STATE_MAX + CPU_STATE_USER];
            sys  += info[cpu * CPU_STATE_MAX + CPU_STATE_SYSTEM];
            idle += info[cpu * CPU_STATE_MAX + CPU_STATE_IDLE];
            nice += info[cpu * CPU_STATE_MAX + CPU_STATE_NICE];
        }

        usleep(1000000);

        processor_info_array_t info2;
        mach_msg_type_number_t count2;

        host_processor_info(
            mach_host_self(),
            PROCESSOR_CPU_LOAD_INFO,
            &cpu_count,
            &info2,
            &count2
        );

        u64 user2 = 0;
        u64 sys2  = 0;
        u64 idle2 = 0;
        u64 nice2 = 0;

        for (natural_t cpu = 0; cpu < cpu_count; cpu++) {
            user2 += info2[cpu * CPU_STATE_MAX + CPU_STATE_USER];
            sys2  += info2[cpu * CPU_STATE_MAX + CPU_STATE_SYSTEM];
            idle2 += info2[cpu * CPU_STATE_MAX + CPU_STATE_IDLE];
            nice2 += info2[cpu * CPU_STATE_MAX + CPU_STATE_NICE];
        }

        u64 delta_user = user2 - user;
        u64 delta_sys = sys2 - sys;
        u64 delta_idle = idle2 - idle;
        u64 delta_nice = nice2 - nice;

        u64 total = delta_user + delta_sys + delta_idle + delta_nice;
        u64 busy = total - delta_idle;

        printf("\rCPU: %.1lf%%\033[K", (double)busy / (double)total * 100.0);
        fflush(stdout);
    }

    tcsetattr(STDIN_FILENO, TCSANOW, &old_termios);
}