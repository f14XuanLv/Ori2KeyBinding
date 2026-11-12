option casemap:none
EXTERN resolve_version:PROC
.code
proxy0 PROC
    mov r11, 0
    jmp dispatch
proxy0 ENDP
proxy1 PROC
    mov r11, 1
    jmp dispatch
proxy1 ENDP
proxy2 PROC
    mov r11, 2
    jmp dispatch
proxy2 ENDP
proxy3 PROC
    mov r11, 3
    jmp dispatch
proxy3 ENDP
proxy4 PROC
    mov r11, 4
    jmp dispatch
proxy4 ENDP
proxy5 PROC
    mov r11, 5
    jmp dispatch
proxy5 ENDP
proxy6 PROC
    mov r11, 6
    jmp dispatch
proxy6 ENDP
proxy7 PROC
    mov r11, 7
    jmp dispatch
proxy7 ENDP
proxy8 PROC
    mov r11, 8
    jmp dispatch
proxy8 ENDP
proxy9 PROC
    mov r11, 9
    jmp dispatch
proxy9 ENDP
proxy10 PROC
    mov r11, 10
    jmp dispatch
proxy10 ENDP
proxy11 PROC
    mov r11, 11
    jmp dispatch
proxy11 ENDP
proxy12 PROC
    mov r11, 12
    jmp dispatch
proxy12 ENDP
proxy13 PROC
    mov r11, 13
    jmp dispatch
proxy13 ENDP
proxy14 PROC
    mov r11, 14
    jmp dispatch
proxy14 ENDP
proxy15 PROC
    mov r11, 15
    jmp dispatch
proxy15 ENDP
proxy16 PROC
    mov r11, 16
    jmp dispatch
proxy16 ENDP
dispatch PROC
    push rcx
    push rdx
    push r8
    push r9
    sub rsp, 104
    movdqu xmmword ptr [rsp+32], xmm0
    movdqu xmmword ptr [rsp+48], xmm1
    movdqu xmmword ptr [rsp+64], xmm2
    movdqu xmmword ptr [rsp+80], xmm3
    mov rcx, r11
    call resolve_version
    movdqu xmm0, xmmword ptr [rsp+32]
    movdqu xmm1, xmmword ptr [rsp+48]
    movdqu xmm2, xmmword ptr [rsp+64]
    movdqu xmm3, xmmword ptr [rsp+80]
    add rsp, 104
    pop r9
    pop r8
    pop rdx
    pop rcx
    jmp rax
dispatch ENDP
END
