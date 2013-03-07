/* siicode
 * 
 * 2013-02-27 jeschke@fjes.de
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#define BYTES_TO_WORD(x,y)          ((((int)y<<8)&0xff00) | (x&0xff))
#define BYTES_TO_DWORD(a,b,c,d)     ((unsigned int)(d&0xff)<<24)  | \
	                            ((unsigned int)(c&0xff)<<16) | \
				    ((unsigned int)(b&0xff)<<8)  | \
				     (unsigned int)(a&0xff)

enum eSection {
	SII_CAT_NOP
	,SII_PREAMBLE
	,SII_STD_CONFIG
	,SII_CAT_STRINGS = 10
	,SII_CAT_DATATYPES = 20 /* future use */
	,SII_CAT_GENERAL = 30
	,SII_CAT_FMMU = 40
	,SII_CAT_SYNCM = 41
	,SII_CAT_TXPDO = 50
	,SII_CAT_RXPDO = 51
	,SII_CAT_DCLOCK = 60/* future use */
	,SII_END = 0xffff
};

int read_eeprom(FILE *f, unsigned char *buffer, size_t size)
{
	size_t count = 0;
	int input;

	while ((input=getchar()) != EOF) {
		printf("%.2x ", (unsigned char)(input&0xff));

		buffer[count++] = (unsigned char)(input&0xff);
	}

	return count;
}

void print_preamble(unsigned char *buffer, size_t size)
{
	size_t count = 0;

	/* pdi control */
	int pdi_ctrl = ((int)buffer[1]<<8) | buffer[0];
	printf("PDI Control: %.4x\n", pdi_ctrl);

	/* pdi config */
	int pdi_conf = ((int)buffer[3]<<8) | buffer[2];
	printf("PDI config: %.4x\n", pdi_conf);

	count = 4;

	/* sync impulse len (multiples of 10ns) */
	int sync_imp = BYTES_TO_WORD(buffer[count++], buffer[count++]);
	printf("Sync Impulse length = %d ns (raw: %.4x)\n", sync_imp*10, sync_imp);

	/* PDI config 2 */
	int pdi_conf2 = BYTES_TO_WORD(buffer[count++], buffer[count++]);
	printf("PDI config 2: %.4x\n", pdi_conf2);

	/* configured station alias */
	printf("Configured station alias: %.4x\n", BYTES_TO_WORD(buffer[count++], buffer[count++]));

	count+=4; /* next 4 bytes are reserved */

	/* checksum FIXME add checksum test */
	printf("DEBUG: buffer[%u] buffer[%u] = %2x %2x\n", (unsigned int)count, (unsigned int)count+1, buffer[count+0], buffer[count+1]);
	/* FIXME wrong bytes are displayed! PANIC */
	int checksum = BYTES_TO_WORD(buffer[count++], buffer[count++]);
	printf("Checksum of preamble: %.4x\n", checksum);
	printf("DEBUG: buffer[%u] buffer[%u] = %2x %2x\n", (unsigned int)count-2, (unsigned int)count-1, buffer[count-2], buffer[count-1]);
}


#define MBOX_EOE    0x0002
#define MBOX_COE    0x0004
#define MBOX_FOE    0x0008
#define MBOX_SOE    0x0010
#define MBOX_VOE    0x0020

static void print_stdconfig(unsigned char *buffer, size_t size)
{
	size_t count =0;
	unsigned char *b = buffer;
	printf("Standard config:\n");

	printf("Vendor ID: %08x\n", BYTES_TO_DWORD(*(b+0), *(b+1), *(b+2), *(b+3)));
	b+=4;
	printf("Product ID: %08x\n", BYTES_TO_DWORD(*(b+0), *(b+1), *(b+2), *(b+3)));
	b+=4;
	printf("Revision ID: %08x\n", BYTES_TO_DWORD(*(b+0), *(b+1), *(b+2), *(b+3)));
	b+=4;
	printf("Serial Number: %08x\n", BYTES_TO_DWORD(*(b+0), *(b+1), *(b+2), *(b+3)));
	b+=4;

	b+=8; /* another reserved 8 bytes */

	/* mailbox settings */
	printf("Bootstrap received mailbox offset: %04x\n", BYTES_TO_WORD(*(b+0), *(b+1)));
	b+=2;
	printf("Bootstrap received mailbox size: %04x\n", BYTES_TO_WORD(*(b+0), *(b+1)));
	b+=2;
	printf("Bootstrap send mailbox offset: %04x\n", BYTES_TO_WORD(*(b+0), *(b+1)));
	b+=2;
	printf("Bootstrap send mailbox size: %04x\n", BYTES_TO_WORD(*(b+0), *(b+1)));
	b+=2;

	printf("Standard received mailbox offset: %04x\n", BYTES_TO_WORD(*(b+0), *(b+1)));
	b+=2;
	printf("Standard received mailbox size: %04x\n", BYTES_TO_WORD(*(b+0), *(b+1)));
	b+=2;
	printf("Standard send mailbox offset: %04x\n", BYTES_TO_WORD(*(b+0), *(b+1)));
	b+=2;
	printf("Standard send mailbox size: %04x\n", BYTES_TO_WORD(*(b+0), *(b+1)));
	b+=2;

	uint16_t recmbox = BYTES_TO_WORD(*(b+0), *(b+1));
	b+=2;
	printf("Supported Mailboxes: ");
	if (recmbox&MBOX_EOE)
		printf("EoE, ");
	if (recmbox&MBOX_COE)
		printf("CoE, ");
	if (recmbox&MBOX_FOE)
		printf("FoE, ");
	if (recmbox&MBOX_SOE)
		printf("SoE, ");
	if (recmbox&MBOX_VOE)
		printf("VoE, ");
	printf("\n");

	b+=66; /* more reserved bytes */

	printf("EEPROM size: %d kbit\n", BYTES_TO_WORD(*(b+0), *(b+1)));
	b+=2;
	printf("Version: %d\n",  BYTES_TO_WORD(*(b+0), *(b+1)));
}

