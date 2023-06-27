# detect-intrinsics.cmake -- Detect compiler intrinsics support
# Licensed under the Zlib license, see LICENSE.md for details

macro(check_acle_compiler_flag)
    if(MSVC)
        # Both ARM and ARM64-targeting msvc support intrinsics, but
        # ARM msvc is missing some intrinsics introduced with ARMv8, e.g. crc32
        if(MSVC_C_ARCHITECTURE_ID STREQUAL "ARM64")
            set(HAVE_ACLE_FLAG TRUE)
        endif()
    else()
        if(CMAKE_C_COMPILER_ID MATCHES "GNU" OR CMAKE_C_COMPILER_ID MATCHES "Clang")
            if(NOT NATIVEFLAG AND NOT HAVE_ACLE_FLAG)
                set(ACLEFLAG "-march=armv8-a+crc" CACHE INTERNAL "Compiler option to enable ACLE support")
            endif()
        endif()
        # Check whether compiler supports ACLE flag
        set(CMAKE_REQUIRED_FLAGS "${ACLEFLAG} ${NATIVEFLAG}")
        check_c_source_compiles(
            "int main() { return 0; }"
            HAVE_ACLE_FLAG FAIL_REGEX "not supported")
        if(NOT NATIVEFLAG AND NOT HAVE_ACLE_FLAG)
            set(ACLEFLAG "-march=armv8-a+crc+simd" CACHE INTERNAL "Compiler option to enable ACLE support" FORCE)
            # Check whether compiler supports ACLE flag
            set(CMAKE_REQUIRED_FLAGS "${ACLEFLAG}")
            check_c_source_compiles(
                "int main() { return 0; }"
                HAVE_ACLE_FLAG2 FAIL_REGEX "not supported")
            set(HAVE_ACLE_FLAG ${HAVE_ACLE_FLAG2} CACHE INTERNAL "Have compiler option to enable ACLE intrinsics" FORCE)
            unset(HAVE_ACLE_FLAG2 CACHE) # Don't cache this internal variable
        endif()
        set(CMAKE_REQUIRED_FLAGS)
    endif()
endmacro()

macro(check_avx512_intrinsics)
    if(CMAKE_C_COMPILER_ID MATCHES "Intel")
        if(CMAKE_HOST_UNIX OR APPLE)
            set(AVX512FLAG "-mavx512f -mavx512dq -mavx512bw -mavx512vl")
        else()
            set(AVX512FLAG "/arch:AVX512")
        endif()
    elseif(CMAKE_C_COMPILER_ID MATCHES "GNU" OR CMAKE_C_COMPILER_ID MATCHES "Clang")
        if(NOT NATIVEFLAG)
            # For CPUs that can benefit from AVX512, it seems GCC generates suboptimal
            # instruction scheduling unless you specify a reasonable -mtune= target
            set(AVX512FLAG "-mavx512f -mavx512dq -mavx512bw -mavx512vl")
            if(NOT CMAKE_GENERATOR_TOOLSET MATCHES "ClangCl")
                check_c_compiler_flag("-mtune=cascadelake" HAVE_CASCADE_LAKE)
                if(HAVE_CASCADE_LAKE)
                    set(AVX512FLAG "${AVX512FLAG} -mtune=cascadelake")
                else()
                    set(AVX512FLAG "${AVX512FLAG} -mtune=skylake-avx512")
                endif()
                unset(HAVE_CASCADE_LAKE)
            endif()
        endif()
    elseif(MSVC)
        set(AVX512FLAG "/arch:AVX512")
    endif()
    # Check whether compiler supports AVX512 intrinsics
    set(CMAKE_REQUIRED_FLAGS "${AVX512FLAG} ${NATIVEFLAG}")
    check_c_source_compile_or_run(
        "#include <immintrin.h>
        int main(void) {
            __m512i x = _mm512_set1_epi8(2);
            const __m512i y = _mm512_set_epi32(0x1020304, 0x5060708, 0x90a0b0c, 0xd0e0f10,
                                               0x11121314, 0x15161718, 0x191a1b1c, 0x1d1e1f20,
                                               0x21222324, 0x25262728, 0x292a2b2c, 0x2d2e2f30,
                                               0x31323334, 0x35363738, 0x393a3b3c, 0x3d3e3f40);
            x = _mm512_sub_epi8(x, y);
            (void)x;
            return 0;
        }"
        HAVE_AVX512_INTRIN
    )

    # Evidently both GCC and clang were late to implementing these
    check_c_source_compile_or_run(
        "#include <immintrin.h>
        int main(void) {
            __mmask16 a = 0xFF;
            a = _knot_mask16(a);
            (void)a;
            return 0;
        }"
        HAVE_MASK_INTRIN
    )
    set(CMAKE_REQUIRED_FLAGS)
