#ifndef _REG_DEFINE_H_
#define _REG_DEFINE_H_

#ifdef __x86_64__
    #define REG_RBX 0
    #define REG_RSP 1
    #define REG_RBP 2
    #define REG_R12 3
    #define REG_R13 4
    #define REG_R14 5
    #define REG_R15 6
    #define REG_RIP 7   
    #define REG_CNT 8
    #define SP_INDX 1
#elif __i386__
    #define REG_EBX 0
    #define REG_ESP 1
    #define REG_EBP 2
    #define REG_ESI 3
    #define REG_EDI 4
    #define REG_EIP 5
    #define REG_CNT 6    
    #define SP_INDX 1
#endif

#endif
