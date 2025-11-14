/**
 * @file buffer.h
 * @brief Simple fixed-size buffer descriptors and pool management.
 *
 * This module provides:
 *  - A single buffer descriptor (@ref buffer_st)
 *  - A small buffer pool abstraction (@ref buffer_pool_st)
 *  - A higher-level context for "N buffers from one memory block"
 *    (@ref buffer_array_ctx_st), useful for DMA RX/TX rings.
 */

#ifndef BUFFER_H_
#define BUFFER_H_

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Single fixed-size buffer descriptor.
 *
 * The descriptor does not own the memory; it only points to it.
 * Lifetime and allocation of @ref data_u8p are managed by the caller.
 */
typedef struct
{
    uint8_t       *data_u8p;         /**< Backing memory pointer. */
    size_t         capacity_bytes;   /**< Capacity in bytes for this buffer. */

    volatile bool  is_available;     /**< True when buffer is free for reuse. */
    bool           is_initialized;   /**< True after @ref buffer_init was called. */
} buffer_st;

/**
 * @brief Small pool of buffer descriptors.
 *
 * The pool itself does not allocate buffers. It is configured to work
 * over a caller-provided array of @ref buffer_st.
 */
typedef struct
{
    buffer_st *buffer_array_sa;      /**< Array of buffer descriptors. */
    size_t     buffer_count;         /**< Number of elements in @ref buffer_array_sa. */

    bool       is_initialized;       /**< True after @ref buffer_pool_init was called. */
} buffer_pool_st;

/**
 * @brief Context for a pool bound to a contiguous memory block.
 *
 * This helper ties together:
 *  - An array of descriptors
 *  - A flat memory block
 *  - A buffer pool
 *
 * It is useful when you have N fixed-size buffers carved out of a single memory region,
 * for example UART DMA RX buffers.
 */
typedef struct
{
    buffer_pool_st pool_s;           /**< Underlying buffer pool. */

    buffer_st *buffer_array_sa;      /**< Descriptor array (same as pool_s.buffer_array_sa). */
    uint8_t   *memory_block_u8p;     /**< Flat memory region: buffer_count * buffer_size. */

    size_t     buffer_count;         /**< Number of buffers in the array. */
    size_t     buffer_size;          /**< Size of each buffer in bytes. */

    bool       is_initialized;       /**< True after @ref buffer_array_ctx_init was called. */
} buffer_array_ctx_st;

/* -------------------------------------------------------------------------- */
/* Single buffer API                                                          */
/* -------------------------------------------------------------------------- */

/**
 * @brief Initialize a single buffer descriptor.
 *
 * @param[in,out] buffer_sp       Pointer to buffer descriptor to initialize.
 * @param[in]     memory_u8p      Pointer to backing memory region.
 * @param[in]     capacity_bytes  Capacity of the buffer in bytes.
 *
 * If @p buffer_sp is NULL, the function returns immediately.
 * If @p memory_u8p is NULL or @p capacity_bytes is zero, the buffer is
 * marked as initialized but not available for acquisition.
 */
void buffer_init(buffer_st *buffer_sp, uint8_t *memory_u8p, size_t capacity_bytes);

/**
 * @brief Get the backing memory pointer and optionally its capacity.
 *
 * @param[in]  buffer_csp            Pointer to buffer descriptor (could be NULL).
 * @param[out] capacity_bytes_out_p  Optional pointer to store capacity in bytes.
 *
 * @return Pointer to the backing memory, or NULL if @p buffer_csp is NULL or
 *         the buffer is not initialized.
 *
 * If @p capacity_bytes_out_p is not NULL, it is set to the buffer capacity or
 * zero if @p buffer_csp is NULL or not initialized.
 */
uint8_t *buffer_data(buffer_st const *buffer_csp, size_t *capacity_bytes_out_p);

/**
 * @brief Mark a buffer as free and available for reuse.
 *
 * @param[in,out] buffer_sp  Pointer to buffer descriptor.
 *
 * If @p buffer_sp is NULL or not initialized, the function does nothing.
 */
void buffer_mark_free(buffer_st *buffer_sp);

/**
 * @brief Mark a buffer as in-use.
 *
 * @param[in,out] buffer_sp  Pointer to buffer descriptor.
 *
 * If @p buffer_sp is NULL or not initialized, the function does nothing.
 */
void buffer_mark_in_use(buffer_st *buffer_sp);

/* -------------------------------------------------------------------------- */
/* Pool API                                                                   */
/* -------------------------------------------------------------------------- */