endmacro()

macro(check_avx512vnni_intrinsics)
    if(CMAKE_C_COMPILER_ID MATCHES "Intel")
        if(CMAKE_HOST_UNIX OR APPLE)
            set(AVX512VNNIFLAG "-mavx512f -mavx512bw -mavx512dq -mavx512vl -mavx512vnni")
        else()
            set(AVX512VNNIFLAG "/arch:AVX512")
        endif()
    elseif(CMAKE_C_COMPILER_ID MATCHES "GNU" OR CMAKE_C_COMPILER_ID MATCHES "Clang")
        if(NOT NATIVEFLAG)
            set(AVX512VNNIFLAG "-mavx512f -mavx512dq -mavx512bw -mavx512vl -mavx512vnni")
            if(NOT CMAKE_GENERATOR_TOOLSET MATCHES "ClangCl")
                check_c_compiler_flag("-mtune=cascadelake" HAVE_CASCADE_LAKE)
                if(HAVE_CASCADE_LAKE)
                    set(AVX512VNNIFLAG "${AVX512VNNIFLAG} -mtune=cascadelake")
                else()
                    set(AVX512VNNIFLAG "${AVX512VNNIFLAG} -mtune=skylake-avx512")
                endif()
                unset(HAVE_CASCADE_LAKE)
            endif()
        endif()
    elseif(MSVC)
        set(AVX512VNNIFLAG "/arch:AVX512")
    endif()

    # Check whether compiler supports AVX512vnni intrinsics
    set(CMAKE_REQUIRED_FLAGS "${AVX512VNNIFLAG} ${NATIVEFLAG}")
    check_c_source_compile_or_run(
        "#include <immintrin.h>
        int main(void) {
            __m512i x = _mm512_set1_epi8(2);
            const __m512i y = _mm512_set_epi8(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
                                              20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37,
                                              38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55,
                                              56, 57, 58, 59, 60, 61, 62, 63, 64);
            __m512i z = _mm512_setzero_epi32();
            z = _mm512_dpbusd_epi32(z, x, y);
            (void)z;
            return 0;
        }"
        HAVE_AVX512VNNI_INTRIN
    )
    set(CMAKE_REQUIRED_FLAGS)
endmacro()

macro(check_avx2_intrinsics)
    if(CMAKE_C_COMPILER_ID MATCHES "Intel")
        if(CMAKE_HOST_UNIX OR APPLE)
            set(AVX2FLAG "-mavx2")
        else()
            set(AVX2FLAG "/arch:AVX2")
        endif()
    elseif(CMAKE_C_COMPILER_ID MATCHES "GNU" OR CMAKE_C_COMPILER_ID MATCHES "Clang")
        if(NOT NATIVEFLAG)
            set(AVX2FLAG "-mavx2")
        endif()
    elseif(MSVC)
        set(AVX2FLAG "/arch:AVX2")
    endif()
    # Check whether compiler supports AVX2 intrinics
    set(CMAKE_REQUIRED_FLAGS "${AVX2FLAG} ${NATIVEFLAG}")
    check_c_source_compile_or_run(
        "#include <immintrin.h>
        int main(void) {
            __m256i x = _mm256_set1_epi16(2);
            const __m256i y = _mm256_set1_epi16(1);
            x = _mm256_subs_epu16(x, y);
            (void)x;
            return 0;
        }"
        HAVE_AVX2_INTRIN
    )
    set(CMAKE_REQUIRED_FLAGS)
endmacro()

macro(check_neon_compiler_flag)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU" OR CMAKE_C_COMPILER_ID MATCHES "Clang")
        if(NOT NATIVEFLAG)
            if("${ARCH}" MATCHES "aarch64")
                set(NEONFLAG "-march=armv8-a+simd")
            else()
                set(NEONFLAG "-mfpu=neon")
            endif()
        endif()
    endif()
    # Check whether compiler supports NEON flag
    set(CMAKE_REQUIRED_FLAGS "${NEONFLAG} ${NATIVEFLAG}")
    check_c_source_compiles(
        "#ifdef _M_ARM64
        #  include <arm64_neon.h>
        #else
        #  include <arm_neon.h>
        #endif
        int main() { return 0; }"
        MFPU_NEON_AVAILABLE FAIL_REGEX "not supported")
    set(CMAKE_REQUIRED_FLAGS)
endmacro()

