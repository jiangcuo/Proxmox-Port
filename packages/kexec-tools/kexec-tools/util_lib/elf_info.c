/*
 * elf_info.c: Architecture independent code for parsing elf files.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation (version 2 of the License).
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <elf_info.h>

/* The 32bit and 64bit note headers make it clear we don't care */
typedef Elf32_Nhdr Elf_Nhdr;

const char *fname;
static Elf64_Ehdr ehdr;
static Elf64_Phdr *phdr;
static int num_pt_loads;

static char osrelease[4096];

/* VMCOREINFO symbols for lockless printk ringbuffer */
static loff_t prb_vaddr;
static size_t printk_ringbuffer_sz;
static size_t prb_desc_sz;
static size_t printk_info_sz;
static uint64_t printk_ringbuffer_desc_ring_offset;
static uint64_t printk_ringbuffer_text_data_ring_offset;
static uint64_t prb_desc_ring_count_bits_offset;
static uint64_t prb_desc_ring_descs_offset;
static uint64_t prb_desc_ring_infos_offset;
static uint64_t prb_data_ring_size_bits_offset;
static uint64_t prb_data_ring_data_offset;
static uint64_t prb_desc_ring_head_id_offset;
static uint64_t prb_desc_ring_tail_id_offset;
static uint64_t atomic_long_t_counter_offset;
static uint64_t prb_desc_state_var_offset;
static uint64_t prb_desc_info_offset;
static uint64_t prb_desc_text_blk_lpos_offset;
static uint64_t prb_data_blk_lpos_begin_offset;
static uint64_t prb_data_blk_lpos_next_offset;
static uint64_t printk_info_seq_offset;
static uint64_t printk_info_caller_id_offset;
static uint64_t printk_info_ts_nsec_offset;
static uint64_t printk_info_level_offset;
static uint64_t printk_info_text_len_offset;

static loff_t log_buf_vaddr;
static loff_t log_end_vaddr;
static loff_t log_buf_len_vaddr;
static loff_t logged_chars_vaddr;

/* record format logs */
static loff_t log_first_idx_vaddr;
static loff_t log_next_idx_vaddr;

/* struct printk_log (or older log) size */
static uint64_t log_sz;

/* struct printk_log (or older log) field offsets */
static uint64_t log_offset_ts_nsec = UINT64_MAX;
static uint16_t log_offset_len = UINT16_MAX;
static uint16_t log_offset_text_len = UINT16_MAX;

static uint64_t phys_offset = UINT64_MAX;

#if __BYTE_ORDER == __LITTLE_ENDIAN
#define ELFDATANATIVE ELFDATA2LSB
#elif __BYTE_ORDER == __BIG_ENDIAN
#define ELFDATANATIVE ELFDATA2MSB
#else
#error "Unknown machine endian"
#endif

static uint16_t file16_to_cpu(uint16_t val)
{
	if (ehdr.e_ident[EI_DATA] != ELFDATANATIVE)
		val = bswap_16(val);
	return val;
}

static uint32_t file32_to_cpu(uint32_t val)
{
	if (ehdr.e_ident[EI_DATA] != ELFDATANATIVE)
		val = bswap_32(val);
	return val;
}

static uint64_t file64_to_cpu(uint64_t val)
{
	if (ehdr.e_ident[EI_DATA] != ELFDATANATIVE)
		val = bswap_64(val);
	return val;
}

static uint64_t vaddr_to_offset(uint64_t vaddr)
{
	/* Just hand the simple case where kexec gets
	 * the virtual address on the program headers right.
	 */
	ssize_t i;
	for (i = 0; i < ehdr.e_phnum; i++) {
		if (phdr[i].p_vaddr > vaddr)
			continue;
		if ((phdr[i].p_vaddr + phdr[i].p_memsz) <= vaddr)
			continue;
		return (vaddr - phdr[i].p_vaddr) + phdr[i].p_offset;
	}
	fprintf(stderr, "No program header covering vaddr 0x%llxfound kexec bug?\n",
		(unsigned long long)vaddr);
	exit(30);
}

static unsigned machine_pointer_bits(void)
{
	uint8_t bits = 0;

	/* Default to the size of the elf class */
	switch(ehdr.e_ident[EI_CLASS]) {
	case ELFCLASS32:        bits = 32; break;
	case ELFCLASS64:        bits = 64; break;
	}

	/* Report the architectures pointer size */
	switch(ehdr.e_machine) {
	case EM_386:            bits = 32; break;
	}

	return bits;
}

