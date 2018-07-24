; ----------------------------------------------------------------------------------------
;   File: dct.asm
;   Description:
;       Implements DCT and iDCT through SIMD assembly code.
;       Uses factorization of Weig and Finnograd.
;       Note: idct = transpose(dct)
; ----------------------------------------------------------------------------------------

extern printf

global setup
global reset_dc
global compress_luminance
global compress_blue
global compress_red
global decompress_luminance
global decompress_blue
global decompress_red


; Constants from the function ga(x) = cos(x * pi / 16), for x = 1..7
; These are all divided by 2
%define     ga1         0.490392640201615   ; 0.980785280403230
%define     ga2         0.461939766255643   ; 0.923879532511287
%define     ga3         0.415734806151272   ; 0.831469612302545
%define     ga4         0.353553390593274   ; 0.707106781186548
%define     ga5         0.277785116509801   ; 0.555570233019602
%define     ga6         0.191341716182545   ; 0.382683432365090
%define     ga7         0.097545161008064   ; 0.195090322016128

%define     block_size  0x100               ; block size in bytes, 64 x 4 (size of float = 4)
%define     row_size    0x20                ; row, in block, size, 8 x 4 (size of float = 4)
%define     max_byte    0xFF
%define     norm        1024

%define     y_mask      0xFF00FF00FF00FF00
%define     u_mask      0x000000FF000000FF
%define     v_mask      0x00FF000000FF0000


section .data
align       16
; NOTE: the m-matrices are all transposed to make SIMD instructions easier.
; top left corner of M matrix
m_41:       dd          ga4,  ga2,  ga4,  ga6,\
                        ga4,  ga6, -ga4, -ga2,\
                        ga4, -ga6, -ga4,  ga2,\
                        ga4, -ga2,  ga4, -ga6
; low right corner of M matrix
m_42:       dd          ga1,  ga3,  ga5,  ga7,\
                        ga3, -ga7, -ga1, -ga5,\
                        ga5, -ga1,  ga7,  ga3,\
                        ga7, -ga5,  ga3, -ga1
; top left corner of M matrix (transposed)
m_41_t:     dd          ga4,  ga4,  ga4,  ga4,\
                        ga2,  ga6, -ga6, -ga2,\
                        ga4, -ga4, -ga4,  ga4,\
                        ga6, -ga2,  ga2, -ga6
; low right corner of M matrix (transposed)
; (this one is actually the same transposed, so we make a macro)
%define     m_42_t      m_42
; quantization matrix ~ 95%
q:          dd           8.0,  5.0,  5.0,  8.0, 12.0, 20.0, 25.0, 30.0,\
                         6.0,  6.0,  7.0,  9.0, 13.0, 29.0, 30.0, 27.0,\
                         7.0,  6.0,  8.0, 12.0, 20.0, 28.0, 34.0, 28.0,\
                         7.0,  8.0, 11.0, 14.0, 25.0, 43.0, 40.0, 31.0,\
                         9.0, 11.0, 18.0, 28.0, 34.0, 54.0, 51.0, 38.0,\
                        12.0, 17.0, 27.0, 32.0, 40.0, 52.0, 56.0, 46.0,\
                        24.0, 32.0, 39.0, 43.0, 51.0, 60.0, 60.0, 50.0,\
                        36.0, 46.0, 47.0, 45.0, 56.0, 50.0, 51.0, 49.0
; inversed quantization matrix
iq:         dd          0.1250, 0.2000, 0.2000, 0.1250, 0.0833, 0.0500, 0.0400, 0.0333,\
                        0.1667, 0.1667, 0.1429, 0.1111, 0.0769, 0.0345, 0.0333, 0.0370,\
                        0.1429, 0.1667, 0.1250, 0.0833, 0.0500, 0.0357, 0.0294, 0.0357,\
                        0.1429, 0.1250, 0.0909, 0.0714, 0.0400, 0.0233, 0.0250, 0.0323,\
                        0.1111, 0.0909, 0.0556, 0.0357, 0.0294, 0.0185, 0.0196, 0.0263,\
                        0.0833, 0.0588, 0.0370, 0.0312, 0.0250, 0.0192, 0.0179, 0.0217,\
                        0.0417, 0.0312, 0.0256, 0.0233, 0.0196, 0.0167, 0.0167, 0.0200,\
                        0.0278, 0.0217, 0.0213, 0.0222, 0.0179, 0.0200, 0.0196, 0.0204

