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
    bitmap_t *bitmap;
    block_t blocks[BLOCK_STORE_NUM_BLOCKS];
    uint32_t bitmap_blocks;
} block_store_t;

/// This creates a new BS device, ready to go
/// \return Pointer to a new block storage device, NULL on error
block_store_t *block_store_create()
{
    // malloc
    block_store_t *bs = (block_store_t *)malloc(sizeof(block_store_t));

    // checks that the malloc was successful
    if (bs == NULL)
    {
        return NULL;
    }

    // create the bitmap
    bs->bitmap = bitmap_overlay(BLOCK_STORE_NUM_BLOCKS, &((bs->blocks)[BITMAP_START_BLOCK]));

    // loop through the bitmap and attempt to allocate the block id
    uint32_t required_blocks = (BLOCK_STORE_NUM_BLOCKS / 8) / 32;
    uint32_t bitmap_index;
    for (bitmap_index = BITMAP_START_BLOCK; bitmap_index < BITMAP_START_BLOCK + required_blocks; bitmap_index++)
    {
        if (!block_store_request(bs, bitmap_index))
            break;
    }

    bs->bitmap_blocks = required_blocks;

    // check for null bitmap
    if (bs->bitmap == NULL)
    {
        return NULL;
    }

    return bs;
}

/// Destroys the provided block storage device
/// This is an idempotent operation, so there is no return value
/// \param bs BS device
void block_store_destroy(block_store_t *const bs)
{
    // check that block store exists
    if (bs != NULL)
    {
        // check that bitmap exists
        if (bs->bitmap != NULL)
        {
            // destruct and destroy bitmap
            bitmap_destroy(bs->bitmap);
        }
        // unallocate block store memory
        free(bs);
    }
}

/// Searches for a free block, marks it as in use, and returns the block's id
/// \param bs BS device
/// \return Allocated block's id, SIZE_MAX on error
size_t block_store_allocate(block_store_t *const bs)
{
    // check for valid parameters
    if (bs == NULL || bs->bitmap == NULL)
    {
        return SIZE_MAX;
    }

    // find the first zero bit address in the bitmap
    size_t block_id = bitmap_ffz(bs->bitmap);

    // check for out of bounds block id
    if (block_id >= (BLOCK_STORE_NUM_BLOCKS - bs->bitmap_blocks) || block_id == SIZE_MAX)
    {
        return SIZE_MAX;
    }

    // set the requested bit
    bitmap_set(bs->bitmap, block_id);
    // return the allocated block's id
    return block_id;
}

/// Attempts to allocate the requested block id
/// \param bs the block store object
/// \param block_id the requested block identifier
/// \return boolean indicating succes of operation
bool block_store_request(block_store_t *const bs, const size_t block_id)
{
    // check for invalid parameters
    if (bs == NULL || bs->bitmap == NULL || block_id >= (BLOCK_STORE_NUM_BLOCKS - bs->bitmap_blocks) || bitmap_test(bs->bitmap, block_id))
    {
        return false;
    }

    // set the requested bit
    bitmap_set(bs->bitmap, block_id);
    return true;
}

/// Frees the specified block
/// \param bs BS device
/// \param block_id The block to free
void block_store_release(block_store_t *const bs, const size_t block_id)
{
    // check for invalid parameters
    if (bs == NULL || block_id >= (BLOCK_STORE_NUM_BLOCKS - bs->bitmap_blocks))
    {
        return;
    }

    // clear the requested bit
    bitmap_reset(bs->bitmap, block_id);
}

/// Counts the number of blocks marked as in use
/// \param bs BS device
/// \return Total blocks in use, SIZE_MAX on error
size_t block_store_get_used_blocks(const block_store_t *const bs)
{
    // check for invalid parameters
    if (bs == NULL)
    {
        return SIZE_MAX;
    }

    // the number of set bits in the bitmap
    return bitmap_total_set(bs->bitmap) - (bs->bitmap_blocks);
}

/// Counts the number of blocks marked free for use
/// \param bs BS device
/// \return Total blocks free, SIZE_MAX on error
size_t block_store_get_free_blocks(const block_store_t *const bs)
{
    // check for invalid parameters
    if (bs == NULL)
    {
        return SIZE_MAX;
    }

    // if i use the available blocks which subtracts 1 it works but that wouldnt make sense
    // conceptually since I already subtract the one block in the get used method
    return (BLOCK_STORE_NUM_BLOCKS)-block_store_get_used_blocks(bs);
}

/// Returns the total number of user-addressable blocks
/// \return Total blocks
size_t block_store_get_total_blocks()
{
    // technically this isnt really true you would want to subtract the blocks used by the bitmap
    // currently its hard coded by the definition to only use 1 block for the bitmap
    return (BLOCK_STORE_AVAIL_BLOCKS);
}

/// Reads data from the specified block and writes it to the designated buffer
/// \param bs BS device
/// \param block_id Source block id
/// \param buffer Data buffer to write to
/// \return Number of bytes read, 0 on error
size_t block_store_read(const block_store_t *const bs, const size_t block_id, void *buffer)
{
    // check for invalid parameters
    if (bs == NULL || buffer == NULL || block_id >= (BLOCK_STORE_NUM_BLOCKS - bs->bitmap_blocks) || block_id == 0)
    {
        return 0;
    }

    // turn into void pointer
    memcpy(buffer, &((bs->blocks)[block_id]), BLOCK_SIZE_BYTES);

    // number of bytes read
    return BLOCK_STORE_NUM_BLOCKS;
}

/// Reads data from the specified buffer and writes it to the designated block
/// \param bs BS device
/// \param block_id Destination block id
/// \param buffer Data buffer to read from
/// \return Number of bytes written, 0 on error
size_t block_store_write(block_store_t *const bs, const size_t block_id, const void *buffer)
{
    // check for invalid parameters
    if (bs == NULL || buffer == NULL)
    {
        return 0;
    }

    // make into a void pointer
    memcpy(&((bs->blocks)[block_id]), buffer, BLOCK_SIZE_BYTES);

    // number of bytes written
    return BLOCK_STORE_NUM_BLOCKS;
}

/// Imports BS device from the given file
/// \param filename The file to load
/// \return Pointer to new BS device, NULL on error
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

    read(file, bs->blocks, BLOCK_STORE_NUM_BLOCKS * BLOCK_STORE_NUM_BLOCKS);
    close(file);
    return bs;
}

/// Writes the entirety of the BS device to file, overwriting it if it exists
/// \param bs BS device
/// \param filename The file to write to
/// \return Number of bytes written, 0 on error
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

    size_t bytes_written = write(file, bs->blocks, BLOCK_STORE_NUM_BLOCKS * BLOCK_STORE_NUM_BLOCKS);

    close(file);
    return bytes_written;
}