/**
 * @brief Initialize a buffer pool over an existing array of descriptors.
 *
 * @param[in,out] pool_sp          Pointer to pool object.
 * @param[in]     buffer_array_sa  Pointer to first element of descriptor array.
 * @param[in]     buffer_count     Number of descriptors in @p buffer_array_sa.
 *
 * This function does not call @ref buffer_init on individual descriptors.
 * The caller is responsible for initializing each @ref buffer_st via
 * @ref buffer_init before using @ref buffer_pool_acquire.
 */
void buffer_pool_init(buffer_pool_st *pool_sp, buffer_st *buffer_array_sa, size_t buffer_count);

/**
 * @brief Acquire any free buffer from the pool.
 *
 * @param[in,out] pool_sp  Pointer to an initialized pool.
 *
 * @return Pointer to a buffer descriptor that has been marked in-use,
 *         or NULL if no free buffer is available or the pool is invalid.
 */
buffer_st *buffer_pool_acquire(buffer_pool_st *pool_sp);

/**
 * @brief Find a buffer descriptor by its backing memory pointer.
 *
 * @param[in,out] pool_sp    Pointer to an initialized pool.
 * @param[in]     memory_u8p Backing memory pointer to search for.
 *
 * @return Pointer to matching buffer descriptor, or NULL if not found.
 */
buffer_st *buffer_pool_find(buffer_pool_st *pool_sp, uint8_t *memory_u8p);

/**
 * @brief Release a buffer by its backing memory pointer.
 *
 * @param[in,out] pool_sp    Pointer to an initialized pool.
 * @param[in]     memory_u8p Backing memory pointer previously used by DMA.
 *
 * @return true  if a matching buffer was found and marked free.
 * @return false if no matching buffer was found or inputs are invalid.
 */
bool buffer_pool_release_by_ptr(buffer_pool_st *pool_sp, uint8_t *memory_u8p);

/**
 * @brief Mark all buffers in the pool as free.
 *
 * @param[in,out] pool_sp  Pointer to an initialized pool.
 *
 * Buffers that were never initialized via @ref buffer_init are left untouched.
 */
void buffer_pool_mark_all_free(buffer_pool_st *pool_sp);

/* -------------------------------------------------------------------------- */
/* Buffer array context API                                                   */
/* -------------------------------------------------------------------------- */

/**
 * @brief Initialize a buffer array context over a flat memory block.
 *
 * @param[in,out] ctx_sp            Pointer to context object.
 * @param[in]     buffer_array_sa   Array of @ref buffer_st descriptors.
 * @param[in]     memory_block_u8p  Flat memory block of size
 *                                  @p buffer_count * @p buffer_size bytes.
 * @param[in]     buffer_count      Number of buffers.
 * @param[in]     buffer_size       Size of each buffer in bytes.
 *
 * This function:
 *  - Initializes each buffer descriptor with @ref buffer_init.
 *  - Initializes the internal @ref buffer_pool_st.
 *  - Marks the context as initialized.
 */
void buffer_array_ctx_init(buffer_array_ctx_st *ctx_sp,
                           buffer_st *buffer_array_sa,
                           uint8_t *memory_block_u8p,
                           size_t buffer_count,
                           size_t buffer_size);

/**
 * @brief Acquire a free buffer from a buffer array context.
 *
 * @param[in,out] ctx_sp  Pointer to an initialized context.
 *
 * @return Pointer to a buffer descriptor, or NULL if none are free or
 *         the context is invalid.
 */
buffer_st *buffer_array_acquire(buffer_array_ctx_st *ctx_sp);

/**
 * @brief Find a buffer descriptor within a context by memory pointer.
 *
 * @param[in,out] ctx_sp      Pointer to an initialized context.
 * @param[in]     memory_u8p  Backing memory pointer.
 *
 * @return Pointer to matching buffer descriptor, or NULL if not found.
 */
buffer_st *buffer_array_find_by_ptr(buffer_array_ctx_st *ctx_sp, uint8_t *memory_u8p);

/**
 * @brief Release a buffer within a context by its memory pointer.
 *
 * @param[in,out] ctx_sp      Pointer to an initialized context.
 * @param[in]     memory_u8p  Backing memory pointer.
 *
 * @return true  if a matching buffer was found and marked free.
 * @return false if no matching buffer was found or inputs are invalid.
 */
bool buffer_array_release_by_ptr(buffer_array_ctx_st *ctx_sp, uint8_t *memory_u8p);

#ifdef __cplusplus
}
#endif

#endif /* BUFFER_H_ */
