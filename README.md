# buffer-pool

Small C utility for managing fixed-size buffers and simple buffer pools.

- No dynamic allocation
- Suitable for embedded and RTOS projects

The module provides:

- `buffer_st`
  Descriptor for a single fixed-size buffer.

- `buffer_pool_st`
  Pool API over an array of buffer descriptors.

- `buffer_array_ctx_st`
  Helper for managing N equal-sized buffers carved out of one flat memory block
  (for example, DMA / UART RX buffers).

## Files

- `include/buffer.h`
  Public API.

- `src/buffer.c`
  Implementation.

## Basic usage

1. Provide memory for N buffers and an array of descriptors.
2. Initialize a `buffer_array_ctx_st`.
3. Acquire and release buffers as needed.

### Example (`main.c`)

```c
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "buffer.h"

#define BUF_COUNT   (3u)
#define BUF_SIZE    (64u)

/* Flat memory block: BUF_COUNT * BUF_SIZE bytes */
static uint8_t             mem_au8[BUF_COUNT * BUF_SIZE];

/* Descriptors */
static buffer_st           desc_as[BUF_COUNT];

/* Context */
static buffer_array_ctx_st ctx_s;

static void buffers_init(void)
{
    buffer_array_ctx_init(&ctx_s,
                          desc_as,
                          mem_au8,
                          BUF_COUNT,
                          BUF_SIZE);
}

int main(void)
{
    buffers_init();

    /* ----------------------------------------------------------------------
     * Basic self-test:
     * 1) Acquire BUF_COUNT buffers (all must succeed)
     * 2) Extra acquire must fail (NULL)
     * 3) Release all by pointer
     * 4) Acquire again (must succeed)
     * --------------------------------------------------------------------*/

    buffer_st *buf_sp;
    size_t     cap_bytes;
    uint8_t   *data_u8p;
    size_t     i;

    /* 1: Acquire all buffers */
    for (i = 0u; i < BUF_COUNT; ++i)
    {
        buf_sp = buffer_array_acquire(&ctx_s);
        if (NULL == buf_sp)
        {
            printf("buffer-pool: FAILED (acquire %u)\n", (unsigned int)i);
            return -1;
        }

        data_u8p = buffer_data(buf_sp, &cap_bytes);
        if ((NULL == data_u8p) || (cap_bytes != BUF_SIZE))
        {
            printf("buffer-pool: FAILED (buffer_data %u)\n", (unsigned int)i);
            return -1;
        }

        /* Mark in use to simulate real usage */
        buffer_mark_in_use(buf_sp);
    }

    /* 2: Extra acquire must fail */
    buf_sp = buffer_array_acquire(&ctx_s);
    if (NULL != buf_sp)
    {
        printf("buffer-pool: FAILED (acquired beyond pool size)\n");
        return -1;
    }

    /* 3: Release all buffers via pointer-based API */
    for (i = 0u; i < BUF_COUNT; ++i)
    {
        uint8_t *mem_base_u8p = &mem_au8[i * BUF_SIZE];

        if (false == buffer_array_release_by_ptr(&ctx_s, mem_base_u8p))
        {
            printf("buffer-pool: FAILED (release_by_ptr %u)\n", (unsigned int)i);
            return -1;
        }
    }

    /* 4: Acquire again (must succeed) */
    buf_sp = buffer_array_acquire(&ctx_s);
    if (NULL == buf_sp)
    {
        printf("buffer-pool: FAILED (re-acquire after release)\n");
        return -1;
    }

    printf("buffer-pool: OK\n");
    return 0;
}
```