static void print_stringsection(const unsigned char *buffer, size_t secsize)
{
	printf("+++ Ignoring String section\n");
}

static void print_syncm_section(const unsigned char *buffer, size_t secsize)
{
	size_t count;
	const unsigned char *b = buffer;
	printf("+++ syncmanager printout not yet implemented\n");
}

static enum eSection get_next_section(const unsigned char *b, size_t len, size_t *secsize)
{
	enum eSection next = SII_CAT_NOP;

	next = BYTES_TO_WORD(*b, *(b+1)) & 0xffff; /* attention, on some section bit 7 is vendor specific indicator */
	unsigned wordsize = BYTES_TO_WORD(*(b+2), *(b+3));
	*secsize = (wordsize<<1); /* bytesize = 2*wordsize */

	return next;
}

int main(int argc, char *argv[])
{
	printf("Hello World\n");
	/* FIXME if '-f' command line argument is available no read from stdin */

	enum eSection section = SII_PREAMBLE;
	size_t count = 0;
	unsigned char eeprom[1024];
	unsigned char *buffer;
	size_t secsize = 0;

	int input = 0;

	read_eeprom(stdin, eeprom, 1024);
	buffer = eeprom;

	while (1) {
		switch (section) {
		case SII_CAT_NOP:
			break;

		case SII_PREAMBLE:
			printf("Print Preamble:\n");
			print_preamble(buffer, 16);
			buffer = eeprom+16;
			section = SII_STD_CONFIG;
			break;

		case SII_STD_CONFIG:
			printf("Print std config:\n");
			print_stdconfig(buffer, 46+66);
			buffer = buffer+46+66;
			section = get_next_section(buffer, 4, &secsize);
			buffer += 4;
			printf("+++ next section: %x\n", section);
			break;

		case SII_CAT_STRINGS:
			print_stringsection(buffer, secsize);
			buffer+=secsize;
			section = get_next_section(buffer, 4, &secsize);
			buffer+=4;
			printf("+++ next section: %x\n", section);
			break;

		case SII_CAT_DATATYPES:
			//print_syncmsection(buffer, secsize);
			printf("+++ datatypes not yet implemented\n");
			buffer+=secsize;
			section = get_next_section(buffer, 4, &secsize);
			buffer+=4;
			printf("+++ next section: %x\n", section);
			break;

		case SII_CAT_GENERAL:
			//print_syncmsection(buffer, secsize);
			printf("+++ general not yet implemented\n");
			buffer+=secsize;
			section = get_next_section(buffer, 4, &secsize);
			buffer+=4;
			printf("+++ next section: %x\n", section);
			break;

		case SII_CAT_FMMU:
			//print_fmmu_section(buffer, secsize);
			printf("+++ fmmu not yet implemented\n");
			buffer+=secsize;
			section = get_next_section(buffer, 4, &secsize);
			buffer+=4;
			printf("+++ next section: %x\n", section);
			break;

		case SII_CAT_SYNCM:
			print_syncm_section(buffer, secsize);
			buffer+=secsize;
			section = get_next_section(buffer, 4, &secsize);
			buffer+=4;
			printf("+++ next section: %x\n", section);
			break;

		case SII_CAT_TXPDO:
			//print_txpdo_section(buffer, secsize);
			printf("+++ txpdo not yet implemented\n");
			buffer+=secsize;
			section = get_next_section(buffer, 4, &secsize);
			buffer+=4;
			printf("+++ next section: %x\n", section);
			break;

		case SII_CAT_RXPDO:
			//print_rxpdo_section(buffer, secsize);
			printf("+++ rxpdo not yet implemented\n");
			buffer+=secsize;
			section = get_next_section(buffer, 4, &secsize);
			buffer+=4;
			printf("+++ next section: %x\n", section);
			break;

		case SII_CAT_DCLOCK:
			//print_dclock_section(buffer, secsize);
			printf("+++ dclock not yet implemented\n");
			buffer+=secsize;
			section = get_next_section(buffer, 4, &secsize);
			buffer+=4;
			printf("+++ next section: %x\n", section);
			break;

		case SII_END:
			printf("finished\n");
			goto finish;
			break;
		}
	}

finish:
	return 0;
}