void read_elf32(int fd)
{
	Elf32_Ehdr ehdr32;
	Elf32_Phdr *phdr32;
	size_t phdrs32_size;
	ssize_t ret, i;

	ret = pread(fd, &ehdr32, sizeof(ehdr32), 0);
	if (ret != sizeof(ehdr32)) {
		fprintf(stderr, "Read of Elf header from %s failed: %s\n",
			fname, strerror(errno));
		exit(10);
	}

	ehdr.e_type		= file16_to_cpu(ehdr32.e_type);
	ehdr.e_machine		= file16_to_cpu(ehdr32.e_machine);
	ehdr.e_version		= file32_to_cpu(ehdr32.e_version);
	ehdr.e_entry		= file32_to_cpu(ehdr32.e_entry);
	ehdr.e_phoff		= file32_to_cpu(ehdr32.e_phoff);
	ehdr.e_shoff		= file32_to_cpu(ehdr32.e_shoff);
	ehdr.e_flags		= file32_to_cpu(ehdr32.e_flags);
	ehdr.e_ehsize		= file16_to_cpu(ehdr32.e_ehsize);
	ehdr.e_phentsize	= file16_to_cpu(ehdr32.e_phentsize);
	ehdr.e_phnum		= file16_to_cpu(ehdr32.e_phnum);
	ehdr.e_shentsize	= file16_to_cpu(ehdr32.e_shentsize);
	ehdr.e_shnum		= file16_to_cpu(ehdr32.e_shnum);
	ehdr.e_shstrndx		= file16_to_cpu(ehdr32.e_shstrndx);

	if (ehdr.e_version != EV_CURRENT) {
		fprintf(stderr, "Bad Elf header version %u\n",
			ehdr.e_version);
		exit(11);
	}
	if (ehdr.e_phentsize != sizeof(Elf32_Phdr)) {
		fprintf(stderr, "Bad Elf progra header size %u expected %zu\n",
			ehdr.e_phentsize, sizeof(Elf32_Phdr));
		exit(12);
	}
	phdrs32_size = ehdr.e_phnum * sizeof(Elf32_Phdr);
	phdr32 = calloc(ehdr.e_phnum, sizeof(Elf32_Phdr));
	if (!phdr32) {
		fprintf(stderr, "Calloc of %u phdrs32 failed: %s\n",
			ehdr.e_phnum, strerror(errno));
		exit(14);
	}
	phdr = calloc(ehdr.e_phnum, sizeof(Elf64_Phdr));
	if (!phdr) {
		fprintf(stderr, "Calloc of %u phdrs failed: %s\n",
			ehdr.e_phnum, strerror(errno));
		exit(15);
	}
	ret = pread(fd, phdr32, phdrs32_size, ehdr.e_phoff);
	if (ret < 0 || (size_t)ret != phdrs32_size) {
		fprintf(stderr, "Read of program header @ 0x%llu for %zu bytes failed: %s\n",
			(unsigned long long)ehdr.e_phoff, phdrs32_size, strerror(errno));
		exit(16);
	}
	for (i = 0; i < ehdr.e_phnum; i++) {
		phdr[i].p_type		= file32_to_cpu(phdr32[i].p_type);
		phdr[i].p_offset	= file32_to_cpu(phdr32[i].p_offset);
		phdr[i].p_vaddr		= file32_to_cpu(phdr32[i].p_vaddr);
		phdr[i].p_paddr		= file32_to_cpu(phdr32[i].p_paddr);
		phdr[i].p_filesz	= file32_to_cpu(phdr32[i].p_filesz);
		phdr[i].p_memsz		= file32_to_cpu(phdr32[i].p_memsz);
		phdr[i].p_flags		= file32_to_cpu(phdr32[i].p_flags);
		phdr[i].p_align		= file32_to_cpu(phdr32[i].p_align);

		if (phdr[i].p_type == PT_LOAD)
			num_pt_loads++;
	}
	free(phdr32);
}

void read_elf64(int fd)
{
	Elf64_Ehdr ehdr64;
	Elf64_Phdr *phdr64;
	size_t phdrs_size;
	ssize_t ret, i;

	ret = pread(fd, &ehdr64, sizeof(ehdr64), 0);
	if (ret < 0 || (size_t)ret != sizeof(ehdr)) {
		fprintf(stderr, "Read of Elf header from %s failed: %s\n",
			fname, strerror(errno));
		exit(10);
	}

	ehdr.e_type		= file16_to_cpu(ehdr64.e_type);
	ehdr.e_machine		= file16_to_cpu(ehdr64.e_machine);
	ehdr.e_version		= file32_to_cpu(ehdr64.e_version);
	ehdr.e_entry		= file64_to_cpu(ehdr64.e_entry);
	ehdr.e_phoff		= file64_to_cpu(ehdr64.e_phoff);
	ehdr.e_shoff		= file64_to_cpu(ehdr64.e_shoff);
	ehdr.e_flags		= file32_to_cpu(ehdr64.e_flags);
	ehdr.e_ehsize		= file16_to_cpu(ehdr64.e_ehsize);
	ehdr.e_phentsize	= file16_to_cpu(ehdr64.e_phentsize);
	ehdr.e_phnum		= file16_to_cpu(ehdr64.e_phnum);
	ehdr.e_shentsize	= file16_to_cpu(ehdr64.e_shentsize);
	ehdr.e_shnum		= file16_to_cpu(ehdr64.e_shnum);
	ehdr.e_shstrndx		= file16_to_cpu(ehdr64.e_shstrndx);

	if (ehdr.e_version != EV_CURRENT) {
		fprintf(stderr, "Bad Elf header version %u\n",
			ehdr.e_version);
		exit(11);
	}
	if (ehdr.e_phentsize != sizeof(Elf64_Phdr)) {
		fprintf(stderr, "Bad Elf progra header size %u expected %zu\n",
			ehdr.e_phentsize, sizeof(Elf64_Phdr));
		exit(12);
	}
	phdrs_size = ehdr.e_phnum * sizeof(Elf64_Phdr);
	phdr64 = calloc(ehdr.e_phnum, sizeof(Elf64_Phdr));
	if (!phdr64) {
		fprintf(stderr, "Calloc of %u phdrs64 failed: %s\n",
			ehdr.e_phnum, strerror(errno));
		exit(14);
	}
	phdr = calloc(ehdr.e_phnum, sizeof(Elf64_Phdr));
	if (!phdr) {
		fprintf(stderr, "Calloc of %u phdrs failed: %s\n",
			ehdr.e_phnum, strerror(errno));
		exit(15);
	}
	ret = pread(fd, phdr64, phdrs_size, ehdr.e_phoff);
	if (ret < 0 || (size_t)ret != phdrs_size) {
		fprintf(stderr, "Read of program header @ %llu for %zu bytes failed: %s\n",
			(unsigned long long)(ehdr.e_phoff), phdrs_size, strerror(errno));
		exit(16);
	}
	for (i = 0; i < ehdr.e_phnum; i++) {
		phdr[i].p_type		= file32_to_cpu(phdr64[i].p_type);
		phdr[i].p_flags		= file32_to_cpu(phdr64[i].p_flags);
		phdr[i].p_offset	= file64_to_cpu(phdr64[i].p_offset);
		phdr[i].p_vaddr		= file64_to_cpu(phdr64[i].p_vaddr);
		phdr[i].p_paddr		= file64_to_cpu(phdr64[i].p_paddr);
		phdr[i].p_filesz	= file64_to_cpu(phdr64[i].p_filesz);
		phdr[i].p_memsz		= file64_to_cpu(phdr64[i].p_memsz);
		phdr[i].p_align		= file64_to_cpu(phdr64[i].p_align);

		if (phdr[i].p_type == PT_LOAD)
			num_pt_loads++;
	}
	free(phdr64);
}

