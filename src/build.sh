set -e
cd ~/mod
CF="$EXTRA --target=aarch64-none-elf -march=armv8-a -O2 -fPIC -ffreestanding -fno-builtin -nostdlib -fno-stack-protector -fvisibility=default -mno-outline-atomics -fno-asynchronous-unwind-tables -ffunction-sections -fno-plt -ffixed-x18"
clang $CF -c start.S -o start.o
clang $CF -c main.c -o main.o
ld.lld -shared -T link.ld --no-undefined-version -z now --hash-style=both start.o main.o -o touchmod.elf --unresolved-symbols=ignore-all -soname subsdk9
python3 elf2nso.py touchmod.elf subsdk9
