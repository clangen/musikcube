/*
     This file is part of libmicrohttpd
     Copyright (C) 2007, 2009, 2010 Daniel Pittman and Christian Grothoff

     This library is free software; you can redistribute it and/or
     modify it under the terms of the GNU Lesser General Public
     License as published by the Free Software Foundation; either
     version 2.1 of the License, or (at your option) any later version.

     This library is distributed in the hope that it will be useful,
     but WITHOUT ANY WARRANTY; without even the implied warranty of
     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
     Lesser General Public License for more details.

     You should have received a copy of the GNU Lesser General Public
     License along with this library; if not, write to the Free Software
     Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

/**
 * @file memorypool.c
 * @brief memory pool
 * @author Christian Grothoff
 */
#include "memorypool.h"

/* define MAP_ANONYMOUS for Mac OS X */
#if defined(MAP_ANON) && !defined(MAP_ANONYMOUS)
#define MAP_ANONYMOUS MAP_ANON
#endif
#ifndef MAP_FAILED
#define MAP_FAILED ((void*)-1)
#endif

/**
 * Align to 2x word size (as GNU libc does).
 */
#define ALIGN_SIZE (2 * sizeof(void*))

/**
 * Round up 'n' to a multiple of ALIGN_SIZE.
 */
#define ROUND_TO_ALIGN(n) ((n+(ALIGN_SIZE-1)) & (~(ALIGN_SIZE-1)))


/**
 * Handle for a memory pool.  Pools are not reentrant and must not be
 * used by multiple threads.
 */
struct MemoryPool
{

  /**
   * Pointer to the pool's memory
   */
  char *memory;

  /**
   * Size of the pool.
   */
  size_t size;

  /**
   * Offset of the first unallocated byte.
   */
  size_t pos;

  /**
   * Offset of the last unallocated byte.
   */
  size_t end;

  /**
   * #MHD_NO if pool was malloc'ed, #MHD_YES if mmapped (VirtualAlloc'ed for W32).
   */
  int is_mmap;
};


/**
 * Create a memory pool.
 *
 * @param max maximum size of the pool
 * @return NULL on error
 */
struct MemoryPool *
MHD_pool_create (size_t max)
{
  struct MemoryPool *pool;

  pool = malloc (sizeof (struct MemoryPool));
  if (NULL == pool)
    return NULL;
#if defined(MAP_ANONYMOUS) || defined(_WIN32)
  if (max <= 32 * 1024)
    pool->memory = MAP_FAILED;
  else
#if defined(MAP_ANONYMOUS) && !defined(_WIN32)
    pool->memory = mmap (NULL,
                         max,
                         PROT_READ | PROT_WRITE,
			 MAP_PRIVATE | MAP_ANONYMOUS,
                         -1,
                         0);
#elif defined(_WIN32)
    pool->memory = VirtualAlloc (NULL,
                                 max,
                                 MEM_COMMIT | MEM_RESERVE,
                                 PAGE_READWRITE);
#endif
#else
  pool->memory = MAP_FAILED;
#endif
  if ( (MAP_FAILED == pool->memory) ||
       (NULL == pool->memory))
    {
      pool->memory = malloc (max);
      if (NULL == pool->memory)
        {
          free (pool);
          return NULL;
        }
      pool->is_mmap = MHD_NO;
    }
  else
    {
      pool->is_mmap = MHD_YES;
    }
  pool->pos = 0;
  pool->end = max;
  pool->size = max;
  return pool;
}


/**
 * Destroy a memory pool.
 *
 * @param pool memory pool to destroy
 */
void
MHD_pool_destroy (struct MemoryPool *pool)
{
  if (NULL == pool)
    return;
  if (MHD_NO == pool->is_mmap)
    free (pool->memory);
  else
#if defined(MAP_ANONYMOUS) && !defined(_WIN32)
    munmap (pool->memory,
            pool->size);
#elif defined(_WIN32)
    VirtualFree (pool->memory,
                 0,
                 MEM_RELEASE);
#else
    abort ();
#endif
  free (pool);
}


/**
 * Check how much memory is left in the @a pool
 *
 * @param pool pool to check
 * @return number of bytes still available in @a pool
 */
size_t
MHD_pool_get_free (struct MemoryPool *pool)
{
  return (pool->end - pool->pos);
}


