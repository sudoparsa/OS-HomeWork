/*
 * mm_alloc.c
 *
 * Stub implementations of the mm_* routines. Remove this comment and provide
 * a summary of your allocator's design here.
 */

#include "mm_alloc.h"
#include <memory.h>
#include <unistd.h>
#include <stdlib.h>

/* Your final implementation should comment out this macro. */
// #define MM_USE_STUBS

s_block_ptr head_ptr = NULL;

s_block_ptr get_block (void *p)
{
    for (s_block_ptr head = head_ptr; head; head = head->next) {
        if (head->ptr == p) {
            return head;
        }
    }
    return NULL;
}

s_block_ptr fusion(s_block_ptr b)
{
    if ((b->next)->free_ == 1) {
        b->size = b->size + sizeof(s_block) +(b->next)->size;
        b->next = (b->next)->next;
        (b->next)->prev = b;
        return b;
    }
    if ((b->prev)->free_ == 1) {
        (b->prev)->size = (b->prev)->size + sizeof(s_block) + b->size;
        (b->prev)->next = b->next;
        (b->next)->prev = b->prev;
        (b->prev)->free_ = b->free_;
        return b->prev;
    }
    return NULL;
}


void split_block (s_block_ptr b, size_t s)
{
    if (b == NULL || s <= 0) {
        return;
    }

    if(b->size - s >= sizeof(s_block)) {
        s_block_ptr p = (s_block_ptr) (b->ptr + s);
        p->prev = b;
        if (b->next) {
            (b->next)->prev = p;
        }
        p->next = b->next;
        b->next = p;
        p->size = b->size - s - sizeof(s_block);
        p->ptr = b->ptr + s + sizeof(s_block);
        b->size = s;
        mm_free(p->ptr);
        memset(b->ptr, 0, b->size);
    }
}


s_block_ptr extend_heap(s_block_ptr last, size_t s)
{
    void *p = sbrk(s + sizeof(s_block));

    if (p == (void *) -1) {
        return NULL;
    }

    s_block_ptr new_block = (s_block_ptr) p;
    if (last) {
        last->next = new_block;
    } else {
        head_ptr = new_block;
    }
    new_block->prev = last;
    new_block->next = NULL;
    new_block->free_ = 0;
    new_block->size = s;
    new_block->ptr = p + sizeof(s_block);
    memset(new_block->ptr, 0, new_block->size);

    return new_block->ptr;
}


void* mm_malloc(size_t size)
{
    if (head_ptr == NULL) {
        return extend_heap(NULL, size);
    }

    if (size == 0) {
        return NULL;
    }

    s_block_ptr head;
	s_block_ptr prev = NULL;

    for (head = head_ptr; head; head = head->next) {
        if (head->free_ == 1 && head->size >= size) {
            head->free_ = 0;
            split_block(head, size);
			return head->ptr;
        }
        prev = head;
    }

    return extend_heap(prev,size);
}

void* mm_realloc(void* ptr, size_t size)
{
    if (size == 0) {
        return NULL;
    }

    if (ptr == NULL) {
        return mm_malloc(size);
    }

    s_block_ptr cur = get_block(ptr);

    if (cur) {
        if (size == cur->size) {
            return cur->ptr;
        }
        if (size < cur->size) {
            split_block(cur, size);
            return cur->ptr;
        }
        if (size > cur->size) {
            void *p = mm_malloc(size);
			s_block_ptr p_block = get_block(p);
			if (p_block) {
                char *cur_start = (char *) cur->ptr;
		        char *p_start = (char *) p_block->ptr;
		        for(int i = 0; i < cur->size; i++) {
                    *(p_start + i) = *(cur_start + i);
                }
				p = p_block->ptr;
				mm_free(cur->ptr);
				return p;
			}
        }
    }
    return NULL;

}

void mm_free(void* ptr)
{
    if (ptr == NULL) {
        return;
    }
    
    s_block_ptr cur = get_block(ptr);

    if (cur) {
        if (cur->next == NULL) {
            if (cur->prev == NULL) {
                head_ptr = NULL;
            } else {
                (cur->prev)->next=NULL;
            }
            sbrk(- cur->size - sizeof(s_block));
        } else {
            cur->free_ = 1;
            fusion(cur);
        }
    } 
}
