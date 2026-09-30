/**
 * \file            size_class_allocator.c
 * \brief           Shared strict and fallback size-class allocation engine
 */
#include <xgen/memory/size_class_allocator.h>

static bool valid_policy(xgm_size_class_policy_t policy) {
    return policy == XGM_SIZE_CLASS_STRICT || policy == XGM_SIZE_CLASS_FALLBACK;
}

static xgs_status_t layout(size_t size, size_t count, size_t* block,
                           size_t* bytes) {
    if (count == 0U) {
        *block = 0U;
        *bytes = 0U;
        return XGS_OK;
    }
    return xgm_pool_measure(size, _Alignof(max_align_t), count, block, bytes);
}

xgs_status_t xgm_size_class_measure(const xgm_size_class_spec_t* specs,
    size_t count, size_t* storage_size, size_t* storage_alignment) {
    if (specs == NULL || count == 0U || storage_size == NULL ||
        storage_alignment == NULL || storage_size == storage_alignment) {
        return XGS_INVALID_ARGUMENT;
    }
    size_t total = 0U;
    for (size_t i = 0U; i < count; ++i) {
        size_t block, bytes;
        xgs_status_t status = layout(specs[i].block_size, specs[i].block_count,
                                    &block, &bytes);
        if (status != XGS_OK) {
            return status;
        }
        if (bytes > SIZE_MAX - total) {
            return XGS_CAPACITY;
        }
        total += bytes;
    }
    *storage_size = total;
    *storage_alignment = _Alignof(max_align_t);
    return XGS_OK;
}

static void* service_alloc(void* ctx, size_t size) {
    return xgm_size_class_alloc(ctx, size);
}
static void service_free(void* ctx, void* ptr) {
    (void)xgm_size_class_free(ctx, ptr);
}
static void finish_init(xgm_size_class_allocator_t* state, xgm_pool_t* pools,
                        size_t count, xgm_size_class_policy_t policy) {
    *state = (xgm_size_class_allocator_t){0};
    state->service = (xgm_allocator_t){state, service_alloc, service_free};
    state->pools = pools;
    state->pool_count = count;
    state->policy = policy;
}

xgs_status_t xgm_size_class_init_ex(xgm_size_class_allocator_t* state,
    xgm_pool_t* pools, size_t pool_capacity, const xgm_size_class_spec_t* specs,
    size_t count, void* storage, size_t storage_size, xgm_size_class_policy_t policy) {
    if (state == NULL || pools == NULL || !valid_policy(policy)) {
        return XGS_INVALID_ARGUMENT;
    }
    size_t required, alignment;
    xgs_status_t status = xgm_size_class_measure(specs, count, &required, &alignment);
    if (status != XGS_OK) {
        return status;
    }
    if (required != 0U && (storage == NULL || (uintptr_t)storage % alignment != 0U)) {
        return XGS_INVALID_ARGUMENT;
    }
    if (pool_capacity < count || storage_size < required) {
        return XGS_CAPACITY;
    }
    uint8_t* cursor = storage;
    for (size_t i = 0U; i < count; ++i) {
        size_t block = 0U, bytes = 0U;
        (void)layout(specs[i].block_size, specs[i].block_count, &block, &bytes);
        if (bytes == 0U) {
            pools[i] = (xgm_pool_t){0};
        } else {
            (void)xgm_pool_init(&pools[i], cursor, bytes, block, alignment,
                                specs[i].block_count);
            cursor += bytes;
        }
    }
    finish_init(state, pools, count, policy);
    return XGS_OK;
}

xgs_status_t xgm_size_class_init(xgm_size_class_allocator_t* state,
    xgm_pool_t* pools, size_t pool_capacity, const xgm_size_class_spec_t* specs,
    size_t count, void* storage, size_t storage_size) {
    return xgm_size_class_init_ex(state, pools, pool_capacity, specs, count,
                                  storage, storage_size, XGM_SIZE_CLASS_STRICT);
}

xgs_status_t xgm_size_class_init_buffers(xgm_size_class_allocator_t* state,
    xgm_pool_t* pools, size_t pool_capacity, const xgm_size_class_buffer_t* buffers,
    size_t count, xgm_size_class_policy_t policy) {
    if (state == NULL || pools == NULL || buffers == NULL || count == 0U ||
        !valid_policy(policy)) {
        return XGS_INVALID_ARGUMENT;
    }
    if (pool_capacity < count) {
        return XGS_CAPACITY;
    }
    size_t total = 0U;
    for (size_t i = 0U; i < count; ++i) {
        size_t block, bytes;
        xgs_status_t status = layout(buffers[i].block_size, buffers[i].block_count,
                                    &block, &bytes);
        if (status != XGS_OK) {
            return status;
        }
        if (bytes == 0U) {
            continue;
        }
        uintptr_t address = (uintptr_t)buffers[i].storage;
        if (buffers[i].storage == NULL || address % _Alignof(max_align_t) != 0U) {
            return XGS_INVALID_ARGUMENT;
        }
        if (bytes > buffers[i].storage_size || bytes > SIZE_MAX - total ||
            bytes > UINTPTR_MAX - address) {
            return XGS_CAPACITY;
        }
        total += bytes;
        for (size_t j = 0U; j < i; ++j) {
            size_t other_block = 0U, other_bytes = 0U;
            (void)layout(buffers[j].block_size, buffers[j].block_count,
                         &other_block, &other_bytes);
            uintptr_t other = (uintptr_t)buffers[j].storage;
            if (other_bytes != 0U &&
                (address <= other ? other - address < bytes : address - other < other_bytes)) {
                return XGS_INVALID_ARGUMENT;
            }
        }
    }
    for (size_t i = 0U; i < count; ++i) {
        size_t block = 0U, bytes = 0U;
        (void)layout(buffers[i].block_size, buffers[i].block_count, &block, &bytes);
        pools[i] = (xgm_pool_t){0};
        if (bytes != 0U) {
            (void)xgm_pool_init(&pools[i], buffers[i].storage, bytes, block,
                                _Alignof(max_align_t), buffers[i].block_count);
        }
    }
    finish_init(state, pools, count, policy);
    return XGS_OK;
}

