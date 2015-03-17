/*
 * GreenCoro: A simple and high performance asymmetric coroutine
 * 
 * Ontly 6 basic API:
 * co_init()
 * co_new()
 * co_clone()
 * co_resume()
 * co_yield()
 * co_getcid()
 *
 * And additional 6 advanced API:
 * co_set_clock()
 * co_set/get_self_local_data()
 * co_set/get_local_data()
 * co_set_self_start_exit_func()
 * co_set_start_exit_func()
 * 
 * HaoyangWei
 * March 2015
 */
#ifndef _GREEN_CORO_H_
#define _GREEN_CORO_H_

#include <sys/mman.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>
#include "register.h"

#ifndef CORO_STACK_SIZE
#define CORO_STACK_SIZE 4096
#endif

#ifndef CORO_LOCDATA_SIZE
#define CORO_LOCDATA_SIZE 8
#endif

#define CORO_ZONE_MARK 0x123456789ABCDEF0ULL

#define CORO_PROC_CID  0
#define CORO_SELF_CID -1

#define CORO_STATE_NONE -1
#define CORO_STATE_IDLE  0
#define CORO_STATE_RUNN  1
#define CORO_STATE_WAIT  2

#define CORO_SWCTX_FAIL -1 
#define CORO_SWCTX_PROC  1
#define CORO_SWCTX_FINI  2
#define CORO_SWCTX_TMOT  3


#if defined(__amd64__) || defined(__x86_64__)
typedef uint64_t arch_reg_t;
#elif defined(__i386__)
typedef uint32_t arch_reg_t;
#endif 

typedef int  (*co_fun_t)(void*);

typedef void (*co_start_t)();
typedef void (*co_exit_t)(int);

typedef uint64_t red_zone_t; 

typedef uint8_t stk_zone_t[CORO_STACK_SIZE];

#pragma pack(push)
#pragma pack(16)
struct co_dlink_type
{
    struct co_dlink_type* pre_ctx;
    struct co_dlink_type* nxt_ctx;
};
typedef struct co_dlink_type co_dlink_t;

struct co_state_type
{
    int32_t  co_cid;  
    int32_t  co_sta; 
    uint64_t co_exp;
};
typedef struct co_state_type co_state_t;

struct co_exenv_type
{
    arch_reg_t co_reg[REG_CNT];
};
typedef struct co_exenv_type co_exenv_t;

struct co_entry_type
{
    int      co_ret;
    co_fun_t co_fun;
    void*    co_arg;
};
typedef struct co_entry_type co_entry_t;

struct co_sefun_type
{
    co_start_t co_sta;
    co_exit_t  co_ext;
};
typedef struct co_sefun_type co_sefun_t;

struct co_lcdat_type
{
    void* co_dat[CORO_LOCDATA_SIZE];
};
typedef struct co_lcdat_type co_lcdat_t;

struct co_context_type
{
    co_dlink_t co_dlink; // coroutine double link
    co_state_t co_state; // coroutine running state
    co_exenv_t co_exenv; // x86 cpu register
    co_entry_t co_entry; // coroutine startup function
    co_sefun_t co_sefun; // start and exit function
    co_lcdat_t co_lcdat; // coroutine local data
    red_zone_t low_zone; // protection zone 
    stk_zone_t co_stack; // coroutine stack
    red_zone_t hig_zone; // protection zone
};
typedef struct co_context_type co_context_t;
#pragma pack(pop)

extern uint64_t g_clock_cache;
#define co_set_clock(co_clk) g_clock_cache = (co_clk);

int _co_set_local_data(int co_cid, int co_key, void* co_dat);
#define co_set_self_local_data(co_key, co_dat) _co_set_local_data(CORO_SELF_CID, co_key, co_dat)
#define co_set_local_data(co_cid, co_key, co_dat) _co_set_local_data(co_cid, co_key, co_dat)

void* _co_get_local_data(int co_cid, int co_key);
#define co_get_self_local_data(co_key) _co_get_local_data(CORO_SELF_CID, co_key)
#define co_get_local_data(co_cid, co_key) _co_get_local_data(co_cid, co_key)

int _co_set_start_exit_func(int co_cid, co_start_t start_fun, co_exit_t exit_fun);
#define co_set_self_start_exit_func(start_fun, exit_fun) _co_set_start_exit_func(CORO_SELF_CID, start_fun, exit_fun)
#define co_set_start_exit_func(co_cid, start_fun, exit_fun) _co_set_start_exit_func(co_cid, start_fun, exit_fun)

/* co_init()
 * initialize coroutine executing environment 
 * 'max_concur' set the maximal concurency coroutines(stack size per coroutine is defined by macro 'CORO_STACK_SIZE')
 * 'max_waittm' set the maximal wait time if coroutine stay in wait queue, but these timeout coroutines will not be deleted immediately 
 * 'clk_drvmod' set the clock driven mode, 0 use internal clock, and if not 0 you need call 'co_set_clock' to update clock mannually
 * return 0 on success, -1 on error
 * this function should be called in main process
 */
int co_init(int max_concur, int max_waittm, int clk_drvmod);

/* co_new()
 * create a new coroutine
 * 'co_fun' set the startup, it's type is 'int (*co_fun_t)(void*)' 
 * 'co_arg' set the argument when call 'co_fun' 
 * the new created coroutine will not execute immediately, it need you manually call 'co_resume()'
 * return cid of new coroutine, great than 0 on success and -1 on error
 * this function should be called in main process
 */
int co_new(co_fun_t co_fun, void* co_arg);

/* co_clone()
 * clone a new coroutine
 * 'co_cid' is coroutine id which you want to clone, this function simply copy the context of coroutine 
 * same as 'co_new()', the coroutine will not execute immediately
 * return cid of new coroutine, great than 0 on success and -1 on error
 */
int co_clone(int co_cid);

/* co_getcid()
 * get current coroutine id
 * return 0 in main process, and great than 0 in coroutine
 */
int co_getcid();

/* co_resume()
 * resume executing of a coroutine
 * 'co_cid' is coroutine id which you want to resume
 * return value listed follow,
 * CORO_SWCTX_PROC: coroutine is executing 
 * CORO_SWCTX_FINI: coroutine finished
 * CORO_SWCTX_FAIL: coroutine is currently unable to resume for some reason
 * this function should be called in main process
 */
int co_resume(int co_cid);

/* co_yield()
 * coroutine yield the cpu to main process
 * this function should be called in coroutine, but not in main process
 * return value listed follow,
 * CORO_SWCTX_PROC: coroutine is executing 
 * CORO_SWCTX_TMOT: coroutine wait time out, in this situation, coroutine should not call 'co_yield()' any more
 * this function should be called in coroutine
 */
int co_yield();

#endif
