.global _start
.extern main
.extern exit

.section .text
_start:
    /* 
     * Initial stack layout provided by the kernel (SysV AMD64):
     * [RSP] = argc
     * [RSP + 8] = argv[0]
     * ...
     * [RSP + 8 + 8*argc] = NULL
     * [RSP + 16 + 8*argc] = envp[0]
     * ...
     * [RSP + 16 + 8*argc + 8*envc] = NULL
     */

    mov %rsp, %rbp      /* Optional: setup frame pointer for debugging */

    /* 1. Extract argc -> RDI */
    mov (%rsp), %rdi    /* RDI = argc */

    /* 2. Extract argv -> RSI */
    lea 8(%rsp), %rsi   /* RSI = &argv[0] */

    /* 3. Extract envp -> RDX */
    /* argv ends at 8 + argc*8. Then there's a NULL pointer (8 bytes). */
    /* envp starts at 16 + argc*8 */
    lea 16(%rsp, %rdi, 8), %rdx /* RDX = &envp[0] */

    /* 4. Align stack to 16 bytes for SysV ABI call conventions */
    /* We must have (RSP % 16) == 0 before the 'call' instruction. */
    and $-16, %rsp

    /* 5. Call main(argc, argv, envp) */
    call main

    /* 6. Call exit(main_return) */
    mov %rax, %rdi
    call exit

    /* 7. Hang if exit somehow returns */
.Lhang:
    jmp .Lhang
