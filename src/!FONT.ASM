#     __                           __    __
#    / /   ____ ___  ______  _____/ /_  / /
#   / /   / __ `/ / / / __ \/ ___/ __ \/ /
#  / /___/ /_/ / /_/ / / / /__/ / / /_/
# /_____/\__,_/\__,_/_/ /_/\___/_/ /_(_)
# Launch! for DOS ---------------------
#
# !FONT.COM - self-resident Launch! VGA/EGA font persistence service.
#
# Unlike the old embedded LAFONT block, this program becomes resident from
# its own PSP.  It therefore belongs with the normal startup TSRs and never
# carves an independent MCB out of !.EXE's transient arena.
#
# Build on a modern GNU binutils host:
#   as --32 !FONT.ASM -o !FONT.O
#   ld -m elf_i386 -Ttext 0x100 --oformat binary --entry=_start \
#      -o !FONT.COM !FONT.O
#
# Runtime API (INT 2Fh):
#   AX=D5C0h  query
#     returns BX=464Eh ("FN"), CX=0100h, ES=resident PSP segment,
#             DH=active font id (0 = inactive), DL=font height.
#   AX=D5C1h  set custom font
#     BX=464Eh, DS:SI=font bytes, CX=3584 or 4096,
#     DH=font id, DL=14 or 16.  Returns AL=0 success, AL=1 invalid.
#   AX=D5C3h  deactivate custom persistence
#     BX=464Eh. Returns AL=0.
#   AX=D5C4h  private cooperative chain-info query
#     returns BX=4643h ("FC"), ES=resident PSP, SI=INT 2F handler offset,
#             DI=resident old-INT-2F pointer field offset.
#
# Command line:
#   !FONT             install resident service and apply FONT.CUR when present
#   !FONT /U          unload, when INT 10h and INT 2Fh are still ours
#   !FONT /UNLOAD     same as /U
#   !FONT /?          help

.code16
.arch i8086
.intel_syntax noprefix
.section .text
.global _start

.equ API_QUERY,   0xD5C0
.equ API_SET,     0xD5C1
.equ API_DISABLE,   0xD5C3
.equ API_CHAININFO, 0xD5C4
.equ API_MAGIC,     0x464E
.equ API_CHAIN_MAGIC, 0x4643            /* "FC": cooperative INT 2F chain info */
.equ API_VERSION,   0x0100

_start:
    jmp installer

/* -------------------------- resident portion -------------------------- */

old_int10:      .word 0,0
old_int2f:      .word 0,0
active_id:      .byte 0
active_height:  .byte 16
resident_mark:  .ascii "!FONT100"

resident_load_custom:
    push ax
    push bx
    push cx
    push dx
    push bp
    push si
    push di
    push ds
    push es
    push cs
    pop es
    mov bp,offset active_font
    mov ax,0x1100
    cmp byte ptr cs:[active_height],14
    je load_ega
    mov bx,0x1000
    jmp do_load
load_ega:
    mov bx,0x0E00
do_load:
    mov cx,256
    xor dx,dx
    pushf
    call far ptr cs:[old_int10]
    cmp byte ptr cs:[active_height],14
    jne load_done
    mov ax,0x1103
    xor bx,bx
    pushf
    call far ptr cs:[old_int10]
load_done:
    pop es
    pop ds
    pop di
    pop si
    pop bp
    pop dx
    pop cx
    pop bx
    pop ax
    ret

resident_mode_is_text25:
    push ax
    push ds
    mov ax,0x0040
    mov ds,ax
    mov al,byte ptr [0x49]
    cmp al,0
    je mode_columns
    cmp al,1
    je mode_columns
    cmp al,2
    je mode_columns
    cmp al,3
    je mode_columns
    cmp al,7
    jne mode_bad
mode_columns:
    cmp word ptr [0x4a],40
    je mode_rows
    cmp word ptr [0x4a],80
    jne mode_bad
mode_rows:
    cmp byte ptr cs:[active_height],14
    je mode_good
    /* VGA BIOS data area reports 25 rows as 24 and 16 scanlines. */
    cmp byte ptr [0x84],24
    jne mode_bad
    cmp word ptr [0x85],16
    jne mode_bad
