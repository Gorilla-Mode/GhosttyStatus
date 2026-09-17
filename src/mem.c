#ifndef UNITY_BUILD
#include "state.c"
#endif

#include <stdio.h>
#include <mach/mach.h>
#include <sys/sysctl.h>

static mem_sample sample_mem(void)
{
    u64 total_bytes = 0;
    size_t size = sizeof(total_bytes);
    if (sysctlbyname("hw.memsize", &total_bytes, &size, nullptr, 0) != 0 ||
        size != sizeof(total_bytes) || total_bytes == 0)
        return (mem_sample){0};

    const host_t host = mach_host_self();
    vm_size_t page_size = 0;
    vm_statistics64_data_t info = {0};
    mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;

    kern_return_t result = host_page_size(host, &page_size);
    if (result == KERN_SUCCESS && page_size > 0)
        result = host_statistics64(host, HOST_VM_INFO64, (host_info64_t)&info, &count);

    mach_port_deallocate(mach_task_self(), host);

    if (result != KERN_SUCCESS || page_size == 0 || count < HOST_VM_INFO64_COUNT)
        return (mem_sample){0};

    // Exclude file cache and purgeable pages; count compressed storage in RAM.
    u64 app_pages = info.internal_page_count > info.purgeable_count
        ? (u64)info.internal_page_count - info.purgeable_count : 0;
    u64 used_pages = app_pages + info.wire_count + (u64)info.compressor_page_count;
    u64 used_bytes = used_pages * page_size;
    if (used_bytes > total_bytes)
        used_bytes = total_bytes;

    return (mem_sample){
        .used_bytes = used_bytes,
        .total_bytes = total_bytes,
        .initialized = 1
    };
}

static component mem_usage(const state* s)
{
    component output = { .header = "Memory", .width = 32 };
    const mem_sample* sample = &s->current_mem;
    int length;

    if (!sample->initialized || sample->total_bytes == 0)
        length = asprintf(&output.text, "--%% --/-- GiB");
    else
    {
        constexpr f64 gib = 1024.0 * 1024.0 * 1024.0;
        length = asprintf(&output.text, "%.1lf%% %.1lf/%.1lf GiB",
            (f64)sample->used_bytes / (f64)sample->total_bytes * 100.0,
            (f64)sample->used_bytes / gib, (f64)sample->total_bytes / gib);
    }

    if (length < 0)
        output.text = nullptr;
    return output;
}
