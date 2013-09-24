/* siitool
 *
 * Simple program to print the values of the SII EEPROM content. It reads the
 * raw binaries.
 *
 * Frank Jeschke <fjeschke@synapticon.de>
 */

/*
 *       Copyright (c) 2013, Synapticon GmbH
 *       All rights reserved.
 *
 *       Redistribution and use in source and binary forms, with or without
 *       modification, are permitted provided that the following conditions are met:
 *
 *       1. Redistributions of source code must retain the above copyright notice, this
 *          list of conditions and the following disclaimer.
 *       2. Redistributions in binary form must reproduce the above copyright notice,
 *          this list of conditions and the following disclaimer in the documentation
 *          and/or other materials provided with the distribution.
 *       3. Execution of this software or parts of it exclusively takes place on hardware
 *          produced by Synapticon GmbH.
 *
 *       THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 *       ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 *       WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *       DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 *       ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 *       (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *       LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 *       ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *       (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 *       SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *       The views and conclusions contained in the software and documentation are those
 *       of the authors and should not be interpreted as representing official policies,
 *       either expressed or implied, of the Synapticon GmbH.
 */

#include "sii.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#define VERSION_MAJOR    0
#define VERSION_MINNOR   1

#define BYTES_TO_WORD(x,y)          ((((int)y<<8)&0xff00) | (x&0xff))
#define BYTES_TO_DWORD(a,b,c,d)     ((unsigned int)(d&0xff)<<24)  | \
	                            ((unsigned int)(c&0xff)<<16) | \
				    ((unsigned int)(b&0xff)<<8)  | \
				     (unsigned int)(a&0xff)

//static char **strings; /* all strings */
static int g_print_offsets = 0;

static const char *base(const char *prog)
{
	const char *p = prog;

	while (*p != '\0') {
		if (*p == '/')
			prog = ++p;
		else
			p++;
	}

	return prog;
}

static void printhelp(const char *prog)
{
	printf("Simple program to dump SII content in human readable form\n");
	printf("Usage: %s [-h] [-v] [filename]\n", prog);
	printf("  -h         print this help and exit\n");
	printf("  -v         print version an exit\n");
	printf("  -o         add offset information to section start\n");
	printf("  filename   path to eeprom file, if missing read from stdin\n");
}

static int read_eeprom(FILE *f, unsigned char *buffer, size_t size)
{
	size_t count = 0;
	int input;

	while ((input=fgetc(f)) != EOF)
		buffer[count++] = (unsigned char)(input&0xff);

	return count;
}


int main(int argc, char *argv[])
{
	FILE *f;
	unsigned char eeprom[1024];
	const char *filename = NULL;
	//int input = 0;
	int i;

	for (i=1; i<argc; i++) {
		switch (argv[i][0]) {
		case '-':
			if (argv[i][1] == 'h') {
				printhelp(base(argv[0]));
				return 0;
			} else if (argv[i][1] == 'v') {
				printf("Version %d.%d\n", VERSION_MAJOR, VERSION_MINNOR);
				return 0;
			} else if (argv[i][1] == 'o') {
				g_print_offsets = 1;
			} else {
				fprintf(stderr, "Invalid argument\n");
				printhelp(base(argv[0]));
				return 0;
			}

		default:
			/* asuming file name */
			filename = argv[i];
			break;
		}
	}

	if (filename == NULL)
		read_eeprom(stdin, eeprom, 1024);
	else {
		f = fopen(filename, "r");
		if (f == NULL) {
			perror("Error open input file");
			return -1;
		}

		printf("Start reading contents of file\n");

		read_eeprom(f, eeprom, 1024);
		fclose(f);
	}

	//return parse_and_print_content(eeprom, 1024);
	SiiInfo *sii = sii_init_string(eeprom, 1024);
	//alternative: SiiInfo *sii = sii_init_file(filename) */

	sii_print(sii);
	sii_release(sii);

	return 0;
}