mode_good:
    pop ds
    pop ax
    clc
    ret
mode_bad:
    pop ds
    pop ax
    stc
    ret

int10_handler:
    cmp ah,0x00
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
    cmp byte ptr cs:[active_id],0
    je mode_reload_done
    call resident_mode_is_text25
    jc mode_reload_done
    call resident_load_custom
mode_reload_done:
    pop es
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

int2f_handler:
    cmp ax,API_QUERY
    jne api_set_test
    mov bx,API_MAGIC
    mov cx,API_VERSION
    mov dl,byte ptr cs:[active_height]
    mov dh,byte ptr cs:[active_id]
    push cs
    pop es
    iret

api_set_test:
    cmp ax,API_SET
    jne api_disable_test
    cmp bx,API_MAGIC
    jne api_invalid
    cmp dl,14
    je api_expect_ega
    cmp dl,16
    jne api_invalid
    cmp cx,4096
    jne api_invalid
    jmp api_copy
api_expect_ega:
    cmp cx,3584
    jne api_invalid
api_copy:
    push bx
    push cx
    push dx
    push si
    push di
    push es
    push cs
    pop es
    mov di,offset active_font
    cld
    rep movsb
    pop es
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    mov byte ptr cs:[active_height],dl
    mov byte ptr cs:[active_id],dh
    cmp dh,0
    je api_set_ok
    call resident_load_custom
api_set_ok:
    xor al,al
    iret

api_disable_test:
    cmp ax,API_DISABLE
    jne api_chaininfo_test
    cmp bx,API_MAGIC
    jne api_invalid
    mov byte ptr cs:[active_id],0
    xor al,al
    iret

/* Private cooperative-unload API.  It exposes only the resident INT 2F
   handler and its saved previous-link field, allowing !KEY to splice !FONT
   out of the multiplex chain (or vice versa) without unloading/reloading the
   other TSR and fragmenting conventional memory. */
api_chaininfo_test:
    cmp ax,API_CHAININFO
    jne api_chain
    mov bx,API_CHAIN_MAGIC
    mov cx,API_VERSION
    push cs
    pop es
    mov si,offset int2f_handler
    mov di,offset old_int2f
    mov bp,offset resident_prepare_unload
    mov dx,offset resident_reload_name
    iret

api_invalid:
    mov al,1
    iret

api_chain:
    jmp far ptr cs:[old_int2f]

/* Called by the other Launch! TSR's transient unloader only after it has
   verified that !FONT is the immediate INT 2F owner.  Restore our vectors
   but do not free our PSP; the caller does that after this far call returns. */
resident_prepare_unload:
    push bx
    push cx
    push dx
    push ds
    push es
    mov ax,0x3510
    int 0x21
    mov ax,es
    push cs
    pop dx
    cmp ax,dx
    jne resident_prepare_fail
    cmp bx,offset int10_handler
    jne resident_prepare_fail
    mov ax,0x352f
    int 0x21
    mov ax,es
    push cs
    pop dx
    cmp ax,dx
    jne resident_prepare_fail
    cmp bx,offset int2f_handler
    jne resident_prepare_fail
    /* INT 10 */
    mov dx,word ptr cs:[old_int10]
    mov ax,word ptr cs:[old_int10+2]
    push ds
    mov ds,ax
    mov ax,0x2510
    int 0x21
    pop ds
    /* INT 2F */
    mov dx,word ptr cs:[old_int2f]
    mov ax,word ptr cs:[old_int2f+2]
    push ds
    mov ds,ax
    mov ax,0x252f
    int 0x21
    pop ds
    xor al,al
    jmp resident_prepare_done
resident_prepare_fail:
    mov al,1
resident_prepare_done:
    pop es
    pop ds
    pop dx
    pop cx
    pop bx
    retf

resident_reload_name: .asciz "!FONT"

active_font:
    .space 4096,0

resident_end:

/* ------------------------- transient installer ------------------------ */

