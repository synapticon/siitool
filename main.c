/* siicode
 * 
 * 2013-02-27 jeschke@fjes.de
 */

#include <stdio.h>
#include <stdint.h>

int main(int argc, char *argv[])
{
	printf("Hello World\n");
	/* FIXME if '-f' command line argument is available no read from stdin */

	int input = 0;
	while ((input=getchar()) != EOF) {
		printf("%c", (char)(input&0xff));
	}

	return 0;
}
