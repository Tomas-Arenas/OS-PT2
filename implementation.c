/*  

    Copyright 2018-21 by

    University of Alaska Anchorage, College of Engineering.

    Copyright 2022-24 by

    University of Texas at El Paso, Department of Computer Science.

    All rights reserved.

    Contributors:  ...
                   ...
		   ...                 and
		   Christoph Lauter

    See file memory.c on how to compile this code.

    Implement the functions __malloc_impl, __calloc_impl,
    __realloc_impl and __free_impl below. The functions must behave
    like malloc, calloc, realloc and free but your implementation must
    of course not be based on malloc, calloc, realloc and free.

    Use the mmap and munmap system calls to create private anonymous
    memory mappings and hence to get basic access to memory, as the
    kernel provides it. Implement your memory management functions
    based on that "raw" access to user space memory.

    As the mmap and munmap system calls are slow, you have to find a
    way to reduce the number of system calls, by "grouping" them into
    larger blocks of memory accesses. As a matter of course, this needs
    to be done sensibly, i.e. without wasting too much memory.

    You must not use any functions provided by the system besides mmap
    and munmap. If you need memset and memcpy, use the naive
    implementations below. If they are too slow for your purpose,
    rewrite them in order to improve them!

    Catch all errors that may occur for mmap and munmap. In these cases
    make malloc/calloc/realloc/free just fail. Do not print out any 
    debug messages as this might get you into an infinite recursion!

    Your __calloc_impl will probably just call your __malloc_impl, check
    if that allocation worked and then set the fresh allocated memory
    to all zeros. Be aware that calloc comes with two size_t arguments
    and that malloc has only one. The classical multiplication of the two
    size_t arguments of calloc is wrong! Read this to convince yourself:

    https://bugzilla.redhat.com/show_bug.cgi?id=853906

    In order to allow you to properly refuse to perform the calloc instead
    of allocating too little memory, the __try_size_t_multiply function is
    provided below for your convenience.
    
*/
#include <stdlib.h>
#include <stddef.h>
#include <sys/mman.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>



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

/* End of predefined helper functions */

/* Your helper functions 

   You may also put some struct definitions, typedefs and global
   variables here. Typically, the instructor's solution starts with
   defining a certain struct, a typedef and a global variable holding
   the start of a linked list of currently free memory blocks. That 
   list probably needs to be kept ordered by ascending addresses.

*/

typedef struct block {
  size_t size;
  size_t alloc_mem;
  size_t free_mem;
  struct block *next;
} block;

struct block *free_list = NULL;

//Block list: each region of memory contains several blocks that are linked to their neighbors. This linked list is used to merge blocks that have been split, unmap unused regions
struct block *block_list = NULL;

/* End of your helper functions */

/* Start of the actual malloc/calloc/realloc/free functions */

void __free_impl(void *);


