/**
 * \file            libc_allocator.h
 * \brief           Explicit libc heap allocator backend
 * \author          X-Gen Lab
 */

#ifndef XGM_LIBC_ALLOCATOR_H
#define XGM_LIBC_ALLOCATOR_H

#include <xgen/memory/allocator.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Requires explicitly linking xgc::libc_allocator. */
/**
 * \brief           Obtain the explicitly linked libc allocator service
 * \return          Process-lifetime allocator descriptor
 */
const xgm_allocator_t* xgm_allocator_libc(void);

#ifdef __cplusplus
}
#endif
#endif
