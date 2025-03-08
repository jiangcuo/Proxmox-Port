#include <stdio.h>

int main(int argc, char **argv)
{
	int c;
	int i;
	const char *name = argv[1];
	printf("#include <stddef.h>\n");
	printf("const char %s[] = {\n", name);
	i = 0;
	while((c = getchar()) != EOF) {
		if ((i % 16) != 0) {
			putchar(' ');
		}
		printf("0x%02x,", c);
		i++;
		if ((i %16) == 0) {
			putchar('\n');
		}
	}
	putchar('\n');
	printf("};\n");
	printf("size_t %s_size = sizeof(%s);\n", name, name);
	return 0;
}