int get_pt_load(int idx,
	unsigned long long *phys_start,
	unsigned long long *phys_end,
	unsigned long long *virt_start,
	unsigned long long *virt_end)
{
	Elf64_Phdr *pls;

	if (num_pt_loads <= idx)
		return 0;

	pls = &phdr[idx];

	if (phys_start)
		*phys_start = pls->p_paddr;
	if (phys_end)
		*phys_end   = pls->p_paddr + pls->p_memsz;
	if (virt_start)
		*virt_start = pls->p_vaddr;
	if (virt_end)
		*virt_end   = pls->p_vaddr + pls->p_memsz;

	return 1;
}

#define NOT_FOUND_LONG_VALUE		(-1)

void (*arch_scan_vmcoreinfo)(char *pos);

void scan_vmcoreinfo(char *start, size_t size)
{
	char *last = start + size - 1;
	char *pos, *eol;
	char temp_buf[1024];
	bool last_line = false;
	char *str, *endp;

#define SYMBOL(sym) {					\
	.str = "SYMBOL(" #sym  ")=",			\
	.name = #sym,					\
	.len = sizeof("SYMBOL(" #sym  ")=") - 1,	\
	.vaddr = & sym ## _vaddr,			\
 }
	static struct symbol {
		const char *str;
		const char *name;
		size_t len;
		loff_t *vaddr;
	} symbol[] = {
		SYMBOL(prb),
		SYMBOL(log_buf),
		SYMBOL(log_end),
		SYMBOL(log_buf_len),
		SYMBOL(logged_chars),
		SYMBOL(log_first_idx),
		SYMBOL(log_next_idx),
	};

	for (pos = start; pos <= last; pos = eol + 1) {
		size_t len, i;
		/* Find the end of the current line */
		for (eol = pos; (eol <= last) && (*eol != '\n') ; eol++)
			;
		if (eol > last) {
			/*
			 * We did not find \n and note ended. Currently kernel
			 * is appending last field CRASH_TIME without \n. It
			 * is ugly but handle it.
			 */
			eol = last;
			len = eol - pos + 1;
			if (len >= sizeof(temp_buf))
				len = sizeof(temp_buf) - 1;
			strncpy(temp_buf, pos, len);
			temp_buf[len] = '\0';

			pos = temp_buf;
			len = len + 1;
			eol = pos + len -1;
			last_line = true;
		} else  {
			len = eol - pos + 1;
		}

		/* Stomp the last character so I am guaranteed a terminating null */
		*eol = '\0';
		/* Copy OSRELEASE if I see it */
		if ((len >= 10) && (memcmp("OSRELEASE=", pos, 10) == 0)) {
			size_t to_copy = len - 10;
			if (to_copy >= sizeof(osrelease))
				to_copy = sizeof(osrelease) - 1;
			memcpy(osrelease, pos + 10, to_copy);
			osrelease[to_copy] = '\0';
		}
		/* See if the line is mentions a symbol I am looking for */
		for (i = 0; i < sizeof(symbol)/sizeof(symbol[0]); i++ ) {
			unsigned long long vaddr;
			if (symbol[i].len >= len)
				continue;
			if (memcmp(symbol[i].str, pos, symbol[i].len) != 0)
				continue;
			/* Found a symbol now decode it */
			vaddr = strtoull(pos + symbol[i].len, NULL, 16);
			/* Remember the virtual address */
			*symbol[i].vaddr = vaddr;
		}

		str = "SIZE(printk_ringbuffer)=";
		if (memcmp(str, pos, strlen(str)) == 0)
			printk_ringbuffer_sz = strtoull(pos + strlen(str),
							NULL, 10);

		str = "SIZE(prb_desc)=";
		if (memcmp(str, pos, strlen(str)) == 0)
			prb_desc_sz = strtoull(pos + strlen(str), NULL, 10);

		str = "SIZE(printk_info)=";
		if (memcmp(str, pos, strlen(str)) == 0)
			printk_info_sz = strtoull(pos + strlen(str), NULL, 10);

		str = "OFFSET(printk_ringbuffer.desc_ring)=";
		if (memcmp(str, pos, strlen(str)) == 0)
			printk_ringbuffer_desc_ring_offset =
				strtoull(pos + strlen(str), NULL, 10);

		str = "OFFSET(printk_ringbuffer.text_data_ring)=";
		if (memcmp(str, pos, strlen(str)) == 0)
			printk_ringbuffer_text_data_ring_offset =
				strtoull(pos + strlen(str), NULL, 10);

		str = "OFFSET(prb_desc_ring.count_bits)=";
		if (memcmp(str, pos, strlen(str)) == 0)
			prb_desc_ring_count_bits_offset =
				strtoull(pos + strlen(str), NULL, 10);

		str = "OFFSET(prb_desc_ring.descs)=";
		if (memcmp(str, pos, strlen(str)) == 0)
			prb_desc_ring_descs_offset =
				strtoull(pos + strlen(str), NULL, 10);

		str = "OFFSET(prb_desc_ring.infos)=";
		if (memcmp(str, pos, strlen(str)) == 0)
			prb_desc_ring_infos_offset =
				strtoull(pos + strlen(str), NULL, 10);

		str = "OFFSET(prb_data_ring.size_bits)=";
		if (memcmp(str, pos, strlen(str)) == 0)
			prb_data_ring_size_bits_offset =
				strtoull(pos + strlen(str), NULL, 10);

		str = "OFFSET(prb_data_ring.data)=";
		if (memcmp(str, pos, strlen(str)) == 0)
			prb_data_ring_data_offset =
				strtoull(pos + strlen(str), NULL, 10);

		str = "OFFSET(prb_desc_ring.head_id)=";
		if (memcmp(str, pos, strlen(str)) == 0)
			prb_desc_ring_head_id_offset =
				strtoull(pos + strlen(str), NULL, 10);

		str = "OFFSET(prb_desc_ring.tail_id)=";
		if (memcmp(str, pos, strlen(str)) == 0)
			prb_desc_ring_tail_id_offset =
				strtoull(pos + strlen(str), NULL, 10);

		str = "OFFSET(atomic_long_t.counter)=";
		if (memcmp(str, pos, strlen(str)) == 0)
			atomic_long_t_counter_offset =
				strtoull(pos + strlen(str), NULL, 10);

		str = "OFFSET(prb_desc.state_var)=";
		if (memcmp(str, pos, strlen(str)) == 0)
			prb_desc_state_var_offset =
				strtoull(pos + strlen(str), NULL, 10);

		str = "OFFSET(prb_desc.info)=";
		if (memcmp(str, pos, strlen(str)) == 0)
			prb_desc_info_offset =
				strtoull(pos + strlen(str), NULL, 10);

		str = "OFFSET(prb_desc.text_blk_lpos)=";
		if (memcmp(str, pos, strlen(str)) == 0)
			prb_desc_text_blk_lpos_offset =
				strtoull(pos + strlen(str), NULL, 10);

		str = "OFFSET(prb_data_blk_lpos.begin)=";
		if (memcmp(str, pos, strlen(str)) == 0)
			prb_data_blk_lpos_begin_offset =
				strtoull(pos + strlen(str), NULL, 10);

		str = "OFFSET(prb_data_blk_lpos.next)=";
		if (memcmp(str, pos, strlen(str)) == 0)
			prb_data_blk_lpos_next_offset =
				strtoull(pos + strlen(str), NULL, 10);

		str = "OFFSET(printk_info.seq)=";
		if (memcmp(str, pos, strlen(str)) == 0)
			printk_info_seq_offset =
				strtoull(pos + strlen(str), NULL, 10);

		str = "OFFSET(printk_info.caller_id)=";
		if (memcmp(str, pos, strlen(str)) == 0)
			printk_info_caller_id_offset =
				strtoull(pos + strlen(str), NULL, 10);

		str = "OFFSET(printk_info.ts_nsec)=";
		if (memcmp(str, pos, strlen(str)) == 0)
			printk_info_ts_nsec_offset =
				strtoull(pos + strlen(str), NULL, 10);

		str = "OFFSET(printk_info.level)=";
		if (memcmp(str, pos, strlen(str)) == 0)
			printk_info_level_offset =
				strtoull(pos + strlen(str), NULL, 10);

		str = "OFFSET(printk_info.text_len)=";
		if (memcmp(str, pos, strlen(str)) == 0)
			printk_info_text_len_offset =
				strtoull(pos + strlen(str), NULL, 10);

		/* Check for "SIZE(printk_log)" or older "SIZE(log)=" */
		str = "SIZE(log)=";
		if (memcmp(str, pos, strlen(str)) == 0)
			log_sz = strtoull(pos + strlen(str), NULL, 10);

		str = "SIZE(printk_log)=";
		if (memcmp(str, pos, strlen(str)) == 0)
			log_sz = strtoull(pos + strlen(str), NULL, 10);

		/* Check for struct printk_log (or older log) field offsets */
		str = "OFFSET(log.ts_nsec)=";
		if (memcmp(str, pos, strlen(str)) == 0)
			log_offset_ts_nsec = strtoull(pos + strlen(str), NULL,
							10);
		str = "OFFSET(printk_log.ts_nsec)=";
		if (memcmp(str, pos, strlen(str)) == 0)
			log_offset_ts_nsec = strtoull(pos + strlen(str), NULL,
							10);

		str = "OFFSET(log.len)=";
		if (memcmp(str, pos, strlen(str)) == 0)
			log_offset_len = strtoul(pos + strlen(str), NULL, 10);

		str = "OFFSET(printk_log.len)=";
		if (memcmp(str, pos, strlen(str)) == 0)
			log_offset_len = strtoul(pos + strlen(str), NULL, 10);

		str = "OFFSET(log.text_len)=";
		if (memcmp(str, pos, strlen(str)) == 0)
			log_offset_text_len = strtoul(pos + strlen(str), NULL,
							10);
		str = "OFFSET(printk_log.text_len)=";
		if (memcmp(str, pos, strlen(str)) == 0)
			log_offset_text_len = strtoul(pos + strlen(str), NULL,
							10);

		/* Check for PHYS_OFFSET number */
		str = "NUMBER(PHYS_OFFSET)=";
		if (memcmp(str, pos, strlen(str)) == 0) {
			phys_offset = strtoul(pos + strlen(str), &endp,
							10);
			if (strlen(endp) != 0)
				phys_offset = strtoul(pos + strlen(str), &endp, 16);
			if ((phys_offset == LONG_MAX) || strlen(endp) != 0) {
				fprintf(stderr, "Invalid data %s\n",
					pos);
				break;
			}
		}

		if (arch_scan_vmcoreinfo != NULL)
			(*arch_scan_vmcoreinfo)(pos);

		if (last_line)
			break;
	}
}

