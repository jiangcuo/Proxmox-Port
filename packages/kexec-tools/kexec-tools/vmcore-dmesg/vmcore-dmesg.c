#include <elf_info.h>

/* The 32bit and 64bit note headers make it clear we don't care */
typedef Elf32_Nhdr Elf_Nhdr;

extern const char *fname;

/* stole this macro from kernel printk.c */
#define LOG_BUF_LEN_MAX (uint32_t)(1U << 31)

static void write_to_stdout(char *buf, unsigned int nr)
{
	ssize_t ret;
	static uint32_t n_bytes = 0;

	n_bytes += nr;
	if (n_bytes > LOG_BUF_LEN_MAX) {
		fprintf(stderr, "The vmcore-dmesg.txt over 2G in size is not supported.\n");
		exit(53);
	}

	ret = write(STDOUT_FILENO, buf, nr);
	if (ret != nr) {
		fprintf(stderr, "Failed to write out the dmesg log buffer!:"
			" %s\n", strerror(errno));
		exit(54);
	}
}

static int read_vmcore_dmesg(int fd, void (*handler)(char*, unsigned int))
{
	int ret;

	ret = read_elf(fd);
	if (ret > 0) {
		fprintf(stderr, "Unable to read ELF information"
			" from vmcore\n");
		return ret;
	}

	dump_dmesg(fd, handler);

	return 0;
}

int main(int argc, char **argv)
{
	ssize_t ret;
	int fd;

	if (argc != 2) {
		fprintf(stderr, "usage: %s <kernel core file>\n", argv[0]);
		return 1;
	}
	fname = argv[1];

	fd = open(fname, O_RDONLY);
	if (fd < 0) {
		fprintf(stderr, "Cannot open %s: %s\n",
			fname, strerror(errno));
		return 2;
	}

	ret = read_vmcore_dmesg(fd, write_to_stdout);
	
	close(fd);

	return ret;
}
