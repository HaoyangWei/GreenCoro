#!/bin/bash

ls ./corotest > /dev/null 2>&1 && rm ./corotest
ls ./corotest_coro_tracing* > /dev/null 2>&1 && rm ./corotest_coro_tracing*
gcc ./corotest.c -o corotest -I../include -I../src -L../lib -static -lgreencoro -lrt
