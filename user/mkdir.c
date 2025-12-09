// mkdir.c
#include <inc/lib.h>

static void
usage(void)
{
	printf("usage: mkdir [dir...]\n");
	exit();
}

void
umain(int argc, char **argv)
{
	int i, r;

	if (argc < 2)
		usage();

	for (i = 1; i < argc; i++) {
		if ((r = mkdir(argv[i])) < 0) {
			printf("mkdir: cannot create directory '%s': %e\n", argv[i], r);
		}
	}
}
