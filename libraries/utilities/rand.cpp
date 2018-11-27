/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */

#include <eosio/utilities/rand.hpp>
#include <fc/crypto/sha512.hpp>

#include <stdlib.h>
#include <chrono>
#include <thread>
#include <atomic>

#include <fcntl.h>
#include <sys/time.h>

#ifdef HAVE_SYS_GETRANDOM
#include <sys/syscall.h>
#include <linux/random.h>
#endif

#if defined(HAVE_GETENTROPY) || (defined(HAVE_GETENTROPY_RAND) && defined(MAC_OSX))
#include <unistd.h>
#endif

#if defined(HAVE_GETENTROPY_RAND) && defined(MAC_OSX)
#include <sys/random.h>
#endif

#ifdef HAVE_SYSCTL_ARND
#include <util/strencodings.h> // for ARRAYLEN
#include <sys/sysctl.h>
#endif

#if defined(__x86_64__) || defined(__amd64__) || defined(__i386__)
#include <cpuid.h>
#endif

#include <openssl/err.h>
#include <openssl/rand.h>

namespace eosio { namespace utilities { namespace rand {

static const int NUM_OS_RANDOM_BYTES = 32;

void memory_cleanse(void *ptr, size_t len)
{
   std::memset(ptr, 0, len);
   __asm__ __volatile__("" : : "r"(ptr) : "memory");
}

#if defined(__x86_64__) || defined(__amd64__) || defined(__i386__)
static std::atomic<bool> hwrand_initialized{false};
static bool rdrand_supported = false;
#endif

#ifdef __i386__
void static inline writele32(unsigned char* ptr, uint32_t x)
{
   uint32_t v = htole32(x);
   memcpy(ptr, (char*)&v, 4);
}
#else
void static inline writele64(unsigned char* ptr, uint64_t x)
{
   uint64_t v = htole64(x);
   memcpy(ptr, (char*)&v, 8);
}
#endif

static bool get_hw_rand(unsigned char* ent32) {
#if defined(__x86_64__) || defined(__amd64__) || defined(__i386__)
   if(!hwrand_initialized.load(std::memory_order_relaxed)){
      return false;
   }
   if (rdrand_supported) {
      uint8_t ok;
      // Not all assemblers support the rdrand instruction, write it in hex.
#ifdef __i386__
      for (int iter = 0; iter < 4; ++iter) {
            uint32_t r1, r2;
            __asm__ volatile (".byte 0x0f, 0xc7, 0xf0;" // rdrand %eax
                              ".byte 0x0f, 0xc7, 0xf2;" // rdrand %edx
                              "setc %2" :
                              "=a"(r1), "=d"(r2), "=q"(ok) :: "cc");
            if (!ok) return false;
            writele32(ent32 + 8 * iter, r1);
            writele32(ent32 + 8 * iter + 4, r2);
        }
#else
      uint64_t r1, r2, r3, r4;
      __asm__ volatile (".byte 0x48, 0x0f, 0xc7, 0xf0, " // rdrand %rax
                        "0x48, 0x0f, 0xc7, 0xf3, " // rdrand %rbx
                        "0x48, 0x0f, 0xc7, 0xf1, " // rdrand %rcx
                        "0x48, 0x0f, 0xc7, 0xf2; " // rdrand %rdx
                        "setc %4" :
      "=a"(r1), "=b"(r2), "=c"(r3), "=d"(r4), "=q"(ok) :: "cc");
      if (!ok) return false;
      writele64(ent32, r1);
      writele64(ent32 + 8, r2);
      writele64(ent32 + 16, r3);
      writele64(ent32 + 24, r4);
#endif
      return true;
   }
#endif
   return false;
}

void rand_add_seed()
{
    // Seed with CPU performance counter
    int64_t c = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    RAND_add(&c, sizeof(c), 1.5);
    memory_cleanse((void*)&c, sizeof(c));
}

/** Fallback: get 32 bytes of system entropy from /dev/urandom. The most
 * compatible way to get cryptographic randomness on UNIX-ish platforms.
 */
static bool get_dev_urandom(unsigned char *ent32)
{
   int f = open("/dev/urandom", O_RDONLY);
   if (f == -1) {
      return false;
   }
   int have = 0;
   do {
      ssize_t n = read(f, ent32 + have, NUM_OS_RANDOM_BYTES - have);
      if (n <= 0 || n + have > NUM_OS_RANDOM_BYTES) {
         close(f);
         return false;
      }
      have += n;
   } while (have < NUM_OS_RANDOM_BYTES);
   close(f);
   return true;
}

/** Get 32 bytes of system entropy. */
bool get_os_rand(unsigned char *ent32)
{
#if defined(HAVE_SYS_GETRANDOM)
   /* Linux. From the getrandom(2) man page:
     * "If the urandom source has been initialized, reads of up to 256 bytes
     * will always return as many bytes as requested and will not be
     * interrupted by signals."
     */
    int rv = syscall(SYS_getrandom, ent32, NUM_OS_RANDOM_BYTES, 0);
    if (rv != NUM_OS_RANDOM_BYTES) {
        if (rv < 0 && errno == ENOSYS) {
            /* Fallback for kernel <3.17: the return value will be -1 and errno
             * ENOSYS if the syscall is not available, in that case fall back
             * to /dev/urandom.
             */
            return get_dev_urandom(ent32);
        } else {
            return false;
        }
    }
    return true;
#elif defined(HAVE_GETENTROPY) && defined(__OpenBSD__)
   /* On OpenBSD this can return up to 256 bytes of entropy, will return an
     * error if more are requested.
     * The call cannot return less than the requested number of bytes.
       getentropy is explicitly limited to openbsd here, as a similar (but not
       the same) function may exist on other platforms via glibc.
     */
    return getentropy(ent32, NUM_OS_RANDOM_BYTES) == 0;
#elif defined(HAVE_GETENTROPY_RAND) && defined(MAC_OSX)
   // We need a fallback for OSX < 10.12
    if (&getentropy != nullptr) {
        if (getentropy(ent32, NUM_OS_RANDOM_BYTES) != 0) {
            return false;
        }
    } else {
        return get_dev_urandom(ent32);
    }
    return true;
#elif defined(HAVE_SYSCTL_ARND)
   /* FreeBSD and similar. It is possible for the call to return less
     * bytes than requested, so need to read in a loop.
     */
    static const int name[2] = {CTL_KERN, KERN_ARND};
    int have = 0;
    do {
        size_t len = NUM_OS_RANDOM_BYTES - have;
        if (sysctl(name, ARRAYLEN(name), ent32 + have, &len, nullptr, 0) != 0) {
            return false;
        }
        have += len;
    } while (have < NUM_OS_RANDOM_BYTES);
    return true;
#else
   /* Fall back to /dev/urandom if there is no specific method implemented to
    * get system entropy for this OS.
    */
   return get_dev_urandom(ent32);
#endif
}

// this func is from BTC imp and add blockID to rand
// as eosforce no support WIN32, so delete all WIN32 logic
bool rand_strong_bytes( const fc::sha256& block_id, unsigned char *out, const std::size_t& num ) {
   if( num > 32 ) {
      return false;
   }

   fc::sha512::encoder hasher;
   unsigned char buf[64];

   // 1 source: OpenSSL's RNG
   rand_add_seed();
   if( RAND_bytes(buf, 32) != 1 ) {
      return false;
   }
   hasher.write(( const char * ) buf, 32);

   // 2 source: OS RNG
   if( !get_os_rand(buf)) {
      return false;
   }
   hasher.write(( const char * ) buf, 32);

   // 3 source: HW RNG, if available.
   if( get_hw_rand(buf)) {
      hasher.write(( const char * ) buf, 32);
   }

   // 4 source: block id
   hasher.write(block_id.data(), block_id.data_size());

   const auto hash = hasher.result();

   // Produce output
   memcpy(out, hash.data(), num);
   memory_cleanse(buf, 64);

   return true;
}

}; }; }; // namespace eosio::utilities::rand