#
# vmcore-dmesg (reading demsg from vmcore)
#

VMCORE_DMESG_SRCS:= vmcore-dmesg/vmcore-dmesg.c

VMCORE_DMESG_OBJS = $(call objify, $(VMCORE_DMESG_SRCS))
VMCORE_DMESG_DEPS = $(call depify, $(VMCORE_DMESG_OBJS))

VMCORE_DMESG = $(SBINDIR)/vmcore-dmesg
VMCORE_DMESG_MANPAGE = $(MANDIR)/man8/vmcore-dmesg.8

dist += vmcore-dmesg/Makefile $(VMCORE_DMESG_SRCS) vmcore-dmesg/vmcore-dmesg.8
clean += $(VMCORE_DMESG_OBJS) $(VMCORE_DMESG_DEPS) $(VMCORE_DMESG) $(VMCORE_DMESG_MANPAGE)

-include $(VMCORE_DMESG_DEPS)

$(VMCORE_DMESG): $(VMCORE_DMESG_OBJS) $(UTIL_LIB)
	@$(MKDIR) -p $(@D)
	$(LINK.o) -o $@ $^ $(CFLAGS) $(LIBS)

$(VMCORE_DMESG_MANPAGE): vmcore-dmesg/vmcore-dmesg.8
	$(MKDIR) -p     $(MANDIR)/man8
	cp $^ $(VMCORE_DMESG_MANPAGE)
echo::
	@echo "VMCORE_DMESG_SRCS $(VMCORE_DMESG_SRCS)"
	@echo "VMCORE_DMESG_DEPS $(VMCORE_DMESG_DEPS)"
	@echo "VMCORE_DMESG_OBJS $(VMCORE_DMESG_OBJS)"

