#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include "bitmap.h"
#include "block_store.h"

// include more if you need

// You might find this handy.  I put it around unused parameters, but you should
// remove it before you submit. Just allows things to compile initially.

#define UNUSED(x) (void)(x)

typedef struct block
{
    unsigned char block[BLOCK_SIZE_BYTES];
} block_t;

typedef struct block_store
{
    bitmap_t * bitmap;
    block_t blocks[BLOCK_STORE_NUM_BLOCKS];
    uint32_t bitmap_blocks;
} block_store_t;

block_store_t *block_store_create()
{
    block_store_t *bs = (block_store_t *)calloc(1, sizeof(block_store_t));

    if (bs == NULL)
    {
        return NULL;
    }

    bs->bitmap = bitmap_overlay(BLOCK_STORE_NUM_BLOCKS, &((bs->blocks)[BITMAP_START_BLOCK]));

    uint32_t required_blocks = (BLOCK_STORE_NUM_BLOCKS / 8) / 32;
    uint32_t bitmap_index;
    for(bitmap_index = BITMAP_START_BLOCK; bitmap_index < BITMAP_START_BLOCK + required_blocks; bitmap_index++)
    {
        if(!block_store_request(bs, bitmap_index)) break;
    }

    bs->bitmap_blocks = required_blocks;

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
    if (bs == NULL || bs->bitmap == NULL)
    {
        return SIZE_MAX;
    }

    size_t block_id = bitmap_ffz(bs->bitmap);

    if (block_id >= (BLOCK_STORE_NUM_BLOCKS - bs->bitmap_blocks) || block_id == SIZE_MAX)
    {
        return SIZE_MAX;
    }
    
    bitmap_set(bs->bitmap, block_id);
    return block_id;
}

bool block_store_request(block_store_t *const bs, const size_t block_id)
{
    if (bs == NULL || bs->bitmap == NULL || block_id >= (BLOCK_STORE_NUM_BLOCKS - bs->bitmap_blocks) || bitmap_test(bs->bitmap, block_id))
    {
        return false;
    }

    bitmap_set(bs->bitmap, block_id);
    return true;
}

void block_store_release(block_store_t *const bs, const size_t block_id)
{
    if (bs == NULL || block_id >= (BLOCK_STORE_NUM_BLOCKS - bs->bitmap_blocks))
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
    return bitmap_total_set(bs->bitmap) - (bs->bitmap_blocks);
}

size_t block_store_get_free_blocks(const block_store_t *const bs)
{
    if (bs == NULL)
    {
        return SIZE_MAX;
    }
    // if i use the available blocks which subtracts 1 it works but that wouldnt make sense
    // conceptually since I already subtract the one block in the get used method
    return (BLOCK_STORE_NUM_BLOCKS) - block_store_get_used_blocks(bs);
}

size_t block_store_get_total_blocks()
{
    // technically this isnt really true you would want to subtract the blocks used by the bitmap
    // currently its hard coded by the definition to only use 1 block for the bitmap
    return (BLOCK_STORE_AVAIL_BLOCKS);
}

size_t block_store_read(const block_store_t *const bs, const size_t block_id, void *buffer)
{
    if (bs == NULL || buffer == NULL || block_id >= (BLOCK_STORE_NUM_BLOCKS - bs->bitmap_blocks) || block_id == 0)
    {
        return 0;
    }

    // turn into void pointer
    memcpy(buffer, &((bs->blocks)[block_id]), BLOCK_SIZE_BYTES);
    return BLOCK_SIZE_BYTES;
}

size_t block_store_write(block_store_t *const bs, const size_t block_id, const void *buffer)
{
    if (bs == NULL || buffer == NULL)
    {
        return 0;
    }

    // make into a void pointer
    memcpy(&((bs->blocks)[block_id]), buffer, BLOCK_SIZE_BYTES);
    return BLOCK_SIZE_BYTES;
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

    read(file, bs->blocks, BLOCK_STORE_NUM_BYTES);
    close(file);
    return bs;
}

size_t block_store_serialize(const block_store_t *const bs, const char *const filename)
{
    if (filename == NULL || bs == NULL)
    {
        return 0;
    }

    int file = creat(filename, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

    if (file == -1)
    {
        return 0;
    }

    size_t bytes_written = write(file, bs->blocks, BLOCK_STORE_NUM_BYTES);

    close(file);
    return bytes_written;
}