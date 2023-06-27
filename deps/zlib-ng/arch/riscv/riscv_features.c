#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../zbuild.h"
#include "riscv_features.h"

/* TODO: detect risc-v cpu info at runtime when the kernel updates hwcap or hwprobe for risc-v */
void Z_INTERNAL riscv_check_features(struct riscv_cpu_features *features) {
#if defined(__riscv_v) && defined(__linux__)
    features->has_rvv = 1;
#else
    features->has_rvv = 0;
#endif
}
