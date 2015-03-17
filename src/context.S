#include "register.h"

    .file "context.S"
    .text

#if defined(__amd64__) || defined(__x86_64__)

    /* __co_save_context(uint64_t*) */
    .globl __co_save_context
    .type __co_save_context, @function
    .align 16
__co_save_context:
    movq %rbx, (REG_RBX*8)(%rdi)
    movq %rbp, (REG_RBP*8)(%rdi)
    movq %r12, (REG_R12*8)(%rdi)
    movq %r13, (REG_R13*8)(%rdi)
    movq %r14, (REG_R14*8)(%rdi)
    movq %r15, (REG_R15*8)(%rdi)
    leaq 8(%rsp), %rdx
    movq %rdx, (REG_RSP*8)(%rdi)
    movq (%rsp), %rax
    movq %rax, (REG_RIP*8)(%rdi)
    xorq %rax, %rax
    ret
    .size __co_save_context, .-__co_save_context

    /* __co_load_context(uint64_t*, int) */
    .globl __co_load_context
    .type __co_load_context, @function
    .align 16
__co_load_context:
    movq (REG_RBX*8)(%rdi), %rbx
    movq (REG_RBP*8)(%rdi), %rbp
    movq (REG_R12*8)(%rdi), %r12
    movq (REG_R13*8)(%rdi), %r13
    movq (REG_R14*8)(%rdi), %r14
    movq (REG_R15*8)(%rdi), %r15
    test %esi, %esi
    mov $01, %eax
    cmove %eax, %esi
    mov %esi, %eax
    movq (REG_RIP*8)(%rdi), %rdx
    movq (REG_RSP*8)(%rdi), %rsp
    jmpq *%rdx
    .size __co_load_context, .-__co_load_context

#elif defined(__i386__)

    /* __co_save_context(uint32_t*) */
    .globl __co_save_context 
    .type __co_save_context, @function
    .align 16
__co_save_context:
    movl 4(%esp), %eax
    movl %ebx, (REG_EBX*4)(%eax)
    movl %esi, (REG_ESI*4)(%eax)
    movl %edi, (REG_EDI*4)(%eax)
    leal 4(%esp), %ecx
    movl %ecx, (REG_ESP*4)(%eax)
    movl 0(%esp), %ecx
    movl %ecx, (REG_EIP*4)(%eax)
    movl %ebp, (REG_EBP*4)(%eax)
    xorl %eax, %eax
    ret
    .size __co_save_context, .-__co_save_context

    /* __co_load_context(uint32_t*, int) */
    .globl __co_load_context 
    .type __co_load_context, @function
    .align 16
__co_load_context:
     movl 4(%esp), %ecx
     movl 8(%esp), %eax
     movl (REG_EIP*4)(%ecx), %edx
     movl (REG_EBX*4)(%ecx), %ebx
     movl (REG_ESI*4)(%ecx), %esi
     movl (REG_EDI*4)(%ecx), %edi
     movl (REG_EBP*4)(%ecx), %ebp
     movl (REG_ESP*4)(%ecx), %esp
     testl %eax, %eax
     jnz  1f
     incl %eax
1:   jmp *%edx
     .size __co_load_context, .-__co_load_context

#endif
