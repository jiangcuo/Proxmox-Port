/*
 * ifdown.c Find all network interfaces on the system and
 *      shut them down.
 *
 */
char *v_ifdown = "@(#)ifdown.c  1.11  02-Jun-1998  miquels@cistron.nl";

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <errno.h>

#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/time.h>

#include <net/if.h>
#include <netinet/in.h>

/*
 *  First, we find all shaper devices and down them. Then we
 *  down all real interfaces. This is because the comment in the
 *  shaper driver says "if you down the shaper device before the
 *  attached inerface your computer will follow".
 */
int ifdown(void)
{
	struct if_nameindex *ifa, *ifp;
	struct ifreq ifr;
	int fd, shaper;

	if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		fprintf(stderr, "ifdown: ");
		perror("socket");
		goto error;
	}

	if ((ifa = if_nameindex()) == NULL) {
		fprintf(stderr, "ifdown: ");
		perror("if_nameindex");
		goto error;
	}

	for (shaper = 1; shaper >= 0; shaper--) {
		for (ifp = ifa; ifp->if_index; ifp++) {

			if ((strncmp(ifp->if_name, "shaper", 6) == 0)
			    != shaper) continue;
			if (strcmp(ifp->if_name, "lo") == 0)
				continue;
			if (strchr(ifp->if_name, ':') != NULL)
				continue;

			strncpy(ifr.ifr_name, ifp->if_name, IFNAMSIZ-1);
			ifr.ifr_name[IFNAMSIZ-1] = 0;
			if (ioctl(fd, SIOCGIFFLAGS, &ifr) < 0) {
				fprintf(stderr, "ifdown: shutdown ");
				perror(ifp->if_name);
				goto error;
			}
			ifr.ifr_flags &= ~(IFF_UP);
			if (ioctl(fd, SIOCSIFFLAGS, &ifr) < 0) {
				fprintf(stderr, "ifdown: shutdown ");
				perror(ifp->if_name);
				goto error;
			}

		}
	}

	close(fd);
	return 0;

error:
	close(fd);
	return -1;
}
