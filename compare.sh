set -e

INPUT="input.asm"
OUT_DIR="output"

mkdir -p "$OUT_DIR"

MY_OUT="$OUT_DIR/my-out.bin"
MY_DIS="$OUT_DIR/my-out.dis"
NASM_OUT="$OUT_DIR/nasm-control.bin"
NASM_DIS="$OUT_DIR/nasm-control.dis"
PROGRAM="$OUT_DIR/my-program"

# 1. Compile and assemble with my program
gcc encoder.c -o "$PROGRAM"
"$PROGRAM" "$INPUT" "$MY_OUT"

# 2. Assemble with NASM
nasm -f bin "$INPUT" -o "$NASM_OUT"

# 3. Disassemble both
ndisasm -b 16 "$MY_OUT" >"$MY_DIS"
ndisasm -b 16 "$NASM_OUT" >"$NASM_DIS"

# 4. Compare
diff -u "$NASM_DIS" "$MY_DIS" && echo "Output matches NASM!" || echo "[X] Mismatch found."
