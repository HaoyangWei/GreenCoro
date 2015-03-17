#include "register.h"
#include "greencoro.h"
#include "tracer.h"


#define CORO_CID(pctx) ((pctx)->co_state.co_cid)

#define CORO_STATE(pctx) ((pctx)->co_state.co_sta)

#define CORO_TMEXP(pctx) ((pctx)->co_state.co_exp)

#define CORO_CID_VALIED(cid) (0 < (cid) && (cid) < g_max_concur)

#define CORO_KEY_VALIED(key) (0 <= (key) && (key) < CORO_LOCDATA_SIZE)

#define CORO_ZONE_CHECK(pctx) ((pctx)->low_zone == CORO_ZONE_MARK && (pctx)->hig_zone == CORO_ZONE_MARK)

#define CORO_IN_PROC_CTX() (CORO_CID(g_co_running) == CORO_PROC_CID)

#define CORO_IN_CORO_CTX() (CORO_CID(g_co_running) != CORO_PROC_CID)


co_context_t* g_coro_ctx = NULL;

int      g_max_concur = 0;
uint64_t g_max_waittm = 0;
int      g_clk_drvmod = 0;

uint64_t g_clock_cache = 0;

co_context_t* g_co_running = NULL;

int g_co_idle_list_size = 0;
co_dlink_t* g_co_idle_list_head = NULL;

int g_co_wait_que_size = 0;
co_dlink_t* g_co_wait_que_head = NULL;
co_dlink_t* g_co_wait_que_tail = NULL;


extern int  __co_save_context(arch_reg_t* co_reg);
#define _co_save_context(pctx) __co_save_context((pctx)->co_exenv.co_reg)

extern void __co_load_context(arch_reg_t* co_reg, int co_rtv);
#define _co_load_context(pctx, rtv) __co_load_context((pctx)->co_exenv.co_reg, rtv)

uint64_t _co_get_clock()
{
    struct timespec tm; 
    clock_gettime(CLOCK_MONOTONIC, &tm);
    return (uint64_t)(tm.tv_sec * 1000 + tm.tv_nsec / 1000000);
}

int _co_set_running(co_context_t* co_ctx)
{
    if ( CORO_STATE(co_ctx) == CORO_STATE_IDLE )
    {
        return -1; 
    } 
    CORO_STATE(co_ctx) = CORO_STATE_RUNN; 
    g_co_running = co_ctx;
    return 0;
}

int _co_set_idle(co_context_t* co_ctx)
{
    if ( CORO_STATE(co_ctx) == CORO_STATE_IDLE )
    {
        return -1;
    }
    CORO_STATE(co_ctx) = CORO_STATE_IDLE;
    co_ctx->co_dlink.nxt_ctx = g_co_idle_list_head;    
    g_co_idle_list_head = &co_ctx->co_dlink;
    ++g_co_idle_list_size;    
    return 0;
}

co_context_t* _co_get_idle()
{
    if ( g_co_idle_list_size == 0 )
    {
        return NULL;
    }
    co_context_t* co_ctx = (co_context_t*)g_co_idle_list_head;
    g_co_idle_list_head = g_co_idle_list_head->nxt_ctx;
    --g_co_idle_list_size;
    return co_ctx;
}

int _co_set_wait(co_context_t* co_ctx)
{
    if ( CORO_STATE(co_ctx) == CORO_STATE_WAIT ) 
    {
        return -1;
    }
    CORO_STATE(co_ctx) = CORO_STATE_WAIT;
    CORO_TMEXP(co_ctx) = g_clock_cache + g_max_waittm; 
    co_ctx->co_dlink.pre_ctx = g_co_wait_que_tail; 
    co_ctx->co_dlink.nxt_ctx = NULL; 
    if ( g_co_wait_que_head == NULL ) 
    { 
        g_co_wait_que_head = &co_ctx->co_dlink; 
    } 
    if ( g_co_wait_que_tail != NULL ) 
    { 
        ((co_context_t*)g_co_wait_que_tail)->co_dlink.nxt_ctx = &co_ctx->co_dlink; 
    } 
    g_co_wait_que_tail = &co_ctx->co_dlink; 
       g_co_wait_que_size += 1; 
    return 0;
}

