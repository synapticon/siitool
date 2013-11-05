#include "crc8.h"

/* Polynom: x^8 + x^2 + x + 1 = 100000111 = 0x107 */
#define POLYNOM   0x07
#define MSB        0x80  /* this is byte specific */

uint8_t crc8byte(uint8_t *crc, uint8_t msg)
{
	uint8_t rem = *crc ^ msg;

	for (uint8_t b=0; b<8; b++) {
		if (rem&MSB)
			rem = (rem<<1) ^ (uint8_t)POLYNOM;
		else
			rem = (rem<<1);
	}

	*crc = rem;

	return rem;
}

uint8_t crc8(const uint8_t *msg, size_t blen)
{
	uint8_t rem = 0xff;
	uint8_t rpoly = POLYNOM;
	int i,b;

	for (i=0; i<blen; i++) {
		rem ^= msg[i];

		for (b=0; b<8; b++) {
			if (rem&MSB)
				rem = (rem<<1) ^ rpoly;
			else
				rem = (rem<<1);
		}
	}

	return rem;
}

#if 0 /* test */
#include <stdio.h>

int main(int argc, char *argv[])
{
	uint8_t crc = 0xff;

	uint8_t input[] = { 0x08, 0x0e, 0x02, 0x88, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x2f, 0x00 };
	crc = crc8(input, sizeof(input)/sizeof(*input));

	printf("polynom: %.2x\n", POLYNOM);
	printf("Message (size %d): 0x", sizeof(input)/sizeof(*input));
	for (int i=0; i<sizeof(input)/sizeof(*input); i++)
		printf("%.2x", input[i]);
	printf("\n");
	printf("CRC: %x\n\n", crc);

	uint8_t input1[] = { 0x08, 0x0e, 0x02, 0x88, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
	crc = crc8(input1, sizeof(input1)/sizeof(*input1));

	printf("polynom: %.2x\n", POLYNOM);
	printf("Message (size %d): 0x", sizeof(input1)/sizeof(*input1));
	for (int i=0; i<sizeof(input1)/sizeof(*input1); i++)
		printf("%.2x", input1[i]);
	printf("\n");
	printf("CRC: %x\n\n", crc);

	printf("Now testing crc8byte()\n");

	uint8_t crcb = 0xff;
	for (int i=0; i<(sizeof(input)/sizeof(*input)); i++) {
		printf("%.2x", input[i]);
		crcb = crc8byte(&crcb, input[i]);
	}

	printf("CRC = 0x%.2x\n", crc);
	return 0;
}
#endif