macro(check_neon_ld4_intrinsics)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU" OR CMAKE_C_COMPILER_ID MATCHES "Clang")
        if(NOT NATIVEFLAG)
            if("${ARCH}" MATCHES "aarch64")
                set(NEONFLAG "-march=armv8-a+simd")
            else()
                set(NEONFLAG "-mfpu=neon")
            endif()
        endif()
    endif()
    # Check whether compiler supports loading 4 neon vecs into a register range
    set(CMAKE_REQUIRED_FLAGS "${NEONFLAG}")
    check_c_source_compiles(
        "#ifdef _M_ARM64
        #  include <arm64_neon.h>
        #else
        #  include <arm_neon.h>
        #endif
        int main(void) {
            int stack_var[16];
            int32x4x4_t v = vld1q_s32_x4(stack_var);
            (void)v;
            return 0;
        }"
        NEON_HAS_LD4)
    set(CMAKE_REQUIRED_FLAGS)
endmacro()

macro(check_pclmulqdq_intrinsics)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU" OR CMAKE_C_COMPILER_ID MATCHES "Clang")
        if(NOT NATIVEFLAG)
            set(PCLMULFLAG "-mpclmul")
        endif()
    endif()
    # Check whether compiler supports PCLMULQDQ intrinsics
    if(NOT (APPLE AND "${ARCH}" MATCHES "i386"))
        # The pclmul code currently crashes on Mac in 32bit mode. Avoid for now.
        set(CMAKE_REQUIRED_FLAGS "${PCLMULFLAG} ${NATIVEFLAG}")
        check_c_source_compile_or_run(
            "#include <immintrin.h>
            int main(void) {
                __m128i a = _mm_setzero_si128();
                __m128i b = _mm_setzero_si128();
                __m128i c = _mm_clmulepi64_si128(a, b, 0x10);
                (void)c;
                return 0;
            }"
            HAVE_PCLMULQDQ_INTRIN
        )
        set(CMAKE_REQUIRED_FLAGS)
    else()
        set(HAVE_PCLMULQDQ_INTRIN OFF)
    endif()
endmacro()

macro(check_vpclmulqdq_intrinsics)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU" OR CMAKE_C_COMPILER_ID MATCHES "Clang")
        if(NOT NATIVEFLAG)
            set(VPCLMULFLAG "-mvpclmulqdq -mavx512f")
        endif()
    endif()
    # Check whether compiler supports VPCLMULQDQ intrinsics
    if(NOT (APPLE AND "${ARCH}" MATCHES "i386"))
        set(CMAKE_REQUIRED_FLAGS "${VPCLMULFLAG} ${NATIVEFLAG}")
        check_c_source_compile_or_run(
            "#include <immintrin.h>
            int main(void) {
                __m512i a = _mm512_setzero_si512();
                __m512i b = _mm512_setzero_si512();
                __m512i c = _mm512_clmulepi64_epi128(a, b, 0x10);
                (void)c;
                return 0;
            }"
            HAVE_VPCLMULQDQ_INTRIN
        )
        set(CMAKE_REQUIRED_FLAGS)
    else()
        set(HAVE_VPCLMULQDQ_INTRIN OFF)
    endif()
endmacro()

macro(check_ppc_intrinsics)
    # Check if compiler supports AltiVec
    set(CMAKE_REQUIRED_FLAGS "-maltivec")
    check_c_source_compiles(
        "#include <altivec.h>
        int main(void)
        {
            vector int a = vec_splats(0);
            vector int b = vec_splats(0);
            a = vec_add(a, b);
            return 0;
        }"
        HAVE_ALTIVEC
        )
    set(CMAKE_REQUIRED_FLAGS)

    if(HAVE_ALTIVEC)
        set(PPCFLAGS "-maltivec")
    endif()

    set(CMAKE_REQUIRED_FLAGS "-maltivec -mno-vsx")
    check_c_source_compiles(
        "#include <altivec.h>
        int main(void)
        {
            vector int a = vec_splats(0);
            vector int b = vec_splats(0);
            a = vec_add(a, b);
            return 0;
        }"
        HAVE_NOVSX
        )
    set(CMAKE_REQUIRED_FLAGS)

    if(HAVE_NOVSX)
        set(PPCFLAGS "${PPCFLAGS} -mno-vsx")
    endif()

    # Check if we have what we need for AltiVec optimizations
    set(CMAKE_REQUIRED_FLAGS "${PPCFLAGS} ${NATIVEFLAG}")
    check_c_source_compiles(
        "#include <sys/auxv.h>
        #ifdef __FreeBSD__
        #include <machine/cpu.h>
        #endif
        int main() {
        #ifdef __FreeBSD__
            unsigned long hwcap;
            elf_aux_info(AT_HWCAP, &hwcap, sizeof(hwcap));
            return (hwcap & PPC_FEATURE_HAS_ALTIVEC);
        #else
            return (getauxval(AT_HWCAP) & PPC_FEATURE_HAS_ALTIVEC);
        #endif
        }"
        HAVE_VMX
    )
    set(CMAKE_REQUIRED_FLAGS)