co_context_t* _co_get_wait(int co_cid)
{
    if ( CORO_STATE(&g_coro_ctx[co_cid]) != CORO_STATE_WAIT )
    {
        return NULL;
    }
    co_context_t* co_ctx = &g_coro_ctx[co_cid];
    if ( g_co_wait_que_head == &co_ctx->co_dlink ) 
    { 
        g_co_wait_que_head = co_ctx->co_dlink.nxt_ctx; 
    } 
    if ( g_co_wait_que_tail == &co_ctx->co_dlink ) 
    {
        g_co_wait_que_tail = co_ctx->co_dlink.pre_ctx; 
    } 
    if ( co_ctx->co_dlink.pre_ctx ) 
    { 
        co_ctx->co_dlink.pre_ctx->nxt_ctx = co_ctx->co_dlink.nxt_ctx; 
    } 
    if ( co_ctx->co_dlink.nxt_ctx ) 
    { 
        co_ctx->co_dlink.nxt_ctx->pre_ctx = co_ctx->co_dlink.pre_ctx; 
    } 
    g_co_wait_que_size -= 1; 
    return &g_coro_ctx[co_cid];
}

void _co_entry_wrapper()
{
    co_context_t* co_ctx = g_co_running;
    if ( co_ctx->co_sefun.co_sta )
    {
        co_ctx->co_sefun.co_sta();
    }
    co_ctx->co_entry.co_ret = co_ctx->co_entry.co_fun(co_ctx->co_entry.co_arg);
    if ( co_ctx->co_sefun.co_ext )
    {
        co_ctx->co_sefun.co_ext(co_ctx->co_entry.co_ret);
    }
#if defined(CORO_ENABLE_TRACE)
    _tracer_update_date(tr_exec_fini_number, 1);
#endif
    _co_set_idle(co_ctx); 
    _co_set_running(&g_coro_ctx[CORO_PROC_CID]);
    _co_load_context(&g_coro_ctx[CORO_PROC_CID], CORO_SWCTX_FINI);
}

void _co_tmout()
{
    while ( g_co_wait_que_size > 0 )
    {
        co_context_t* co_ctx = (co_context_t*)g_co_wait_que_head;
        if ( g_clock_cache < CORO_TMEXP(co_ctx) )
        {
            break;
        }        
#if defined(CORO_ENABLE_TRACE)
        _tracer_update_date(tr_wait_tmot_number, 1);
#endif
        if ( g_co_wait_que_head == g_co_wait_que_tail ) 
        {
            g_co_wait_que_tail = NULL; 
        } 
        g_co_wait_que_head = ((co_context_t*)g_co_wait_que_head)->co_dlink.nxt_ctx;
        if ( g_co_wait_que_head != NULL ) 
        {
            ((co_context_t*)g_co_wait_que_head)->co_dlink.pre_ctx = NULL; 
        } 
        g_co_wait_que_size -= 1; 
        if ( !_co_save_context(&g_coro_ctx[CORO_PROC_CID]) )
        {
            _co_set_running(co_ctx);
            _co_load_context(co_ctx, CORO_SWCTX_TMOT);    
        }
        _co_set_running(&g_coro_ctx[CORO_PROC_CID]);
        if ( CORO_STATE(co_ctx) != CORO_STATE_IDLE )
        {
            _co_set_idle(co_ctx); /* terminate coro by force */
        }
    }
}

void* _co_mem_map(uint32_t mem_size)
{
    int   coro_fd;
    void* mem_addr;
    if ( (coro_fd = open("/dev/zero", O_RDWR, 0)) < 0 )
    {
        return NULL;
    }
    mem_addr = mmap(NULL, mem_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, coro_fd, 0);
    if ( mem_addr == (void*)MAP_FAILED )
    {
        return NULL;
    }
    close(coro_fd);
    return (void*)mem_addr;
}

