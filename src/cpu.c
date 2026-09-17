#ifndef UNITY_BUILD
#include "state.c"
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
}cpu_delta;

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

cpu_delta get_cpu_delta(const state* s)
{
    cpu_delta delta = {0};

    if (!s->previous_cpu.initialized || !s->current_cpu.initialized)
        return delta;

    delta.user = s->current_cpu.user - s->previous_cpu.user;
    delta.sys = s->current_cpu.sys - s->previous_cpu.sys;
    delta.idle = s->current_cpu.idle - s->previous_cpu.idle;
    delta.nice = s->current_cpu.nice - s->previous_cpu.nice;

    return delta;
}

static void append_sample(state* s)
{
    cpu_delta d = get_cpu_delta(s);

    u64 total = d.user + d.sys + d.idle + d.nice;
    if (total == 0)
        return;

    u64 busy_total = total - d.idle;
    f64 usage = (f64)busy_total / (f64)total * 100.0;

    f64_list_append(&s->history, usage, CPU_HISTORY_LIMIT);
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

static component cpu_usage(const state* s)
{
    component output = { .header = "CPU", .width = 41 };
    if (0 > asprintf(&output.text, "Total: %-6s System: %-6s User: %-6s", "--", "--", "--"))
        output.text = nullptr;

    cpu_delta d = get_cpu_delta(s);

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