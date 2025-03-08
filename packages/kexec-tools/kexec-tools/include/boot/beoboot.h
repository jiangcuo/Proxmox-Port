
/*--- Boot image definitions ---------------------------------------*/
struct beoboot_header {
    char magic[4];
    uint8_t  arch;
    uint8_t  flags;
    uint16_t cmdline_size;/* length of command line (including null) */
    /* The alpha chunk is a backward compatibility hack.  The original
     * assumption was that integer sizes didn't matter because we
     * would never mix architectures.  x86_64 + i386 broke that
     * assumption.  It's fixed for that combination and the future.
     * However, alpha needs a little hack now... */
#ifdef __alpha__
    unsigned long kernel_size;
    unsigned long initrd_size;
#else
    uint32_t kernel_size;
    uint32_t initrd_size;
#endif
};
#define BEOBOOT_MAGIC     "BeoB"
#define BEOBOOT_ARCH_I386  1
#define BEOBOOT_ARCH_ALPHA 2
#define BEOBOOT_ARCH_PPC   3
#define BEOBOOT_ARCH_PPC64 4
#if defined(__i386__) || defined(__x86_64__)
#define BEOBOOT_ARCH BEOBOOT_ARCH_I386
#elif defined(__alpha__)
#define BEOBOOT_ARCH BEOBOOT_ARCH_ALPHA
#elif defined(powerpc)
#define BEOBOOT_ARCH BEOBOOT_ARCH_PPC
#elif defined(__powerpc64__)
#define BEOBOOT_ARCH BEOBOOT_ARCH_PPC64
#else
#error Unsupported architecture.
#endif
#define BEOBOOT_INITRD_PRESENT 1
/*------------------------------------------------------------------*/

