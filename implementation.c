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

#include <stddef.h>
#include <sys/mman.h>

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


/* End of your helper functions */

/* Start of the actual malloc/calloc/realloc/free functions */

void __free_impl(void *);


/*
*/
void *__malloc_impl(size_t size) {
  /* allocates size bytes of memory, 
  RETURNS: pointer to the allocated memory, 
  if size is 0, the function returns NULL or a unique pointer to be passed to free()
  
   */
  if (size == ((size_t)0)) {
    return NULL;
  }
  void *ptr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (ptr == MAP_FAILED) {
    return NULL;
  }
  return ptr;
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
  if (__try_size_t_multiply(&total_size, nmemb, size)) {
    void *ptr = __malloc_impl(total_size);
    if (ptr) {
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
  if (ptr == NULL) {
    return __malloc_impl(size);
  }
  if (size == ((size_t)0)) {
    __free_impl(ptr);
    return NULL;
  }
  void *new_ptr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (new_ptr == MAP_FAILED) {
    return NULL;
  }
  __memcpy(new_ptr, ptr, size);
  __free_impl(ptr);
  return new_ptr;
 
}

void __free_impl(void *ptr) {
  /*ptr: pointer to the memory to be deallocated,
  RETURNS: nothing
  
  if ptr is NULL, the function does nothing
  */
  if (ptr == NULL) {
    return;
  }
  if (munmap(ptr, 0) == -1) {
    //error here not sure what to do tho
    return;
  }
  
  return;
}

/* End of the actual malloc/calloc/realloc/free functions */