align       8
N:          dd          1024.0


section .bss
align       16
block:      resq        1
buf:        resq        1

section .bss
align       8
width:      resq        1
height:     resq        1


section .text

; Setup width and height parameters.
setup:
    mov         [width], rdi
    mov         [height], rsi
    mov         [block], rdx
    mov         [buf], rcx
    ret


; Round a single precision float point to byte.
; if [ v > 255 ];
;   v = 255
; else if [ v < 0 ];
;   v = 0
; else
;   v = v
; XMM0 contains the float in its 4 least significant bytes.
; RAX will contain the byte when returned.
roundfloat:
    cvttss2si   rax, xmm0           ; convert float to int
roundint:
    mov         rbx, max_byte
    mov         rcx, 0
    ; test signed (negativ) value
    test        rax, rax
    cmovs       rax, rcx
    js          doneround
    ; test larger than max byte (255)
    cmp         rax, max_byte
    cmovg       rax, rbx
doneround:
    ret


; Extract and compress a luminance block stored
; to a supplied 16-bit integer array pointer
; RDI points to source byte array
; RSI points to destination 16bit int array
compress_luminance:
    push        rbx
    push        rsi
    mov         r8, 2               ; 2 x 8 bytes / row
    mov         r9, 8               ; 8 rows / block
    mov         r10, [width]        ; UYVY so we have
    shl         r10, 1              ; width in bytes = width in pixels x 2
    sub         r10, 0x10
    mov         rsi, [block]
    mov         rbx, y_mask

__extract_y__:
    prefetchnta [rdi + r10 + 0x10]
    ; load 8 bytes from memory, i.e 4 Y-values
    mov         rax, [rdi]
    and         rax, rbx            ; our mask cleares every other byte
    shr         rax, 8
    ; interleave to keep the original byte as the least significant of four
    ; and then store as float
    movq        xmm0, rax           ; load RAX to XMM0
    pmovsxwd    xmm0, xmm0          ; unpack to 32bit ints
    cvtdq2ps    xmm0, xmm0          ; convert to float
    movdqa      [rsi], xmm0

    add         rdi, 8              ; next 4 values in row
    add         rsi, 0x10
    dec         r8
    jnz         __extract_y__
    ; go to next row
    add         rdi, r10
    mov         r8, 2
    dec         r9
    jnz         __extract_y__
    ; a full block has been extracted, now compress
    mov         rdi, [block]
    pop         rsi
    call        compress_block
    ; done
    pop         rbx
    ret


; Extract and compress a color (blue/red) block from a supplied byte array
; to a second parameter destination 16bit int array.
; RDI points to source byte array
; RSI points to destination 16bit int array
compress_blue:
    mov         rcx, 0
    jmp         __compress_color__

compress_red:
    mov         rcx, 16

__compress_color__:
    push        rbx
    push        rsi
    mov         r8, 2
    mov         r9, 8               ; 8 rows / block
    mov         r10, [width]        ; UYVY so we have
    shl         r10, 1              ; width in bytes = width in pixels x 2
    sub         r10, 0x20
    mov         rsi, [block]
    ; create mask to keep the least significant byte of four
    mov         rdx, u_mask
    movq        xmm1, rdx
    pshufd      xmm1, xmm1, 0x44
    ; bits to shift
    pxor        xmm2, xmm2
    movq        xmm2, rcx

__extract_color__:
    movdqa      xmm0, [rdi]         ; one DQWORD contains 4 values
    psrld       xmm0, xmm2          ; shift to place the wanted byte as least significant
    pand        xmm0, xmm1          ; zero all other bytes to create 32bit integers
    cvtdq2ps    xmm0, xmm0          ; convert to float
