#include <stdlib.h>
#include <stddef.h>
#include <sys/mman.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

/* Define page size if not already defined */
#ifndef PAGE_SIZE
#define PAGE_SIZE 4096
#endif

/* Define block struct */
typedef struct block {
    size_t size;
    struct block *next;
} block;

/* Global variables */
block *free_list = NULL;
block *block_list = NULL;

/* Predefined helper functions */

static void *__memset(void *s, int c, size_t n) {
  unsigned char *p;
  size_t i;

  if (n == ((size_t) 0)) return s;
  for (i=(size_t) 0,p=(unsigned char *)s;
       i<=(n-((size_t) 1));
       i++,p++) {
    *p = (unsigned char) c;
  }
  return s;
}

static void *__memcpy(void *dest, const void *src, size_t n) {
  unsigned char *pd;
  const unsigned char *ps;
  size_t i;

  if (n == ((size_t) 0)) return dest;
  for (i=(size_t) 0,pd=(unsigned char *)dest,ps=(const unsigned char *)src;
       i<=(n-((size_t) 1));
       i++,pd++,ps++) {
    *pd = *ps;
  }
  return dest;
}

/* Tries to multiply the two size_t arguments a and b.

   If the product holds on a size_t variable, sets the 
   variable pointed to by c to that product and returns a 
   non-zero value.
   
   Otherwise, does not touch the variable pointed to by c and 
   returns zero.

   This implementation is kind of naive as it uses a division.
   If performance is an issue, try to speed it up by avoiding 
   the division while making sure that it still does the right 
   thing (which is hard to prove).

*/
static int __try_size_t_multiply(size_t *c, size_t a, size_t b) {
  size_t t, r, q;

  /* If any of the arguments a and b is zero, everthing works just fine. */
  if ((a == ((size_t) 0)) ||
      (b == ((size_t) 0))) {
    *c = a * b;
    return 1;
  }

  /* Here, neither a nor b is zero. 

     We perform the multiplication, which may overflow, i.e. present
     some modulo-behavior.

  */
  t = a * b;

  /* Perform Euclidian division on t by a:

     t = a * q + r

     As we are sure that a is non-zero, we are sure
     that we will not divide by zero.

  */
  q = t / a;
  r = t % a;

  /* If the rest r is non-zero, the multiplication overflowed. */
  if (r != ((size_t) 0)) return 0;

  /* Here the rest r is zero, so we are sure that t = a * q.

     If q is different from b, the multiplication overflowed.
     Otherwise we are sure that t = a * b.

  */
  if (q != b) return 0;
  *c = t;
  return 1;
}


/* Memory allocation functions */
void *__malloc_impl(size_t size) {
    if (size == 0) {
        return NULL;
    }

    size_t total_size = size + sizeof(block);
    block *curr = free_list;
    block *prev = NULL;

    while (curr) {
        if (curr->size >= total_size) {
            if (curr->size > total_size + sizeof(block)) {
                block *new_block = (block *)((char *)curr + total_size);
                new_block->size = curr->size - total_size;
                new_block->next = curr->next;
                curr->size = size;
                curr->next = new_block;
                if (!prev) {
                    free_list = new_block;
                } else {
                    prev->next = new_block;
                }
            } else {
                if (!prev) {
                    free_list = curr->next;
                } else {
                    prev->next = curr->next;
                }
            }
            return (void *)(curr + 1);
        }
        prev = curr;
        curr = curr->next;
    }

    size_t num_pages = (total_size + PAGE_SIZE - 1) / PAGE_SIZE;
    total_size = num_pages * PAGE_SIZE;
    block *new_block = (block *)mmap(NULL, total_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (new_block == MAP_FAILED) {
        return NULL;
    }
    new_block->size = total_size - sizeof(block);
    new_block->next = block_list;
    block_list = new_block;

    return (void *)(new_block + 1);
}

void *__calloc_impl(size_t nmemb, size_t size) {
    size_t total_size;
    if (!__try_size_t_multiply(&total_size, nmemb, size)) {
        return NULL;
    }
    void *ptr = __malloc_impl(total_size);
    if (ptr) {
        __memset(ptr, 0, total_size);
    }
    return ptr;
}

void *__realloc_impl(void *ptr, size_t size) {
    if (!ptr) {
        return __malloc_impl(size);
    }
    if (size == 0) {
        __free_impl(ptr);
        return NULL;
    }
    block *curr = (block *)((char *)ptr - sizeof(block));
    if (size <= curr->size) {
        return ptr;
    }
    void *new_ptr = __malloc_impl(size);
    if (new_ptr) {
        __memcpy(new_ptr, ptr, curr->size);
        __free_impl(ptr);
    }
    return new_ptr;
}

void __free_impl(void *ptr) {
    if (!ptr) {
        return;
    }
    block *curr = (block *)((char *)ptr - sizeof(block));
    curr->size = 0;
    curr->next = free_list;
    free_list = curr;
}

/* End of the actual malloc/calloc/realloc/free functions */
