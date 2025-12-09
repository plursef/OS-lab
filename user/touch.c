// touch.c
#include <inc/lib.h>

static void
usage(void)
{
	printf("usage: touch [file...]\n");
	exit();
}

void
umain(int argc, char **argv)
{
	int i, r;

	if (argc < 2)
		usage();

	for (i = 1; i < argc; i++) {
		if ((r = open(argv[i], O_CREAT | O_RDWR)) < 0) {
			printf("touch: cannot create file '%s': %e\n", argv[i], r);
		}
		else {
			close(r);
		}
	}
}
