/**
 * \file            size_class_allocator.c
 * \brief           Fixed size classes backed by caller-owned block pools
 * \author          X-Gen Lab
 */

#include <xgen/memory/size_class_allocator.h>

static xgs_status_t rounded_size(size_t size, size_t *result)
{
    const size_t alignment = _Alignof(max_align_t);
    if (size == 0U) {
        return XGS_INVALID_ARGUMENT;
    }
    if (size < sizeof(void *)) {
        size = sizeof(void *);
    }
    const size_t padding = (alignment - size % alignment) % alignment;
    if (size > SIZE_MAX - padding) {
        return XGS_CAPACITY;
    }
    *result = size + padding;
    return XGS_OK;
}

xgs_status_t xgm_size_class_measure(const xgm_size_class_spec_t *specs,
                                    size_t count, size_t *storage_size,
                                    size_t *storage_alignment)
{
    if (specs == NULL || count == 0U || storage_size == NULL ||
        storage_alignment == NULL || storage_size == storage_alignment) {
        return XGS_INVALID_ARGUMENT;
    }
    size_t total = 0U;
    for (size_t i = 0U; i < count; ++i) {
        size_t block_size;
        xgs_status_t status = rounded_size(specs[i].block_size, &block_size);
        if (status != XGS_OK) {
            return status;
        }
        if (specs[i].block_count == 0U) {
            return XGS_INVALID_ARGUMENT;
        }
        if (specs[i].block_count > (SIZE_MAX - total) / block_size) {
            return XGS_CAPACITY;
        }
        total += block_size * specs[i].block_count;
    }
    *storage_size = total;
    *storage_alignment = _Alignof(max_align_t);
    return XGS_OK;
}

static void *service_alloc(void *ctx, size_t size)
{
    return xgm_size_class_alloc((xgm_size_class_allocator_t *) ctx, size);
}

static void service_free(void *ctx, void *ptr)
{
    (void) xgm_size_class_free((xgm_size_class_allocator_t *) ctx, ptr);
}

xgs_status_t xgm_size_class_init(xgm_size_class_allocator_t *state,
                                 xgm_pool_t *pools, size_t pool_capacity,
                                 const xgm_size_class_spec_t *specs,
                                 size_t count, void *storage,
                                 size_t storage_size)
{
    if (state == NULL || pools == NULL || storage == NULL) {
        return XGS_INVALID_ARGUMENT;
    }
    size_t required;
    size_t alignment;
    xgs_status_t status =
        xgm_size_class_measure(specs, count, &required, &alignment);
    if (status != XGS_OK) {
        return status;
    }
    if ((uintptr_t) storage % alignment != 0U) {
        return XGS_INVALID_ARGUMENT;
    }
    if (pool_capacity < count || storage_size < required) {
        return XGS_CAPACITY;
    }

    /* The complete layout has been validated, so these pool initializations
     * cannot fail for disjoint caller objects satisfying the contract. */
    uint8_t *cursor = (uint8_t *) storage;
    for (size_t i = 0U; i < count; ++i) {
        size_t block_size = 0U;
        (void) rounded_size(specs[i].block_size, &block_size);
        size_t bytes = block_size * specs[i].block_count;
        (void) xgm_pool_init(&pools[i], cursor, bytes, block_size, alignment,
                             specs[i].block_count);
        cursor += bytes;
    }
    *state = (xgm_size_class_allocator_t) {
        {state, service_alloc, service_free}, pools, count};
    return XGS_OK;
}

void *xgm_size_class_alloc(xgm_size_class_allocator_t *state, size_t size)
{
    if (state == NULL || state->pools == NULL || size == 0U) {
        return NULL;
    }
    size_t selected_size = SIZE_MAX;
    bool found = false;
    for (size_t i = 0U; i < state->pool_count; ++i) {
        size_t block_size = state->pools[i].block_size;
        if (block_size >= size && (!found || block_size < selected_size)) {
            selected_size = block_size;
            found = true;
        }
    }
    if (!found) {
        return NULL;
    }
    for (size_t i = 0U; i < state->pool_count; ++i) {
        if (state->pools[i].block_size == selected_size) {
            void *block = xgm_pool_alloc(&state->pools[i]);
            if (block != NULL) {
                return block;
            }
        }
    }
    return NULL;
}

xgs_status_t xgm_size_class_free(xgm_size_class_allocator_t *state, void *ptr)
{
    if (state == NULL || state->pools == NULL) {
        return XGS_INVALID_ARGUMENT;
    }
    if (ptr == NULL) {
        return XGS_OK;
    }
    for (size_t i = 0U; i < state->pool_count; ++i) {
        if (xgm_pool_contains(&state->pools[i], ptr)) {
            return xgm_pool_free(&state->pools[i], ptr);
        }
    }
    return XGS_INVALID_ARGUMENT;
}

xgs_status_t xgm_size_class_deinit(xgm_size_class_allocator_t *state)
{
    if (state == NULL || state->pools == NULL) {
        return XGS_INVALID_ARGUMENT;
    }
    for (size_t i = 0U; i < state->pool_count; ++i) {
        if (xgm_pool_used_count(&state->pools[i]) != 0U) {
            return XGS_BUSY;
        }
    }
    for (size_t i = 0U; i < state->pool_count; ++i) {
        xgm_pool_deinit(&state->pools[i]);
    }
    *state = (xgm_size_class_allocator_t) {0};
    return XGS_OK;
}
