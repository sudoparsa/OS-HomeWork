/*
 * mm_alloc.c
 *
 * Stub implementations of the mm_* routines. Remove this comment and provide
 * a summary of your allocator's design here.
 */

#include "mm_alloc.h"

#include <stdlib.h>

/* Your final implementation should comment out this macro. */
// #define MM_USE_STUBS

s_block_ptr head_ptr = NULL;


void split_block (s_block_ptr b, size_t s)
{
    if (b == NULL || s < sizeof(void*)) {
        return;
    }

    if(b->size - s >= sizeof(s_block) + sizeof(void*)) {
        s_block_ptr p = (s_block_ptr) (b->ptr + s);
        p->prev = b;
        (b->next)->prev = p;
        p->next = b->next;
        b->next = p;
        p->size = b->size - s - sizeof(s_block);
        p->ptr = b->ptr + s + sizeof(s_block);
        b->size = s;
    }
}


s_block_ptr extend_heap(s_block_ptr last, size_t s)
{
    void *p = sbrk(s + sizeof(s_block));

    if (p == (void *)-1) {
        return NULL;
    }

    s_block_ptr new_block = (s_block_ptr) p;
    if(last) {
        last->next = new_block;
    } else {
        head_ptr = new_block;
    }
    new_block->prev = last;
    new_block->next = NULL;
    new_block->free = 0;
    new_block->size = s;
    new_block->ptr = p + sizeof(s_block);

    return new_block->ptr;
}

void* mm_malloc(size_t size)
{
    if (size == 0) {
        return NULL;
    }
    if (head_ptr == NULL) {
        return extend_heap(NULL, size);
    }

    s_block_ptr head;
	s_block_ptr prev = NULL;

    for (head = head_ptr; head; head = head->next) {
        if (head->free == 1 && head->size >= size) {
            if (size < head->size) {
                split_block(head, size);
			}
			head->free = 0;
			return head->ptr;
        }
        prev = head;
    }

    return extend_heap(prev,size);
}

void* mm_realloc(void* ptr, size_t size)
{

}

void mm_free(void* ptr)
{

}
