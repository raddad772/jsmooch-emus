//
// Created by RadDad772 on 3/10/24.
//

#include "helpers/int.h"

static u64 cR8(void *ptr, u32 addr) {
    return ((u8 *)ptr)[addr];
}

static u64 cR16(void *ptr, u32 addr) {
    return *(u16*)(((u8*)ptr)+addr);
}

static u64 cR16_be(void *ptr, u32 addr) {
    u16 a = *(u16*)(((u8*)ptr)+addr);
    return bswap_16(a);
}

static u64 cR32(void *ptr, u32 addr) {
    return *(u32*)(((u8*)ptr)+addr);
}

static u64 cR32_be(void *ptr, u32 addr) {
    u32 a = *(u32*)(((u8*)ptr)+addr);
    return bswap_32(a);
}

static u64 cR64(void *ptr, u32 addr) {
    return *(u64*)(((u8*)ptr)+addr);
}

static u64 cR64_be(void *ptr, u32 addr) {
    u64 a = *(u64*)(((u8*)ptr)+addr);
    return bswap_64(a);
}

static void cW8(void *ptr, u32 addr, u64 val) {
    *(((u8*)ptr)+addr) = val;
}

static void cW16(void *ptr, u32 addr, u64 val) {
    *(u16 *)(((u8*)ptr)+addr) = val;
}

static void cW32(void *ptr, u32 addr, u64 val) {
    *(u32 *)(((u8*)ptr)+addr) = val;
}

static void cW64(void *ptr, u32 addr, u64 val) {
    *(u64 *)(((u8*)ptr)+addr) = val;
}

static u64 (*cR[9])(void *, u32) = {
        NULL,
        &cR8,
        &cR16,
        NULL,
        &cR32,
        NULL,
        NULL,
        NULL,
        &cR64
};

static u64 (*cR_be[9])(void *, u32) = {
        NULL,
        &cR8,
        &cR16_be,
        NULL,
        &cR32_be,
        NULL,
        NULL,
        NULL,
        &cR64_be
};


static void (*cW[9])(void *, u32, u64) = {
        NULL,
        &cW8,
        &cW16,
        NULL,
        &cW32,
        NULL,
        NULL,
        NULL,
        &cW64
};
