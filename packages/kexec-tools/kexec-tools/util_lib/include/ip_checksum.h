#ifndef IP_CHECKSUM_H
#define IP_CHECKSUM_H

unsigned long compute_ip_checksum(void *addr, unsigned long length);
unsigned long add_ip_checksums(unsigned long offset, unsigned long sum, unsigned long new);
unsigned long negate_ip_checksum(unsigned long sum);

#endif /* IP_CHECKSUM_H */