installer:
    push cs
    pop ds
    call parse_command
    cmp al,1
    je show_help
    cmp al,2
    je unload_font
    cmp al,3
    je syntax_error

    mov ax,API_QUERY
    xor bx,bx
    int 0x2f
    cmp bx,API_MAGIC
    je already_installed

    /* FONT.CUR is written by !.EXE when Persist is enabled.  Resolve it
       beside this COM program so AUTOEXEC.BAT need not change directory. */
    call build_cache_path
    call load_cache

    mov ax,0x3510
    int 0x21
    mov word ptr [old_int10],bx
    mov word ptr [old_int10+2],es
    mov ax,0x352f
    int 0x21
    mov word ptr [old_int2f],bx
    mov word ptr [old_int2f+2],es

    push ds
    push cs
    pop ds
    mov dx,offset int10_handler
    mov ax,0x2510
    int 0x21
    mov dx,offset int2f_handler
    mov ax,0x252f
    int 0x21
    pop ds

    cmp byte ptr [active_id],0
    je install_font_done
    call resident_load_custom
install_font_done:
    call show_font_message

    /* A TSR does not need a private copy of its launch environment. */
    mov es,word ptr cs:[0x2c]
    mov ax,es
    or ax,ax
    jz stay_resident
    mov ah,0x49
    int 0x21
stay_resident:
    mov dx,offset resident_end+15
    mov cl,4
    shr dx,cl
    mov ax,0x3100
    int 0x21

already_installed:
    mov dx,offset msg_already
    jmp print_ok

unload_font:
    mov ax,API_QUERY
    xor bx,bx
    int 0x2f
    cmp bx,API_MAGIC
    jne not_installed
    cmp cx,API_VERSION
    jne unload_unsafe
    mov word ptr [resident_seg],es

    mov ax,0x3510
    int 0x21
    mov ax,es
    cmp ax,word ptr [resident_seg]
    jne unload_unsafe
    cmp bx,offset int10_handler
    jne unload_unsafe

    /* INT 2Fh may legitimately have !KEY immediately above !FONT.  Cycle
       that Launch! TSR cleanly: prepare/free it now, remove !FONT, then queue
       its own executable name for COMMAND.COM to reload after this transient
       unloader exits.  This preserves a contiguous top-of-memory TSR layout. */
    mov byte ptr [reload_needed],0
    mov ax,0x352f
    int 0x21
    mov ax,es
    cmp ax,word ptr [resident_seg]
    jne unload_try_key_above
    cmp bx,offset int2f_handler
    je unload_int2f_ready
unload_try_key_above:
    mov word ptr [top2f_off],bx
    mov word ptr [top2f_seg],es
    mov ax,0xd5b5
    xor bx,bx
    int 0x2f
    cmp bx,0x4b43
    jne unload_unsafe
    cmp cx,0x030b
    jne unload_unsafe
    mov ax,es
    cmp ax,word ptr [top2f_seg]
    jne unload_unsafe
    cmp si,word ptr [top2f_off]
    jne unload_unsafe
    cmp word ptr es:[di],offset int2f_handler
    jne unload_unsafe
    mov ax,word ptr [resident_seg]
    cmp word ptr es:[di+2],ax
    jne unload_unsafe
    mov word ptr [other_resident],es
    mov word ptr [other_prepare],bp
    mov word ptr [other_prepare+2],es
    call copy_reload_from_es_dx
    lcall [other_prepare]
    or al,al
    jnz unload_unsafe
    mov es,word ptr [other_resident]
    mov ah,0x49
    int 0x21
    jc unload_unsafe
    mov byte ptr [reload_needed],1
    /* The other TSR restored its previous INT 2F link, so !FONT must now
       be the current owner before we touch our own resident block. */
    mov ax,0x352f
    int 0x21
    mov ax,es
    cmp ax,word ptr [resident_seg]
    jne unload_unsafe
    cmp bx,offset int2f_handler
    jne unload_unsafe
