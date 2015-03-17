#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include "greencoro.h"

int cid[200000];
uint64_t clk = 0;

char fncall(char x)
{
    return x+1;
}

int coro(void* arg)
{
    while ( 1 )
    {
        if ( rand() % 2 )
        {
            char buf[1024];
            buf[100] = fncall(buf[100]);
            buf[1000] = buf[1001];
            int ret;
            if ( (ret = co_yield()) == CORO_SWCTX_TMOT )
            {
                return -1;
            }
        }
        else
        {
            return 0;
        }
    }
    return 1;
}

int proc()
{
    int cnt = 0;
    while ( 1 )
    {
        int gen = rand() % 1000 + 1;
        while ( gen-- && cnt < 200000 )
        {
            if ( rand() % 2 )
            {
                co_set_clock(++clk);
            }
            cid[cnt] = co_new(coro, NULL);
            if ( cid[cnt] != -1 )
            {
                cnt++;
            }
        }
        int exe = rand() % 1000 + 1;
        while ( exe-- )
        {
            if ( cnt )
            {
                int idx = rand() % cnt;
                int res = co_resume(cid[idx]);
                if ( res == CORO_SWCTX_FAIL || res == CORO_SWCTX_FINI )
                {
                    cid[idx] = cid[cnt-1];
                    --cnt;
                }
            }
        }
    }
}

int main()
{
    if ( co_init(100001, 10000, 1) != 0 )
    {
        return -1;
    }
    proc();
    return 0;
}
