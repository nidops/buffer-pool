/**
 * @file buffer.c
 * @brief Implementation of fixed-size buffer descriptors and pools.
 */

#include "buffer.h"

/* -------------------------------------------------------------------------- */
/* Static Functions                                                           */
/* -------------------------------------------------------------------------- */

/**
 * @brief Check if a buffer descriptor is non-NULL and initialized.
 *
 * @param[in] buffer_csp  Pointer to buffer descriptor (could be NULL).
 *
 * @return true if @p buffer_csp is not NULL and initialized, false otherwise.
 */
static bool buffer_is_valid(buffer_st const *buffer_csp)
{
    return ((NULL != buffer_csp) && (true == buffer_csp->is_initialized));
}

/**
 * @brief Check if a pool is not NULL, initialized, and has a descriptor array.
 *
 * @param[in] pool_csp  Pointer to pool object (could be NULL).
 *
 * @return true if @p pool_csp and its array are valid and initialized.
 */
static bool buffer_pool_is_valid(buffer_pool_st const *pool_csp)
{
    return ((NULL != pool_csp)                  &&
            (true == pool_csp->is_initialized)  &&
            (NULL != pool_csp->buffer_array_sa) &&
            (0u   < pool_csp->buffer_count));
}

/* -------------------------------------------------------------------------- */
/* Single buffer API                                                          */
/* -------------------------------------------------------------------------- */

void buffer_init(buffer_st *buffer_sp, uint8_t *memory_u8p, size_t capacity_bytes)
{
    if (NULL == buffer_sp)
    {
        return;
    }

    buffer_sp->data_u8p       = memory_u8p;
    buffer_sp->capacity_bytes = capacity_bytes;
    buffer_sp->is_initialized = true;

    if ((NULL != memory_u8p) && (0u != capacity_bytes))
    {
        buffer_sp->is_available = true;
    }
    else
    {
        buffer_sp->is_available = false;
    }
}

uint8_t *buffer_data(buffer_st const *buffer_csp, size_t *capacity_bytes_out_p)
{
    if (NULL != capacity_bytes_out_p)
    {
        if (true == buffer_is_valid(buffer_csp))
        {
            *capacity_bytes_out_p = buffer_csp->capacity_bytes;
        }
        else
        {
            *capacity_bytes_out_p = 0u;
        }
    }

    if (true == buffer_is_valid(buffer_csp))
    {
        return buffer_csp->data_u8p;
    }

    return NULL;
}

void buffer_mark_free(buffer_st *buffer_sp)
{
    if (true == buffer_is_valid(buffer_sp))
    {
        buffer_sp->is_available = true;
    }
}

void buffer_mark_in_use(buffer_st *buffer_sp)
{
    if (true == buffer_is_valid(buffer_sp))
    {
        buffer_sp->is_available = false;
    }
}

/* -------------------------------------------------------------------------- */
/* Pool API                                                                   */
/* -------------------------------------------------------------------------- */

void buffer_pool_init(buffer_pool_st *pool_sp, buffer_st *buffer_array_sa, size_t buffer_count)
{
    if ((NULL == pool_sp) || (NULL == buffer_array_sa) || (0u == buffer_count))
    {
        return;
    }

    pool_sp->buffer_array_sa = buffer_array_sa;
    pool_sp->buffer_count    = buffer_count;
    pool_sp->is_initialized  = true;
}

buffer_st *buffer_pool_acquire(buffer_pool_st *pool_sp)
{
    size_t index;

    if (false == buffer_pool_is_valid(pool_sp))
    {
        return NULL;
    }

    for (index = 0u; index < pool_sp->buffer_count; ++index)
    {
        buffer_st *current_sp = &pool_sp->buffer_array_sa[index];

        if ((true == buffer_is_valid(current_sp)) &&
            (true == current_sp->is_available))
        {
            current_sp->is_available = false;
            return current_sp;
        }
    }

    return NULL;
}

buffer_st *buffer_pool_find(buffer_pool_st *pool_sp, uint8_t *memory_u8p)
{
    size_t index;

    if ((false == buffer_pool_is_valid(pool_sp)) || (NULL == memory_u8p))
    {
        return NULL;
    }

    for (index = 0u; index < pool_sp->buffer_count; ++index)
    {
        buffer_st *current_sp = &pool_sp->buffer_array_sa[index];

        if ((true == buffer_is_valid(current_sp)) &&
            (current_sp->data_u8p == memory_u8p))
        {
            return current_sp;
        }
    }

    return NULL;
}

bool buffer_pool_release_by_ptr(buffer_pool_st *pool_sp, uint8_t *memory_u8p)
{
    buffer_st *buffer_sp = buffer_pool_find(pool_sp, memory_u8p);

    if (NULL == buffer_sp)
    {
        return false;
    }

    buffer_mark_free(buffer_sp);
    return true;
}

void buffer_pool_mark_all_free(buffer_pool_st *pool_sp)
{
    size_t index;

    if (false == buffer_pool_is_valid(pool_sp))
    {
        return;
    }

    for (index = 0u; index < pool_sp->buffer_count; ++index)
    {
        buffer_st *current_sp = &pool_sp->buffer_array_sa[index];

        if (true == buffer_is_valid(current_sp))
        {
            current_sp->is_available = true;
        }
    }
}

/* -------------------------------------------------------------------------- */
/* Buffer array context API                                                   */
/* -------------------------------------------------------------------------- */

void buffer_array_ctx_init(buffer_array_ctx_st *ctx_sp,
                           buffer_st *buffer_array_sa,
                           uint8_t *memory_block_u8p,
                           size_t buffer_count,
                           size_t buffer_size)
{
    size_t index;

    if ((NULL == ctx_sp)           ||
        (NULL == buffer_array_sa)  ||
        (NULL == memory_block_u8p) ||
        (0u   == buffer_count)     ||
        (0u   == buffer_size))
    {
        return;
    }

    ctx_sp->buffer_array_sa  = buffer_array_sa;
    ctx_sp->memory_block_u8p = memory_block_u8p;
    ctx_sp->buffer_count     = buffer_count;
    ctx_sp->buffer_size      = buffer_size;

    for (index = 0u; index < buffer_count; ++index)
    {
        uint8_t *chunk_u8p = &memory_block_u8p[index * buffer_size];
        buffer_init(&buffer_array_sa[index], chunk_u8p, buffer_size);
    }

    buffer_pool_init(&ctx_sp->pool_s, buffer_array_sa, buffer_count);

    ctx_sp->is_initialized = true;
}

buffer_st *buffer_array_acquire(buffer_array_ctx_st *ctx_sp)
{
    if ((NULL == ctx_sp) || (false == ctx_sp->is_initialized))
    {
        return NULL;
    }

    return buffer_pool_acquire(&ctx_sp->pool_s);
}

buffer_st *buffer_array_find_by_ptr(buffer_array_ctx_st *ctx_sp, uint8_t *memory_u8p)
{
    if ((NULL == ctx_sp) || (false == ctx_sp->is_initialized))
    {
        return NULL;
    }

    return buffer_pool_find(&ctx_sp->pool_s, memory_u8p);
}

bool buffer_array_release_by_ptr(buffer_array_ctx_st *ctx_sp, uint8_t *memory_u8p)
{
    if ((NULL == ctx_sp) || (false == ctx_sp->is_initialized))
    {
        return false;
    }

    return buffer_pool_release_by_ptr(&ctx_sp->pool_s, memory_u8p);
}
