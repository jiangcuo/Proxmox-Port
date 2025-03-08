
#if !defined(FUNC) || !defined(EHDR) || !defined(PHDR)
#error FUNC, EHDR and PHDR must be defined
#endif

#if (ELF_WIDTH == 64)
#define dbgprintf_phdr(prefix, phdr)						\
do {										\
	dbgprintf("%s: p_type = %u, p_offset = 0x%llx p_paddr = 0x%llx "	\
		"p_vaddr = 0x%llx p_filesz = 0x%llx p_memsz = 0x%llx\n",	\
		(prefix), (phdr)->p_type,					\
		(unsigned long long)((phdr)->p_offset),				\
		(unsigned long long)((phdr)->p_paddr),				\
		(unsigned long long)((phdr)->p_vaddr),				\
		(unsigned long long)((phdr)->p_filesz),				\
		(unsigned long long)((phdr)->p_memsz));				\
} while(0)
#else
#define dbgprintf_phdr(prefix, phdr)					\
do {									\
	dbgprintf("%s: p_type = %u, p_offset = 0x%x " "p_paddr = 0x%x "	\
		"p_vaddr = 0x%x p_filesz = 0x%x p_memsz = 0x%x\n",	\
		(prefix), (phdr)->p_type, (phdr)->p_offset, (phdr)->p_paddr, \
		(phdr)->p_vaddr, (phdr)->p_filesz, (phdr)->p_memsz);	\
} while(0)
#endif

