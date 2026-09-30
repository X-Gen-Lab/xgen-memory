/**
 * \file            arena.h
 * \brief           Caller-owned bounded storage with one shared lifetime
 */
#ifndef XGM_ARENA_H
#define XGM_ARENA_H
#include <stddef.h>
#include <stdint.h>
#include <xgen/status/status.h>
#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief Arena state borrowing fixed caller storage.
 * \note All calls require external serialization. State and storage remain
 * stable while in use. No heap, callbacks, waiting or implicit synchronization
 * are used. Every operation takes constant work. Returned blocks must not be
 * individually freed; reset/deinit invalidate all outstanding blocks.
 */
typedef struct {
    uint8_t* storage; /**< Borrowed bytes, disjoint from this descriptor. */
    size_t capacity; /**< Total storage bytes. */
    size_t used; /**< Used bytes including alignment padding. */
    size_t peak_used; /**< Highest used bytes since initialization. */
} xgm_arena_t;

/**
 * \brief Initialize a shared allocation lifetime without touching storage.
 * \param[out] arena: Stable caller-owned state, disjoint from storage.
 * \param[in,out] storage: Writable storage; NULL only for zero capacity.
 * \param[in] capacity: Storage size in bytes.
 * \return XGS_OK, XGS_INVALID_ARGUMENT, or XGS_CAPACITY for address overflow.
 * \note Failure leaves state unchanged. Reinitialization is allowed only
 * after all users have relinquished previous allocations. No storage is freed.
 */
xgs_status_t xgm_arena_init(xgm_arena_t* arena, void* storage, size_t capacity);

/**
 * \brief Allocate bytes with an explicit power-of-two alignment.
 * \param[in,out] arena: Initialized arena.
 * \param[in] size: Nonzero requested bytes.
 * \param[in] alignment: Nonzero power of two; larger than max_align_t is allowed.
 * \return Aligned block, or NULL on invalid input, overflow or exhaustion.
 * \note Padding consumes capacity. Failure changes no state. The returned
 * block remains valid until reset/deinit; storage need not start aligned.
 */
void* xgm_arena_alloc(xgm_arena_t* arena, size_t size, size_t alignment);

/** \brief Invalidate every allocation and reuse storage, preserving the peak.
 * \param[in,out] arena: Arena, or NULL.
 * \note Caller must first stop every user, including DMA and interrupt users.
 */
void xgm_arena_reset(xgm_arena_t* arena);
/** \brief Invalidate state and allocations without erasing borrowed storage.
 * \param[in,out] arena: Arena, or NULL; all users must already be stopped.
 */
void xgm_arena_deinit(xgm_arena_t* arena);
/** \brief Query consumed bytes including alignment padding.
 * \param[in] arena: Arena, or NULL.
 * \return Consumed bytes, or zero for NULL.
 */
size_t xgm_arena_used(const xgm_arena_t* arena);
/** \brief Query remaining raw bytes before request-specific alignment.
 * \param[in] arena: Arena, or NULL.
 * \return Remaining bytes, or zero for NULL.
 */
size_t xgm_arena_remaining(const xgm_arena_t* arena);
/** \brief Query maximum consumed bytes since initialization.
 * \param[in] arena: Arena, or NULL.
 * \return Peak bytes, or zero for NULL.
 */
size_t xgm_arena_peak_used(const xgm_arena_t* arena);
#ifdef __cplusplus
}
#endif
#endif
