; hookstub.asm — stubs de hook x64 (MASM) para a sonda do CABAL_Login.
; Cada stub: preserva todos os registradores, chama um notifier C, restaura
; os registradores, e salta de volta ao alvo (cujos bytes o notifier restaurou).

.code
EXTERN probe_notify_disc:PROC
EXTERN probe_notify_tog:PROC
EXTERN probe_notify_enc:PROC
EXTERN probe_notify_chan:PROC       ; Canal 0x8C
EXTERN probe_notify_ew1:PROC        ; EnterWorld 0x8E #1
EXTERN probe_notify_ew2:PROC        ; EnterWorld 0x8E #2
EXTERN probe_notify_ew3:PROC        ; EnterWorld 0x8E #3
EXTERN probe_notify_ew4:PROC        ; EnterWorld 0x8E #4

.data
PUBLIC g_disc_target
g_disc_target QWORD 0
PUBLIC g_tog_target
g_tog_target QWORD 0
PUBLIC g_enc_target
g_enc_target QWORD 0
PUBLIC g_chan_target
g_chan_target QWORD 0
PUBLIC g_ew1_target
g_ew1_target QWORD 0
PUBLIC g_ew2_target
g_ew2_target QWORD 0
PUBLIC g_ew3_target
g_ew3_target QWORD 0
PUBLIC g_ew4_target
g_ew4_target QWORD 0

; --- Hook entry para o disconnect (base+0x356B30) -------------------------
HookEntryDisc PROC
    push rax
    push rcx
    push rdx
    push r8
    push r9
    push r10
    push r11
    push rbx
    push rsi
    push rdi
    push rbp
    mov  rbp, rsp
    sub  rsp, 20h                       ; shadow space (mantem rsp%16==0 no call)
    mov  rcx, qword ptr [rbp+58h]       ; return address original
    mov  rdx, qword ptr [rbp+48h]       ; this original (rcx na entrada)
    lea  r8,  [rbp+60h]                 ; entry_rsp
    mov  r9,  qword ptr [rbp+0h]        ; entry_rbp
    call probe_notify_disc
    add  rsp, 20h
    pop  rbp
    pop  rdi
    pop  rsi
    pop  rbx
    pop  r11
    pop  r10
    pop  r9
    pop  r8
    pop  rdx
    pop  rcx
    pop  rax
    mov  rax, qword ptr [g_disc_target]
    jmp  rax
HookEntryDisc ENDP

; --- Hook entry para o toggle_ui (base+0x549FD8) ---------------------------
HookEntryTog PROC
    push rax
    push rcx
    push rdx
    push r8
    push r9
    push r10
    push r11
    push rbx
    push rsi
    push rdi
    push rbp
    mov  rbp, rsp
    sub  rsp, 20h
    mov  rcx, qword ptr [rbp+58h]
    mov  rdx, qword ptr [rbp+48h]
    lea  r8,  [rbp+60h]
    mov  r9,  qword ptr [rbp+0h]
    call probe_notify_tog
    add  rsp, 20h
    pop  rbp
    pop  rdi
    pop  rsi
    pop  rbx
    pop  r11
    pop  r10
    pop  r9
    pop  r8
    pop  rdx
    pop  rcx
    pop  rax
    mov  rax, qword ptr [g_tog_target]
    jmp  rax
HookEntryTog ENDP

; --- Hook entry para o ENCRYPT (base+0x389698) -----------------------------
HookEntryEnc PROC
    push rax
    push rcx
    push rdx
    push r8
    push r9
    push r10
    push r11
    push rbx
    push rsi
    push rdi
    push rbp
    mov  rbp, rsp
    sub  rsp, 20h
    mov  rcx, qword ptr [rbp+58h]
    mov  rdx, qword ptr [rbp+48h]
    lea  r8,  [rbp+60h]
    mov  r9,  qword ptr [rbp+0h]
    call probe_notify_enc
    add  rsp, 20h
    pop  rbp
    pop  rdi
    pop  rsi
    pop  rbx
    pop  r11
    pop  r10
    pop  r9
    pop  r8
    pop  rdx
    pop  rcx
    pop  rax
    mov  rax, qword ptr [g_enc_target]
    jmp  rax
HookEntryEnc ENDP