/**
 * Allocate size bytes from the pool.
 *
 * @param pool memory pool to use for the operation
 * @param size number of bytes to allocate
 * @param from_end allocate from end of pool (set to #MHD_YES);
 *        use this for small, persistent allocations that
 *        will never be reallocated
 * @return NULL if the pool cannot support size more
 *         bytes
 */
void *
MHD_pool_allocate (struct MemoryPool *pool,
		   size_t size,
                   int from_end)
{
  void *ret;
  size_t asize;

  asize = ROUND_TO_ALIGN (size);
  if ( (0 == asize) && (0 != size) )
    return NULL; /* size too close to SIZE_MAX */
  if ( (pool->pos + asize > pool->end) ||
       (pool->pos + asize < pool->pos))
    return NULL;
  if (from_end == MHD_YES)
    {
      ret = &pool->memory[pool->end - asize];
      pool->end -= asize;
    }
  else
    {
      ret = &pool->memory[pool->pos];
      pool->pos += asize;
    }
  return ret;
}


/**
 * Reallocate a block of memory obtained from the pool.
 * This is particularly efficient when growing or
 * shrinking the block that was last (re)allocated.
 * If the given block is not the most recently
 * (re)allocated block, the memory of the previous
 * allocation may be leaked until the pool is
 * destroyed (and copying the data maybe required).
 *
 * @param pool memory pool to use for the operation
 * @param old the existing block
 * @param old_size the size of the existing block
 * @param new_size the new size of the block
 * @return new address of the block, or
 *         NULL if the pool cannot support @a new_size
 *         bytes (old continues to be valid for @a old_size)
 */
void *
MHD_pool_reallocate (struct MemoryPool *pool,
                     void *old,
		     size_t old_size,
		     size_t new_size)
{
  void *ret;
  size_t asize;

  asize = ROUND_TO_ALIGN (new_size);
  if ( (0 == asize) &&
       (0 != new_size) )
    return NULL; /* new_size too close to SIZE_MAX */
  if ( (pool->end < old_size) ||
       (pool->end < asize) )
    return NULL;                /* unsatisfiable or bogus request */

  if ( (pool->pos >= old_size) &&
       (&pool->memory[pool->pos - old_size] == old) )
    {
      /* was the previous allocation - optimize! */
      if (pool->pos + asize - old_size <= pool->end)
        {
          /* fits */
          pool->pos += asize - old_size;
          if (asize < old_size)      /* shrinking - zero again! */
            memset (&pool->memory[pool->pos],
                    0,
                    old_size - asize);
          return old;
        }
      /* does not fit */
      return NULL;
    }
  if (asize <= old_size)
    return old;                 /* cannot shrink, no need to move */
  if ((pool->pos + asize >= pool->pos) &&
      (pool->pos + asize <= pool->end))
    {
      /* fits */
      ret = &pool->memory[pool->pos];
      if (0 != old_size)
        memmove (ret,
                 old,
                 old_size);
      pool->pos += asize;
      return ret;
    }
  /* does not fit */
  return NULL;
}


/**
 * Clear all entries from the memory pool except
 * for @a keep of the given @a size. The pointer
 * returned should be a buffer of @a new_size where
 * the first @a copy_bytes are from @a keep.
 *
 * @param pool memory pool to use for the operation
 * @param keep pointer to the entry to keep (maybe NULL)
 * @param copy_bytes how many bytes need to be kept at this address
 * @param new_size how many bytes should the allocation we return have?
 *                 (should be larger or equal to @a copy_bytes)
 * @return addr new address of @a keep (if it had to change)
 */
void *
MHD_pool_reset (struct MemoryPool *pool,
		void *keep,
		size_t copy_bytes,
                size_t new_size)
{
  if ( (NULL != keep) &&
       (keep != pool->memory) )
    {
      if (0 != copy_bytes)
        memmove (pool->memory,
                 keep,
                 copy_bytes);
      keep = pool->memory;
    }
  pool->end = pool->size;
  /* technically not needed, but safer to zero out */
  if (pool->size > copy_bytes)
    memset (&pool->memory[copy_bytes],
            0,
            pool->size - copy_bytes);
  if (NULL != keep)
    pool->pos = ROUND_TO_ALIGN (new_size);
  return keep;
}


/* end of memorypool.c */