__debug__:
    movdqa      [rsi], xmm0
    ; next 4 values
    add         rdi, 0x10
    add         rsi, 0x10
    dec         r8
    jnz         __extract_color__
    ; next row
    add         rdi, r10
    mov         r8, 2
    dec         r9
    jnz         __extract_color__
    ; full block extracted - compress
    mov         rdi, [block]
    pop         rsi
    call        compress_block
    pop         rbx
    ret


; Decompress a block of 8x8 16-bit integers
; to a block of 8x8 bytes and store them correctly interleaved in memory
; RDI points to the 16bit integers
; RSI points to destination byte buffer.
decompress_luminance:
    push        rbx
    call        decompress_block
    ; convert and store the 8x8 floats as bytes into memory
    mov         rdi, [block]
    mov         r8, 8               ; 8 rows / block
    add         rsi, 1              ; offset
    mov         r10, [width]        ; UYVY so we have
    shl         r10, 1              ; width in bytes = width in pixels x 2
    sub         r10, 0x10
__store_luminance_row__:
    mov         r9, 2               ; 2 x 4 floats / row
__store_luminance_halfrow__:
    ; load 4 floats
    movdqa      xmm0, [rdi]
    mov         r11, 4              ; 4 floats in a double qword
__store_luminance_byte__:
    ; round each and store
    call        roundfloat          ; round float to byte
    mov         [rsi], al           ; store byte
    psrldq      xmm0, 4             ; shift right to next float
    add         rsi, 2
    dec         r11
    jnz         __store_luminance_byte__
    ; half-row done
    add         rdi, 0x10
    dec         r9
    jnz         __store_luminance_halfrow__
    ; row done
    add         rsi, r10
    dec         r8
    jnz         __store_luminance_row__
    ; block done
    pop         rbx
    ret


; Decompress a block of 8x8 16-bit integers to bytes
; and store them into memory accordingly to channel.
; RDI: source 16-bit block in memory
; RSI: destination memory address
decompress_red:
    add         rsi, 2
decompress_blue:
    push        rbx
    call        decompress_block
    ; convert and store the 8x8 floats as bytes into memory
    mov         rdi, [block]
    mov         r8, 8               ; 8 rows / block
    mov         r10, [width]        ; UYVY so we have
    shl         r10, 1              ; width in bytes = width in pixels x 2
    sub         r10, 0x20
__store_color_row__:
    mov         r9, 2               ; 2 x 4 floats / row
__store_color_halfrow__:
    ; load 4 floats
    movdqa      xmm0, [rdi]
    mov         r11, 4              ; 4 floats in a double qword
__store_color_byte__:
    ; round each and store
    call        roundfloat          ; round float to byte
    mov         [rsi], al           ; store byte
    psrldq      xmm0, 4             ; shift right to next float
    add         rsi, 4
    dec         r11
    jnz         __store_color_byte__
    ; half-row done
    add         rdi, 0x10
    dec         r9
    jnz         __store_color_halfrow__
    ; row done
    add         rsi, r10
    dec         r8
    jnz         __store_color_row__
    ; block done
    pop         rbx
    ret


; Compress an 8x8 block by performing 2d dct then quantization.
; RDI contains the source 8x8 floats
; RSI points to destination of 16bit ints.
compress_block:
    ; transform and normalize
    call    dct
    fld     dword [rdi]
    fsub    dword [N]
    fstp    dword [rdi]
    ; quantize
    call    quantize
    ret


; Decompress an 8x8 block by first dequantizing
; and then performing inverse 2d dct.
; RDI points to the source 16bit int.
; RSI is untouched
decompress_block:
    push    rsi
    ; dequantize
    mov     rsi, [block]
    call    dequantize
    ; normalize
    mov     rdi, [block]
    fld     dword [rdi]
    fadd    dword [N]
    fstp    dword [rdi]
    ; inverse dct
    call    idct
    ; done
    pop     rsi
    ret


