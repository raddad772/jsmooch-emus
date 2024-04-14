//
// Created by RadDad772 on 3/10/24.
//

static u64 cR8(void *ptr, u32 addr) {
    return ((u8 *)ptr)[addr];
}

static u64 cR16(void *ptr, u32 addr) {
    return *(u16*)(((u8*)ptr)+addr);
}

static u64 cR32(void *ptr, u32 addr) {
    return *(u32*)(((u8*)ptr)+addr);
}

static u64 cR64(void *ptr, u32 addr) {
    return *(u64*)(((u8*)ptr)+addr);
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