int co_init(int max_concur, int max_waittm, int clk_drvmod)
{
    if ( g_coro_ctx != NULL ) 
    {
        return -1;
    }
    if ( max_concur <= 0 || max_waittm < 0 )
    {
        return -1;
    }
    g_max_concur = max_concur;
    g_max_waittm = max_waittm;
    g_clk_drvmod = clk_drvmod;
    g_coro_ctx = (co_context_t*)_co_mem_map(g_max_concur * sizeof(co_context_t));
    if ( g_coro_ctx == NULL )
    {
        return -1;
    } 
#if defined(CORO_ENABLE_TRACE)
    _tracer_update_date(tr_total_memory_use, g_max_concur * sizeof(co_context_t));
    _tracer_update_date(tr_concurency_limit, g_max_concur - 1);
    _tracer_update_date(tr_coro_stack_limit, CORO_STACK_SIZE);
#endif
    memset((void*)g_coro_ctx, 0, g_max_concur * sizeof(co_context_t));
    g_co_idle_list_size = 0;
    g_co_idle_list_head = NULL;
    g_co_wait_que_size = 0;
    g_co_wait_que_head = NULL;
    g_co_wait_que_tail = NULL;
    int i = CORO_PROC_CID;
    CORO_CID(&g_coro_ctx[i]) = i;
    CORO_STATE(&g_coro_ctx[i]) = CORO_STATE_NONE; 
    g_coro_ctx[i].low_zone = CORO_ZONE_MARK;
    g_coro_ctx[i].hig_zone = CORO_ZONE_MARK; 
    _co_set_running(&g_coro_ctx[i]);
    for ( i = CORO_PROC_CID + 1; i < g_max_concur; ++i )
    {
        CORO_CID(&g_coro_ctx[i]) = i;
        CORO_STATE(&g_coro_ctx[i]) = CORO_STATE_NONE; 
        g_coro_ctx[i].low_zone = CORO_ZONE_MARK;
        g_coro_ctx[i].hig_zone = CORO_ZONE_MARK; 
        _co_set_idle(&g_coro_ctx[i]);
    }
    return 0;
}

int co_new(co_fun_t co_fun, void* co_arg)
{
    if ( !g_clk_drvmod )
    {
        g_clock_cache = _co_get_clock();
    }
    if ( g_coro_ctx == NULL || !CORO_IN_PROC_CTX() )
    {
        return -1;
    }
    if ( g_co_idle_list_size == 0 )
    {
        _co_tmout();
    }
    co_context_t* co_ctx = _co_get_idle();
    if ( co_ctx == NULL )
    {
#if defined(CORO_ENABLE_TRACE)
        _tracer_update_date(tr__resource_runout, 1);
#endif
        return -1;
    }
    co_ctx->co_entry.co_fun = co_fun;
    co_ctx->co_entry.co_arg = co_arg;
    memset(&co_ctx->co_sefun, 0, sizeof(co_sefun_t));
    memset(&co_ctx->co_lcdat, 0, sizeof(co_lcdat_t));
    co_ctx->low_zone = CORO_ZONE_MARK;
    co_ctx->hig_zone = CORO_ZONE_MARK; 
    if ( _co_save_context(co_ctx) )
    { 
        _co_entry_wrapper();
    } 
    co_ctx->co_exenv.co_reg[SP_INDX] = (arch_reg_t)(&co_ctx->co_stack[CORO_STACK_SIZE]);
    _co_set_wait(co_ctx);
#if defined(CORO_ENABLE_TRACE)
    _tracer_update_date(tr_new_coros_number, 1);
    _tracer_update_date(tr_idle_coro_number, g_co_idle_list_size);
    _tracer_update_date(tr_wait_coro_number, g_co_wait_que_size);
#endif
    return CORO_CID(co_ctx); 
}

int co_clone(int co_cid)
{
    if ( g_coro_ctx ==NULL || !CORO_CID_VALIED(co_cid) )
    {
        return -1;
    }
    if ( CORO_STATE(&g_coro_ctx[co_cid]) != CORO_STATE_WAIT )
    {
        return -1;
    }    
    co_context_t* co_src = &g_coro_ctx[co_cid];
    co_context_t* co_des = _co_get_idle();
    if ( co_des == NULL )
    {
#if defined(CORO_ENABLE_TRACE)
        _tracer_update_date(tr__resource_runout, 1);
#endif
        return -1;
    } 
    int      des_cid = co_des->co_state.co_cid;
    uint64_t rsp_off = co_src->co_exenv.co_reg[SP_INDX] - (arch_reg_t)&co_src->co_stack[0];
    uint32_t cpy_len = (uint32_t)((uint64_t)(&co_src->low_zone) - (uint64_t)co_src);
    memcpy((void*)co_des, (void*)co_src, cpy_len);    
    co_des->co_state.co_cid = des_cid;
    co_des->co_exenv.co_reg[SP_INDX] = rsp_off + (arch_reg_t)&co_des->co_stack[0];
    cpy_len = (uint32_t)((uint64_t)(&co_src->co_stack[CORO_STACK_SIZE]) - co_src->co_exenv.co_reg[SP_INDX]);
    memcpy((void*)co_des->co_exenv.co_reg[SP_INDX], (void*)co_src->co_exenv.co_reg[SP_INDX], cpy_len);
    _co_set_wait(co_des);
#if defined(CORO_ENABLE_TRACE)
    _tracer_update_date(tr_idle_coro_number, g_co_idle_list_size);
    _tracer_update_date(tr_wait_coro_number, g_co_wait_que_size);
#endif
    return CORO_CID(co_des); 
}

