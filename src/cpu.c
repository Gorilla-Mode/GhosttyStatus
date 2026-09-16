#ifndef UNITY_BUILD
#include "global_typedefs.c"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <mach/mach.h>

typedef struct
{
    u64 user;
    u64 sys;
    u64 idle;
    u64 nice;
    int initialized;
} cpu_sample;

static cpu_sample sample_cpu(void)
{
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
        return (cpu_sample){0};

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

    return current;
}

static component cpu_usage()
{
    component output = { .header = "CPU Usage Monitor", .width = 41 };
    if (0 > asprintf(&output.text, "Total: %-6s System: %-6s User: %-6s", "--", "--", "--"))
        output.text = nullptr;

    static cpu_sample previous = {0};
    cpu_sample current = sample_cpu();
    if (!current.initialized)
        return output;

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
