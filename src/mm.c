/*
 * mm-naive.c - A not the least memory-efficient but not the fastest malloc package.
 * 
 * In this package, I'll try to implement the strategy on the book
 * -- to realize the linked list with header.
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "psi-cmd",
    /* First member's full name */
    "Longbang Liu",
    /* First member's email address */
    "longbang_liu@mail.ustc.edu.cn",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8
#define VALID 1
#define INVALID 0

#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7)
/* rounds up to the nearest multiple of ALIGNMENT */
#define SIZE_T_SIZE (ALIGN(0x8)) /* size_t allows for the compatible features for x86 and x86_64 */
#define MIN_BLOCK (1 << 10)      /* 1 kB */

#define COMBINE(aligned_total_length, valid_bit) (aligned_total_length + valid_bit) /*  */
#define CONT_TO_HEAD(contp) (void *)((char *)contp - 0x8)
#define HEAD_TO_CONT(head) (void *)((char *)head + 0x8)
#define NEXT(head) (void *)((*(size_t *)head & ~0x7) + (char *)head)
#define LENGTH(head) (*(size_t *)head & ~0x7)
#define VALIDATE(head) (*(size_t *)head & 1)

/* Globals */
size_t largest_trunk_size, size_before_realloc;
void *mem_start;
void *now, *prev; /* public for re_apply() to use: "previous" should be the last trunk after each for loop */
void *realloc_p = NULL;
/* Global ends */

// int debug_counter = 5;

static void re_apply(size_t size);
int mm_check();
/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    void *p = mem_sbrk(MIN_BLOCK);
    if (p == (void *)-1)
        return 1;
    *(size_t *)p = COMBINE(ALIGN(MIN_BLOCK - 0x8), INVALID);
    *(size_t *)((char *)p + MIN_BLOCK - 0x8) = 0; /* Zero mark at the end */
    mem_start = p;
    prev = mem_start;
    now = NEXT(mem_start); /* in case no for loop experienced */
    largest_trunk_size = MIN_BLOCK - 0x8;
    //printf("\n-+-+-+-+-+-+-+-+-+-+-%d-+-+-+-+-+-+-+-+-+-\n", debug_counter++);
    return 0;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    if (!size)
        return NULL;

    size_t newsize = ALIGN(size + 0x8);
    size_t remain_len, perfect_len;
    size_t local_largest = 0, local_second = 0;
    void *perfect_pos;

    if (newsize > largest_trunk_size)
        re_apply(newsize);

    perfect_len = -1; /* largest unsigned */
    for (prev = now = mem_start; *(size_t *)now != 0; prev = now, now = NEXT(now))
    {
        if (!VALIDATE(now)){
            if (prev != now && !VALIDATE(prev))
            {
                *(size_t *)prev = COMBINE(LENGTH(prev) + LENGTH(now), INVALID);
                now = prev;
            }
            if (LENGTH(now) >= newsize && LENGTH(now) < perfect_len)
            {
                perfect_len = LENGTH(now);
                perfect_pos = now;
            }
            if (LENGTH(now) > local_largest)
            {
                local_largest = LENGTH(now);
            } else if (LENGTH(now) >= local_second)
            {
                local_second = LENGTH(now);
            }
        }
    }
    remain_len = LENGTH(perfect_pos) - newsize;
    largest_trunk_size = LENGTH(perfect_pos) == local_largest ? (local_second < remain_len ? remain_len : local_second) : local_largest;
    /* must copy data before edge carving */
    if (realloc_p){
        memcpy(HEAD_TO_CONT(perfect_pos), realloc_p, size_before_realloc);
        realloc_p = NULL;
    }

    if (remain_len > 0x8) 
    {
        *(size_t *)perfect_pos = COMBINE(newsize, VALID);
        *(size_t *)NEXT(perfect_pos) = COMBINE(remain_len, INVALID);
    }
    else
    {
        *(size_t *)perfect_pos = COMBINE(LENGTH(perfect_pos), VALID);
    }

    //mm_check();
    //printf("\n%ld\n-+-+-+-+-+-+-+-+-+-+-%d-+-+-+-+-+-+-+-+-+-\n", largest_trunk_size, debug_counter++);

    return HEAD_TO_CONT(perfect_pos);
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr) /* I know that when trunk freed, no immediate update to largest_trunk_size */
{
    *(size_t *)CONT_TO_HEAD(ptr) = COMBINE(LENGTH(CONT_TO_HEAD(ptr)), INVALID);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    if (!size)
    {
        mm_free(ptr);
        return NULL;
    }

    void *oldhead = CONT_TO_HEAD(ptr);
    void *newptr;
    size_t newsize = ALIGN(size + 0x8);
    size_t remain_len = LENGTH(oldhead) + LENGTH(NEXT(oldhead)) - newsize;

    if (!VALIDATE(NEXT(oldhead)) && LENGTH(oldhead) + LENGTH(NEXT(oldhead)) >= newsize)
    {
        if (remain_len > 0x8)
        {
            *(size_t *)oldhead = COMBINE(newsize, VALID);
            *(size_t *)NEXT(oldhead) = COMBINE(remain_len, INVALID);
        }
        else
        {
            *(size_t *)oldhead = COMBINE(LENGTH(oldhead) + LENGTH(NEXT(oldhead)), VALID);
        }
        return ptr;
    }
    else
    {
        realloc_p = ptr;
        size_before_realloc = LENGTH(oldhead);
        mm_free(ptr); /* Lock is required for thread safety */
        newptr = mm_malloc(size);
        return newptr;
    }
}

static void re_apply(size_t newsize)
{
    if (newsize < MIN_BLOCK)
    {
        newsize = MIN_BLOCK;
    }
    *(size_t *)now = COMBINE(newsize, INVALID);
    mem_sbrk(newsize);
    prev = now;
    now = NEXT(now); 
    *(size_t *)now = 0;
    largest_trunk_size = newsize;
}

/* Debug */
/* print all headers with their own position, and their length */
int mm_check()
{
    void *now_dbg = mem_start;
    void *prev_dbg = now_dbg;
    for (; *(size_t *)now_dbg != 0; prev_dbg = now_dbg, now_dbg = NEXT(now_dbg))
    {
        if (prev_dbg != now_dbg)
        {
            printf("\n |%ld| \n", now_dbg - prev_dbg);
        }
        printf("0x%lx %ld (%ld)", now_dbg, LENGTH(now_dbg), VALIDATE(now_dbg));
    }
    return 0;
}