xgs_status_t xgm_size_class_init_owned(xgm_size_class_owned_t* state,
    xgm_pool_t* pools, size_t pool_capacity, const xgm_size_class_spec_t* specs,
    size_t count, const xgm_allocator_t* backend, xgm_size_class_policy_t policy) {
    if (state == NULL || pools == NULL || !valid_policy(policy)) {
        return XGS_INVALID_ARGUMENT;
    }
    size_t required, alignment;
    xgs_status_t status = xgm_size_class_measure(specs, count, &required, &alignment);
    if (status != XGS_OK) {
        return status;
    }
    if (pool_capacity < count) {
        return XGS_CAPACITY;
    }
    if (required != 0U && !xgm_allocator_is_valid(backend)) {
        return XGS_INVALID_ARGUMENT;
    }
    xgm_allocator_t copied = backend != NULL ? *backend : (xgm_allocator_t){0};
    void* storage = required != 0U ? xgm_alloc(&copied, required) : NULL;
    if (required != 0U && storage == NULL) {
        return XGS_NO_MEMORY;
    }
    status = xgm_size_class_init_ex(&state->allocator, pools, pool_capacity, specs, count,
                                    storage, required, policy);
    if (status != XGS_OK) {
        xgm_free(&copied, storage);
        return status;
    }
    state->backend = copied;
    state->storage = storage;
    return XGS_OK;
}

void* xgm_size_class_alloc(xgm_size_class_allocator_t* state, size_t size) {
    if (state == NULL || state->pools == NULL || size == 0U) {
        return NULL;
    }
    size_t selected = SIZE_MAX;
    for (size_t i = 0U; i < state->pool_count; ++i) {
        xgm_pool_t* pool = &state->pools[i];
        if (pool->block_count != 0U && pool->block_size >= size &&
            (state->policy == XGM_SIZE_CLASS_STRICT || pool->free_count != 0U) &&
            (selected == SIZE_MAX || pool->block_size < state->pools[selected].block_size)) {
            selected = i;
        }
    }
    if (selected == SIZE_MAX) {
        return NULL;
    }
    size_t block_size = state->pools[selected].block_size;
    for (size_t i = 0U; i < state->pool_count; ++i) {
        if (state->pools[i].block_size == block_size) {
            void* block = xgm_pool_alloc(&state->pools[i]);
            if (block != NULL) {
                state->used_memory += block_size;
                if (state->used_memory > state->peak_memory) {
                    state->peak_memory = state->used_memory;
                }
                return block;
            }
        }
    }
    return NULL;
}

xgs_status_t xgm_size_class_free(xgm_size_class_allocator_t* state, void* ptr) {
    if (state == NULL || state->pools == NULL) {
        return XGS_INVALID_ARGUMENT;
    }
    if (ptr == NULL) {
        return XGS_OK;
    }
    for (size_t i = 0U; i < state->pool_count; ++i) {
        if (xgm_pool_contains(&state->pools[i], ptr)) {
            xgs_status_t status = xgm_pool_free(&state->pools[i], ptr);
            if (status == XGS_OK) {
                state->used_memory -= state->pools[i].block_size;
            }
            return status;
        }
    }
    return XGS_INVALID_ARGUMENT;
}

xgs_status_t xgm_size_class_deinit(xgm_size_class_allocator_t* state) {
    if (state == NULL || state->pools == NULL) {
        return XGS_INVALID_ARGUMENT;
    }
    if (state->used_memory != 0U) {
        return XGS_BUSY;
    }
    for (size_t i = 0U; i < state->pool_count; ++i) {
        (void)xgm_pool_deinit(&state->pools[i]);
    }
    *state = (xgm_size_class_allocator_t){0};
    return XGS_OK;
}

xgs_status_t xgm_size_class_deinit_owned(xgm_size_class_owned_t* state) {
    if (state == NULL) {
        return XGS_INVALID_ARGUMENT;
    }
    xgs_status_t status = xgm_size_class_deinit(&state->allocator);
    if (status != XGS_OK) {
        return status;
    }
    xgm_free(&state->backend, state->storage);
    *state = (xgm_size_class_owned_t){0};
    return XGS_OK;
}

size_t xgm_size_class_used_memory(const xgm_size_class_allocator_t* state) {
    return state != NULL ? state->used_memory : 0U;
}
size_t xgm_size_class_peak_memory(const xgm_size_class_allocator_t* state) {
    return state != NULL ? state->peak_memory : 0U;
}
void xgm_size_class_reset_stats(xgm_size_class_allocator_t* state) {
    if (state != NULL) {
        state->peak_memory = state->used_memory;
        for (size_t i = 0U; i < state->pool_count; ++i) {
            xgm_pool_reset_stats(&state->pools[i]);
        }
    }
}
