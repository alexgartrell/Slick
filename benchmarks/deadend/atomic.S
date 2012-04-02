.globl atomic_add
.globl _atomic_add
.globl atomic_xchg
.globl _atomic_xchg

atomic_add:
_atomic_add:
#if __LP64__
        lock xadd %rsi, (%rdi)
        mov %rsi, %rax
#else        
        movl 0x4(%esp), %edx
        movl 0x8(%esp), %eax
        lock xadd %eax, (%edx)
#endif
        ret

atomic_xchg:
_atomic_xchg:
#if __LP64__
        lock xchg %rsi, (%rdi)
        mov %rsi, %rax
#else        
        movl 0x4(%esp), %edx
        movl 0x8(%esp), %eax
        lock xchg %eax, (%edx)
#endif
        ret
