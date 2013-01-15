/* -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.
* File Name : task.c
* Creation Date : 09-01-2013
* Last Modified : Fri 11 Jan 2013 12:11:36 AM EET
* Created By : Greg Liras <gregliras@gmail.com>
_._._._._._._._._._._._._._._._._._._._._.*/

#include "task.h"

static struct_task task_array[MAX_TASKS];
static task_node task_nodes_array[MAX_TASKS];
static uint32_t free_tasks[MAX_TASKS];
static uint32_t free_tasks_next = 0; /* */

static uint32_t ready_tasks[MAX_TASKS];
static uint32_t ready_tasks_count = 0;


cilk void execute(struct_task *t, int id)
{
    *t->value = (*t->func)(t->args, id);
}

cilk struct_task * set_task(void *func, void *args)
{
    struct_task *t = malloc(sizeof(struct task));
    t -> func = func;
    t -> args = args;
    return t;
}

cilk void run_task(uint32_t id)
{
    struct_task *t = task_array+id;
    t->value = (*t->func)(t->args, id);
}



void initialize(void)
{
    int i;
    for(i = 0; i < MAX_TASKS; i++) {
        free_tasks[i] = i;
        ready_tasks[i] = 0;
    }
}

uint32_t register_task(void *func, void *args)
{
    uint32_t id = -1;
    if(free_tasks_next < MAX_TASKS) {
        id = free_tasks[free_tasks_next];
        struct_task *t = task_array + id;
        t -> func = func;
        t -> args = args;
        free_tasks_next++;
    }
    return id;
}

void del_task(uint32_t id)
{
    struct_task *t = task_array + id;
    /* XXX: should we free the args?
     * How can we check if it's in the heap?
     * free(t -> args);
     */

    if(free_tasks_next > 0) {
        free_tasks_next--;
        free_tasks[free_tasks_next] = id;
    }
}