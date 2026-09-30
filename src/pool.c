/**
 * \file            pool.c
 * \brief           Caller-owned fixed block pool with bounded capacity
 * \author          X-Gen Lab
 */

#include <xgen/memory/pool.h>

#include <string.h>

/**
 * \brief           Allocate one pool block for a fitting byte request
 */
static void *pool_service_alloc(void *ctx, size_t size)
{
    xgm_pool_t *pool = ctx;
    if (pool == NULL || size == 0U || size > pool->block_size) {
        return NULL;
    }
    return xgm_pool_alloc(pool);
}

/**
 * \brief           Release an allocation to its explicit pool context
 */
static void pool_service_free(void *ctx, void *ptr)
{
    (void) xgm_pool_free(ctx, ptr);
}

xgm_allocator_t xgm_pool_allocator(xgm_pool_t *pool)
{
    return (xgm_allocator_t) {pool, pool_service_alloc, pool_service_free};
}

static void store_next(void *block, void *next)
{
    memcpy(block, &next, sizeof(next));
}

static void *load_next(const void *block)
{
    void *next;
    memcpy(&next, block, sizeof(next));
    return next;
}

xgs_status_t xgm_pool_init(xgm_pool_t *pool, void *storage, size_t storage_size,
                           size_t block_size, size_t alignment, size_t count)
{
    if (pool == NULL || storage == NULL || alignment == 0U ||
        (alignment & (alignment - 1U)) != 0U || block_size < sizeof(void *) ||
        block_size % alignment != 0U || (uintptr_t) storage % alignment != 0U ||
        count == 0U) {
        return XGS_INVALID_ARGUMENT;
    }
    if (count > SIZE_MAX / block_size || count * block_size > storage_size) {
        return XGS_CAPACITY;
    }

    xgm_pool_t initialized = {
        (uint8_t *) storage, NULL, block_size, count, count, 0U};
    for (size_t i = 0U; i < count; ++i) {
        void *block = initialized.storage + i * block_size;
        store_next(block, initialized.free_list);
        initialized.free_list = block;
    }
    *pool = initialized;
    return XGS_OK;
}

void xgm_pool_deinit(xgm_pool_t *pool)
{
    if (pool != NULL) {
        *pool = (xgm_pool_t) {0};
    }
}

void *xgm_pool_alloc(xgm_pool_t *pool)
{
    if (pool == NULL || pool->free_list == NULL || pool->free_count == 0U) {
        return NULL;
    }
    void *block = pool->free_list;
    pool->free_list = load_next(block);
    --pool->free_count;
    size_t used = pool->block_count - pool->free_count;
    if (used > pool->peak_used) {
        pool->peak_used = used;
    }
    return block;
}

bool xgm_pool_contains(const xgm_pool_t *pool, const void *ptr)
{
    if (pool == NULL || pool->storage == NULL || ptr == NULL ||
        pool->block_size == 0U ||
        pool->block_count > SIZE_MAX / pool->block_size) {
        return false;
    }
    uintptr_t address = (uintptr_t) ptr;
    uintptr_t start = (uintptr_t) pool->storage;
    if (address < start) {
        return false;
    }
    uintptr_t offset = address - start;
    return offset < pool->block_count * pool->block_size &&
           offset % pool->block_size == 0U;
}

xgs_status_t xgm_pool_free(xgm_pool_t *pool, void *ptr)
{
    if (pool == NULL || pool->storage == NULL) {
        return XGS_INVALID_ARGUMENT;
    }
    if (ptr == NULL) {
        return XGS_OK;
    }
    if (!xgm_pool_contains(pool, ptr)) {
        return XGS_INVALID_ARGUMENT;
    }

    void *current = pool->free_list;
    for (size_t scanned = 0U; current != NULL && scanned < pool->block_count;
         ++scanned) {
        if (current == ptr) {
            return XGS_ALREADY_EXISTS;
        }
        current = load_next(current);
    }
    if (pool->free_count >= pool->block_count) {
        return XGS_INVALID_ARGUMENT;
    }
    store_next(ptr, pool->free_list);
    pool->free_list = ptr;
    ++pool->free_count;
    return XGS_OK;
}

size_t xgm_pool_free_count(const xgm_pool_t *pool)
{
    return pool == NULL ? 0U : pool->free_count;
}

size_t xgm_pool_used_count(const xgm_pool_t *pool)
{
    return pool == NULL ? 0U : pool->block_count - pool->free_count;
}

size_t xgm_pool_peak_used(const xgm_pool_t *pool)
{
    return pool == NULL ? 0U : pool->peak_used;
}

void xgm_pool_reset_stats(xgm_pool_t *pool)
{
    if (pool != NULL) {
        pool->peak_used = xgm_pool_used_count(pool);
    }
}
