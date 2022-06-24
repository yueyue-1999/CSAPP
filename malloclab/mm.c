/*
 * mm.c - The fastest, least memory-efficient malloc package.
 * 
 * In this approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
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
    "yueyue",
    /* First member's full name */
    "Hou Haoyue",
    /* First member's email address */
    "yueyue19991013@outlook.com",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

#define WSIZE 4
#define DSIZE 8
#define CHUNKSIZE (1 << 10) // extend heap by this amount

#define MAX(x, y) ((x) > (y) ? (x):(y))

// 用于设置SIZE和有效位
#define PACK(size, alloc) ((size) | (alloc))

// Read and write a word at address p
#define GET(p) (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int*)(p) = (val))

// Read the size and allocated fields from address p
#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

// Given block ptr bp, compute address of its header and footer
#define HDRP(bp) ((char* )(bp) - WSIZE)
#define FTRP(bp) ((char* )(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

// Given block ptr bp, compute address of next and previous blocks
#define NEXT_BLKP(bp) ((char*)(bp) + GET_SIZE(((char*)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char*)(bp) - GET_SIZE(((char*)(bp) - DSIZE)))

#define NUMLIST 7

static int findlisti(int sizebp){
    int listi;
    if (sizebp < (int)(1<<5)) listi = 0;
    else if (sizebp < (int)(1<<6)) listi = 1;
    else if (sizebp < (int)(1<<7)) listi = 2;
    else if (sizebp < (int)(1<<8)) listi = 3;
    else if (sizebp < (int)(1<<9)) listi = 4;
    else if (sizebp < (int)(1<<10)) listi = 5;
    else listi = 6;
    return listi;
}

/*
    delete - 从空闲链表中删除空闲块
*/
static void delete(void* bp){
    char* prevbp = (char*)GET(bp);
    char* nextbp = (char*)GET(bp + WSIZE);
    PUT(prevbp + WSIZE, (unsigned int)nextbp);
    PUT(nextbp, (unsigned int)prevbp);
}

/*
    insert - 将空闲块插入空闲链表表头
*/
static void insert_head(void* bp){
    // 先找到要插入哪一个空闲链表
    int sizebp = GET_SIZE(HDRP(bp));
    int listi = findlisti(sizebp);

    // 对第listi个空闲链表进行操作
    char* firstbp = mem_heap_lo() + WSIZE + listi*DSIZE; // 第listi个空闲链表头的位置
    char* nextbp = (char*)(GET(firstbp + WSIZE)); // 插入前的下一个空闲块的位置
    PUT(bp, (unsigned int)(firstbp));
    PUT(bp + WSIZE, (unsigned int)(nextbp));
    PUT(firstbp + WSIZE, (unsigned int)(bp));
    PUT(nextbp, (unsigned int)(bp));
}

/*
    insert_tail - 将空闲块插入空闲链表表尾
*/
static void insert_tail(void* bp){
    // 先找到要插入哪一个空闲链表
    int sizebp = GET_SIZE(HDRP(bp));
    int listi = findlisti(sizebp);

    // 对第listi个空闲链表进行操作
    char* lastbp = mem_heap_hi() - (7-listi)*DSIZE + 1; // 空闲链表尾的位置
    char* prevbp = (char*)(GET(lastbp)); // 插入前的第一个空白快的位置
    PUT(bp, (unsigned int)(prevbp));
    PUT(bp + WSIZE, (unsigned int)(lastbp));
    PUT(prevbp + WSIZE, (unsigned int)(bp));
    PUT(lastbp, (unsigned int)(bp));
}