void *__malloc_impl(size_t size) {
  /* allocates size bytes of memory, 
  RETURNS: pointer to the allocated memory, 
  if size is 0, the function returns NULL or a unique pointer to be passed to free()*/
  if(size == 0){
    return NULL;
  }
  size_t total_size = size + sizeof(struct block);
  struct block *curr = free_list;

  while(curr){
    //found a block that is big enough
    if(curr->free_mem > total_size){
      //creating a new block at the end of the allocated memory
      char *block_end = (char *)curr + curr->free_mem;
      struct block *new_block = (struct block *)block_end;

      new_block->size = curr->free_mem - sizeof(struct block);
      new_block->alloc_mem = size;
      new_block->free_mem = new_block->size - new_block->alloc_mem;

      curr->size = curr->alloc_mem;
      curr->free_mem = 0;
      new_block->next = curr->next;
      curr->next = new_block;

      //new block is big enough to split
      if(new_block->free_mem > sizeof(struct block)){
        char *block_end = (char *)new_block + new_block->free_mem;
        struct block *free_block = (struct block *)block_end;
        free_block->size = new_block->free_mem - sizeof(struct block);
        free_block->alloc_mem = 0;
        free_block->free_mem = free_block->size - free_block->alloc_mem;
        new_block->size = new_block->alloc_mem;
        new_block->free_mem = 0;
        free_block->next = new_block->next;
        new_block->next = free_block;
        if(free_list == NULL){
          free_list = free_block;
        }
        else{
          struct block *temp = free_list;
          while(temp){
            if(temp->next == NULL){
              temp->next = free_block;
              break;
            }
            temp = temp->next;
          }
        }
      }
      return (void *)(new_block + 1);
    }
    curr = curr->next;

  }

  //no block is big enough, need to allocate a new block using mmap and linking it to the block list
  size_t page_size = getpagesize();
  size_t num_pages = (total_size + page_size - 1) / page_size;
  total_size = num_pages * page_size;
  struct block *new_block = (struct block *)mmap(NULL, total_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (new_block == MAP_FAILED) {
      return NULL;
  }
  new_block->size = total_size - sizeof(struct block);
  new_block->alloc_mem = size;
  new_block->free_mem = new_block->size - new_block->alloc_mem;
  new_block->next = block_list;
  if(block_list == NULL){
    block_list = new_block;
  }
  //insert to end of block list
  else{
    struct block *temp = block_list;
    while(temp){
      if(temp->next == NULL){
        temp->next = new_block;
        break;
      }
      temp = temp->next;
    }
  }

  return (void *)(new_block + 1);

}

void *__calloc_impl(size_t nmemb, size_t size) {
  /*nmemb:  number of elements in array ,
  size: amount of bytes in each element
  RETURNS: pointer to the allocated memory, or NULL if the request fails and
  sets memory to zero,

  if nmemb or size is 0, the function returns NULL or a unique pointer to be passed to free()
  if multiplication of nmemb and size results in overflow, the function returns an error
  
  */
  size_t total_size;
  if(__try_size_t_multiply(&total_size, nmemb, size)){
    void *ptr = __malloc_impl(total_size);
    if(ptr){
      __memset(ptr, 0, total_size);
    }
    return ptr;
  }
  return NULL;

  
}

void *__realloc_impl(void *ptr, size_t size) {
  /*ptr: pointer to the memory block to be reallocated,
  size: new size for the memory block
  RETURNS: pointer to the reallocated memory, or NULL if the request fails
  
  if ptr is NULL, the function behaves like malloc() for the specified size
  if size is 0 and ptr is not NULL, the function behaves like free() and returns NULL
  if the size is less than the size of the original memory block, the memory block is truncated to the new size
  if the size is greater than the size of the original memory block, the function behaves like malloc() for the specified size
  */
  if(ptr == NULL){
    return __malloc_impl(size);
  }
  if(size == 0){
    __free_impl(ptr);
    return NULL;
  }
  struct block *curr = (struct block *)((char *)ptr - sizeof(struct block));
  if(size <= curr->alloc_mem){
    return ptr;
  }
  void *new_ptr = __malloc_impl(size);
  if(new_ptr){
    __memcpy(new_ptr, ptr, curr->alloc_mem);
    __free_impl(ptr);
  }
  return new_ptr;
}

void __free_impl(void *ptr) {
  /*ptr: pointer to the memory to be deallocated,
  RETURNS: nothing
  
       The free() function frees the memory space pointed to by ptr, which must have been returned by a previous call
       to malloc(), calloc(), or realloc().  Otherwise, or if free(ptr) has already been called before, undefined be‐
       havior occurs.  If ptr is NULL, no operation is performed.
  
  if ptr is NULL, the function does nothing
  */
  if(ptr == NULL){
    return;
  }
  struct block *curr = (struct block *)((char *)ptr - sizeof(struct block));
  curr->alloc_mem = 0;
  curr->free_mem = curr->size;
  if(free_list == NULL){
    free_list = curr;
  }
  //insert to end of free list
  else{
    struct block *temp = free_list;
    while(temp){
      if(temp->next == NULL){
        temp->next = curr;
        break;
      }
      temp = temp->next;
    }
  }

}

/* End of the actual malloc/calloc/realloc/free functions */

