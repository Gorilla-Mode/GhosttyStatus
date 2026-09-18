#ifndef UNITY_BUILD
#include "ds.c"
#endif

#include <stdlib.h>

typedef f64_list cpu_history;

typedef struct
{
    // State owns component text; headers are borrowed string literals.
    component cpu;
    cpu_history history;
    component cpu_timeline;
    cpu_sample previous_cpu;
    cpu_sample current_cpu;
    mem_sample current_mem;
    component mem;
} state;

static void state_clear_components(state* s)
{
    free(s->cpu.text);
    free(s->mem.text);
    free(s->cpu_timeline.text);
    s->cpu = (component){0};
    s->mem = (component){0};
    s->cpu_timeline = (component){0};
}