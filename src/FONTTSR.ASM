;     __                           __    __
;    / /   ____ ___  ______  _____/ /_  / /
;   / /   / __ `/ / / / __ \/ ___/ __ \/ / 
;  / /___/ /_/ / /_/ / / / /__/ / / /_/  
; /_____/\__,_/\__,_/_/ /_/\___/_/ /_(_)   
; Launch! for DOS ---------------------
/*
 * MAINTAINER NOTES - Launch! 3.73
 * File: FONTTSR.ASM
 * Role: VGA/EGA font TSR/loader support
 * Build/ownership: Low-level font persistence/loading code.
 * Maintainer contract: Touches video BIOS/VGA state; preserve mode/font-height assumptions.
 * Documentation note: comments describe intent and invariants; behavior remains defined by the code and Release requirements.
 * DOS constraints: code targets 16-bit DOS/MS C 7-era models. Watch DGROUP (<64K in small model), stack use, far/near pointers, BIOS/DOS reentrancy and text-mode screen restoration.
 */
/* Launch! resident VGA/EGA font keeper.
 * Hooks INT 10h mode changes. VGA retains the proven 8x16 path; EGA uses a separate 8x14 branch.
 */
.code16
.arch i8086
.intel_syntax noprefix
.section .text
.global _start

_start:
old_int10:      .word 0, 0
active_id:      .byte 0
/* Stable resident ABI signature; this is not the Launch! release number. */
resident_mark:  .ascii "LAFONT24"
active_height:  .byte 16
active_font:
    .space 4096, 0

resident_load_custom:
    push ax
    push bx
    push cx
    push dx
    push bp
    push es
    push cs
    pop es
    mov bp, offset active_font
    mov ax, 0x1100
    cmp byte ptr cs:[active_height], 14
    je .ega_load
    /* Preserve the long-standing Launch! VGA loader byte-for-byte in
       register semantics: 8x16, block 0, 256 glyphs from character 0. */
    mov bx, 0x1000
    jmp .do_load
.ega_load:
    mov bx, 0x0E00
.do_load:
    mov cx, 256
    xor dx, dx
    pushf
    call far ptr cs:[old_int10]
    cmp byte ptr cs:[active_height], 14
    jne .load_done
    /* EGA can retain a different character-generator block mapping.
       Explicitly select block 0 for both intensity states. */
    mov ax, 0x1103
    xor bx, bx
    pushf
    call far ptr cs:[old_int10]
.load_done:
    pop es
    pop bp
    pop dx
    pop cx
    pop bx
    pop ax
    ret

resident_mode_is_80x25:
    push ax
    push ds
    mov ax, 0x0040
    mov ds, ax
    mov al, byte ptr [0x49]
    cmp al, 2
    je 1f
    cmp al, 3
    je 1f
    cmp al, 7
    jne 3f
1:  cmp word ptr [0x4a], 80
    jne 3f
    cmp byte ptr cs:[active_height], 14
    je 2f
    /* Original VGA persistence test: do not change this path. */
    cmp byte ptr [0x84], 24
    jne 3f
    cmp word ptr [0x85], 16
    jne 3f
    jmp 4f
2:  /* On older EGA BIOSes the rows/font-height BDA extension is not
       consistently maintained.  Mode 2/3/7 + 80 columns is sufficient;
       the 8x14 loader itself restores the selected 25-line EGA font. */
4:  pop ds
    pop ax
    clc
    ret
3:  pop ds
    pop ax
    stc
    ret

int10_handler:
    cmp ah, 0x00
    je int10_mode_set
    jmp far ptr cs:[old_int10]

int10_mode_set:
    pushf
    call far ptr cs:[old_int10]
    pushf
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    push bp
    push ds
    push es
    cmp byte ptr cs:[active_id], 0
    je 1f
    call resident_mode_is_80x25
    jc 1f
    call resident_load_custom
1:  pop es
    pop ds
    pop bp
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    popf
    iret

resident_end:
