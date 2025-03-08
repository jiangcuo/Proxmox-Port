#ifndef HVCALL_H
#define HVCALL_H

#define H_PUT_TERM_CHAR	0x58

long plpar_hcall_norets(unsigned long opcode, ...);

#endif
