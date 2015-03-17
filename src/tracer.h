#ifndef _TRACER_H_
#define _TRACER_H_

#include <linux/limits.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <libgen.h>

/* tracer keys*/
#define tr_total_memory_use  0
#define tr_concurency_limit  1
#define tr_coro_stack_limit  2
#define tr_idle_coro_number  3
#define tr_wait_coro_number  4
#define tr_max_coro_stk_use  5
#define tr_new_coros_number  6
#define tr_exec_fini_number  7
#define tr_wait_tmot_number  8
#define tr__resource_runout  9
#define tr__stacks_overflow  10
#define TRACER_KEYS_COUNT    11

void _tracer_update_date(int64_t tr_key, int64_t tr_val);

#endif
