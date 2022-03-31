#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>
#include "bitmap.h"
#include "block_store.h"
// include more if you need

// You might find this handy.  I put it around unused parameters, but you should
// remove it before you submit. Just allows things to compile initially.

#define UNUSED(x) (void)(x)

#define BYTES_PER_BITMAP 32
#define BLOCKS_PER_BLOCK_STORE 256

#define BYTES_PER_BLOCK 256
#define BLOCK_BITS (BYTES_PER_BLOCK *)

#define BLOCK_STORE_TOTAL_BYTES (BLOCKS_PER_BLOCK_STORE * BYTES_PER_BLOCK)

typedef struct block_store
{
    bitmap_t *bitmap;
    char arr[BLOCKS_PER_BLOCK_STORE - 1][BLOCKS_PER_BLOCK_STORE - 1];
} block_store_t;

block_store_t *block_store_create()
{
    block_store_t *bs = malloc(sizeof(block_store_t));

    if (bs == NULL)
    {
        return NULL;
    }

    bs->bitmap = bitmap_create(BYTES_PER_BLOCK);

    if (bs->bitmap == NULL)
    {
        return NULL;
    }

    return bs;
}

void block_store_destroy(block_store_t *const bs)
{
    if (bs != NULL)
    {
        bitmap_destroy(bs->bitmap);
        free(bs);
    }
}
size_t block_store_allocate(block_store_t *const bs)
{
    if (bs == NULL)
    {
        return SIZE_MAX;
    }

    size_t bm = bitmap_ffz(bs->bitmap);

    if (bm >= (BLOCKS_PER_BLOCK_STORE - 1) || bm == SIZE_MAX)
    {
        return SIZE_MAX;
    }

    bitmap_set(bs->bitmap, bm);
    return bm;
}

bool block_store_request(block_store_t *const bs, const size_t block_id)
{
    if (bs == NULL || block_id > (BLOCKS_PER_BLOCK_STORE - 1) || block_id == 0 || bitmap_test(bs->bitmap, block_id))
    {
        return false;
    }

    bitmap_set(bs->bitmap, block_id);
    return true;
}

void block_store_release(block_store_t *const bs, const size_t block_id)
{
    if (bs == NULL || block_id > (BLOCKS_PER_BLOCK_STORE - 1))
    {
        return;
    }

    bitmap_reset(bs->bitmap, block_id);
}

size_t block_store_get_used_blocks(const block_store_t *const bs)
{
    if (bs == NULL)
    {
        return SIZE_MAX;
    }
    return bitmap_total_set(bs->bitmap);
}

size_t block_store_get_free_blocks(const block_store_t *const bs)
{
    if (bs == NULL)
    {
        return SIZE_MAX;
    }
    return (BLOCKS_PER_BLOCK_STORE - 1) - bitmap_total_set(bs->bitmap);
}

size_t block_store_get_total_blocks()
{
    return (BLOCKS_PER_BLOCK_STORE - 1);
}

size_t block_store_read(const block_store_t *const bs, const size_t block_id, void *buffer)
{
    if (bs == NULL || buffer == NULL || block_id > (BLOCKS_PER_BLOCK_STORE - 1) || block_id == 0)
    {
        return 0;
    }

    memcpy(buffer, bs->arr[block_id], BLOCKS_PER_BLOCK_STORE);

    return BLOCKS_PER_BLOCK_STORE;
}

size_t block_store_write(block_store_t *const bs, const size_t block_id, const void *buffer)
{
    if (bs == NULL || buffer == NULL || block_id == 0)
    {
        return 0;
    }

    memcpy(bs->arr[block_id], buffer, BLOCKS_PER_BLOCK_STORE);
    return BLOCKS_PER_BLOCK_STORE;
}

block_store_t *block_store_deserialize(const char *const filename)
{
    if (filename == NULL)
    {
        return NULL;
    }

    block_store_t *bs = block_store_create();
    int file = open(filename, O_RDONLY);

    if (file == -1)
    {
        return NULL;
    }

    read(file, bs, BLOCKS_PER_BLOCK_STORE * BLOCKS_PER_BLOCK_STORE);
    close(file);
    return bs;
}

size_t block_store_serialize(const block_store_t *const bs, const char *const filename)
{
    if (filename == NULL || bs == NULL)
    {
        return 0;
    }

    int file = open(filename, O_CREAT | O_WRONLY);

    if (file == -1)
    {
        return NULL;
    }

    write(file, bs, BLOCKS_PER_BLOCK_STORE * BLOCKS_PER_BLOCK_STORE);

    close(file);
    return BLOCK_STORE_TOTAL_BYTES;
}