int co_getcid()
{
    if ( g_coro_ctx == NULL )
    {
        return -1;
    }
    return CORO_CID(g_co_running);
}

int _co_resume(int co_cid, int co_res)
{
    if ( g_coro_ctx == NULL || !CORO_IN_PROC_CTX() || !CORO_CID_VALIED(co_cid) )
    {
        return CORO_SWCTX_FAIL;
    }
    co_context_t* co_ctx = _co_get_wait(co_cid);
    if ( co_ctx == NULL )
    {
        return CORO_SWCTX_FAIL;
    }
    if ( !CORO_ZONE_CHECK(co_ctx) ) /* stack overflow, main proc terminate coro by force */
    {
#if defined(CORO_ENABLE_TRACE)
        _tracer_update_date(tr__stacks_overflow, 1);
#endif
        CORO_STATE(co_ctx) = CORO_STATE_NONE;
        _co_set_idle(co_ctx);
        return CORO_SWCTX_FAIL;
    }
    int co_ret;
    if ( !(co_ret = _co_save_context(&g_coro_ctx[CORO_PROC_CID])) )
    {
        _co_set_running(co_ctx);
        _co_load_context(co_ctx, co_res);    
    }
#if defined(CORO_ENABLE_TRACE)
    int64_t co_stk_use = (int64_t)((uint64_t)&co_ctx->co_stack[CORO_STACK_SIZE] - co_ctx->co_exenv.co_reg[SP_INDX]);
    _tracer_update_date(tr_max_coro_stk_use, co_stk_use);
#endif
    _co_set_running(&g_coro_ctx[CORO_PROC_CID]);
    if ( co_ret == CORO_SWCTX_FINI )
    {
        return co_ret;
    }
    if ( !CORO_ZONE_CHECK(co_ctx) ) /* stack overflow, main proc terminate coro by force */
    {
#if defined(CORO_ENABLE_TRACE)
        _tracer_update_date(tr__stacks_overflow, 1);
#endif
        CORO_STATE(co_ctx) = CORO_STATE_NONE;
        _co_set_idle(co_ctx);
        return CORO_SWCTX_FAIL;
    }
    _co_set_wait(co_ctx);
    return co_ret;
}

int co_resume(int co_cid)
{
    return _co_resume(co_cid, CORO_SWCTX_PROC);
}

int co_yield()
{
    if ( g_coro_ctx == NULL || !CORO_IN_CORO_CTX() )
    {
        return CORO_SWCTX_FAIL;
    }
    if ( CORO_TMEXP(g_co_running) < g_clock_cache )
    {
        return CORO_SWCTX_TMOT;
    }
    int co_ret;
    if ( !(co_ret = _co_save_context(g_co_running)) )
    {
        _co_load_context(&g_coro_ctx[CORO_PROC_CID], CORO_SWCTX_PROC);
    }
    return co_ret;    
}

int _co_set_local_data(int co_cid, int co_key, void* co_dat)
{
    if ( g_coro_ctx == NULL )
    {
        return -1;
    }
    co_cid = (co_cid == CORO_SELF_CID) ? CORO_CID(g_co_running) : co_cid;
    if ( !CORO_CID_VALIED(co_cid) || !CORO_KEY_VALIED(co_key) )
    {
        return -1;
    }
    g_coro_ctx[co_cid].co_lcdat.co_dat[co_key] = co_dat; 
    return 0;
}

void* _co_get_local_data(int co_cid, int co_key)
{    
    if ( g_coro_ctx == NULL )
    {
        return NULL;
    }
    co_cid = (co_cid == CORO_SELF_CID) ? CORO_CID(g_co_running) : co_cid;
    if ( !CORO_CID_VALIED(co_cid) || !CORO_KEY_VALIED(co_key) )
    {
        return NULL;
    }
    return g_coro_ctx[co_cid].co_lcdat.co_dat[co_key]; 
}

int _co_set_start_exit_func(int co_cid, co_start_t start_fun, co_exit_t exit_fun)
{
    if ( g_coro_ctx == NULL )
    {   
        return -1; 
    }   
    co_cid = (co_cid == CORO_SELF_CID) ? CORO_CID(g_co_running) : co_cid;
    if ( !CORO_CID_VALIED(co_cid) ) 
    {   
       return -1; 
    }   
    g_coro_ctx[co_cid].co_sefun.co_sta = start_fun;
    g_coro_ctx[co_cid].co_sefun.co_ext = exit_fun;
    return 0;
}