unload_int2f_ready:

    /* Restore INT 10h from resident storage. */
    mov es,word ptr [resident_seg]
    mov dx,word ptr es:[old_int10]
    mov ax,word ptr es:[old_int10+2]
    push ds
    mov ds,ax
    mov ax,0x2510
    int 0x21
    pop ds

    /* Restore INT 2Fh from resident storage. */
    mov es,word ptr [resident_seg]
    mov dx,word ptr es:[old_int2f]
    mov ax,word ptr es:[old_int2f+2]
    push ds
    mov ds,ax
    mov ax,0x252f
    int 0x21
    pop ds

    mov es,word ptr [resident_seg]
    mov ah,0x49
    int 0x21
    jc unload_unsafe
    cmp byte ptr [reload_needed],0
    je unload_no_reload
    call queue_reload_command
    jc unload_reload_failed
unload_no_reload:
    mov dx,offset msg_unloaded
    jmp print_ok
unload_reload_failed:
    mov dx,offset msg_reload_failed
    jmp print_error

not_installed:
    mov dx,offset msg_not_installed
    jmp print_error
unload_unsafe:
    mov dx,offset msg_unload_unsafe
    jmp print_error
show_help:
    mov dx,offset msg_help
    jmp print_ok
syntax_error:
    mov dx,offset msg_syntax
    jmp print_error

print_ok:
    mov ah,9
    int 0x21
    mov ax,0x4c00
    int 0x21
print_error:
    mov ah,9
    int 0x21
    mov ax,0x4c01
    int 0x21

/* Resolve FONT.CUR beside the executable using the DOS 3+ environment
   trailer.  If no executable path is available, fall back to the current
   directory.  This code is transient and is not retained by AH=31h. */
build_cache_path:
    push ax
    push bx
    push dx
    push bp
    push si
    push di
    push es

    mov si,offset cache_leaf
    mov di,offset cache_path
cache_default_copy:
    lodsb
    mov byte ptr [di],al
    inc di
    or al,al
    jne cache_default_copy

    mov ax,word ptr cs:[0x2c]
    or ax,ax
    jz cache_path_done
    mov es,ax
    xor si,si
cache_find_env_end:
    cmp byte ptr es:[si],0
    jne cache_env_next
    cmp byte ptr es:[si+1],0
    je cache_env_found
cache_env_next:
    inc si
    cmp si,0xfff0
    jb cache_find_env_end
    jmp cache_path_done
cache_env_found:
    add si,4                       /* double NUL + DOS path-count word */
    cmp byte ptr es:[si],0
    je cache_path_done
    mov di,offset cache_path
    xor bp,bp
cache_copy_exe:
    mov al,byte ptr es:[si]
    or al,al
    jz cache_exe_done
    mov byte ptr [di],al
    cmp al,'\\'
    je cache_mark_separator
    cmp al,'/'
    je cache_mark_separator
    cmp al,':'
    jne cache_not_separator
cache_mark_separator:
    mov bp,di
cache_not_separator:
    inc si
    inc di
    cmp di,offset cache_path+118
    jb cache_copy_exe
cache_exe_done:
    or bp,bp
    jnz cache_have_separator
    mov bp,offset cache_path-1
cache_have_separator:
    inc bp
    mov di,bp
    mov si,offset cache_leaf
cache_leaf_copy:
    lodsb
    mov byte ptr [di],al
    inc di
    or al,al
    jne cache_leaf_copy
cache_path_done:
    pop es
    pop di
    pop si
    pop bp
    pop dx
    pop bx
    pop ax
    ret

/* Copy the other TSR's resident ASCIIZ reload command from ES:DX. */
copy_reload_from_es_dx:
    push ax
    push cx
    push si
    push di
    push ds
    push es
    mov si,dx
    push es
    pop ds
    push cs
    pop es
    mov di,offset reload_command
    mov cx,15
copy_reload_loop:
    lodsb
    stosb
    or al,al
    jz copy_reload_done
    loop copy_reload_loop
    mov byte ptr es:[di-1],0
copy_reload_done:
    pop es
    pop ds
    pop di
    pop si
    pop cx
    pop ax
    ret

/* Queue a short command in the BIOS keyboard ring.  COMMAND.COM consumes it
   only after this transient process exits, so the reloaded TSR occupies the
   newly coalesced top-of-memory block rather than leaving a hole. */
queue_reload_command:
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    push es
    mov si,offset reload_command
    xor cx,cx
