#include <purgatory.h>
#include "io.h"

#define VGABASE         UNCACHED(0xb8000)

/* code based on i386 console code
 * TODO add serial support
 */
#define MAX_YPOS        25
#define MAX_XPOS        80

unsigned long current_ypos = 1, current_xpos = 0;

static void putchar_vga(int ch)
{
	int  i, k, j;

	if (current_ypos >= MAX_YPOS) {
		/* scroll 1 line up */
		for (k = 1, j = 0; k < MAX_YPOS; k++, j++) {
			for (i = 0; i < MAX_XPOS; i++) {
				writew(readw(VGABASE + 2*(MAX_XPOS*k + i)),
						VGABASE + 2*(MAX_XPOS*j + i));
			}
		}
		for (i = 0; i < MAX_XPOS; i++)
			writew(0x720, VGABASE + 2*(MAX_XPOS*j + i));
		current_ypos = MAX_YPOS-1;
	}
	if (ch == '\n') {
		current_xpos = 0;
		current_ypos++;
	} else if (ch != '\r')  {
		writew(((0x7 << 8) | (unsigned short) ch),
				VGABASE + 2*(MAX_XPOS*current_ypos +
					current_xpos++));
		if (current_xpos >= MAX_XPOS) {
			current_xpos = 0;
			current_ypos++;
		}
	}
}

void putchar(int ch)
{
	putchar_vga(ch);
}
