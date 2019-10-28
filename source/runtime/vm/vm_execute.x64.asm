

;
;       rax     ret             xmm0    a0/ret
;       rbx     callee          xmm1    a1
;       rcx     a3/ms-a0        xmm2    a2
;       rdx     a2/ms-a1        xmm3    a3
;       rsp                     xmm4    a4/ms
;       rbp     callee          xmm5    a5/ms
;       rsi     a1/ms-callee    xmm6    a6/ms-callee
;       rdi     a0/ms-callee    xmm7    a7/ms-callee
;       r8      a4/ms-a2        xmm8    ms-callee
;       r9      a5/ms-a3        xmm9    ms-callee
;       r10                     xmm10   ms-callee
;       r11                     xmm11   ms-callee
;       r12     callee          xmm12   ms-callee
;       r13     callee          xmm13   ms-callee
;       r14     callee          xmm14   ms-callee
;       r15     callee          xmm15   ms-callee
;

;   xmm1 <- 0xFFFFFFFFFFFFFFFF
;   xmm2 <- -0.0
;   xmm3 <- 0x1p63


;   rbp <- r
;   r11 <- 0x000F'FFFF'FFFF'FFFF
;   r12 <- constants
;   r14 <- ip
;   r15 <- jump_table

vm_execute:

    movsd xmm1, [rel .xor_double]
    movsd xmm2, [rel .neg_double]
    movsd xmm3, [rel .max_double]
    mov r11, -1
    shr r11, 12
    mov r15, [rel .jump_table]

.loop:
    mov eax, [r14]
    add r14, 4
    movzx esi, al
    movzx edi, ah
    shr eax, 16
    jmp [r15+rsi*8]

.mov:
    mov rbx, [rbp+rax*8]
    mov [rbp+rdi*8], rbx
    jmp .loop

.swp:
    mov rbx, [rbp+rax*8]
    mov rcx, [rbp+rdi*8]
    mov [rbp+rax*8], rcx
    mov [rbp+rdi*8], rbx
    jmp .loop

.ldv:
    mov [rbp+rdi*8], rax
    jmp .loop

.ldk:
    mov rbx, [r12+rax*8]
    mov [rbp+rdi*8], rbx
    jmp .loop

.ldj:
    movsx eax, ax
    cvtsi2sd xmm0, eax
    xorpd xmm0, xmm1
    movsd [rbp+rdi*8], xmm0
    jmp .loop

.len:
    mov rax, [rbp+rax*8]

    ; check if rax is an object value
    cmp rax, 3
    jbe .type_error
    cmp rax, r12
    jae .type_error

    ; check type of object
    mov bl, [rax-3]
    cmp bl, 2
    je .len_table_array
    cmp bl, 3
    jne .len_string
    .len_table_array:
        add rax, 8
        jmp .len_write
    .len_string:
        cmp bl, 1
        jne .type_error
    .len_write:
    mov rax, [rax]
    mov [rbp+rdi*8], rax
    jmp .loop

.neg:
    mov rax, [rbp+rax*8]
    cmp rax, r12
    jb .type_error
    mov rbx, 0x8000000000000000
    xor rax, rbx
    mov [rbp+rdi*8], rax
    jmp .loop

.pos:
    mov rax, [rbp+rax*8]
    cmp rax, r12
    jb .type_error
    mov [rbp+rdi*8], rbx
    jmp .loop

.not:
    mov rax, [rbp+rax*8]


.bitnot:
    mov rbx, [rbp+rax*8]
    cmp rbx, r12
    jb .type_error
    movq xmm0, rbx
    xorpd xmm0, xmm1
    andnpd xmm0, xmm2
    ucomisd xmm0, xmm3
    jp .bitnot_unordered
    jae .bitnot_overflow
    cvttsd2si rax, xmm0
    .bitnot_write:
    not eax
    cvtsi2sd xmm0, rax
    movsd [rbp+rdi*8], xmm0
    jmp .loop

    .bitnot_unordered:

    .bitnot_overflow:
    ; TODO
    jmp .loop

.type_error:
    ; TODO
    ret

.jump_table:
    dq .mov
    dq .swp
    dq .ldv
    dq .ldk
    dq .ldj
    dq .len
    dq .neg
    dq .pos
    dq .bitnot
    dq .not

.xor_double:
    dq 0xFFFFFFFFFFFFFFFF
.neg_double:
    dq -0.0
.max_double:
    dq 0x1p63

