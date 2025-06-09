bits 16

; COMMENTS SUPPORTED AS WELL!

; reg-to-reg
mov cx, bx
mov cx, bx
mov ch, ah
mov dx, bx
mov si, bx
mov bx, di
mov al, cl
mov ch, ch
mov bx, ax
mov bx, si
mov sp, di
mov bp, ax
mov dh, al

; 8-bit imm-to-reg
mov cl, 12
mov ch, -12

; 16-bit imm-to-reg
mov dx, 3948
mov dx, -3948

; source address calculation
mov al, [bx + si]
mov bx, [bp + di]
mov dx, [bp]

; source address calculation plus 8-bit displacement
mov ah, [bx + si + 4]

; source address calculation plus 16-bit displacement
mov al, [bx + si + 4999]

; dest address calculation
mov [bx + di], cx
mov [bp + si], cl
mov [bp], ch

; signed displacements
mov ax, [bx + di - 37]
mov [si - 300], cx
mov dx, [bx - 32]

; explicit sizes
mov [bp + di], byte 7
mov [di + 901], word 347

; direct address
mov bp, [5]
mov bx, [3458]

; mem-to-acc test
mov ax, [2555]
mov ax, [16]

; acc-to-mem test
mov [2554], ax
mov [15], ax