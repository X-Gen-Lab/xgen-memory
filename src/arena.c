/**
 * \file            arena.c
 * \brief           Overflow-safe aligned allocation from a shared lifetime
 */
#include <xgen/memory/arena.h>

xgs_status_t xgm_arena_init(xgm_arena_t* arena, void* storage, size_t capacity) {
    if (arena == NULL || (storage == NULL && capacity != 0U)) {
        return XGS_INVALID_ARGUMENT;
    }
    if (capacity > UINTPTR_MAX - (uintptr_t)storage) {
        return XGS_CAPACITY;
    }
    *arena = (xgm_arena_t){storage, capacity, 0U, 0U};
    return XGS_OK;
}

void* xgm_arena_alloc(xgm_arena_t* arena, size_t size, size_t alignment) {
    if (arena == NULL || arena->storage == NULL || size == 0U ||
        alignment == 0U || (alignment & (alignment - 1U)) != 0U) {
        return NULL;
    }
    uintptr_t address = (uintptr_t)arena->storage + arena->used;
    size_t padding = (alignment - address % alignment) % alignment;
    size_t available = arena->capacity - arena->used;
    if (padding > available || size > available - padding) {
        return NULL;
    }
    void* block = arena->storage + arena->used + padding;
    arena->used += padding + size;
    if (arena->used > arena->peak_used) {
        arena->peak_used = arena->used;
    }
    return block;
}

void xgm_arena_reset(xgm_arena_t* arena) {
    if (arena != NULL) {
        arena->used = 0U;
    }
}
void xgm_arena_deinit(xgm_arena_t* arena) {
    if (arena != NULL) {
        *arena = (xgm_arena_t){0};
    }
}
size_t xgm_arena_used(const xgm_arena_t* arena) {
    return arena == NULL ? 0U : arena->used;
}
size_t xgm_arena_remaining(const xgm_arena_t* arena) {
    return arena == NULL ? 0U : arena->capacity - arena->used;
}
size_t xgm_arena_peak_used(const xgm_arena_t* arena) {
    return arena == NULL ? 0U : arena->peak_used;
}
