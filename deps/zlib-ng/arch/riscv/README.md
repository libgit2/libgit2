# Building RISC-V Target with Cmake #

> **Warning**
> We cannot detect rvv support at runtime, running the rvv code on a no-rvv target is a risk. Users should disable the rvv when the target does not support it. 
>
> We will have a better solution when the kernels update `hwcap` or `hwprobe` for risc-v.

## Prerequisite: Build RISC-V Clang Toolchain and QEMU ##

If you don't have prebuilt clang and riscv64 qemu, you can refer to the [script](https://github.com/sifive/prepare-riscv-toolchain-qemu/blob/main/prepare_riscv_toolchain_qemu.sh) to get the source. Copy the script to the zlib-ng root directory, and run it to download the source and build them. Modify the content according to your conditions (e.g., toolchain version).

```bash
./prepare_riscv_toolchain_qemu.sh
```

After running script, clang & qemu are built in `build-toolchain-qemu/riscv-clang/` & `build-toolchain-qemu/riscv-qemu/`.

`build-toolchain-qemu/riscv-clang/` is your `TOOLCHAIN_PATH`.
`build-toolchain-qemu/riscv-qemu/bin/qemu-riscv64` is your `QEMU_PATH`.

You can also download the prebuilt toolchain & qemu from [the release page](https://github.com/sifive/prepare-riscv-toolchain-qemu/releases), and enjoy using them.

## Cross-Compile for RISC-V Target ##

```bash
cmake -G Ninja -B ./build-riscv \
  -D CMAKE_TOOLCHAIN_FILE=./cmake/toolchain-riscv.cmake \
  -D CMAKE_INSTALL_PREFIX=./build-riscv/install \
  -D TOOLCHAIN_PATH={TOOLCHAIN_PATH} \
  -D QEMU_PATH={QEMU_PATH} \
  .

cmake --build ./build-riscv
```

Disable the option if there is no RVV support:
```
-D WITH_RVV=OFF
```

## Run Unittests on User Mode QEMU ##

```bash
cd ./build-riscv && ctest --verbose
```