; --- Hook entry para o CANAL 0x8C (base+0xA01B34) -------------------------
HookEntryChan PROC
    push rax
    push rcx
    push rdx
    push r8
    push r9
    push r10
    push r11
    push rbx
    push rsi
    push rdi
    push rbp
    mov  rbp, rsp
    sub  rsp, 20h
    mov  rcx, qword ptr [rbp+58h]
    mov  rdx, qword ptr [rbp+48h]
    lea  r8,  [rbp+60h]
    mov  r9,  qword ptr [rbp+0h]
    call probe_notify_chan
    add  rsp, 20h
    pop  rbp
    pop  rdi
    pop  rsi
    pop  rbx
    pop  r11
    pop  r10
    pop  r9
    pop  r8
    pop  rdx
    pop  rcx
    pop  rax
    mov  rax, qword ptr [g_chan_target]
    jmp  rax
HookEntryChan ENDP

; --- Hook entry para ENTERWORLD 0x8E #1 (base+0x5643A3) -------------------
HookEntryEW1 PROC
    push rax
    push rcx
    push rdx
    push r8
    push r9
    push r10
    push r11
    push rbx
    push rsi
    push rdi
    push rbp
    mov  rbp, rsp
    sub  rsp, 20h
    mov  rcx, qword ptr [rbp+58h]
    mov  rdx, qword ptr [rbp+48h]
    lea  r8,  [rbp+60h]
    mov  r9,  qword ptr [rbp+0h]
    call probe_notify_ew1
    add  rsp, 20h
    pop  rbp
    pop  rdi
    pop  rsi
    pop  rbx
    pop  r11
    pop  r10
    pop  r9
    pop  r8
    pop  rdx
    pop  rcx
    pop  rax
    mov  rax, qword ptr [g_ew1_target]
    jmp  rax
HookEntryEW1 ENDP

; --- Hook entry para ENTERWORLD 0x8E #2 (base+0x7222B3) -------------------
HookEntryEW2 PROC
    push rax
    push rcx
    push rdx
    push r8
    push r9
    push r10
    push r11
    push rbx
    push rsi
    push rdi
    push rbp
    mov  rbp, rsp
    sub  rsp, 20h
    mov  rcx, qword ptr [rbp+58h]
    mov  rdx, qword ptr [rbp+48h]
    lea  r8,  [rbp+60h]
    mov  r9,  qword ptr [rbp+0h]
    call probe_notify_ew2
    add  rsp, 20h
    pop  rbp
    pop  rdi
    pop  rsi
    pop  rbx
    pop  r11
    pop  r10
    pop  r9
    pop  r8
    pop  rdx
    pop  rcx
    pop  rax
    mov  rax, qword ptr [g_ew2_target]
    jmp  rax
HookEntryEW2 ENDP

; --- Hook entry para ENTERWORLD 0x8E #3 (base+0x80A0B2) -------------------
HookEntryEW3 PROC
    push rax
    push rcx
    push rdx
    push r8
    push r9
    push r10
    push r11
    push rbx
    push rsi
    push rdi
    push rbp
    mov  rbp, rsp
    sub  rsp, 20h
    mov  rcx, qword ptr [rbp+58h]
    mov  rdx, qword ptr [rbp+48h]
    lea  r8,  [rbp+60h]
    mov  r9,  qword ptr [rbp+0h]
    call probe_notify_ew3
    add  rsp, 20h
    pop  rbp
    pop  rdi
    pop  rsi
    pop  rbx
    pop  r11
    pop  r10
    pop  r9
    pop  r8
    pop  rdx
    pop  rcx
    pop  rax
    mov  rax, qword ptr [g_ew3_target]
    jmp  rax
HookEntryEW3 ENDP

; --- Hook entry para ENTERWORLD 0x8E #4 (base+0xAC7267) -------------------
HookEntryEW4 PROC
    push rax
    push rcx
    push rdx
    push r8
    push r9
    push r10
    push r11
    push rbx
    push rsi
    push rdi
    push rbp
    mov  rbp, rsp
    sub  rsp, 20h
    mov  rcx, qword ptr [rbp+58h]
    mov  rdx, qword ptr [rbp+48h]
    lea  r8,  [rbp+60h]
    mov  r9,  qword ptr [rbp+0h]
    call probe_notify_ew4
    add  rsp, 20h
    pop  rbp
    pop  rdi
    pop  rsi
    pop  rbx
    pop  r11
    pop  r10
    pop  r9
    pop  r8
    pop  rdx
    pop  rcx
    pop  rax
    mov  rax, qword ptr [g_ew4_target]
    jmp  rax
HookEntryEW4 ENDP

END