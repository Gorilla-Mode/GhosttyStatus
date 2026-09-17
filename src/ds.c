#ifndef UNITY_BUILD
#include "global_typedefs.c"
#endif

#include <stdlib.h>

typedef struct f64_list_node
{
    f64 value;
    struct f64_list_node* next;
} f64_list_node;

typedef struct
{
    f64_list_node* head;
    f64_list_node* tail;
    size_t count;
} f64_list;

static int f64_list_append(f64_list* fifo, f64 value, size_t limit)
{
    if (limit == 0)
        return 0;

    f64_list_node* node = malloc(sizeof(*node));
    if (!node)
        return 0;

    *node = (f64_list_node){ .value = value };
    if (fifo->tail)
        fifo->tail->next = node;
    else
        fifo->head = node;
    fifo->tail = node;
    fifo->count++;

    while (fifo->count > limit)
    {
        f64_list_node* oldest = fifo->head;
        fifo->head = oldest->next;
        free(oldest);
        fifo->count--;
    }

    return 1;
}

static void f64_list_clear(f64_list* fifo)
{
    while (fifo->head)
    {
        f64_list_node* node = fifo->head;
        fifo->head = node->next;
        free(node);
    }
    *fifo = (f64_list){0};
}