static int scan_notes(int fd, loff_t start, loff_t lsize)
{
	char *buf, *last, *note, *next;
	size_t size;
	ssize_t ret;

	if (lsize > SSIZE_MAX) {
		fprintf(stderr, "Unable to handle note section of %llu bytes\n",
			(unsigned long long)lsize);
		exit(20);
	}

	size = lsize;
	buf = malloc(size);
	if (!buf) {
		fprintf(stderr, "Cannot malloc %zu bytes\n", size);
		exit(21);
	}
	last = buf + size - 1;
	ret = pread(fd, buf, size, start);
	if (ret != (ssize_t)size) {
		fprintf(stderr, "Cannot read note section @ 0x%llx of %zu bytes: %s\n",
			(unsigned long long)start, size, strerror(errno));
		exit(22);
	}

	for (note = buf; (note + sizeof(Elf_Nhdr)) < last; note = next)
	{
		Elf_Nhdr *hdr;
		char *n_name, *n_desc;
		size_t n_namesz, n_descsz, n_type;

		hdr = (Elf_Nhdr *)note;
		n_namesz = file32_to_cpu(hdr->n_namesz);
		n_descsz = file32_to_cpu(hdr->n_descsz);
		n_type   = file32_to_cpu(hdr->n_type);

		n_name = note + sizeof(*hdr);
		n_desc = n_name + ((n_namesz + 3) & ~3);
		next = n_desc + ((n_descsz + 3) & ~3);

		if (next > (last + 1))
			break;

		if ((memcmp(n_name, "VMCOREINFO", 11) != 0) || (n_type != 0))
			continue;
		scan_vmcoreinfo(n_desc, n_descsz);
	}

	if ((note + sizeof(Elf_Nhdr)) == last) {
		free(buf);
		return -1;
	}

	free(buf);

	return 0;
}

