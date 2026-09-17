#ifndef UNITY_BUILD
#include "ds.c"
#endif

typedef f64_list cpu_history;

typedef struct
{
    component cpu;
    cpu_history history;
    component cpu_timeline;
    cpu_sample previous_cpu;
    cpu_sample current_cpu;
} state;