/*
    coalesce - 将当前块与其前后的空闲块合并，并返回合并后的空闲块bp
*/
static void* coalesce(void* bp){
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc && next_alloc) return bp; // 前后两块都不是空闲块

    else if (prev_alloc && !next_alloc){
        delete(NEXT_BLKP(bp));
        delete(bp);
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
        insert_tail(bp);
    }

    else if (!prev_alloc && next_alloc){
        delete(bp);
        delete(PREV_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        bp = PREV_BLKP(bp);
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
        insert_tail(bp);
    }

    else{
        delete(bp);
        delete(NEXT_BLKP(bp));
        delete(PREV_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(HDRP(NEXT_BLKP(bp)));
        bp = PREV_BLKP(bp);
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
        insert_tail(bp);
    }
    return bp;

}

/*
    extend_heap - 扩展一个大小为words的空闲块，并返回块的bp指针
*/
static void *extend_heap(size_t words){
    char* bp;
    size_t size = ALIGN(words);

    if ( (bp = mem_sbrk(size)) == (void*)-1 ) return NULL;
    bp = bp - NUMLIST*DSIZE;

    // 复制所有链表的链表尾内容
    memcpy(bp + size, bp, NUMLIST*DSIZE);
    // 让链表尾的前一块指向链表尾
    for (int i = 0; i < NUMLIST; i++){
        PUT((char*)(GET(bp + i*DSIZE)) + WSIZE, (unsigned int)(bp + size + i*DSIZE));
    }

    // 设置结尾块
    PUT(bp + size - WSIZE, PACK(0,1));

    // 设置空闲块并插入空闲链表表头
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    insert_tail(bp);

    return coalesce(bp);
}

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    char* heap_listp;

    if ( (heap_listp = mem_sbrk((NUMLIST*4 + 4)*WSIZE)) == (void*)(-1)) return -1;

    // 设置一个空白块为了对齐
    PUT(heap_listp, 0);
    
    // 设置七个空闲链表表头和表尾
    // size的范围：[2^4, 2^5), [2^5, 2^6)...[2^9, 2^10), [2^10...)
    for (int i = 0; i < NUMLIST; i++){
        PUT(heap_listp + (2*i+1)*WSIZE, 0);
        PUT(heap_listp + (2*i+2)*WSIZE, (unsigned int)(heap_listp + (2*i+18)*WSIZE));
        PUT(heap_listp + (2*i+18)*WSIZE, (unsigned int)(heap_listp + (2*i+1)*WSIZE));
        PUT(heap_listp + (2*i+19)*WSIZE, 0);
    }

    // 设置序言块，只包含头部和尾部，大小为8，非空闲
    PUT(heap_listp + (15*WSIZE), PACK(DSIZE, 1));
    PUT(heap_listp + (16*WSIZE), PACK(DSIZE, 1));

    // 设置结尾快，一个非空闲的大小为0的块
    PUT(heap_listp + (17*WSIZE), PACK(0,1));

    // 扩展一个大小为CHUNKSIZE的空闲块
    if (extend_heap(CHUNKSIZE) == NULL) return -1;

    return 0;
}

// 首次适配找一个空闲块
static void* find_fit(size_t asize){
    int listi = findlisti(asize);
    char* bp = (char*)GET(mem_heap_lo() + (listi+1)*DSIZE); // 找到第i个空闲链表中第一个空闲块的地址
    while(listi <= 6){
        // 第i小的空闲链表为空
        if (bp == mem_heap_hi() - (7-listi)*DSIZE + 1){
            listi++;
            bp = (char*)GET(mem_heap_lo() + (listi+1)*DSIZE);
            continue;
        }
        // 直到有一个空闲链表不为空
        while(bp != mem_heap_hi() - (7-listi)*DSIZE + 1){
            if ((GET_SIZE(HDRP(bp)) >= asize) && !GET_ALLOC(HDRP(bp))) return bp;
            bp = (char*)(GET(bp + WSIZE));
        }
    }
    return NULL;
}