static int scan_note_headers(int fd)
{
	int i, ret = 0;

	for (i = 0; i < ehdr.e_phnum; i++) {
		if (phdr[i].p_type != PT_NOTE)
			continue;
		ret = scan_notes(fd, phdr[i].p_offset, phdr[i].p_filesz);
	}

	return ret;
}

static uint64_t read_file_pointer(int fd, uint64_t addr)
{
	uint64_t result;
	ssize_t ret;

	if (machine_pointer_bits() == 64) {
		uint64_t scratch;
		ret = pread(fd, &scratch, sizeof(scratch), addr);
		if (ret != sizeof(scratch)) {
			fprintf(stderr, "Failed to read pointer @ 0x%llx: %s\n",
				(unsigned long long)addr, strerror(errno));
			exit(40);
		}
		result = file64_to_cpu(scratch);
	} else {
		uint32_t scratch;
		ret = pread(fd, &scratch, sizeof(scratch), addr);
		if (ret != sizeof(scratch)) {
			fprintf(stderr, "Failed to read pointer @ 0x%llx: %s\n",
				(unsigned long long)addr, strerror(errno));
			exit(40);
		}
		result = file32_to_cpu(scratch);
	}
	return result;
}

static uint32_t read_file_u32(int fd, uint64_t addr)
{
	uint32_t scratch;
	ssize_t ret;
	ret = pread(fd, &scratch, sizeof(scratch), addr);
	if (ret != sizeof(scratch)) {
		fprintf(stderr, "Failed to read value @ 0x%llx: %s\n",
			(unsigned long long)addr, strerror(errno));
		exit(41);
	}
	return file32_to_cpu(scratch);
}

static int32_t read_file_s32(int fd, uint64_t addr)
{
	return read_file_u32(fd, addr);
}

static void dump_dmesg_legacy(int fd, void (*handler)(char*, unsigned int))
{
	uint64_t log_buf, log_buf_offset;
	unsigned log_end, logged_chars, log_end_wrapped;
	int log_buf_len, to_wrap;
	char *buf;
	ssize_t ret;

	if (!log_buf_vaddr) {
		fprintf(stderr, "Missing the log_buf symbol\n");
		exit(50);
	}
	if (!log_end_vaddr) {
		fprintf(stderr, "Missing the log_end symbol\n");
		exit(51);
	}
	if (!log_buf_len_vaddr) {
		fprintf(stderr, "Missing the log_bug_len symbol\n");
		exit(52);
	}
	if (!logged_chars_vaddr) {
		fprintf(stderr, "Missing the logged_chars symbol\n");
		exit(53);
	}

	log_buf = read_file_pointer(fd, vaddr_to_offset(log_buf_vaddr));
	log_end = read_file_u32(fd, vaddr_to_offset(log_end_vaddr));
	log_buf_len = read_file_s32(fd, vaddr_to_offset(log_buf_len_vaddr));
	logged_chars = read_file_u32(fd, vaddr_to_offset(logged_chars_vaddr));

	log_buf_offset = vaddr_to_offset(log_buf);

	buf = calloc(1, log_buf_len);
	if (!buf) {
		fprintf(stderr, "Failed to malloc %d bytes for the logbuf: %s\n",
			log_buf_len, strerror(errno));
		exit(51);
	}

	log_end_wrapped = log_end % log_buf_len;
	to_wrap = log_buf_len - log_end_wrapped;

	ret = pread(fd, buf, to_wrap, log_buf_offset + log_end_wrapped);
	if (ret != to_wrap) {
		fprintf(stderr, "Failed to read the first half of the log buffer: %s\n",
			strerror(errno));
		exit(52);
	}
	ret = pread(fd, buf + to_wrap, log_end_wrapped, log_buf_offset);
	if (ret != log_end_wrapped) {
		fprintf(stderr, "Faield to read the second half of the log buffer: %s\n",
			strerror(errno));
		exit(53);
	}

	/*
	 * To collect full dmesg including the part before `dmesg -c` is useful
	 * for later debugging. Use same logic as what crash utility is using.
	 */
	logged_chars = log_end < log_buf_len ? log_end : log_buf_len;

	if (handler)
		handler(buf + (log_buf_len - logged_chars), logged_chars);
}

static inline uint16_t struct_val_u16(char *ptr, unsigned int offset)
{
	return(file16_to_cpu(*(uint16_t *)(ptr + offset)));
}

static inline uint32_t struct_val_u32(char *ptr, unsigned int offset)
{
	return(file32_to_cpu(*(uint32_t *)(ptr + offset)));
}

static inline uint64_t struct_val_u64(char *ptr, unsigned int offset)
{
	return(file64_to_cpu(*(uint64_t *)(ptr + offset)));
}