queue_count_loop:
    cmp byte ptr [si],0
    je queue_count_done
    inc si
    inc cx
    jmp queue_count_loop
queue_count_done:
    inc cx                              /* trailing Return */
    mov ax,0x40
    mov es,ax
    mov bx,word ptr es:[0x1c]
    mov dx,word ptr es:[0x1a]
    mov di,bx
queue_space_loop:
    add di,2
    cmp di,0x3e
    jb queue_space_nowrap
    mov di,0x1e
queue_space_nowrap:
    cmp di,dx
    je queue_reload_fail
    loop queue_space_loop
    cli
    mov bx,word ptr es:[0x1c]
    mov si,offset reload_command
queue_write_loop:
    mov al,byte ptr [si]
    or al,al
    jz queue_write_return
    xor ah,ah
    mov word ptr es:[bx],ax
    add bx,2
    cmp bx,0x3e
    jb queue_write_next
    mov bx,0x1e
queue_write_next:
    inc si
    jmp queue_write_loop
queue_write_return:
    mov word ptr es:[bx],0x1c0d
    add bx,2
    cmp bx,0x3e
    jb queue_store_tail
    mov bx,0x1e
queue_store_tail:
    mov word ptr es:[0x1c],bx
    sti
    clc
    jmp queue_reload_done
queue_reload_fail:
    stc
queue_reload_done:
    pop es
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    ret

/* Load the cache prepared by !.EXE.  Invalid/missing cache simply leaves the
   service resident but inactive.  Format:
     0..7  "LFCUR100"
     8     height (14/16)
     9     Launch font id (0 = inactive/Standard)
     10..11 byte count (0, 3584 or 4096)
     12.. font bytes
 */
load_cache:
    push ax
    push bx
    push cx
    push dx
    push si
    mov byte ptr [display_name],0
    mov byte ptr [display_name+31],0
    mov dx,offset cache_path
    mov ax,0x3d00
    int 0x21
    jc load_cache_done
    mov bx,ax
    mov dx,offset cache_header
    mov cx,12
    mov ah,0x3f
    int 0x21
    jc load_cache_close
    cmp ax,12
    jne load_cache_close
    cmp word ptr [cache_header+0],0x464c
    jne load_cache_close
    cmp word ptr [cache_header+2],0x5543
    jne load_cache_close
    cmp word ptr [cache_header+4],0x3152
    jne load_cache_close
    cmp word ptr [cache_header+6],0x3030
    jne load_cache_close
    mov al,byte ptr [cache_header+8]
    cmp al,14
    je load_cache_height_ok
    cmp al,16
    jne load_cache_close
load_cache_height_ok:
    mov ah,byte ptr [cache_header+9]
    or ah,ah
    jnz load_cache_custom
    mov byte ptr [active_height],al
    mov byte ptr [active_id],0
    jmp load_cache_read_name
load_cache_custom:
    cmp al,14
    jne load_cache_vga
    cmp word ptr [cache_header+10],3584
    jne load_cache_close
    mov cx,3584
    jmp load_cache_read_font
load_cache_vga:
    cmp word ptr [cache_header+10],4096
    jne load_cache_close
    mov cx,4096
load_cache_read_font:
    push ax
    mov dx,offset active_font
    mov ah,0x3f
    int 0x21
    cmp ax,cx
    pop ax
    jne load_cache_close
    mov byte ptr [active_height],al
    mov byte ptr [active_id],ah
load_cache_read_name:
    /* New caches append the display name used by Configuration.  Old caches
       simply return EOF here and use a small built-in fallback below. */
    mov dx,offset display_name
    mov cx,31
    mov ah,0x3f
    int 0x21
load_cache_close:
    mov ah,0x3e
    int 0x21
load_cache_done:
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    ret

/* Startup output is deliberately concise.  FONT.CUR carries the same display
   name shown in Configuration; factory/legacy caches fall back to Standard,
   Launch!, or Custom as appropriate. */
