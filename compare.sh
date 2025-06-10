set -e

INPUT="input.asm"
OUT_DIR="output"

mkdir -p "$OUT_DIR"

MY_OUT="$OUT_DIR/my-out.bin"
MY_DIS="$OUT_DIR/my-out.dis"
NASM_OUT="$OUT_DIR/nasm-control.bin"
NASM_DIS="$OUT_DIR/nasm-control.dis"
PROGRAM="$OUT_DIR/my-program"

# 1. Compile and encode with my program
gcc main.c -o "$PROGRAM"
"$PROGRAM" "$INPUT" "$MY_OUT"

# 2. encode with NASM
nasm -f bin "$INPUT" -o "$NASM_OUT"

# 3. disassemble both
ndisasm -b 16 "$MY_OUT" >"$MY_DIS"
ndisasm -b 16 "$NASM_OUT" >"$NASM_DIS"

# 4. compare
diff -u "$NASM_DIS" "$MY_DIS" && echo "Output matches NASM!" || echo "[X] Mismatch found."
