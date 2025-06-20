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

; same for add
add bx, [bx+si]
add bx, [bp]
add si, 2
add bp, 2
add cx, 8
add bx, [bp + 0]
add cx, [bx + 2]
add bh, [bp + si + 4]
add di, [bp + di + 6]
add [bx+si], bx
add [bp], bx
add [bp + 0], bx
add [bx + 2], cx
add [bp + si + 4], bh
add [bp + di + 6], di
add byte [bx], 34
add word [bp + si + 1000], 29
add ax, [bp]
add al, [bx + si]
add ax, bx
add al, ah
add ax, 1000
add al, -30
add al, 9

; same for sub
sub bx, [bx+si]
sub bx, [bp]
sub si, 2
sub bp, 2
sub cx, 8
sub bx, [bp + 0]
sub cx, [bx + 2]
sub bh, [bp + si + 4]
sub di, [bp + di + 6]
sub [bx+si], bx
sub [bp], bx
sub [bp + 0], bx
sub [bx + 2], cx
sub [bp + si + 4], bh
sub [bp + di + 6], di
sub byte [bx], 34
sub word [bx + di], 29
sub ax, [bp]
sub al, [bx + si]
sub ax, bx
sub al, ah
sub ax, 1000
sub al, -30
sub al, 9

; same for cmp
cmp bx, [bx+si]
cmp bx, [bp]
cmp si, 2
cmp bp, 2
cmp cx, 8
cmp bx, [bp + 0]
cmp cx, [bx + 2]
cmp bh, [bp + si + 4]
cmp di, [bp + di + 6]
cmp [bx+si], bx
cmp [bp], bx
cmp [bp + 0], bx
cmp [bx + 2], cx
cmp [bp + si + 4], bh
cmp [bp + di + 6], di
cmp byte [bx], 34
cmp word [4834], 29
cmp ax, [bp]
cmp al, [bx + si]
cmp ax, bx
cmp al, ah
cmp ax, 1000
cmp al, -30
cmp al, 9