; 2 dimensional Forward/Inverse Discrete Cosine Transform
; RDI points to the source 8x8 floats
;   Result is stored to the same memory address.
; RSI is untouched.
dct:
    mov     rcx, __dct__            ; rcx contains address to 1d transform to be applied
    jmp     transform_2d

idct:
    mov     rcx, __idct__

transform_2d:
    push    rsi
    mov     rsi, [buf]              ; transform to buffer first round
    mov     r9, 2                   ; number of dimensions

transform:                          ; over 8 rows/columns
    push    rdi
    push    rsi
    mov     r8, 8

    ; try load block into to cache to speed it up
    prefetchnta [rdi]
    prefetchnta [rdi + 0x20]
    prefetchnta [rdi + 0x40]
    prefetchnta [rdi + 0x60]
    prefetchnta [rdi + 0x80]
    prefetchnta [rdi + 0xB0]
    prefetchnta [rdi + 0xD0]
    prefetchnta [rdi + 0xE0]

__transform__:                      ; transform row
    ; load from memory
    movdqa  xmm4, [rdi]
    movdqa  xmm5, [rdi + 0x10]
    call    rcx                     ; call transform subroutine
    ; store to memory transposed
    movss   [rsi], xmm4
    psrldq  xmm4, 4
    movss   [rsi + 0x20], xmm4
    psrldq  xmm4, 4
    movss   [rsi + 0x40], xmm4
    psrldq  xmm4, 4
    movss   [rsi + 0x60], xmm4

    movss   [rsi + 0x80], xmm5
    psrldq  xmm5, 4
    movss   [rsi + 0xA0], xmm5
    psrldq  xmm5, 4
    movss   [rsi + 0xC0], xmm5
    psrldq  xmm5, 4
    movss   [rsi + 0xE0], xmm5

    ; next row
    add     rdi, row_size
    add     rsi, 4
    dec     r8
    jnz     __transform__
    ; a dimension is done here
    pop     rsi                     ; restore mem pointers
    pop     rdi
    dec     r9
    ; if we've done both dimensions return
    jz      __done_transform__
    ; redo transforms over next dimension
    mov     rsi, rdi
    mov     rdi, [buf]
    jmp     transform

__done_transform__:
    mov     rdi, rsi
    pop     rsi
    ret


; 1-D Discrete Cosine Transform
; xmm4 contains upper 4 floats
; xmm5 contains lower 4 floats
__dct__:
   ; A - preadditions
   pshufd  xmm5, xmm5, 0x1B
   movdqa  xmm0, xmm4
   addps   xmm4, xmm5
   subps   xmm0, xmm5
   movdqa  xmm5, xmm0

   ; M
   movdqa  xmm0, [m_41]
   movdqa  xmm1, [m_41 + 0x10]
   movdqa  xmm2, [m_41 + 0x20]
   movdqa  xmm3, [m_41 + 0x30]
   call    matrixmul_4x4

   movdqa  xmm4, xmm5
   movdqa  xmm5, xmm0

   movdqa  xmm0, [m_42]
   movdqa  xmm1, [m_42 + 0x10]
   movdqa  xmm2, [m_42 + 0x20]
   movdqa  xmm3, [m_42 + 0x30]
   call    matrixmul_4x4

   movdqa  xmm4, xmm5
   movdqa  xmm5, xmm0

   ; P - permutations
   movdqa  xmm0, xmm4
   shufps  xmm4, xmm5, 0x44
   shufps  xmm0, xmm5, 0xEE
   pshufd  xmm4, xmm4, 0xD8
   pshufd  xmm5, xmm0, 0xD8

   ret