show_font_message:
    push ax
    push dx
    push si
    mov si,offset display_name
    cmp byte ptr [si],0
    jne font_message_have_name
    cmp byte ptr [active_id],0
    je font_message_standard
    cmp byte ptr [active_id],6
    je font_message_launch
    mov si,offset name_custom
    jmp font_message_have_name
font_message_standard:
    mov si,offset name_standard
    jmp font_message_have_name
font_message_launch:
    mov si,offset name_launch
font_message_have_name:
    mov dx,offset msg_font_prefix
    mov ah,9
    int 0x21
font_message_loop:
    lodsb
    or al,al
    jz font_message_suffix
    mov dl,al
    mov ah,2
    int 0x21
    jmp font_message_loop
font_message_suffix:
    mov dx,offset msg_font_suffix
    mov ah,9
    int 0x21
    pop si
    pop dx
    pop ax
    ret

/* AL=0 install, 1 help, 2 unload, 3 syntax error. */
parse_command:
    xor cx,cx
    mov cl,byte ptr [0x80]
    mov si,0x81
parse_skip_space:
    jcxz parse_install
    mov al,byte ptr [si]
    cmp al,' '
    je parse_advance_space
    cmp al,9
    jne parse_token
parse_advance_space:
    inc si
    dec cx
    jmp parse_skip_space
parse_install:
    xor al,al
    ret
parse_token:
    cmp al,'/'
    je parse_after_prefix
    cmp al,'-'
    jne parse_bad
parse_after_prefix:
    inc si
    dec cx
    jcxz parse_bad
    mov al,byte ptr [si]
    cmp al,'?'
    je parse_help
    and al,0xdf
    cmp al,'U'
    jne parse_bad
    inc si
    dec cx
    jcxz parse_unload
    mov al,byte ptr [si]
    cmp al,' '
    je parse_unload
    cmp al,9
    je parse_unload
    /* Accept the full word UNLOAD as well as /U. */
    mov di,offset word_nload
parse_word_loop:
    jcxz parse_unload
    mov al,byte ptr [si]
    cmp al,' '
    je parse_unload
    cmp al,9
    je parse_unload
    and al,0xdf
    cmp al,byte ptr [di]
    jne parse_bad
    inc si
    inc di
    dec cx
    cmp byte ptr [di],0
    jne parse_word_loop
    jmp parse_trailing
parse_help:
    mov al,1
    ret
parse_unload:
    mov al,2
    ret
parse_trailing:
    jcxz parse_unload
    mov al,byte ptr [si]
    cmp al,' '
    je parse_trailing_next
    cmp al,9
    jne parse_bad
parse_trailing_next:
    inc si
    dec cx
    jmp parse_trailing
parse_bad:
    mov al,3
    ret

word_nload: .asciz "NLOAD"
resident_seg: .word 0
top2f_off: .word 0
top2f_seg: .word 0
other_resident: .word 0
other_prepare: .word 0,0
reload_needed: .byte 0
reload_command: .space 16,0
cache_leaf: .asciz "FONT.CUR"
cache_path: .space 120,0
cache_header: .space 12,0
display_name: .space 32,0
name_standard: .asciz "Standard"
name_launch: .asciz "Launch!"
name_custom: .asciz "Custom"
msg_font_prefix: .ascii "Display font set to $"
msg_font_suffix: .ascii ".\r\n$"

msg_already:
    .ascii "!FONT is already resident.\r\n$"
msg_unloaded:
    .ascii "!FONT unloaded.\r\n$"
msg_not_installed:
    .ascii "!FONT is not resident.\r\n$"
msg_unload_unsafe:
    .ascii "!FONT cannot unload: another TSR is above it.\r\n$"
msg_reload_failed:
    .ascii "!FONT unloaded, but the other Launch! TSR could not be reloaded automatically.\r\n$"
msg_syntax:
    .ascii "Syntax: !FONT [/U|/UNLOAD|/?]\r\n$"
msg_help:
    .ascii "Launch! font persistence service\r\n"
    .ascii "  !FONT           Install service; apply FONT.CUR beside !FONT.COM\r\n"
    .ascii "  !FONT /U        Unload the resident service\r\n"
    .ascii "  !FONT /UNLOAD   Same as /U\r\n$"
