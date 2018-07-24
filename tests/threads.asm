
%define ARRAY_SIZE 25

section .tdata tls
y:  dq  1
x:  dq  1

section .text
global fillarray
global setx

fillarray:
    mov rax, ARRAY_SIZE
    mov rbx, [x]
__fill__:
    mov [rdi], rbx
    add rdi, 8
    dec rax
    jnz __fill__
    ret

setx:
    mov [x], rdi
    ret