/* Read headers of log records and dump accordingly */
static void dump_dmesg_structured(int fd, void (*handler)(char*, unsigned int))
{
#define OUT_BUF_SIZE	4096
	uint64_t log_buf, log_buf_offset, ts_nsec;
	uint32_t log_buf_len, log_first_idx, log_next_idx, current_idx, len = 0, i;
	char *buf, out_buf[OUT_BUF_SIZE];
	bool has_wrapped_around = false;
	ssize_t ret;
	char *msg;
	uint16_t text_len;
	imaxdiv_t imaxdiv_sec, imaxdiv_usec;

	if (!log_buf_vaddr) {
		fprintf(stderr, "Missing the log_buf symbol\n");
		exit(60);
	}

	if (!log_buf_len_vaddr) {
		fprintf(stderr, "Missing the log_bug_len symbol\n");
		exit(61);
	}

	if (!log_first_idx_vaddr) {
		fprintf(stderr, "Missing the log_first_idx symbol\n");
		exit(62);
	}

	if (!log_next_idx_vaddr) {
		fprintf(stderr, "Missing the log_next_idx symbol\n");
		exit(63);
	}

	if (!log_sz) {
		fprintf(stderr, "Missing the struct log size export\n");
		exit(64);
	}

	if (log_offset_ts_nsec == UINT64_MAX) {
		fprintf(stderr, "Missing the log.ts_nsec offset export\n");
		exit(65);
	}

	if (log_offset_len == UINT16_MAX) {
		fprintf(stderr, "Missing the log.len offset export\n");
		exit(66);
	}

	if (log_offset_text_len == UINT16_MAX) {
		fprintf(stderr, "Missing the log.text_len offset export\n");
		exit(67);
	}

	log_buf = read_file_pointer(fd, vaddr_to_offset(log_buf_vaddr));
	log_buf_len = read_file_s32(fd, vaddr_to_offset(log_buf_len_vaddr));

	log_first_idx = read_file_u32(fd, vaddr_to_offset(log_first_idx_vaddr));
	log_next_idx = read_file_u32(fd, vaddr_to_offset(log_next_idx_vaddr));

	log_buf_offset = vaddr_to_offset(log_buf);

	buf = calloc(1, log_sz);
	if (!buf) {
		fprintf(stderr, "Failed to malloc %" PRId64 " bytes for the log:"
				" %s\n", log_sz, strerror(errno));
		exit(64);
	}

	/* Parse records and write out data at standard output */

	current_idx = log_first_idx;
	len = 0;
	while (current_idx != log_next_idx) {
		uint16_t loglen;

		ret = pread(fd, buf, log_sz, log_buf_offset + current_idx);
		if (ret != log_sz) {
			fprintf(stderr, "Failed to read log of size %" PRId64 " bytes:"
				" %s\n", log_sz, strerror(errno));
			exit(65);
		}
		ts_nsec = struct_val_u64(buf, log_offset_ts_nsec);
		imaxdiv_sec = imaxdiv(ts_nsec, 1000000000);
		imaxdiv_usec = imaxdiv(imaxdiv_sec.rem, 1000);

		len += sprintf(out_buf + len, "[%5llu.%06llu] ",
			(long long unsigned int)imaxdiv_sec.quot,
			(long long unsigned int)imaxdiv_usec.quot);

		/* escape non-printable characters */
		text_len = struct_val_u16(buf, log_offset_text_len);
		msg = calloc(1, text_len);
		if (!msg) {
			fprintf(stderr, "Failed to malloc %u bytes for log text:"
				" %s\n", text_len, strerror(errno));
			exit(64);
		}

		ret = pread(fd, msg, text_len, log_buf_offset + current_idx + log_sz);
		if (ret != text_len) {
			fprintf(stderr, "Failed to read log text of size %u bytes:"
				" %s\n", text_len, strerror(errno));
			exit(65);
		}
		for (i = 0; i < text_len; i++) {
			unsigned char c = msg[i];

			if (!isprint(c) && !isspace(c))
				len += sprintf(out_buf + len, "\\x%02x", c);
			else
				out_buf[len++] = c;

			if (len >= OUT_BUF_SIZE - 64) {
				if (handler)
					handler(out_buf, len);
				len = 0;
			}
		}

		out_buf[len++] = '\n';
		free(msg);
		/*
		 * A length == 0 record is the end of buffer marker. Wrap around
		 * and read the message at the start of the buffer.
		 */
		loglen = struct_val_u16(buf, log_offset_len);
		if (!loglen) {
			if (has_wrapped_around) {
				if (len && handler)
					handler(out_buf, len);
				fprintf(stderr, "Cycle when parsing dmesg detected.\n");
				fprintf(stderr, "The prink log_buf is most likely corrupted.\n");
				fprintf(stderr, "log_buf = 0x%lx, idx = 0x%x\n",
					log_buf, current_idx);
				exit(68);
			}
			current_idx = 0;
			has_wrapped_around = true;
		} else {
			/* Move to next record */
			current_idx += loglen;
			if(current_idx > log_buf_len - log_sz) {
				if (len && handler)
					handler(out_buf, len);
				fprintf(stderr, "Index outside log_buf detected.\n");
				fprintf(stderr, "The prink log_buf is most likely corrupted.\n");
				fprintf(stderr, "log_buf = 0x%lx, idx = 0x%x\n",
					log_buf, current_idx);
				exit(69);
			}
		}
	}
	free(buf);
	if (len && handler)
		handler(out_buf, len);
}

/* convenience struct for passing many values to helper functions */
struct prb_map {
	char		*prb;

	char		*desc_ring;
	unsigned long	desc_ring_count;
	char		*descs;

	char		*infos;

	char		*text_data_ring;
	unsigned long	text_data_ring_size;
	char		*text_data;
};

/*
 * desc_state and DESC_* definitions taken from kernel source:
 *
 * kernel/printk/printk_ringbuffer.h
 *
 * DESC_* definitions modified to provide 32-bit and 64-bit variants.
 */