/* Prepares the crash memory headers and stores in supplied buffer. */
int FUNC(struct kexec_info *info,
	 struct crash_elf_info *elf_info,
	 struct memory_range *range, int ranges,
	 void **buf, unsigned long *size, unsigned long align)
{
	EHDR *elf;
	PHDR *phdr;
	int i;
	unsigned long sz;
	char *bufp;
	long int nr_cpus = 0;
	uint64_t notes_addr, notes_len;
	uint64_t vmcoreinfo_addr, vmcoreinfo_len;
	int has_vmcoreinfo = 0;
	int (*get_note_info)(int cpu, uint64_t *addr, uint64_t *len);
	long int count_cpu;

	if (xen_present())
		nr_cpus = xen_get_nr_phys_cpus();
	else
		nr_cpus = sysconf(_SC_NPROCESSORS_CONF);

	if (nr_cpus < 0) {
		return -1;
	}

	if (xen_present()) {
		if (!get_xen_vmcoreinfo(&vmcoreinfo_addr, &vmcoreinfo_len))
			has_vmcoreinfo = 1;
	} else
		if (!get_kernel_vmcoreinfo(&vmcoreinfo_addr, &vmcoreinfo_len))
			has_vmcoreinfo = 1;

	sz = sizeof(EHDR) + (nr_cpus + has_vmcoreinfo) * sizeof(PHDR) +
	     ranges * sizeof(PHDR);

	/*
	 * Certain architectures such as x86_64 and ia64 require a separate
	 * PT_LOAD program header for the kernel. This is controlled through
	 * elf_info->kern_size.
	 *
	 * The separate PT_LOAD program header is required either because the
	 * kernel is mapped at a different location than the rest of the
	 * physical memory or because we need to support relocatable kernels.
	 * Or both as on x86_64.
	 *
	 * In the relocatable kernel case this PT_LOAD segment is used to tell
	 * where the kernel was actually loaded which may be different from
	 * the load address present in the vmlinux file.
	 *
	 * The extra kernel PT_LOAD program header results in a vmcore file
	 * which is larger than the size of the physical memory. This is
	 * because the memory for the kernel is present both in the kernel
	 * PT_LOAD program header and in the physical RAM program headers.
	 */

	if (elf_info->kern_size && !xen_present()) {
		sz += sizeof(PHDR);
	}

	/*
	 * Make sure the ELF core header is aligned to at least 1024.
	 * We do this because the secondary kernel gets the ELF core
	 * header address on the kernel command line through the memmap=
	 * option, and this option requires 1k granularity.
	 */

	if (align % ELF_CORE_HEADER_ALIGN) {
		return -1;
	}

	sz = _ALIGN(sz, align);

	bufp = xmalloc(sz);
	memset(bufp, 0, sz);

	*buf = bufp;
	*size = sz;

	/* Setup ELF Header*/
	elf = (EHDR *) bufp;
	bufp += sizeof(EHDR);
	memcpy(elf->e_ident, ELFMAG, SELFMAG);
	elf->e_ident[EI_CLASS]  = elf_info->class;
	elf->e_ident[EI_DATA]   = elf_info->data;
	elf->e_ident[EI_VERSION]= EV_CURRENT;
	elf->e_ident[EI_OSABI] = ELFOSABI_NONE;
	memset(elf->e_ident+EI_PAD, 0, EI_NIDENT-EI_PAD);
	elf->e_type	= ET_CORE;
	elf->e_machine  = crash_architecture(elf_info);
	elf->e_version	= EV_CURRENT;
	elf->e_entry	= 0;
	elf->e_phoff	= sizeof(EHDR);
	elf->e_shoff	= 0;
	elf->e_flags	= 0;
	elf->e_ehsize   = sizeof(EHDR);
	elf->e_phentsize= sizeof(PHDR);
	elf->e_phnum    = 0;
	elf->e_shentsize= 0;
	elf->e_shnum    = 0;
	elf->e_shstrndx = 0;

	/* Default way to get crash notes is by get_crash_notes_per_cpu() */

	get_note_info = elf_info->get_note_info;
	if (!get_note_info)
		get_note_info = get_crash_notes_per_cpu;

	if (xen_present())
		get_note_info = xen_get_note;

	/* PT_NOTE program headers. One per cpu */

	count_cpu = nr_cpus;
	for (i = 0; count_cpu > 0; i++) {
		int ret;

		ret = get_note_info(i, &notes_addr, &notes_len);
		count_cpu--;
		if (ret < 0) /* This cpu is not present. Skip it. */
			continue;

		phdr = (PHDR *) bufp;
		bufp += sizeof(PHDR);
		phdr->p_type	= PT_NOTE;
		phdr->p_flags	= 0;
		phdr->p_offset  = phdr->p_paddr = notes_addr;
		phdr->p_vaddr   = 0;
		phdr->p_filesz	= phdr->p_memsz	= notes_len;
		/* Do we need any alignment of segments? */
		phdr->p_align	= 0;

		/* Increment number of program headers. */
		(elf->e_phnum)++;
		dbgprintf_phdr("Elf header", phdr);
	}

	if (has_vmcoreinfo && !(info->kexec_flags & KEXEC_PRESERVE_CONTEXT)) {
		phdr = (PHDR *) bufp;
		bufp += sizeof(PHDR);
		phdr->p_type	= PT_NOTE;
		phdr->p_flags	= 0;
		phdr->p_offset  = phdr->p_paddr = vmcoreinfo_addr;
		phdr->p_vaddr   = 0;
		phdr->p_filesz	= phdr->p_memsz	= vmcoreinfo_len;
		/* Do we need any alignment of segments? */
		phdr->p_align	= 0;

		(elf->e_phnum)++;
		dbgprintf_phdr("vmcoreinfo header", phdr);
	}

	/* Setup an PT_LOAD type program header for the region where
	 * Kernel is mapped if elf_info->kern_size is non-zero.
	 */

	if (elf_info->kern_size && !xen_present()) {
		phdr = (PHDR *) bufp;
		bufp += sizeof(PHDR);
		phdr->p_type	= PT_LOAD;
		phdr->p_flags	= PF_R|PF_W|PF_X;
		phdr->p_offset	= phdr->p_paddr = elf_info->kern_paddr_start;
		phdr->p_vaddr	= elf_info->kern_vaddr_start;
		phdr->p_filesz	= phdr->p_memsz	= elf_info->kern_size;
		phdr->p_align	= 0;
		(elf->e_phnum)++;
		dbgprintf_phdr("Kernel text Elf header", phdr);
	}

	/* Setup PT_LOAD type program header for every system RAM chunk.
	 * A seprate program header for Backup Region*/
	for (i = 0; i < ranges; i++, range++) {
		unsigned long long mstart, mend;
		if (range->type != RANGE_RAM)
			continue;
		mstart = range->start;
		mend = range->end;
		if (!mstart && !mend)
			continue;
		phdr = (PHDR *) bufp;
		bufp += sizeof(PHDR);
		phdr->p_type	= PT_LOAD;
		phdr->p_flags	= PF_R|PF_W|PF_X;
		phdr->p_offset	= mstart;

		if (mstart == info->backup_src_start
		    && (mend - mstart + 1) == info->backup_src_size)
			phdr->p_offset	= info->backup_start;

		/* We already prepared the header for kernel text. Map
		 * rest of the memory segments to kernel linearly mapped
		 * memory region.
		 */
		phdr->p_paddr = mstart;
		phdr->p_vaddr = phys_to_virt(elf_info, mstart);
		phdr->p_filesz	= phdr->p_memsz	= mend - mstart + 1;
		/* Do we need any alignment of segments? */
		phdr->p_align	= 0;

		/* HIGMEM has a virtual address of -1 */

		if (elf_info->lowmem_limit
		    && (mend > (elf_info->lowmem_limit - 1)))
			phdr->p_vaddr = -1;

		/* Increment number of program headers. */
		(elf->e_phnum)++;
		dbgprintf_phdr("Elf header", phdr);
	}
	return 0;
}

#undef dbgprintf_phdr
