#ifndef WIN32
// this code works to compile on linux with glibc < = 2.27 and will then use compatibility down to 2.2.5
// will not work with glibc_2_28 which has other global symbols (statx) that are difficult to replace

// https://thecharlatan.ch/GLIBC-Back-Compat/

#include <string.h>
#include <glob.h>
#include <unistd.h>
#include <fnmatch.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

extern void __chk_fail(void) __attribute__((__noreturn__));

#if defined(HAVE_SYS_SELECT_H)
#include <sys/select.h>
#endif

int glob_old(const char* pattern, int flags, int (*errfunc) (const char* epath, int eerrno), glob_t * pglob);
#ifdef __i386__
__asm__(".symver glob_old,glob@GLIBC_2.1");
#elif defined(__amd64__)
__asm__(".symver glob_old,glob@GLIBC_2.2.5");
#elif defined(__arm__)
__asm(".symver glob_old,glob@GLIBC_2.4");
#elif defined(__aarch64__)
__asm__(".symver glob_old,glob@GLIBC_2.17");
#endif

int __wrap_glob(const char* pattern, int flags, int (*errfunc) (const char* epath, int eerrno), glob_t * pglob)
{
    return glob_old(pattern, flags, errfunc, pglob);
}

/*Lastly we need to make sure that this newly defined glob is wrapped in the old function
by adding - Wl, --wrap = glob to the linker flags.
Now lets run objdump - T monerod | grep 2\\.25:
0000000000000000      DF * UND * 0000000000000000  GLIBC_2.25  __explicit_bzero_chk
The conflicting flag here is __explicit_bzero_chk.This function is not complex, it just checks if a memory region is zeroed.Instead of selecting a different version, we just redefine the functionand add an alias to use it when library internal calls to it are made.Th
*/

/* glibc-internal users use __explicit_bzero_chk, and explicit_bzero
redirects to that.  */
//#undef explicit_bzero
/* Set LEN bytes of S to 0.  The compiler will not delete a call to
this function, even if S is dead after the call.  */
void explicit_bzero(void* s, size_t len)
{
    memset(s, '\0', len);
    /* Compiler barrier.  */
    asm volatile ("" ::: "memory");
}

// Redefine explicit_bzero_chk
void __explicit_bzero_chk(void* dst, size_t len, size_t dstlen)
{
    /* Inline __memset_chk to avoid a PLT reference to __memset_chk.  */
    if (__glibc_unlikely(dstlen < len))
       __chk_fail();

    memset(dst, '\0', len);
    /* Compiler barrier.  */
    asm volatile ("" ::: "memory");
}
/* libc-internal references use the hidden
__explicit_bzero_chk_internal symbol.  This is necessary if
__explicit_bzero_chk is implemented as an IFUNC because some
targets do not support hidden references to IFUNC symbols.  */
#define strong_alias (__explicit_bzero_chk, __explicit_bzero_chk_internal)


#ifdef __cplusplus
}
#endif

#endif

