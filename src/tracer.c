#include "greencoro.h"
#include "tracer.h"

#define MAX_TRACER_DATA_SIZE 512

#define CAL_CUR_VAL 0
#define CAL_MIN_VAL 1
#define CAL_MAX_VAL 2
#define CAL_ADD_VAL 3
#define CAL_MNS_VAL 4


struct tracer_data_type
{
    char*   tr_tag;
    char*   tr_des;
    int64_t tr_val;
    int64_t tr_bas;
    char*   tr_suf; 
    int     tr_way;
    int     tr_urg;    
};
typedef struct tracer_data_type tracer_data_t;

tracer_data_t g_tracer_data[] = {
    {"[S]", "total memory use",          0, 1048576, "M", CAL_CUR_VAL, 1},
    {"[S]", "concurency limit",          0,       1,  "", CAL_CUR_VAL, 1},
    {"[S]", "coro stack limit",          0,    1024, "K", CAL_CUR_VAL, 1},
    {"[D]", "idle coro number",          0,       1,  "", CAL_CUR_VAL, 0},
    {"[D]", "wait coro number",          0,       1,  "", CAL_CUR_VAL, 0},
    {"[D]", "coro max stack use",        0,    1024, "K", CAL_MAX_VAL, 0},
    {"[D]", "created coro number",       0,       1,  "", CAL_ADD_VAL, 0},
    {"[D]", "finished coro number",      0,       1,  "", CAL_ADD_VAL, 0},
    {"[D]", "wait timeout number",       0,       1,  "", CAL_ADD_VAL, 0},
    {"[E]", "coro resource runout",      0,       1,  "", CAL_ADD_VAL, 1},
    {"[E]", "coro stack overflow",       0,       1,  "", CAL_ADD_VAL, 1}
};

char  g_dump_name[PATH_MAX];

void* g_mmap_addr = NULL;

extern uint64_t g_clock_cache;

void _tracer_dump_data(int urg_flag)
{
    static uint64_t last_dump = 0;
    if ( !urg_flag && last_dump + 1000 >= g_clock_cache )
    {
        return;
    }
    last_dump = g_clock_cache;
    char line_buf[MAX_TRACER_DATA_SIZE];
    char data_buf[MAX_TRACER_DATA_SIZE];
    memset(data_buf, 0, sizeof(data_buf));
    strcat(data_buf, g_dump_name);
    int i = 0;
    for ( i = 0; i < TRACER_KEYS_COUNT; ++i )
    {
        snprintf(line_buf, sizeof(line_buf), "%s %-20.20s : %ld%s\n",
            g_tracer_data[i].tr_tag,
            g_tracer_data[i].tr_des, 
            g_tracer_data[i].tr_val / g_tracer_data[i].tr_bas,
            g_tracer_data[i].tr_suf);
        strcat(data_buf, line_buf);
    }
    memcpy(g_mmap_addr, data_buf, MAX_TRACER_DATA_SIZE);
}

void* _tracer_init()
{
    int   tracer_fd;
    void* mem_addr;
    char  full_dir[PATH_MAX];
    char* exe_dir1;
    char* exe_dir2;
    char* dir_name;
    char* bin_name;
    char  mmap_dir[PATH_MAX];
    if ( readlink("/proc/self/exe", full_dir, sizeof(full_dir)) < 0 )
    {
        return NULL;
    }
    exe_dir1 = strdup(full_dir);
    exe_dir2 = strdup(full_dir);
    dir_name = dirname(exe_dir1);
    bin_name = basename(exe_dir2);
    snprintf(g_dump_name, sizeof(g_dump_name), "Corountine Tracing Data\n%s[PID:%d]\n\n", bin_name, getpid());
    snprintf(mmap_dir, sizeof(mmap_dir), "%s/%s_coro_tracing_%d", dir_name, bin_name, getpid());    
    tracer_fd = open(mmap_dir, O_RDWR | O_CREAT | O_TRUNC, 0644);
    if ( tracer_fd < 0 ||
         lseek(tracer_fd, MAX_TRACER_DATA_SIZE - 1, SEEK_SET) == -1 || 
         write(tracer_fd, " ", 1) != 1 )
    {
        return NULL;
    }
    mem_addr = mmap(NULL, sizeof(tracer_data_t), PROT_READ | PROT_WRITE, MAP_SHARED, tracer_fd, 0);
    if ( mem_addr == (void*)MAP_FAILED )
    {
        return NULL;
    }
    memset(mem_addr, 0, MAX_TRACER_DATA_SIZE);
    close(tracer_fd);
    return mem_addr;
}

void _tracer_update_date(int64_t tr_key, int64_t tr_val)
{
    static int try_init = 1;
    if ( try_init ) 
    { 
        try_init = 0;
        g_mmap_addr = _tracer_init(); 
    } 
    if ( g_mmap_addr == NULL ) 
    { 
        return; 
    } 
    switch (g_tracer_data[tr_key].tr_way)
    {
        case CAL_CUR_VAL:
            g_tracer_data[tr_key].tr_val = tr_val;
            break;
        case CAL_MIN_VAL:
            g_tracer_data[tr_key].tr_val = tr_val < g_tracer_data[tr_key].tr_val ? tr_val : g_tracer_data[tr_key].tr_val;
            break;
        case CAL_MAX_VAL:
            g_tracer_data[tr_key].tr_val = tr_val > g_tracer_data[tr_key].tr_val ? tr_val : g_tracer_data[tr_key].tr_val;
            break;
        case CAL_ADD_VAL:
            g_tracer_data[tr_key].tr_val += tr_val;
            break;
        case CAL_MNS_VAL:
            g_tracer_data[tr_key].tr_val -= tr_val;
            break;
        default:
            break;    
    }
    _tracer_dump_data(g_tracer_data[tr_key].tr_urg);
}
