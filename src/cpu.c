#ifndef UNITY_BUILD
#include "ds.c"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <mach/mach.h>

#define CPU_HISTORY_LIMIT 10

typedef struct
{
    u64 user;
    u64 sys;
    u64 idle;
    u64 nice;
    int initialized;
} cpu_sample;

typedef struct
{
    u64 user;
    u64 sys;
    u64 idle;
    u64 nice;
}cpu_delta;

typedef f64_list cpu_history;

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

cpu_delta get_cpu_delta(cpu_sample start, cpu_sample end)
{
    cpu_delta delta = {0};

    delta.user = end.user - start.user;
    delta.sys = end.sys - start.sys;
    delta.idle = end.idle - start.idle;
    delta.nice = end.nice - start.nice;

    return delta;
}

static void append_sample(cpu_history* history)
{
    static cpu_sample previous = {0};
    cpu_sample current = sample_cpu();
    if (!current.initialized)
        return;

    if (!previous.initialized)
    {
        previous = current;
    }

    cpu_delta d = get_cpu_delta(previous, current);
    previous = current;

    u64 total = d.user + d.sys + d.idle + d.nice;
    if (total == 0)
        return;

    u64 busy_total = total - d.idle;
    f64 usage = (f64)busy_total / (f64)total * 100.0;

    f64_list_append(history, usage, CPU_HISTORY_LIMIT);
}

static component cpu_timeline(const cpu_history* history)
{
    component output = { .header = "CPU Timeline", .width = 41 };

    if (0 > asprintf(&output.text, "Total: %-6s System: %-6s User: %-6s", "--", "--", "--"))
        output.text = nullptr;

    char* text = nullptr;
    const f64_list_node* node = history->head;

    for (size_t i = 0; i < CPU_HISTORY_LIMIT; i++)
    {
        char* next = nullptr;
        if (0 > asprintf(&next, "%s%s%.1lf%%",
            text ? text : "", i ? " " : "", node ? node->value : 0.0))
        {
            free(text);
            return output;
        }
        free(text);
        text = next;
        if (node)
            node = node->next;
    }

    free(output.text);
    output.text = text;
    return output;
}

static component cpu_usage()
{
    component output = { .header = "CPU", .width = 41 };
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

    cpu_delta d = get_cpu_delta(previous, current);
    previous = current;

    u64 total = d.user + d.sys + d.idle + d.nice;
    if (total == 0)
        return output;

    u64 busy_total = total - d.idle;
    u64 busy_user = busy_total - d.sys;
    u64 busy_sys = busy_total - d.user;

    char* total_text = nullptr;
    char* system_text = nullptr;
    char* user_text = nullptr;

    if (0 > asprintf(&total_text, "%.1lf%%",
        (f64)busy_total / (f64)total * 100.0))
        return output;
    if (0 > asprintf(&system_text, "%.1lf%%",
        (f64)busy_sys / (f64)total * 100.0))
    {
        free(total_text);

        return output;
    }
    if (0 > asprintf(&user_text, "%.1lf%%",
        (f64)busy_user / (f64)total * 100.0))
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