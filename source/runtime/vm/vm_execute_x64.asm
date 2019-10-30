;;
;;  vm_execute_x64.asm
;;
;;  Created by Edmund Kapusniak on 28/10/2019.
;;  Copyright © 2019 Edmund Kapusniak.
;;
;;  Licensed under the MIT License. See LICENSE file in the project root for
;;  full license information.
;;

;;
;;  arguments   : rdi, rsi, rdx, rcx, r8, r9 / xmm0-7
;;  callee-saved: rbp, rbx, r12-r15
;;
;;  rax rbx rcx rdx rsi rdi rsp rbp
;;

extern __ZN2kf15vm_active_stackEPNS_10vm_contextEPNS_14vm_stack_frameE

;;
;;  rbp     value* r
;;  r12     op* op
;;  r13     xp
;;  r14     ref_value* k
;;  r15     key_selector* s
;;
;;  rsp+0   vm_stack_frame->function
;;  rsp+8       ->ip    /   ->bp
;;  rsp+16      ->fp    /   ->xp
;;  rsp+24  program
;;  rsp+32  vm_context* vm
;;          saved-r15
;;          saved-r14
;;          saved-r13
;;          saved-r12
;;          saved-rbx
;;          saved-rbp
;;          return address
;;

__ZN2kf10vm_executeEPNS_10vm_contextE:
    push rbp
    push rbx
    push r12
    push r13
    push r14
    push r15
    push rdi
    sub rsp, 40

    mov rsi, rsp
    call __ZN2kf15vm_active_stackEPNS_10vm_contextEPNS_14vm_stack_frameE
    mov rbp, rax

    mov rax, [rsp]          ; stack_frame->function
    mov rax, [rax]          ; function->program
    mov [rsp+24], rax
    mov ecx, [rsp+8]        ; stack_frame->ip
    lea r12, [rax+48+rcx*4] ; program->ops + ip*4
    mov r13d, [rsp+20]      ; stack_frame->xp
    mov r14, [rax]          ; program->constants
    mov r15, [rax+8]        ; program->selectors

    mov r8, [rel .jump_table]

.loop:
    mov eax, [r12]
    add r12, 4
    movzx ecx, al
    movzx edi, ah
    shr eax, 16
    jmp [r8+rcx*8]

.op_mov:
    mov rax, [rbp+rax*8]
    mov [rbp+rdi], rax
    jmp .loop

.op_swp:
    mov rcx, [rbp+rax*8]
    mov rdx, [rbp+rdi*8]
    mov [rbp+rax*8], rdx
    mov [rbp+rdi*8], rcx
    jmp .loop

.op_ldv:
    mov [rbp+rdi*8], rax
    jmp .loop

.op_ldk:
    mov rax, [r14+rax*8]
    mov [rbp+rdi*8], rax
    jmp .loop

.op_len:


.jump_table:
    dq .op_mov
    dq .op_swp
    dq .op_ldv
    dq .op_ldk
    dq .op_len