endmacro()

macro(check_power8_intrinsics)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU" OR CMAKE_C_COMPILER_ID MATCHES "Clang")
        if(NOT NATIVEFLAG)
            set(POWER8FLAG "-mcpu=power8")
        endif()
    endif()
    # Check if we have what we need for POWER8 optimizations
    set(CMAKE_REQUIRED_FLAGS "${POWER8FLAG} ${NATIVEFLAG}")
    check_c_source_compiles(
        "#include <sys/auxv.h>
        #ifdef __FreeBSD__
        #include <machine/cpu.h>
        #endif
        int main() {
        #ifdef __FreeBSD__
            unsigned long hwcap;
            elf_aux_info(AT_HWCAP2, &hwcap, sizeof(hwcap));
            return (hwcap & PPC_FEATURE2_ARCH_2_07);
        #else
            return (getauxval(AT_HWCAP2) & PPC_FEATURE2_ARCH_2_07);
        #endif
        }"
        HAVE_POWER8_INTRIN
    )
    set(CMAKE_REQUIRED_FLAGS)
endmacro()

macro(check_rvv_intrinsics)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU" OR CMAKE_C_COMPILER_ID MATCHES "Clang")
        if(NOT NATIVEFLAG)
            set(RISCVFLAG "-march=rv64gcv")
        endif()
    endif()
    # Check whether compiler supports RVV
    set(CMAKE_REQUIRED_FLAGS "${RISCVFLAG} ${NATIVEFLAG}")
    check_c_source_compiles(
        "#include <riscv_vector.h>
        int main() { 
            return 0; 
        }" 
        HAVE_RVV_INTRIN
    )
    set(CMAKE_REQUIRED_FLAGS)
endmacro()

macro(check_s390_intrinsics)
    check_c_source_compiles(
        "#include <sys/auxv.h>
        #ifndef HWCAP_S390_VXRS
        #define HWCAP_S390_VXRS HWCAP_S390_VX
        #endif
        int main() {
            return (getauxval(AT_HWCAP) & HWCAP_S390_VXRS);
        }"
        HAVE_S390_INTRIN
    )
endmacro()

macro(check_power9_intrinsics)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU" OR CMAKE_C_COMPILER_ID MATCHES "Clang")
        if(NOT NATIVEFLAG)
            set(POWER9FLAG "-mcpu=power9")
        endif()
    endif()
    # Check if we have what we need for POWER9 optimizations
    set(CMAKE_REQUIRED_FLAGS "${POWER9FLAG} ${NATIVEFLAG}")
    check_c_source_compiles(
        "int main() {
            return 0;
        }"
        HAVE_POWER9_INTRIN
    )
    set(CMAKE_REQUIRED_FLAGS)
endmacro()

macro(check_sse2_intrinsics)
    if(CMAKE_C_COMPILER_ID MATCHES "Intel")
        if(CMAKE_HOST_UNIX OR APPLE)
            set(SSE2FLAG "-msse2")
        else()
            set(SSE2FLAG "/arch:SSE2")
        endif()
    elseif(MSVC)
        if(NOT "${ARCH}" MATCHES "x86_64")
            set(SSE2FLAG "/arch:SSE2")
        endif()
    elseif(CMAKE_C_COMPILER_ID MATCHES "GNU" OR CMAKE_C_COMPILER_ID MATCHES "Clang")
        if(NOT NATIVEFLAG)
            set(SSE2FLAG "-msse2")
        endif()
    endif()
    # Check whether compiler supports SSE2 intrinsics
    set(CMAKE_REQUIRED_FLAGS "${SSE2FLAG} ${NATIVEFLAG}")
    check_c_source_compile_or_run(
        "#include <immintrin.h>
        int main(void) {
            __m128i zero = _mm_setzero_si128();
            (void)zero;
            return 0;
        }"
        HAVE_SSE2_INTRIN
    )
    set(CMAKE_REQUIRED_FLAGS)
endmacro()