/* The possible responses of a descriptor state-query. */
enum desc_state {
	desc_miss	=  -1,	/* ID mismatch (pseudo state) */
	desc_reserved	= 0x0,	/* reserved, in use by writer */
	desc_committed	= 0x1,	/* committed by writer, could get reopened */
	desc_finalized	= 0x2,	/* committed, no further modification allowed */
	desc_reusable	= 0x3,	/* free, not yet used by any writer */
};

#define DESC_SV_BITS		(sizeof(uint64_t) * 8)
#define DESC_FLAGS_SHIFT	(DESC_SV_BITS - 2)
#define DESC_FLAGS_MASK		(3ULL << DESC_FLAGS_SHIFT)
#define DESC_STATE(sv)		(3ULL & (sv >> DESC_FLAGS_SHIFT))
#define DESC_ID_MASK		(~DESC_FLAGS_MASK)
#define DESC_ID(sv)		((sv) & DESC_ID_MASK)

#define DESC32_SV_BITS		(sizeof(uint32_t) * 8)
#define DESC32_FLAGS_SHIFT	(DESC32_SV_BITS - 2)
#define DESC32_FLAGS_MASK	(3UL << DESC32_FLAGS_SHIFT)
#define DESC32_STATE(sv)	(3UL & (sv >> DESC32_FLAGS_SHIFT))
#define DESC32_ID_MASK		(~DESC32_FLAGS_MASK)
#define DESC32_ID(sv)		((sv) & DESC32_ID_MASK)

/*
 * get_desc_state() taken from kernel source:
 *
 * kernel/printk/printk_ringbuffer.c
 *
 * get_desc32_state() added as 32-bit variant.
 */

/* Query the state of a descriptor. */
static enum desc_state get_desc_state(unsigned long id,
				      uint64_t state_val)
{
	if (id != DESC_ID(state_val))
		return desc_miss;

	return DESC_STATE(state_val);
}

static enum desc_state get_desc32_state(unsigned long id,
					uint64_t state_val)
{
	if (id != DESC32_ID(state_val))
		return desc_miss;

	return DESC32_STATE(state_val);
}

static bool record_committed(unsigned long id, uint64_t state_var)
{
	enum desc_state state;

	if (machine_pointer_bits() == 32)
		state = get_desc32_state(id, state_var);
	else
		state = get_desc_state(id, state_var);

	return (state == desc_committed || state == desc_finalized);
}

static uint64_t id_inc(uint64_t id)
{
	id++;

	if (machine_pointer_bits() == 32)
		return (id & DESC32_ID_MASK);

	return (id & DESC_ID_MASK);
}

static uint64_t get_ulong(char *addr)
{
	if (machine_pointer_bits() == 32)
		return struct_val_u32(addr, 0);
	return struct_val_u64(addr, 0);
}

static uint64_t sizeof_ulong(void)
{
	return (machine_pointer_bits() >> 3);
}

static void dump_record(struct prb_map *m, unsigned long id,
			void (*handler)(char*, unsigned int))
{
#define OUT_BUF_SIZE	4096
	char out_buf[OUT_BUF_SIZE];
	imaxdiv_t imaxdiv_usec;
	imaxdiv_t imaxdiv_sec;
	uint32_t offset = 0;
	unsigned short len;
	uint64_t state_var;
	uint64_t ts_nsec;
	uint64_t begin;
	uint64_t next;
	char *info;
	char *text;
	char *desc;
	int i;

	desc = m->descs + ((id % m->desc_ring_count) * prb_desc_sz);
	info = m->infos + ((id % m->desc_ring_count) * printk_info_sz);

	/* skip non-committed record */
	state_var = get_ulong(desc + prb_desc_state_var_offset +
					atomic_long_t_counter_offset);
	if (!record_committed(id, state_var))
		return;

	begin = get_ulong(desc + prb_desc_text_blk_lpos_offset +
			  prb_data_blk_lpos_begin_offset) %
		m->text_data_ring_size;
	next = get_ulong(desc + prb_desc_text_blk_lpos_offset +
			 prb_data_blk_lpos_next_offset) %
	       m->text_data_ring_size;

	ts_nsec = struct_val_u64(info, printk_info_ts_nsec_offset);
	imaxdiv_sec = imaxdiv(ts_nsec, 1000000000);
	imaxdiv_usec = imaxdiv(imaxdiv_sec.rem, 1000);

	offset += sprintf(out_buf + offset, "[%5llu.%06llu] ",
		(long long unsigned int)imaxdiv_sec.quot,
		(long long unsigned int)imaxdiv_usec.quot);

	/* skip data-less text blocks */
	if (begin == next)
		goto out;

	len = struct_val_u16(info, printk_info_text_len_offset);

	/* handle wrapping data block */
	if (begin > next)
		begin = 0;

	/* skip over descriptor ID */
	begin += sizeof_ulong();

	/* handle truncated messages */
	if (next - begin < len)
		len = next - begin;

	text = m->text_data + begin;

	/* escape non-printable characters */
	for (i = 0; i < len; i++) {
		unsigned char c = text[i];

		if (!isprint(c) && !isspace(c))
			offset += sprintf(out_buf + offset, "\\x%02x", c);
		else
			out_buf[offset++] = c;

		if (offset >= OUT_BUF_SIZE - 64) {
			if (handler)
				handler(out_buf, offset);
			offset = 0;
		}
	}
out:
	out_buf[offset++] = '\n';

	if (offset && handler)
		handler(out_buf, offset);
}

/*
 *  Handle the lockless printk_ringbuffer.
 */