; 1-D Inverse Discrete Cosine Transform
; xmm4 contains upper 4 floats
; xmm5 contains lower 4 floats
__idct__:
   ; P - permute
   movdqa  xmm0, xmm4
   shufps  xmm4, xmm5, 0x88
   shufps  xmm0, xmm5, 0xDD
   movdqa  xmm5, xmm0

   ; M
   movdqa  xmm0, [m_41_t]
   movdqa  xmm1, [m_41_t + 0x10]
   movdqa  xmm2, [m_41_t + 0x20]
   movdqa  xmm3, [m_41_t + 0x30]
   call    matrixmul_4x4

   movdqa  xmm4, xmm5
   movdqa  xmm5, xmm0

   movdqa  xmm0, [m_42_t]
   movdqa  xmm1, [m_42_t + 0x10]
   movdqa  xmm2, [m_42_t + 0x20]
   movdqa  xmm3, [m_42_t + 0x30]
   call    matrixmul_4x4

   movdqa  xmm4, xmm5
   movdqa  xmm5, xmm0

   ; A - postadditions
   movdqa  xmm0, xmm4
   addps   xmm4, xmm5
   subps   xmm0, xmm5,
   pshufd  xmm5, xmm0, 0x1B

   ret


; Quantize an 8x8 block of floats into a 8x8 block of 16bit integers
; RDI points to source floats
; RSI points to destination block of ints
quantize:
    push        rsi
    mov         r8, 8
    mov         rdx, iq
    ; try and load into cache to speed it up
    prefetchnta [rdi]
    prefetchnta [rdi + 0x20]
    prefetchnta [rdi + 0x40]
    prefetchnta [rdi + 0x60]
    prefetchnta [rdx]
    prefetchnta [rdx + 0x20]
    prefetchnta [rdx + 0x40]
    prefetchnta [rdx + 0x60]

__quantize__:
    ; load 4 floats and multiply
    movdqa      xmm0, [rdi]
    movdqa      xmm1, [rdx]
    mulps       xmm0, xmm1
    cvttps2dq   xmm0, xmm0              ; convert to 32bit int

    add         rdi, 0x10
    add         rdx, 0x10

    ; next 4 floats
    movdqa      xmm2, [rdi]
    movdqa      xmm1, [rdx]
    mulps       xmm2, xmm1
    cvttps2dq   xmm2, xmm2              ; convert to 32bit int

    ; convert to 16bit int
    packssdw    xmm0, xmm2
    movdqa      [rsi], xmm0

    ; next row
    add         rsi, 0x10
    add         rdi, 0x10
    add         rdx, 0x10
    dec         r8
    jnz         __quantize__
    pop         rsi
    ret


; Dequantize a block of 8x8 16-bit integers to a 8x8 block of floats.
; RDI points to source block
; RSI points to destination float block.
dequantize:
    mov         r8, 16              ; 2 fetch / row, 8 rows / block
    mov         rdx, q
    prefetchnta [rdi]
    prefetchnta [rdx]

__dequantize__:
    movdqa      xmm1, [rdx]
    pmovsxwd    xmm0, [rdi]         ; move 4 16-bit ints into xmm0 as 32-byte ints
    cvtdq2ps    xmm0, xmm0          ; convert to floats
    mulps       xmm0, xmm1
    movdqa      [rsi], xmm0
    ; next
    add         rdi, 8
    add         rsi, 0x10
    add         rdx, 0x10
    dec         r8
    jnz         __dequantize__
    ret


; Multiply a 4x4 matrix with a 1x4 vector.
; Assumes col 1-4 of the matrix are in registers xmm[0-3]
; and the vector we are mutliplying by is in xmm4.
; The result will be in xmm0.
matrixmul_4x4:
    ; multiply matrix columns
    pshufd  xmm6, xmm4, 0x00
    mulps   xmm0, xmm6
    pshufd  xmm6, xmm4, 0x55
    mulps   xmm1, xmm6
    pshufd  xmm6, xmm4, 0xAA
    mulps   xmm2, xmm6
    pshufd  xmm6, xmm4, 0xFF
    mulps   xmm3, xmm6
    ; add them up
    addps   xmm0, xmm1
    addps   xmm2, xmm3
    addps   xmm0, xmm2
    ret
