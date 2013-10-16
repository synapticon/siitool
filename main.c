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
#include "esi.h"
#include "esifile.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#define VERSION_MAJOR    0
#define VERSION_MINNOR   1

#define MAX_BUFFER_SIZE    (1000*1024)
#define MAX_FILENAME_SIZE  (256)


//static int g_print_offsets = 0;

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
	printf("Usage: %s [-h] [-v] [filename]\n", prog);
	printf("  -h         print this help and exit\n");
	printf("  -v         print version an exit\n");
	printf("  -o <name>  write output to file <name>\n");
	printf("  filename   path to eeprom file, if missing read from stdin\n");
	printf("\nRecognized file types: SII and ESI/XML.\n");
}

/* FIXME the read_eeprom is misleading since the data are read from a every input stream */
static int read_eeprom(FILE *f, unsigned char *buffer, size_t size)
{
	size_t count = 0;
	int input;

	while ((input=fgetc(f)) != EOF)
		buffer[count++] = (unsigned char)(input&0xff);

	if (count>size)
		fprintf(stderr, "Error: read size is larger than expected (bytes read: %d)\n", count);

	return count;
}


int main(int argc, char *argv[])
{
	FILE *f;
	unsigned char eeprom[MAX_BUFFER_SIZE];
	const char *filename = NULL;
	char output[MAX_FILENAME_SIZE];

	for (int i=1; i<argc; i++) {
		switch (argv[i][0]) {
		case '-':
			if (argv[i][1] == 'h') {
				printhelp(base(argv[0]));
				return 0;
			} else if (argv[i][1] == 'v') {
				printf("Version %d.%d\n", VERSION_MAJOR, VERSION_MINNOR);
				return 0;
			} else if (argv[i][1] == 'o') {
				i++;
				strncpy(output, argv[i], strlen(argv[i]));
				printf("[DEBUG output file is %s\n", output);
			} else if (argv[i][1] == '-') {
				filename = NULL;
			} else {
				fprintf(stderr, "Invalid argument\n");
				printhelp(base(argv[0]));
				return 0;
			}
			break;

		default:
			/* asuming file name */
			filename = argv[i];
			break;
		}
	}

	/* test compatibility between compiled and used library version */

	if (filename == NULL)
		read_eeprom(stdin, eeprom, MAX_BUFFER_SIZE);
	else {
		f = fopen(filename, "r");
		if (f == NULL) {
			perror("Error open input file");
			return -1;
		}

		printf("Start reading contents of file %s\n", filename);

		read_eeprom(f, eeprom, MAX_BUFFER_SIZE);
		fclose(f);
	}

	EsiData *esi = esi_init_string(eeprom, MAX_BUFFER_SIZE);
	//esi_print_xml(esi);
	if (esi_parse(esi))
		fprintf(stderr, "Error something went wrong in XML parsing\n");

	esi_print_sii(esi);
	esi_release(esi);

	return 0;

	//return parse_and_print_content(eeprom, 1024);
	SiiInfo *sii = sii_init_string(eeprom, 1024);
	//alternative: SiiInfo *sii = sii_init_file(filename) */

	sii_print(sii);
	sii_generate(sii);
	int ret = sii_write_bin(sii, "siiout.bin");
	if (ret < 0)
		fprintf(stderr, "Error, couldn't write output file\n");

	sii_release(sii);

	return 0;
}