static void dump_dmesg_lockless(int fd, void (*handler)(char*, unsigned int))
{
	struct prb_map m;
	uint64_t head_id;
	uint64_t tail_id;
	uint64_t kaddr;
	uint64_t id;
	int ret;

	/* setup printk_ringbuffer */
	kaddr = read_file_pointer(fd, vaddr_to_offset(prb_vaddr));
	m.prb = calloc(1, printk_ringbuffer_sz);
	if (!m.prb) {
		fprintf(stderr, "Failed to malloc %zu bytes for prb: %s\n",
			printk_ringbuffer_sz, strerror(errno));
		exit(64);
	}
	ret = pread(fd, m.prb, printk_ringbuffer_sz, vaddr_to_offset(kaddr));
	if (ret != printk_ringbuffer_sz) {
		fprintf(stderr, "Failed to read prb of size %zu bytes: %s\n",
			printk_ringbuffer_sz, strerror(errno));
		exit(65);
	}

	/* setup descriptor ring */
	m.desc_ring = m.prb + printk_ringbuffer_desc_ring_offset;
	m.desc_ring_count = 1 << struct_val_u32(m.desc_ring,
					prb_desc_ring_count_bits_offset);
	kaddr = get_ulong(m.desc_ring + prb_desc_ring_descs_offset);
	m.descs = calloc(1, prb_desc_sz * m.desc_ring_count);
	if (!m.descs) {
		fprintf(stderr, "Failed to malloc %lu bytes for descs: %s\n",
			prb_desc_sz * m.desc_ring_count, strerror(errno));
		exit(64);
	}
	ret = pread(fd, m.descs, prb_desc_sz * m.desc_ring_count,
		    vaddr_to_offset(kaddr));
	if (ret != prb_desc_sz * m.desc_ring_count) {
		fprintf(stderr,
			"Failed to read descs of size %lu bytes: %s\n",
			prb_desc_sz * m.desc_ring_count, strerror(errno));
		exit(65);
	}

	/* setup info ring */
	kaddr = get_ulong(m.prb + prb_desc_ring_infos_offset);
	m.infos = calloc(1, printk_info_sz * m.desc_ring_count);
	if (!m.infos) {
		fprintf(stderr, "Failed to malloc %lu bytes for infos: %s\n",
			printk_info_sz * m.desc_ring_count, strerror(errno));
		exit(64);
	}
	ret = pread(fd, m.infos, printk_info_sz * m.desc_ring_count,
		    vaddr_to_offset(kaddr));
	if (ret != printk_info_sz * m.desc_ring_count) {
		fprintf(stderr,
			"Failed to read infos of size %lu bytes: %s\n",
			printk_info_sz * m.desc_ring_count, strerror(errno));
		exit(65);
	}

	/* setup text data ring */
	m.text_data_ring = m.prb + printk_ringbuffer_text_data_ring_offset;
	m.text_data_ring_size = 1 << struct_val_u32(m.text_data_ring,
					prb_data_ring_size_bits_offset);
	kaddr = get_ulong(m.text_data_ring + prb_data_ring_data_offset);
	m.text_data = calloc(1, m.text_data_ring_size);
	if (!m.text_data) {
		fprintf(stderr,
			"Failed to malloc %lu bytes for text_data: %s\n",
			m.text_data_ring_size, strerror(errno));
		exit(64);
	}
	ret = pread(fd, m.text_data, m.text_data_ring_size,
		    vaddr_to_offset(kaddr));
	if (ret != m.text_data_ring_size) {
		fprintf(stderr,
			"Failed to read text_data of size %lu bytes: %s\n",
			m.text_data_ring_size, strerror(errno));
		exit(65);
	}

	/* ready to go */

	tail_id = get_ulong(m.desc_ring + prb_desc_ring_tail_id_offset +
						atomic_long_t_counter_offset);
	head_id = get_ulong(m.desc_ring + prb_desc_ring_head_id_offset +
						atomic_long_t_counter_offset);

	for (id = tail_id; id != head_id; id = id_inc(id))
		dump_record(&m, id, handler);

	/* dump head record */
	dump_record(&m, id, handler);

	free(m.text_data);
	free(m.infos);
	free(m.descs);
	free(m.prb);
}

void dump_dmesg(int fd, void (*handler)(char*, unsigned int))
{
	if (prb_vaddr)
		dump_dmesg_lockless(fd, handler);
	else if (log_first_idx_vaddr)
		dump_dmesg_structured(fd, handler);
	else
		dump_dmesg_legacy(fd, handler);
}

int read_elf(int fd)
{
	int ret;

	ret = pread(fd, ehdr.e_ident, EI_NIDENT, 0);
	if (ret != EI_NIDENT) {
		fprintf(stderr, "Read of e_ident from %s failed: %s\n",
			fname, strerror(errno));
		return 3;
	}
	if (memcmp(ehdr.e_ident, ELFMAG, SELFMAG) != 0) {
		fprintf(stderr, "Missing elf signature\n");
		return 4;
	}
	if (ehdr.e_ident[EI_VERSION] != EV_CURRENT) {
		fprintf(stderr, "Bad elf version\n");
		return 5;
	}
	if ((ehdr.e_ident[EI_CLASS] != ELFCLASS32) &&
	    (ehdr.e_ident[EI_CLASS] != ELFCLASS64))
	{
		fprintf(stderr, "Unknown elf class %u\n",
			ehdr.e_ident[EI_CLASS]);
		return 6;
	}
	if ((ehdr.e_ident[EI_DATA] != ELFDATA2LSB) &&
	    (ehdr.e_ident[EI_DATA] != ELFDATA2MSB))
	{
		fprintf(stderr, "Unkown elf data order %u\n",
			ehdr.e_ident[EI_DATA]);
		return 7;
	}
	if (ehdr.e_ident[EI_CLASS] == ELFCLASS32)
		read_elf32(fd);
	else
		read_elf64(fd);

	ret = scan_note_headers(fd);
	if (ret < 0)
		return ret;

	return 0;
}

int read_phys_offset_elf_kcore(int fd, long *phys_off)
{
	int ret;

	*phys_off = ULONG_MAX;

	ret = read_elf(fd);
	if (!ret) {
		/* If we have a valid 'PHYS_OFFSET' by now,
		 * return it to the caller now.
		 */
		if (phys_offset != UINT64_MAX) {
			*phys_off = phys_offset;
			return ret;
		}
	}

	return 2;
}