macro(check_ssse3_intrinsics)
    if(CMAKE_C_COMPILER_ID MATCHES "Intel")
        if(CMAKE_HOST_UNIX OR APPLE)
            set(SSSE3FLAG "-mssse3")
        else()
            set(SSSE3FLAG "/arch:SSSE3")
        endif()
    elseif(CMAKE_C_COMPILER_ID MATCHES "GNU" OR CMAKE_C_COMPILER_ID MATCHES "Clang")
        if(NOT NATIVEFLAG)
            set(SSSE3FLAG "-mssse3")
        endif()
    endif()
    # Check whether compiler supports SSSE3 intrinsics
    set(CMAKE_REQUIRED_FLAGS "${SSSE3FLAG} ${NATIVEFLAG}")
    check_c_source_compile_or_run(
        "#include <immintrin.h>
        int main(void) {
            __m128i u, v, w;
            u = _mm_set1_epi32(1);
            v = _mm_set1_epi32(2);
            w = _mm_hadd_epi32(u, v);
            (void)w;
            return 0;
        }"
        HAVE_SSSE3_INTRIN
    )
endmacro()

macro(check_sse42_intrinsics)
    if(CMAKE_C_COMPILER_ID MATCHES "Intel")
        if(CMAKE_HOST_UNIX OR APPLE)
            set(SSE42FLAG "-msse4.2")
        else()
            set(SSE42FLAG "/arch:SSE4.2")
        endif()
    elseif(CMAKE_C_COMPILER_ID MATCHES "GNU" OR CMAKE_C_COMPILER_ID MATCHES "Clang")
        if(NOT NATIVEFLAG)
            set(SSE42FLAG "-msse4.2")
        endif()
    endif()
    # Check whether compiler supports SSE4.2 CRC inline asm
    set(CMAKE_REQUIRED_FLAGS "${SSE42FLAG} ${NATIVEFLAG}")
    check_c_source_compile_or_run(
        "int main(void) {
            unsigned val = 0, h = 0;
        #if defined(_MSC_VER)
            { __asm mov edx, h __asm mov eax, val __asm crc32 eax, edx __asm mov h, eax }
        #else
            __asm__ __volatile__ ( \"crc32 %1,%0\" : \"+r\" (h) : \"r\" (val) );
        #endif
            return (int)h;
        }"
        HAVE_SSE42CRC_INLINE_ASM
    )
    # Check whether compiler supports SSE4.2 CRC intrinsics
    check_c_source_compile_or_run(
        "#include <immintrin.h>
        int main(void) {
            unsigned crc = 0;
            char c = 'c';
        #if defined(_MSC_VER)
            crc = _mm_crc32_u32(crc, c);
        #else
            crc = __builtin_ia32_crc32qi(crc, c);
        #endif
            (void)crc;
            return 0;
        }"
        HAVE_SSE42CRC_INTRIN
    )
    set(CMAKE_REQUIRED_FLAGS)
endmacro()

macro(check_vgfma_intrinsics)
    if(NOT NATIVEFLAG)
        set(VGFMAFLAG "-march=z13")
        if(CMAKE_C_COMPILER_ID MATCHES "GNU")
            set(VGFMAFLAG "${VGFMAFLAG} -mzarch")
        endif()
        if(CMAKE_C_COMPILER_ID MATCHES "Clang")
            set(VGFMAFLAG "${VGFMAFLAG} -fzvector")
        endif()
    endif()
    # Check whether compiler supports "VECTOR GALOIS FIELD MULTIPLY SUM AND ACCUMULATE" intrinsic
    set(CMAKE_REQUIRED_FLAGS "${VGFMAFLAG} ${NATIVEFLAG}")
    check_c_source_compiles(
        "#include <vecintrin.h>
        int main(void) {
            unsigned long long a __attribute__((vector_size(16))) = { 0 };
            unsigned long long b __attribute__((vector_size(16))) = { 0 };
            unsigned char c __attribute__((vector_size(16))) = { 0 };
            c = vec_gfmsum_accum_128(a, b, c);
            return c[0];
        }"
        HAVE_VGFMA_INTRIN FAIL_REGEX "not supported")
    set(CMAKE_REQUIRED_FLAGS)
endmacro()

macro(check_xsave_intrinsics)
    if(NOT NATIVEFLAG AND NOT MSVC)
        set(XSAVEFLAG "-mxsave")
    endif()
    set(CMAKE_REQUIRED_FLAGS "${XSAVEFLAG} ${NATIVEFLAG}")
    check_c_source_compiles(
        "#ifdef _WIN32
         #  include <intrin.h>
         #else
         #  include <x86gprintrin.h>
         #endif
         int main(void) {
             return _xgetbv(0);
         }"
        HAVE_XSAVE_INTRIN FAIL_REGEX "not supported")
    set(CMAKE_REQUIRED_FLAGS)
endmacro()