static void place(void* bp, size_t asize){
    size_t blocksize = GET_SIZE(HDRP(bp));
    // 从空闲链表中删除空闲块
    delete(bp);
    if ((blocksize - asize) >= (2*DSIZE)){
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp);
        // 设置切割后的空闲块并插入到空闲链表表尾
        PUT(HDRP(bp), PACK(blocksize - asize, 0));
        PUT(FTRP(bp), PACK(blocksize - asize, 0));
        insert_tail(bp);
        coalesce(bp);
    }
    else{
        PUT(HDRP(bp), PACK(blocksize, 1));
        PUT(FTRP(bp), PACK(blocksize, 1));
    }
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    char* bp;
    size_t extendsize;
    size_t asize = ALIGN(size + SIZE_T_SIZE);

    if (size == 0) return NULL;

    // 在空闲链表中查找asize大小的空闲块，找到则分配
    if ((bp = find_fit(asize)) != NULL){
        place(bp, asize);
        return bp;
    }

    // 当前空闲链表中没有asize大小的空闲块，扩张heap
    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize)) == NULL) return NULL;
    place(bp, asize);
    
    return bp;
}

/*
 * mm_free - 释放一个块并插入空闲链表头
 */
void mm_free(void *ptr)
{
    size_t size = GET_SIZE(HDRP(ptr));
    PUT(HDRP(ptr), PACK(size, 0));
    PUT(FTRP(ptr), PACK(size, 0));
    insert_tail(ptr); // 对于realloc-bal.rep, 用insert_head()效果更好
    coalesce(ptr);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    if (ptr == NULL) return mm_malloc(size);
    if (size == 0){
        mm_free(ptr);
        return NULL;
    }

    size_t oldsize = GET_SIZE(HDRP(ptr));
    size_t newsize = ALIGN(size + SIZE_T_SIZE);

    if ((int)(oldsize - newsize) >= (int)(2*DSIZE)){
        // 需要分割已分配块，就地更改
        PUT(HDRP(ptr), PACK(newsize, 1));
        PUT(FTRP(ptr), PACK(newsize, 1));
        // 将分割的空闲块插入到空闲链表中
        PUT(HDRP(NEXT_BLKP(ptr)), PACK(oldsize - newsize, 0));
        PUT(FTRP(NEXT_BLKP(ptr)), PACK(oldsize - newsize, 0));
        insert_tail(NEXT_BLKP(ptr));
        coalesce(NEXT_BLKP(ptr));
        return ptr;
    }
    else if ((int)(oldsize - newsize) >= 0){
        // 剩余大小太小无法分割，不做操作，直接返回
        return ptr;
    }
    else{
        // 如果后面的空闲块足够大，则用后面的空闲块进行扩充
        size_t needsize = newsize - oldsize;
        size_t nextsize = GET_SIZE(HDRP(NEXT_BLKP(ptr)));
        if (!GET_ALLOC(HDRP(NEXT_BLKP(ptr))) && (int)(nextsize - needsize) >= (int)(2*DSIZE)){
            delete(NEXT_BLKP(ptr));
            PUT(HDRP(ptr), PACK(newsize, 1));
            PUT(FTRP(ptr), PACK(newsize, 1));
            // 分割的空闲块放入空闲链表尾
            PUT(HDRP(NEXT_BLKP(ptr)), PACK(nextsize - needsize, 0));
            PUT(FTRP(NEXT_BLKP(ptr)), PACK(nextsize - needsize, 0));
            insert_tail(NEXT_BLKP(ptr));
            coalesce(NEXT_BLKP(ptr));
            return ptr;
        }
        else if (!GET_ALLOC(HDRP(NEXT_BLKP(ptr))) && (int)(nextsize - needsize) >= 0){
            delete(NEXT_BLKP(ptr));
            PUT(HDRP(ptr), PACK(oldsize + nextsize, 1));
            PUT(FTRP(ptr), PACK(oldsize + nextsize, 1));
            return ptr;
        }
        else { 
            // 新要求的块大小更大，重新分配块
            void* newptr = mm_malloc(newsize);
            memcpy(newptr, ptr, oldsize);
            mm_free(ptr);
            return newptr; 
        }
    }
}