/* sii.c
 *
 * 2013 Frank Jeschke <fjeschke@synapticon.de>
 */

#include "sii.h"
#include "crc8.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#define SKIP_DC       0x0001
#define SKIP_TXPDO    0x0010
#define SKIP_RXPDO    0x0020

/* category functions */
static struct _sii_cat *cat_new(uint16_t type, uint16_t size);
static void cat_data_cleanup(struct _sii_cat *cat);
static int cat_add(SiiInfo *sii, struct _sii_cat *new);
static int cat_rm(SiiInfo *sii);
static struct _sii_cat * cat_next(SiiInfo *sii);
static void cat_rewind(SiiInfo *sii);

static void cat_print(struct _sii_cat *cats);
static void cat_print_strings(struct _sii_cat *cat);
static void cat_print_datatypes(struct _sii_cat *cat);
static void cat_print_general(struct _sii_cat *cat);
static void cat_print_fmmu(struct _sii_cat *cat);
static void cat_print_syncm(struct _sii_cat *cat);
static void cat_print_rxpdo(struct _sii_cat *cat);
static void cat_print_txpdo(struct _sii_cat *cat);
static void cat_print_pdo(struct _sii_cat *cat);
static void cat_print_dc(struct _sii_cat *cat);

static uint16_t sii_cat_write_strings(struct _sii_cat *cat, unsigned char *buf);
static uint16_t sii_cat_write_datatypes(struct _sii_cat *cat, unsigned char *buf);
static uint16_t sii_cat_write_general(struct _sii_cat *cat, unsigned char *buf);
static uint16_t sii_cat_write_fmmu(struct _sii_cat *cat, unsigned char *buf);
static uint16_t sii_cat_write_syncm(struct _sii_cat *cat, unsigned char *buf);
//static void sii_cat_write_rxpdo(struct _sii_cat *cat);
//static void sii_cat_write_txpdo(struct _sii_cat *cat);
static uint16_t sii_cat_write_pdo(struct _sii_cat *cat, unsigned char *buf);
static uint16_t sii_cat_write_dc(struct _sii_cat *cat, unsigned char *buf);
#if DEBUG == 1
static uint16_t sii_cat_write_cat(struct _sii_cat *cat, unsigned char *buf);
#endif
static size_t sii_cat_write(struct _sii *sii, uint16_t skip_mask);
static void sii_write(SiiInfo *sii, unsigned int add_pdo_mapping);

static int read_eeprom(FILE *f, unsigned char *buffer, size_t size)
{
	size_t count = 0;
	int input;

	while ((input=fgetc(f)) != EOF) {
		if (count >= size) {
			fprintf(stderr, "Error, buffer too small\n");
			return -1;
		}
		buffer[count++] = (unsigned char)(input&0xff);
	}

	return count;
}

static struct _sii_preamble * parse_preamble(const unsigned char *buffer, size_t size)
{
	struct _sii_preamble *preamble = calloc(1, sizeof(struct _sii_preamble));

	size_t count = 0;
	uint8_t crc = 0xff; /* init value for crc */

	/* pdi control */
	preamble->pdi_ctrl = ((int)(buffer[1]&0xff)<<8) | buffer[0];
	crc8byte(&crc, buffer[0]);
	crc8byte(&crc, buffer[1]);

	/* pdi config */
	preamble->pdi_conf = ((int)(buffer[3]&0xff)<<8) | buffer[2];
	crc8byte(&crc, buffer[2]);
	crc8byte(&crc, buffer[3]);

	count = 4;

	/* sync impulse len (multiples of 10ns), stored as nano seconds */
	preamble->sync_impulse = BYTES_TO_WORD(buffer[count], buffer[count+1]);
	crc8byte(&crc, buffer[count]);
	crc8byte(&crc, buffer[count+1]);
	count+=2;

	/* PDI config 2 */
	preamble->pdi_conf2 = BYTES_TO_WORD(buffer[count], buffer[count+1]);
	crc8byte(&crc, buffer[count]);
	crc8byte(&crc, buffer[count+1]);
	count+=2;

	/* configured station alias */
	preamble->alias = BYTES_TO_WORD(buffer[count], buffer[count+1]);
	crc8byte(&crc, buffer[count]);
	crc8byte(&crc, buffer[count+1]);
	count+=2;

	crc8byte(&crc, buffer[count]);
	crc8byte(&crc, buffer[count+1]);
	crc8byte(&crc, buffer[count+2]);
	crc8byte(&crc, buffer[count+3]);
	count+=4; /* next 4 bytes are reserved */

	/* checksum test */
	preamble->checksum = BYTES_TO_WORD(buffer[count], buffer[count+1]);
	crc8byte(&crc, buffer[count]);
	crc8byte(&crc, buffer[count+1]);
	count += 2;

	if (size != count)
		printf("%s: Warning counter differs from size\n", __func__);

	preamble->checksum_ok = 1;
	if (crc != 0) {
		preamble->checksum_ok = 0;
		fprintf(stderr, "Error, checksum is not correct!\n");
	}

	return preamble;
}

static struct _sii_stdconfig *parse_stdconfig(const unsigned char *buffer, size_t size)
{
	size_t count =0;
	const unsigned char *b = buffer;

	struct _sii_stdconfig *stdc = calloc(1, sizeof(struct _sii_stdconfig));

	stdc->vendor_id = BYTES_TO_DWORD(*(b+0), *(b+1), *(b+2), *(b+3));
	b+=4;
	stdc->product_id = BYTES_TO_DWORD(*(b+0), *(b+1), *(b+2), *(b+3));
	b+=4;
	stdc->revision_id = BYTES_TO_DWORD(*(b+0), *(b+1), *(b+2), *(b+3));
	b+=4;
	stdc->serial = BYTES_TO_DWORD(*(b+0), *(b+1), *(b+2), *(b+3));
	b+=4;
	b+=8; /* another reserved 8 bytes */

	stdc->bs_rec_mbox_offset = BYTES_TO_WORD(*(b+0), *(b+1));
	b+=2;
	stdc->bs_rec_mbox_size =  BYTES_TO_WORD(*(b+0), *(b+1));
	b+=2;
	stdc->bs_snd_mbox_offset = BYTES_TO_WORD(*(b+0), *(b+1));
	b+=2;
	stdc->bs_snd_mbox_size = BYTES_TO_WORD(*(b+0), *(b+1));
	b+=2;

	stdc->std_rec_mbox_offset = BYTES_TO_WORD(*(b+0), *(b+1));
	b+=2;
	stdc->std_rec_mbox_size = BYTES_TO_WORD(*(b+0), *(b+1));
	b+=2;
	stdc->std_snd_mbox_offset = BYTES_TO_WORD(*(b+0), *(b+1));
	b+=2;
	stdc->std_snd_mbox_size = BYTES_TO_WORD(*(b+0), *(b+1));
	b+=2;

	stdc->mailbox_protocol.word = BYTES_TO_WORD(*(b+0), *(b+1));
	b+=2;

	b+=66; /* more reserved bytes */

	stdc->eeprom_size = BYTES_TO_WORD(*(b+0), *(b+1));
	b+=2;
	stdc->version =  BYTES_TO_WORD(*(b+0), *(b+1));
	b+=2;

	count = b-buffer;
	if (size != count)
		printf("%s: Warning counter differs from size\n", __func__);

	return stdc;
}

static struct _string *string_new(const char *string, size_t size)
{
	struct _string *new = calloc(1, sizeof(struct _string));

	new->length = size;
	new->data = malloc(size+1);
	memmove(new->data, string, size);
	new->data[size] = 0;

	return new;
}

static void strings_entry_add(struct _sii_strings *str, struct _string *new)
{
	if (str->head == NULL) { /* first entry */
		str->head = new;
		new->id = 1;
	} else {
		struct _string *s = str->head;
		while (s->next)
			s = s->next;

		s->next = new;
		new->prev = s;
		new->id = s->id+1;
	}

	str->count += 1;
	str->size += new->length;
}

static struct _sii_strings *parse_string_section(const unsigned char *buffer, size_t size)
{
	const unsigned char *pos = buffer;
	unsigned index = 0;
	char str[1024];
	size_t len = 0;
	memset(str, '\0', 1024);

	struct _sii_strings *strings = (struct _sii_strings *)calloc(1, sizeof(struct _sii_strings));

	int stringcount = *pos++;
	int counter = 0;

	for (index=0; counter < stringcount; index++) {
		len = *pos++;
		memmove(str, pos, len);
		pos += len;

		strings_entry_add(strings, string_new(str, len));
		counter++;
	}

	if ((size_t)(pos-buffer) > size)
		printf("%s: Warning counter differs from size\n", __func__);

	return strings;
}

static void parse_datatype_section(const unsigned char *buffer, size_t size)
{
	printf("\n+++ parsing of datatype section not yet implemented (first byte: 0x%.2x, size: %zu)\n",
			*buffer, size);
}

static char *physport_type(uint8_t b)
{
	switch (b) {
	case 0x00:
		return "not used";
	case 0x01:
		return "MII";
	case 0x02:
		return "reserved";
	case 0x03:
		return "EBUS";
	default:
		return "unknown";
	}

	return NULL;
}

static struct _sii_general *parse_general_section(const unsigned char *buffer, size_t size)
{
	const unsigned char *b = buffer;
	struct _sii_general *siig = calloc(1, sizeof(struct _sii_general));

	siig->groupindex = *b;
	b++;
	siig->imageindex = *b;
	b++;
	siig->orderindex = *b;
	b++;
	siig->nameindex = *b;
	b++;
	b++;

	siig->coe_enable_sdo = (*b&0x01) == 0 ? 0 : 1;
	siig->coe_enable_sdo_info = (*b&0x02) == 0 ? 0 : 1;
	siig->coe_enable_pdo_assign = (*b&0x04) == 0 ? 0 : 1;
	siig->coe_enable_pdo_conf = (*b&0x08) == 0 ? 0 : 1;
	siig->coe_enable_upload_start = (*b&0x10) == 0 ? 0 : 1;
	siig->coe_enable_sdo_complete = (*b&0x20) == 0 ? 0 : 1;
	b++;

	siig->foe_enabled = (*b & 0x01) == 0 ? 0 : 1;
	b++;
	siig->eoe_enabled = (*b & 0x01) == 0 ? 0 : 1;
	b++;
	b+=3; /* SoE Channels, DS402 Channels, Sysman Class - reserved */

	siig->flag_safe_op           = (*b & 0x01) == 0 ? 0 : 1;
	siig->flag_notLRW            = (*b & 0x02) == 0 ? 0 : 1;
	siig->flag_MBoxDataLinkLayer = (*b & 0x04) == 0 ? 0 : 1;
	siig->flag_IdentALSts        = (*b & 0x08) == 0 ? 0 : 1;
	siig->flag_IdentPhyM         = (*b & 0x10) == 0 ? 0 : 1;
	b++;

	siig->current_ebus = BYTES_TO_WORD(*b, *(b+1));
	b+=2;
	b+=2;

	siig->phys_port_0 = *b&0x0f;
	siig->phys_port_1 = (*b>>4)&0xf;
	b++;
	siig->phys_port_2 = *b&0xf;
	siig->phys_port_3 = (*b>>4)&0xf;
	b++;

	siig->physical_address = BYTES_TO_WORD(*b, *(b+1));
	b+=2;

	b+=12; /* reserved */

	size_t count = b-buffer;
	if (size != count)
		printf("%s: Warning counter differs from size\n", __func__);

	return siig;
}

static void fmmu_rm_entry(struct _sii_fmmu *fmmu)
{
	struct _fmmu_entry *fe = fmmu->list;
	while (fe->next != NULL)
		fe = fe->next;

	if (fe->prev != NULL)
		fe->prev->next = NULL;
	else
		fmmu->list = NULL;

	free(fe);
}

void fmmu_add_entry(struct _sii_fmmu *fmmu, int usage)
{
	struct _fmmu_entry *new = calloc(1, sizeof(struct _fmmu_entry));
	new->id = -1;
	new->usage = usage;
	if (usage == FMMU_UNUSED) { /* skip unused entries */
		free(new);
		return;
	}

	if (fmmu->list == NULL) {
		new->id = 0;
		fmmu->list = new;
		fmmu->count = 1;
	} else {
		struct _fmmu_entry *f = fmmu->list;
		while (f->next != NULL)
			f = f->next;

		f->next = new;
		new->prev = f;
		new->id = f->id+1;
		fmmu->count += 1;
	}
}

static struct _sii_fmmu *parse_fmmu_section(const unsigned char *buffer, size_t secsize)
{
	const unsigned char *b = buffer;

	struct _sii_fmmu *fmmu = calloc(1, sizeof(struct _sii_fmmu));

	while ((unsigned int)(b-buffer)<secsize) {
		fmmu_add_entry(fmmu, *b);
		b++;
	}

	return fmmu;
}

static void syncm_rm_entry(struct _sii_syncm *sm)
{
	struct _syncm_entry *entry = sm->list;

	if (entry == NULL)
		return;

	while (entry->next != NULL)
		entry = entry->next;

	if (entry->prev != NULL)
		entry->prev->next = NULL;
	else
		sm->list = NULL;

	free(entry);
}

void syncm_entry_add(struct _sii_syncm *sm, struct _syncm_entry *entry)
{
	if (sm->list == NULL) { /* first element */
		entry->id = 0;
		sm->list = entry;
		sm->count++;
	} else {
		struct _syncm_entry *list = sm->list;
		while (list->next != NULL)
			list = list->next;

		list->next = entry;
		entry->id = list->id+1;
		entry->prev = list;
		entry->next = NULL;
		sm->count++;
	}
}

static void syncm_add_entry(struct _sii_syncm *sm,
		int phys_address, int length, int control, int status, int enable, int type)
{
	struct _syncm_entry *entry = calloc(1, sizeof(struct _syncm_entry));
	entry->id = -1;
	entry->phys_address = phys_address;
	entry->length = length;
	entry->control = control;
	entry->status = status;
	entry->enable = enable;
	entry->type = type;

	// FIXME substitude with syncm_entry_add()
	if (sm->list == NULL) { /* first element */
		entry->id = 0;
		sm->list = entry;
		sm->count++;
	} else {
		struct _syncm_entry *list = sm->list;
		while (list->next != NULL)
			list = list->next;

		list->next = entry;
		entry->id = list->id+1;
		entry->prev = list;
		entry->next = NULL;
		sm->count++;
	}
}

static struct _sii_syncm *parse_syncm_section(const unsigned char *buffer, size_t secsize)
{
	size_t count=0;
	int smnbr = 0;
	const unsigned char *b = buffer;

	struct _sii_syncm *sm = calloc(1, sizeof(struct _sii_syncm));

	while (count<secsize) {
		int physadr = BYTES_TO_WORD(*b, *(b+1));
		b+=2;
		int length =  BYTES_TO_WORD(*b, *(b+1));
		b+=2;
		int control = *b;
		b++;
		int status = *b;
		b++;
		int enable = *b;
		b++;
		int type = *b;
		b++;

		syncm_add_entry(sm, physadr, length, control, status, enable, type);

		count=(size_t)(b-buffer);
		smnbr++;
	}

	return sm;
}

static void pdo_rm_entry(struct _sii_pdo *pdo)
{
	struct _pdo_entry *pe = pdo->list;

	if (pe == NULL)
		return;

	while (pe->next != NULL)
		pe = pe->next;

	if (pe->prev != NULL)
		pe->prev->next = NULL;

	if (pe->id == 0)
		pdo->list = NULL;

	free(pe);
}

void pdo_entry_add(struct _sii_pdo *pdo, struct _pdo_entry *entry)
{
	if (pdo->list == NULL) {
		entry->id = 0;
		entry->next = NULL;
		entry->prev = NULL;
		pdo->list = entry;
	} else {
		struct _pdo_entry *list = pdo->list;
		while (list->next != NULL)
			list = list->next;

		list->next = entry;
		entry->id = list->id+1;
		entry->prev = list;
		entry->next = NULL;
	}
}

static void pdo_add_entry(struct _sii_pdo *pdo,
		int index, int subindex, int string_index, int data_type,
		int bit_length, int flags)
{
	struct _pdo_entry *entry = calloc(1, sizeof(struct _pdo_entry));

	entry->id = -1;
	entry->index = index;
	entry->subindex = subindex;
	entry->string_index = string_index;
	entry->data_type = data_type;
	entry->bit_length = bit_length;
	entry->flags = flags;

	/* FIXME make use of pdo_entry_add() */
	if (pdo->list == NULL) {
		entry->id = 0;
		pdo->list = entry;
	} else {
		struct _pdo_entry *list = pdo->list;
		while (list->next != NULL)
			list = list->next;

		list->next = entry;
		entry->id = list->id+1;
		entry->prev = list;
		entry->next = NULL;
	}
}

static struct _sii_pdo *parse_pdo_section(const unsigned char *buffer, size_t secsize, enum ePdoType t)
{
	const unsigned char *b = buffer;
	int entry = 0;

	struct _sii_pdo *pdo = calloc(1, sizeof(struct _sii_pdo));

	switch (t) {
	case RxPDO:
		pdo->type = SII_RX_PDO;
		break;
	case TxPDO:
		pdo->type = SII_TX_PDO;
		break;
	default:
		pdo->type = SII_UNDEF_PDO;
		break;
	}

	pdo->index = BYTES_TO_WORD(*b, *(b+1));
	b+=2;
	pdo->entries = *b;
	b++;
	pdo->syncmanager = *b;
	b++;
	pdo->dcsync = *b;
	b++;
	pdo->name_index = *b;
	b++;
	pdo->flags = BYTES_TO_WORD(*b, *(b+1));
	b+=2;

	while ((unsigned int)(b-buffer)<secsize) {
		int index = BYTES_TO_WORD(*b, *(b+1));
		b+=2;
		int subindex =  *b;
		b++;
		int string_index = *b;
		b++;
		int data_type = *b;
		b++;
		int bit_length = *b;
		b++;
		int flags = BYTES_TO_WORD(*b, *(b+1));
		b+=2;

		pdo_add_entry(pdo, index, subindex, string_index, data_type, bit_length, flags);

		entry++;
	}

	return pdo;
}

static enum eSection get_next_section(const unsigned char *b, size_t *secsize)
{
	enum eSection next = SII_CAT_NOP;

	next = BYTES_TO_WORD(*b, *(b+1)) & 0xffff; /* attention, on some section bit 7 is vendor specific indicator */
	unsigned wordsize = BYTES_TO_WORD(*(b+2), *(b+3));
	*secsize = (wordsize<<1); /* bytesize = 2*wordsize */

	return next;
}


static struct _sii_dclock *parse_dclock_section(const unsigned char *buffer, size_t size)
{
	const unsigned char *b = buffer;

	struct _sii_dclock *dc = calloc(1, sizeof(struct _sii_dclock));

	b++; /* first byte is reserved */

	dc->cyclic_op_enabled = (*b & 0x01) == 0 ? 0 : 1;
	dc->sync0_active = (*b & 0x02) == 0 ? 0 : 1;
	dc->sync1_active = (*b & 0x04) == 0 ? 0 : 1;
	b++; /* next 5 bit reserved */

	dc->sync_pulse = BYTES_TO_WORD(*b, *(b+1));
	b+=2;

	dc->int0_status = (*b & 0x01) == 0 ? 0 : 1;
	b++;
	dc->int1_status = (*b & 0x01) == 0 ? 0 : 1;
	b++;

	dc->cyclic_op_starttime = BYTES_TO_DWORD(*b, *(b+1), *(b+2), *(b+3));
	b+=4;

	dc->sync0_cycle_time = BYTES_TO_DWORD(*b, *(b+1), *(b+2), *(b+3));
	b+=4;
	dc->sync1_cycle_time = BYTES_TO_DWORD(*b, *(b+1), *(b+2), *(b+3));
	b+=4;

	dc->latch0_pos_edge = (*b & 0x01) == 0 ? 0 : 1;
	dc->latch0_neg_edge = (*b & 0x02) == 0 ? 0 : 1;
	b+=1;

	dc->latch1_pos_edge = (*b & 0x01) == 0 ? 0 : 1;
	dc->latch1_neg_edge = (*b & 0x02) == 0 ? 0 : 1;
	b+=1;

	dc->latch0_pos_event = (*b & 0x01) == 0 ? 0 : 1;
	dc->latch0_neg_event = (*b & 0x02) == 0 ? 0 : 1;
	b+=1;

	dc->latch1_pos_event = (*b & 0x01) == 0 ? 0 : 1;
	dc->latch1_neg_event = (*b & 0x02) == 0 ? 0 : 1;
	b+=1;

	b+=10; /* reserved */
	dc->latch0_pos_edge_value = BYTES_TO_DWORD(*b, *(b+1), *(b+2), *(b+3));
	b+=4;

	dc->latch0_neg_edge_value = BYTES_TO_DWORD(*b, *(b+1), *(b+2), *(b+3));
	b+=4;

	dc->latch1_pos_edge_value = BYTES_TO_DWORD(*b, *(b+1), *(b+2), *(b+3));
	b+=4;

	dc->latch1_neg_edge_value = BYTES_TO_DWORD(*b, *(b+1), *(b+2), *(b+3));
	b+=4;

	size_t count = b-buffer;
	if (size != count)
		printf("%s: Warning counter differs from size\n", __func__);

	return dc;
}

#if 0 // FIXME this could become usefull in the future.
static int g_print_offsets = 0;

static void print_offsets(const unsigned char *start, const unsigned char *current)
{
	if (!g_print_offsets) {
		printf("\n");
		return;
	}

	printf("\n[Offset: 0x%0x (%zu)] ", (unsigned int)(current-start), current-start);
}
#endif

static int parse_content(struct _sii *sii, const unsigned char *eeprom, size_t maxsize)
{
	enum eSection section = SII_PREAMBLE;
	const size_t max_bytes_size = 2 * maxsize;
	size_t secsize = 0;
	const unsigned char *buffer = eeprom;

	struct _sii_cat *newcat;

	struct _sii_strings *strings;
	struct _sii_general *general;
	struct _sii_fmmu *fmmu;
	struct _sii_syncm *syncmanager;
	struct _sii_pdo *txpdo;
	struct _sii_pdo *rxpdo;
	struct _sii_dclock *distributedclock;

	while (max_bytes_size > (size_t)(buffer - eeprom)) {
		switch (section) {
		case SII_PREAMBLE:
			sii->preamble = parse_preamble(buffer, 16);
			buffer = eeprom+16;
			section = SII_STD_CONFIG;
			break;

		case SII_STD_CONFIG:
			sii->config = parse_stdconfig(buffer, 46+66);
			buffer = buffer+46+66;
			section = get_next_section(buffer, &secsize);
			buffer += 4;
			break;

		case SII_CAT_STRINGS:
			newcat = cat_new((uint16_t)(section&0xffff), (uint16_t)(secsize&0xffff));
			strings = parse_string_section(buffer, secsize);
			newcat->data = (void *)strings;
			cat_add(sii, newcat);
#if DEBUG == 1
			printf("DEBUG Added string section\n");
#endif

			buffer+=secsize;
			section = get_next_section(buffer, &secsize);
			buffer+=4;
			break;

		case SII_CAT_DATATYPES:
			parse_datatype_section(buffer, secsize);
			buffer+=secsize;
			section = get_next_section(buffer, &secsize);
			buffer+=4;
			break;

		case SII_CAT_GENERAL:
			newcat = cat_new((uint16_t)(section&0xffff), (uint16_t)(secsize&0xffff));
			general = parse_general_section(buffer, secsize);
			newcat->data = (void *)general;
			cat_add(sii, newcat);
#if DEBUG == 1
			printf("DEBUG Added general section\n");
#endif

			buffer+=secsize;
			section = get_next_section(buffer, &secsize);
			buffer+=4;
			break;

		case SII_CAT_FMMU:
			newcat = cat_new((uint16_t)(section&0xffff), (uint16_t)(secsize&0xffff));
			fmmu = parse_fmmu_section(buffer, secsize);
			newcat->data = (void *)fmmu;
			cat_add(sii, newcat);
#if DEBUG == 1
			printf("DEBUG Added fmmu section\n");
#endif

			buffer+=secsize;
			section = get_next_section(buffer, &secsize);
			buffer+=4;
			break;

		case SII_CAT_SYNCM:
			newcat = cat_new((uint16_t)(section&0xffff), (uint16_t)(secsize&0xffff));
			syncmanager = parse_syncm_section(buffer, secsize);
			newcat->data = (void *)syncmanager;
			cat_add(sii, newcat);
#if DEBUG == 1
			printf("DEBUG Added syncm section\n");
#endif

			buffer+=secsize;
			section = get_next_section(buffer, &secsize);
			buffer+=4;
			break;

		case SII_CAT_TXPDO:
			newcat = cat_new((uint16_t)(section&0xffff), (uint16_t)(secsize&0xffff));
			txpdo = parse_pdo_section(buffer, secsize, TxPDO);
			newcat->data = (void *)txpdo;
			cat_add(sii, newcat);
#if DEBUG == 1
			printf("DEBUG Added txpdo section\n");
#endif

			buffer+=secsize;
			section = get_next_section(buffer, &secsize);
			buffer+=4;
			break;

		case SII_CAT_RXPDO:
			newcat = cat_new((uint16_t)(section&0xffff), (uint16_t)(secsize&0xffff));
			rxpdo = parse_pdo_section(buffer, secsize, RxPDO);
			newcat->data = (void *)rxpdo;
			cat_add(sii, newcat);
#if DEBUG == 1
			printf("DEBUG Added rxpdo section\n");
#endif

			buffer+=secsize;
			section = get_next_section(buffer, &secsize);
			buffer+=4;
			break;

		case SII_CAT_DCLOCK:
			newcat = cat_new((uint16_t)(section&0xffff), (uint16_t)(secsize&0xffff));
			distributedclock = parse_dclock_section(buffer, secsize);
			newcat->data = (void *)distributedclock;
			cat_add(sii, newcat);
#if DEBUG == 1
			printf("DEBUG Added dclock section\n");
#endif

			buffer+=secsize;
			section = get_next_section(buffer, &secsize);
			buffer+=4;
			break;

		case SII_END:
			goto finish;
			break;

		case SII_CAT_NOP:
		default:
			fprintf(stderr, "[WARNING] Category 0x%.4x unknown, skipping ....\n", section);
			buffer+=secsize;
			section = get_next_section(buffer, &secsize);
			buffer+=4;
			break;
		}
	}

	fprintf(stderr, "Error, SII probably malformed. No 0xffff at the end found\n");
	return 1;

finish:

	return 0;
}

/*** categroy list handling ***/

static void cat_data_cleanup_fmmu(struct _sii_fmmu *fmmu)
{
	while (fmmu->list != NULL)
		fmmu_rm_entry(fmmu);

	free(fmmu);
}

static void cat_data_cleanup_syncm(struct _sii_syncm *syncm)
{
	while (syncm->list != NULL)
		syncm_rm_entry(syncm);
	free(syncm);
}

static void cat_data_cleanup_pdo(struct _sii_pdo *pdo)
{
	while (pdo->list != NULL)
		pdo_rm_entry(pdo);

	free(pdo);
}

static void cat_data_cleanup_strings(struct _sii_strings *str)
{
	if (str == NULL)
		return;

	struct _string *current = str->head;
	struct _string *tmp;

	for (struct _string *s = current; s; ) {
		free(s->data);
		if (s->next)
			s->next->prev = NULL;

		tmp = s;
		s = s->next;
		free(tmp);
	}

	free(str);
}

static void cat_data_cleanup(struct _sii_cat *cat)
{
	/* clean up type specific data */
	switch (cat->type) {
	case SII_CAT_STRINGS:
		cat_data_cleanup_strings((struct _sii_strings *)cat->data);
		break;

	case SII_CAT_DATATYPES:
		fprintf(stderr, "The Datatype categroie isn't implemented.\n");
		break;

	case SII_CAT_GENERAL:
		free(cat->data); /* section general is simple */
		break;

	case SII_CAT_FMMU:
		cat_data_cleanup_fmmu((struct _sii_fmmu *)cat->data);
		break;

	case SII_CAT_SYNCM:
		cat_data_cleanup_syncm((struct _sii_syncm *)cat->data);
		break;

	case SII_CAT_TXPDO:
	case SII_CAT_RXPDO:
		cat_data_cleanup_pdo((struct _sii_pdo *)cat->data);
		break;

	case SII_CAT_DCLOCK:
		free(cat->data); /* section dc is simple */
		break;

	default:
		printf("Warning: cleanup received unknown category\n");
		break;
	}
}

#if 0
static struct _sii_cat *cat_new_data(uint16_t type, uint16_t size, void *data)
{
	struct _sii_cat *new = calloc(1, sizeof(struct _sii_cat));

	new->type   = type&0x7fff;
	new->vendor = (type>>16)&0x1;
	new->size   = size;
	new->data   = data;

	return new;
}
#endif

static struct _sii_cat *cat_new(uint16_t type, uint16_t size)
{
	struct _sii_cat *new = calloc(1, sizeof(struct _sii_cat));

	new->type   = type&0x7fff;
	new->vendor = (type>>16)&0x1;
	new->size   = size;

	return new;
}

/* add new element to the end of the list */
static int cat_add(SiiInfo *sii, struct _sii_cat *new)
{
	struct _sii_cat *curr = sii->cat_head;

	if (curr == NULL) {
		sii->cat_head = new;
		new->prev = NULL;
		new->next = NULL;
		return 0;
	}

	while (curr->next != NULL)
		curr = curr->next;

	curr->next = new;
	new->prev = curr;

	return 0;
}

/* removes the last category element */
static int cat_rm(SiiInfo *sii)
{
	struct _sii_cat *curr = sii->cat_head;

	if (curr == NULL)
		return 1;

	while (curr->next != NULL)
		curr = curr->next;

	if (curr->prev != NULL) {
		curr->prev->next = NULL;
		curr->prev = NULL;
	}

	cat_data_cleanup(curr);
	curr->data = NULL;
	curr->type = 0;
	if (curr == sii->cat_head)
		sii->cat_head = NULL;

	free(curr);

	return 0;
}

static struct _sii_cat * cat_next(SiiInfo *sii)
{
	sii->cat_current = sii->cat_current->next;
	return sii->cat_current;
}

static void cat_rewind(SiiInfo *sii)
{
	sii->cat_current = sii->cat_head;
}

static const char *cat_name(enum eSection type)
{
	const char *cat_string[] = {
		"Unknown",
		"Strings",
		"Datatypes",
		"Standard Config",
		"General",
		"FMMU", "SyncManager",
		"TX PDO", "RX PDO",
		"DC CLOCK"
	};
	unsigned int string_index = 0;

	switch (type) {
	case SII_CAT_STRINGS:
		string_index = 1;
		break;
	case SII_CAT_DATATYPES:
		string_index = 2;
		break;
	case SII_CAT_GENERAL:
		string_index = 4;
		break;
	case SII_CAT_FMMU:
		string_index = 5;
		break;
	case SII_CAT_SYNCM:
		string_index = 6;
		break;
	case SII_CAT_TXPDO:
		string_index = 7;
		break;
	case SII_CAT_RXPDO:
		string_index = 8;
		break;
	case SII_CAT_DCLOCK:
		string_index = 9;
		break;
	default:
		string_index = 0;
		break;
	}

	return cat_string[string_index];
}

static void cat_print(struct _sii_cat *cat)
{
	/* preamble and std config should printed here */
	printf("Print Categorie: %s (0x%x)\n", cat_name(cat->type), cat->type);
	switch (cat->type) {
	case SII_CAT_STRINGS:
		cat_print_strings(cat);
		break;
	case SII_CAT_DATATYPES:
		cat_print_datatypes(cat);
		break;
	case SII_CAT_GENERAL:
		cat_print_general(cat);
		break;
	case SII_CAT_FMMU:
		cat_print_fmmu(cat);
		break;
	case SII_CAT_SYNCM:
		cat_print_syncm(cat);
		break;
	case SII_CAT_TXPDO:
		cat_print_txpdo(cat);
		break;
	case SII_CAT_RXPDO:
		cat_print_rxpdo(cat);
		break;
	case SII_CAT_DCLOCK:
		cat_print_dc(cat);
		break;
	default:
		printf("Warning no valid categorie\n");
		break;
	}
}

static void cat_print_strings(struct _sii_cat *cat)
{
	struct _sii_strings *str = (struct _sii_strings *)cat->data;
	if (str == NULL)
		return;

	printf("  Size: %d Bytes with %d strings\n", cat->size, str->count);

	printf("  ID   Size (Bytes)    String\n");
	for (struct _string *s = str->head; s; s = s->next)
		printf("  %3d: (%3d) ......... '%s'\n", s->id, s->length, s->data);
	printf("\n");
}

static void cat_print_datatypes(struct _sii_cat *cat)
{
	printf("Size: %d Bytes\n", cat->size);
	printf(".... tba\n");
}

static struct _sii_cat *sii_category_find_neighbor(struct _sii_cat *cat, enum eSection sec)
{
	struct _sii_cat *sc = cat;

	/* rewind */
	while (sc->prev != NULL)
		sc = sc->prev;

	/* search */
	while (sc->next != NULL) {
		if (sc->type == sec)
			return sc;

		sc = sc->next;
	}

	return NULL;
}

static void cat_print_general(struct _sii_cat *cat)
{
	printf("  Size: %d Bytes\n", cat->size);
	struct _sii_general *gen = (struct _sii_general *)cat->data;

	//printf("General:\n");
	struct _sii_cat *sc = sii_category_find_neighbor(cat, SII_CAT_STRINGS);
	const char *tmpstr = NULL;

	printf("  Vendor Specific (Index of String)\n");

	tmpstr = string_search_id((struct _sii_strings *)(sc->data), gen->nameindex);
	if (NULL == tmpstr)
		tmpstr = "not set";
	printf("    Name  Index: %d: ............. %s\n", gen->nameindex,  tmpstr);

	tmpstr = string_search_id((struct _sii_strings *)(sc->data), gen->groupindex);
	if (NULL == tmpstr)
		tmpstr = "not set";
	printf("    Group Index: %d: ............. %s\n", gen->groupindex, tmpstr);

	tmpstr = string_search_id((struct _sii_strings *)(sc->data), gen->imageindex);
	if (NULL == tmpstr)
		tmpstr = "not set";
	printf("    Image Index: %d: ............. %s\n", gen->imageindex, tmpstr);

	tmpstr = string_search_id((struct _sii_strings *)(sc->data), gen->orderindex);
	if (NULL == tmpstr)
		tmpstr = "not set";
	printf("    Order Index: %d: ............. %s\n", gen->orderindex, tmpstr);
	tmpstr = NULL;
	printf("\n");

	printf("  CoE Details:\n");
	printf("    Enable SDO: .................. %s\n", gen->coe_enable_sdo == 0          ? "no" : "yes");
	printf("    Enable SDO Info: ............. %s\n", gen->coe_enable_sdo_info == 0     ? "no" : "yes");
	printf("    Enable PDO Assign: ........... %s\n", gen->coe_enable_pdo_assign == 0   ? "no" : "yes");
	printf("    Enable PDO Configuration: .... %s\n", gen->coe_enable_pdo_conf == 0     ? "no" : "yes");
	printf("    Enable Upload at Startup: .... %s\n", gen->coe_enable_upload_start == 0 ? "no" : "yes");
	printf("    Enable SDO complete access: .. %s\n", gen->coe_enable_sdo_complete == 0 ? "no" : "yes");

	printf("  FoE Details: ................... %s\n", gen->foe_enabled == 0 ? "not enabled" : "enabled");
	printf("  EoE Details: ................... %s\n", gen->eoe_enabled == 0 ? "not enabled" : "enabled");

	printf("\n");
	printf("  Flag SafeOp: ................... %s\n", gen->flag_safe_op == 0 ? "not enabled" : "enabled");
	printf("  Flag notLRW: ................... %s\n", gen->flag_notLRW == 0 ? "not enabled" : "enabled");
	printf("  Flag MBox Data Link Layer ...... %s\n", gen->flag_MBoxDataLinkLayer == 0 ? "not enabled" : "enabled");
	printf("  Flag Ident AL Status ........... %s\n", gen->flag_IdentALSts == 0 ? "not enabled" : "enabled");
	printf("  Flag Ident Physical Memory ..... %s\n", gen->flag_IdentPhyM == 0  ? "not enabled" : "enabled");
	printf("\n");

	printf("  CurrentOnEBus: ................. %d mA\n", gen->current_ebus);

	printf("  Physical Ports:\n");
	printf("     Port 0: ..................... %s\n", physport_type(gen->phys_port_0));
	printf("     Port 1: ..................... %s\n", physport_type(gen->phys_port_1));
	printf("     Port 2: ..................... %s\n", physport_type(gen->phys_port_2));
	printf("     Port 3: ..................... %s\n", physport_type(gen->phys_port_3));
	printf("\n");
	printf("  Physical Memory Address ........ 0x%.4x\n", gen->physical_address);
	printf("\n");
}

static void cat_print_fmmu(struct _sii_cat *cat)
{
	printf("  Size: %d Bytes\n", cat->size);

	struct _sii_fmmu *fmmus = cat->data;
	printf("  Number of FMMUs: %d\n", fmmus->count);

	struct _fmmu_entry *fmmu = fmmus->list;

	while (fmmu != NULL) {
		printf("    FMMU%d: ", fmmu->id);
		switch (fmmu->usage) {
		case 0x00:
		case 0xff:
			printf("not used\n");
			break;
		case FMMU_OUTPUTS:
			printf("used for Outputs\n");
			break;
		case FMMU_INPUTS:
			printf("used for Inputs\n");
			break;
		case FMMU_SYNCMSTAT:
			printf("used for SyncM mailbox status (MBoxStat)\n");
			break;
		default:
			printf("WARNING: undefined behavior\n");
			break;
		}

		fmmu = fmmu->next;
	}

	printf("\n");
}

static void cat_print_syncm_entries(struct _syncm_entry *sme)
{
	struct _syncm_entry *e = sme;
	int smnbr = 0;

	while (e != NULL) {
		printf("  SyncManager SM%d\n", smnbr);
		printf("    Physical Startaddress: ... 0x%04x\n", e->phys_address);
		printf("    Length: .................. %d\n", e->length);
		printf("    Control Register: ........ 0x%02x\n", e->control);
		printf("    Status Register: ......... 0x%02x\n", e->status);
		printf("    Enable byte: ............. 0x%02x\n", e->enable);
		printf("    SM Type: ................. ");
		switch (e->type) {
		case SMT_UNUSED:
			printf("not used or unknown\n");
			break;
		case SMT_MBOXOUT:
			printf("Mailbox Out\n");
			break;
		case SMT_MBOXIN:
			printf("Mailbox In\n");
			break;
		case SMT_OUTPUTS:
			printf("Process Data Out\n");
			break;
		case SMT_INPUTS:
			printf("Process Data In\n");
			break;
		default:
			printf("undefined\n");
			break;
		}

		smnbr++;
		e = e->next;
	}
}

static void cat_print_syncm(struct _sii_cat *cat)
{
	struct _sii_syncm *sm = (struct _sii_syncm *)cat->data;

	printf("  Size: %d Bytes\n", cat->size);
	printf("  Number of SyncManager: %d\n", sm->count);

	cat_print_syncm_entries(sm->list);
	printf("\n");
}

static void cat_print_rxpdo(struct _sii_cat *cat)
{
	cat_print_pdo(cat);
}

static void cat_print_txpdo(struct _sii_cat *cat)
{
	cat_print_pdo(cat);
}

static void cat_print_pdo(struct _sii_cat *cat)
{
	printf("  Size: %d Bytes\n", cat->size);

	struct _sii_pdo *pdo = (struct _sii_pdo *)cat->data;

	const char *pdo_flags_description[] = {
	    "PDO is Mandatory",
	    "PDO is Default",
	    "Reserved (PdoOversample)",
	    "undefined",
	    "PDO is Fixed",
	    "PDO is Virtual",
	    "Reserved (PDO Download Anyway)",
	    "Reserved (PDO From Module)",
	    "PDO is Module Align",
	    "PDO is Depend on Slot",
	    "PDO is Depend on Slot Group",
	    "PDO is Overwritten by Module",
	    "Reserved (PdoConfigurable)",
	    "Reserved (PdoAutoPdoName)",
	    "Reserved (PdoDisAutoExclude)",
	    "Reserved (PdoWritable)"
	};

	char *pdostr = NULL;
	switch (pdo->type) {
	case RxPDO:
		pdostr = "RxPDO";
		break;
	case TxPDO:
		pdostr = "TxPDO";
		break;
	default:
		pdostr = "undefined";
		break;
	}
	printf("  %s:\n", pdostr);
	printf("    PDO Index: .................. 0x%04x\n", pdo->index);
	printf("    Entries: .................... %d\n", pdo->entries);
	printf("    SyncM: ...................... %d\n", pdo->syncmanager);
	printf("    Synchronization: ............ 0x%02x\n", pdo->dcsync);
	printf("    Name Index: ................. %d\n", pdo->name_index);
	printf("    Flags ....................... 0x%04x\n", pdo->flags);
	for (int i=0; i<16; i++) {
		if (pdo->flags&(1<<i))
			printf("                                  %s\n", pdo_flags_description[i]);
	}

	struct _pdo_entry *list = pdo->list;
	struct _sii_cat *sc = sii_category_find_neighbor(cat, SII_CAT_STRINGS);
	const char *tmpstr = NULL;

	while (list != NULL) {
		tmpstr = string_search_id((struct _sii_strings *)(sc->data), list->string_index);
		if (NULL == tmpstr)
			tmpstr = "not set";

		printf("\n");
		printf("      Entry %d:\n", list->id);
		printf("      Entry Index: .............. 0x%04x\n", list->index);
		printf("      Subindex: ................. 0x%02x\n", list->subindex);
		printf("      String Index: ............. %d (%s)\n", list->string_index, tmpstr);
		printf("      Data Type: ................ 0x%02x (Index in CoE Object Dictionary)\n", list->data_type);
		printf("      Bitlength: ................ %d\n", list->bit_length);

		list = list->next;
	}

	printf("\n");
}


static void cat_print_dc(struct _sii_cat *cat)
{
	printf("Size: %d Bytes\n", cat->size);

	struct _sii_dclock *dc = (struct _sii_dclock *)cat->data;

	printf("  Cyclic Operation Enable: ...... %s\n",       dc->cyclic_op_enabled == 0 ? "no" : "yes");
	printf("  SYNC0 activate: ............... %s\n",       dc->sync0_active == 0      ? "no" : "yes");
	printf("  SYNC1 activate: ............... %s\n",       dc->sync1_active == 0      ? "no" : "yes");
	printf("  SYNC Pulse: ................... %d (ns?)\n", dc->sync_pulse);
	printf("  Interrupt 0 Status: ........... %s\n",       dc->int0_status == 0       ? "not active" : "active");
	printf("  Interrupt 1 Status: ........... %s\n",       dc->int1_status == 0       ? "not active" : "active");
	printf("  Cyclic Operation Startime: .... %d ns\n",    dc->cyclic_op_starttime);
	printf("  SYNC0 Cycle Time: ............. %d (ns?)\n", dc->sync0_cycle_time);
	printf("  SYNC0 Cycle Time: ............. %d (ns?)\n", dc->sync1_cycle_time);
	printf("\n");

	printf("  Latch Description\n");
	printf("    Latch 0 PosEdge: ............ %s\n",       dc->latch0_pos_edge == 0   ? "continous" : "single");
	printf("    Latch 0 NegEdge: ............ %s\n",       dc->latch0_neg_edge == 0   ? "continous" : "single");
	printf("    Latch 1 PosEdge: ............ %s\n",       dc->latch1_pos_edge == 0   ? "continous" : "single");
	printf("    Latch 1 NegEdge: ............ %s\n",       dc->latch1_neg_edge == 0   ? "continous" : "single");
	printf("    Latch 0 PosEvnt: ............ %s\n",       dc->latch0_pos_event == 0  ? "no Event" : "Event stored");
	printf("    Latch 0 NegEvnt: ............ %s\n",       dc->latch0_neg_event == 0  ? "no Event" : "Event stored");
	printf("    Latch 1 PosEvnt: ............ %s\n",       dc->latch1_pos_event == 0  ? "no Event" : "Event stored");
	printf("    Latch 1 NegEvnt: ............ %s\n",       dc->latch1_neg_event == 0  ? "no Event" : "Event stored");
	printf("    Latch0PosEdgeValue: ......... 0x%08x\n",   dc->latch0_pos_edge_value);
	printf("    Latch0NegEdgeValue: ......... 0x%08x\n",   dc->latch0_neg_edge_value);
	printf("    Latch1PosEdgeValue: ......... 0x%08x\n",   dc->latch1_pos_edge_value);
	printf("    Latch1NegEdgeValue: ......... 0x%08x\n",   dc->latch1_neg_edge_value);
	printf("\n");
}

/* write sii binary data */

static uint16_t sii_cat_write_strings(struct _sii_cat *cat, unsigned char *buf)
{
	unsigned char *strc = buf;
	unsigned char *b = buf+1;
	struct _sii_strings *strings = (struct _sii_strings *)cat->data;

	if (strings == NULL)
		return 0;

	for (struct _string *s = strings->head; s; s = s->next) {
		char *str = s->data;
		*b = s->length;
		b++;
		memmove(b, str, strlen(str));
		b+= strlen(str);
	}
	*strc = strings->count;

	return (uint16_t)(b-buf);
}

static uint16_t sii_cat_write_datatypes(struct _sii_cat *cat, unsigned char *buf)
{
	unsigned char *b = buf;
	printf("TODO: binary write of datatypes section (0x%x)\n", cat->type);
	return (uint16_t)(b-buf);
}

static uint16_t sii_cat_write_general(struct _sii_cat *cat, unsigned char *buf)
{
#if 1
	unsigned char *b = buf;
#if DEBUG == 1
	size_t size = sizeof(struct _sii_general)/sizeof(unsigned char);
	printf("DEBUG Categorie general is %lu bytes\n", size);
#endif

	struct _sii_general *bcat = (struct _sii_general *)cat->data;

	*b++ = bcat->groupindex;
	*b++ = bcat->imageindex;
	*b++ = bcat->orderindex;
	*b++ = bcat->nameindex;
	*b++ = 0x00; /* reserved */
	*b++ = (bcat->coe_enable_sdo&0x01) |
		((bcat->coe_enable_sdo_info<<1)&0x02) |
		((bcat->coe_enable_pdo_assign<<2)&0x04) |
		((bcat->coe_enable_pdo_conf<<3)&0x08) |
		((bcat->coe_enable_upload_start<<4)&0x10) |
		((bcat->coe_enable_sdo_complete<<5)&0x20);
	*b++ = bcat->foe_enabled&0x01;
	*b++ = bcat->eoe_enabled&0x01;
	*b++ = 0x00; /* soe_channels */
	*b++ = 0x00; /* ds402_channels */
	*b++ = 0x00; /* sysman class */
	*b++ = (bcat->flag_safe_op&0x01) |
		((bcat->flag_notLRW<<1)&0x02) |
		((bcat->flag_MBoxDataLinkLayer<<2)&0x04) |
		((bcat->flag_IdentALSts<<3)&0x08) |
		((bcat->flag_IdentPhyM<<4)&0x10);
	*b++ = bcat->current_ebus&0xff;
	*b++ = (bcat->current_ebus>>8)&0xff;
	*b++ = bcat->groupindex&0xff; /* mirrored groupindex */
	*b++ = 0x00;
	*b++ = (bcat->phys_port_0&0x0f) | ((bcat->phys_port_1<<4)&0xf0);
	*b++ = (bcat->phys_port_2&0x0f) | ((bcat->phys_port_3<<4)&0xf0);

	*b++ = bcat->physical_address&0xff;
	*b++ = (bcat->physical_address>>8)&0xff;

	for (int i=0; i<12; i++)
		*b++ = 0x00;

	return (uint16_t)(b-buf);
#else
	return sii_cat_write_cat(cat, buf);
#endif
}

static uint16_t sii_cat_write_fmmu(struct _sii_cat *cat, unsigned char *buf)
{
	unsigned char *b = buf;
	struct _sii_fmmu *data = cat->data;
	struct _fmmu_entry *entry = data->list;

	while (entry != NULL) {
		*b++ = entry->usage;
		entry = entry->next;
	}

    /* add padding if odd number of FMMU is defined */
    if (data->count % 2 != 0) {
        *b++ = 0x00;
    }

	return (uint16_t)(b-buf);
}

static uint16_t sii_cat_write_syncm(struct _sii_cat *cat, unsigned char *buf)
{
	unsigned char *b = buf;
	struct _sii_syncm *sm = cat->data;
	struct _syncm_entry *entry = sm->list;

	while (entry != NULL) {
		*b++ = entry->phys_address&0xff;
		*b++ = (entry->phys_address>>8)&0xff;
		*b++ = entry->length&0xff;
		*b++ = (entry->length>>8)&0xff;
		*b++ = entry->control;
		*b++ = entry->status;
		*b++ = entry->enable;
		*b++ = entry->type;

		entry = entry->next;
	}

	return (uint16_t)(b-buf);
}

static uint16_t sii_cat_write_pdo(struct _sii_cat *cat, unsigned char *buf)
{
	unsigned char *b = buf;
	struct _sii_pdo *pdo = cat->data;

	*b++ = pdo->index&0xff;
	*b++ = (pdo->index>>8)&0xff;
	*b++ = pdo->entries;
	*b++ = pdo->syncmanager;
	*b++ = pdo->dcsync;
	*b++ = pdo->name_index;
	*b++ = pdo->flags&0xff;
	*b++ = (pdo->flags>>8)&0xff;

	struct _pdo_entry *entry = pdo->list;

	while (entry != NULL) {
		*b++ = entry->index&0xff;
		*b++ = (entry->index>>8)&0xff;
		*b++ = entry->subindex;
		*b++ = entry->string_index;
		*b++ = entry->data_type;
		*b++ = entry->bit_length;
		*b++ = entry->flags&0xff;
		*b++ = (entry->flags>>8)&0xff;

		entry = entry->next;
	}

	return (uint16_t)(b-buf);
}

static uint16_t sii_cat_write_dc(struct _sii_cat *cat, unsigned char *buf)
{
#if 1
	unsigned char *b = buf;
	struct _sii_dclock *dc = cat->data;

	*b++ = 0; /* reserved */
	*b++ = (dc->cyclic_op_enabled&0x01) |
		((dc->sync0_active<<1)&0x02) |
		((dc->sync1_active<<2)&0x04);
	*b++ = dc->sync_pulse&0xff;
	*b++ = (dc->sync_pulse>>8)&0xff;
	*b++ = (dc->int0_status&0x01);
	*b++ = (dc->int1_status&0x01);

	*b++ = dc->cyclic_op_starttime&0xff;
	*b++ = (dc->cyclic_op_starttime>>8)&0xff;
	*b++ = (dc->cyclic_op_starttime>>16)&0xff;
	*b++ = (dc->cyclic_op_starttime>>24)&0xff;

	*b++ = dc->sync0_cycle_time&0xff;
	*b++ = (dc->sync0_cycle_time>>8)&0xff;
	*b++ = (dc->sync0_cycle_time>>16)&0xff;
	*b++ = (dc->sync0_cycle_time>>24)&0xff;

	*b++ = dc->sync1_cycle_time&0xff;
	*b++ = (dc->sync1_cycle_time>>8)&0xff;
	*b++ = (dc->sync1_cycle_time>>16)&0xff;
	*b++ = (dc->sync1_cycle_time>>24)&0xff;

	*b++ = (dc->latch0_pos_edge&0x01) | ((dc->latch0_neg_edge<<1)&0x02);

	*b++ = (dc->latch1_pos_edge&0x01) | ((dc->latch1_neg_edge<<1)&0x02);

	*b++ = (dc->latch0_pos_event&0x01) | ((dc->latch0_neg_event<<1)&0x02);
	*b++ = (dc->latch1_pos_event&0x01) | ((dc->latch1_neg_event<<1)&0x02);

	b+=10; /* reserved */

	*b++ = dc->latch0_pos_edge_value&0xff;
	*b++ = (dc->latch0_pos_edge_value>>8)&0xff;
	*b++ = (dc->latch0_pos_edge_value>>16)&0xff;
	*b++ = (dc->latch0_pos_edge_value>>24)&0xff;

	*b++ = dc->latch0_neg_edge_value&0xff;
	*b++ = (dc->latch0_neg_edge_value>>8)&0xff;
	*b++ = (dc->latch0_neg_edge_value>>16)&0xff;
	*b++ = (dc->latch0_neg_edge_value>>24)&0xff;

	*b++ = dc->latch1_pos_edge_value&0xff;
	*b++ = (dc->latch1_pos_edge_value>>8)&0xff;
	*b++ = (dc->latch1_pos_edge_value>>16)&0xff;
	*b++ = (dc->latch1_pos_edge_value>>24)&0xff;

	*b++ = dc->latch1_neg_edge_value&0xff;
	*b++ = (dc->latch1_neg_edge_value>>8)&0xff;
	*b++ = (dc->latch1_neg_edge_value>>16)&0xff;
	*b++ = (dc->latch1_neg_edge_value>>24)&0xff;

	return (uint16_t)(b-buf);
#else
	/* NOTE: dclock has 82 bytes, but the struct is 84 bytes. */
#if DEBUG == 1
	printf("DEBUG: size of struct _sii_dclock: %d\n", sizeof(struct _sii_dclock));
#endif
	return sii_cat_write_cat(cat, buf);
#endif
}


#if DEBUG == 1
static size_t cat_size(struct _sii_cat *cat)
{
	size_t sz = 0;

	switch (cat->type) {
	case SII_CAT_STRINGS:
		//number of strings bytes for each string...
		fprintf(stderr, "Error, category 0x%.x not implemented\n", cat->type);
		break;

	case SII_CAT_DATATYPES:
		// unimplemented
		fprintf(stderr, "Error, category 0x%.x not implemented\n", cat->type);
		break;

	case SII_CAT_GENERAL:
		sz = sizeof(struct _sii_general)/sizeof(unsigned char);
		break;

	case SII_CAT_FMMU:
		sz = sizeof(struct _sii_fmmu)/sizeof(unsigned char);
		break;

	case SII_CAT_SYNCM:
		sz = sizeof(struct _sii_syncm)/sizeof(unsigned char);
		break;

	case SII_CAT_TXPDO:
	case SII_CAT_RXPDO:
		sz = sizeof(struct _sii_pdo)/sizeof(unsigned char);
		break;

	case SII_CAT_DCLOCK:
		sz = sizeof(struct _sii_dclock)/sizeof(unsigned char);
		break;

	default:
		fprintf(stderr, "Error, unknown category 0x%.x\n", cat->type);
		return 0;
	}

	return sz;
}
#endif

#if DEBUG == 1
static uint16_t sii_cat_write_cat(struct _sii_cat *cat, unsigned char *buf)
{
	unsigned char *b = buf;
	size_t catbsz = cat_size(cat);

	if (catbsz == 0) /* nothing to write */
		return 0;

	unsigned char *catb = (unsigned char *)cat->data;
	for (size_t i=0; i<catbsz; i++)
		*b++ = *catb++;

	printf("DEBUG: print cat: 0x%x; expected write: %lu, written: %ld\n",
			cat->type, catbsz, (b-buf));

	return (uint16_t)(b-buf);
}
#endif

static size_t sii_cat_write(struct _sii *sii, uint16_t skipmask)
{
	unsigned char *buf = sii->rawbytes+sii->rawsize;
	//size_t *bufsize = &sii->rawsize;
	struct _sii_cat *cat = sii->cat_head;
	size_t written = 0;
	uint16_t catsize = 0;

	// for each category:
	// buf[0], buf[1] <- type
	// buf[2], buf[3] <- size
	// buf[N>3] category data

	while (cat != NULL) {
		unsigned char *ct = buf;
		unsigned char *cs = buf+2;
		buf += 4;

		*ct = cat->type&0xff;
		*(ct+1) = (cat->type>>8)&0xff;

		switch (cat->type) {
		case SII_CAT_STRINGS:
			catsize = sii_cat_write_strings(cat, buf);
			break;

		case SII_CAT_DATATYPES:
			catsize = sii_cat_write_datatypes(cat, buf);
			break;

		case SII_CAT_GENERAL:
			catsize = sii_cat_write_general(cat, buf);
			break;

		case SII_CAT_FMMU:
			catsize = sii_cat_write_fmmu(cat, buf);
			break;

		case SII_CAT_SYNCM:
			catsize = sii_cat_write_syncm(cat, buf);
			break;

		case SII_CAT_TXPDO:
		case SII_CAT_RXPDO:
			if (skipmask & (SKIP_TXPDO | SKIP_RXPDO)) {
				buf -= 4;
				cat = (cat->next != NULL) ? cat->next : NULL;
				continue;
			} else {
				catsize = sii_cat_write_pdo(cat, buf);
			}
			break;

		case SII_CAT_DCLOCK:
			if (skipmask & SKIP_DC) {
				buf -= 4;
				cat = (cat->next != NULL) ? cat->next : NULL;
				continue;
			} else {
				catsize = sii_cat_write_dc(cat, buf);
			}
			break;

		default:
			fprintf(stderr, "Warning Unknown category - skipping!\n");
			buf -= 4;
			goto nextcat;
			break;
		}

		// pad to be word alligned
		if (catsize & 1) {
			buf[catsize] = 0;
			catsize++;
		}

#if DEBUG == 1
		printf("[DEBUG %s] section type 0x%.4x size: 0x%.4x\n", __func__, cat->type, catsize/2);
#endif

		if (catsize == 0) {
			fprintf(stderr, "Warning, existing category %s (0x%.x) unexpected empty\n",
					cat2string(cat->type), cat->type);
			buf -= 4; /* rewind */
			goto nextcat;
		}

		/* hot patch for uneven string category sizes */
		if ((SII_CAT_STRINGS == cat->type) && (catsize % 2 != 0)) {
			catsize += 1;
		}

		written += catsize+4;
		buf += catsize;
		catsize = catsize/2; /* byte -> word count */
		*cs = catsize&0xff;
		*(cs+1) = (catsize>>8)&0xff;

nextcat:
		cat = cat->next;
	}

	return written;
}

static struct _sii_cat *sii_cat_get_min(struct _sii_cat *head)
{
	struct _sii_cat *h = head;
	struct _sii_cat *min = head;

	while (h != NULL) {
		if (min->type > h->type)
			min = h;

		h = h->next;
	}

	return min;
}

static void sii_write(SiiInfo *sii, unsigned int add_pdo_mapping)
{
	unsigned char *outbuf = sii->rawbytes;
	uint8_t crc = 0xff;
    uint16_t skip_mask = SKIP_DC | SKIP_TXPDO | SKIP_RXPDO;
	if (add_pdo_mapping) {
		skip_mask &= ~(SKIP_TXPDO | SKIP_RXPDO);
	}


	// - write preamble
	struct _sii_preamble *pre = sii->preamble;
	*outbuf = pre->pdi_ctrl&0xff;
	crc8byte(&crc, *outbuf);
	outbuf++;
	*outbuf = (pre->pdi_ctrl>>8)&0xff;
	crc8byte(&crc, *outbuf);
	outbuf++;
	*outbuf = pre->pdi_conf&0xff;
	crc8byte(&crc, *outbuf);
	outbuf++;
	*outbuf = (pre->pdi_conf>>8)&0xff;
	crc8byte(&crc, *outbuf);
	outbuf++;
	*outbuf = pre->sync_impulse&0xff;
	crc8byte(&crc, *outbuf);
	outbuf++;
	*outbuf = (pre->sync_impulse>>8)&0xff;
	crc8byte(&crc, *outbuf);
	outbuf++;
	*outbuf = pre->pdi_conf2&0xff;
	crc8byte(&crc, *outbuf);
	outbuf++;
	*outbuf = (pre->pdi_conf2>>8)&0xff;
	crc8byte(&crc, *outbuf);
	outbuf++;
	*outbuf = pre->alias&0xff;
	crc8byte(&crc, *outbuf);
	outbuf++;
	*outbuf = (pre->alias>>8)&0xff;
	crc8byte(&crc, *outbuf);
	outbuf++;

	// reserved[4]; /* shall be zero */
	*outbuf = 0;
	crc8byte(&crc, *outbuf);
	outbuf++;
	*outbuf = 0;
	crc8byte(&crc, *outbuf);
	outbuf++;
	*outbuf = 0;
	crc8byte(&crc, *outbuf);
	outbuf++;
	*outbuf = 0;
	crc8byte(&crc, *outbuf);
	outbuf++;

	*outbuf = pre->checksum&0xff;
	crc8byte(&crc, *outbuf);
	outbuf++;
	*outbuf = (pre->checksum>>8)&0xff;
	crc8byte(&crc, *outbuf);
	outbuf++;

	/* checksum should be 0 now */
	if (crc != 0) {
		fprintf(stderr, "Error checksum mismatch - abort write operation.\n");
		return;
	}

	// - write standard config
	struct _sii_stdconfig *scfg = sii->config;

	*outbuf = scfg->vendor_id&0xff;
	outbuf++;
	*outbuf = (scfg->vendor_id>>8)&0xff;
	outbuf++;
	*outbuf = (scfg->vendor_id>>16)&0xff;
	outbuf++;
	*outbuf = (scfg->vendor_id>>24)&0xff;
	outbuf++;

	*outbuf = scfg->product_id&0xff;
	outbuf++;
	*outbuf = (scfg->product_id>>8)&0xff;
	outbuf++;
	*outbuf = (scfg->product_id>>16)&0xff;
	outbuf++;
	*outbuf = (scfg->product_id>>24)&0xff;
	outbuf++;

	*outbuf = scfg->revision_id&0xff;
	outbuf++;
	*outbuf = (scfg->revision_id>>8)&0xff;
	outbuf++;
	*outbuf = (scfg->revision_id>>16)&0xff;
	outbuf++;
	*outbuf = (scfg->revision_id>>24)&0xff;
	outbuf++;

	*outbuf = scfg->serial&0xff;
	outbuf++;
	*outbuf = (scfg->serial>>8)&0xff;
	outbuf++;
	*outbuf = (scfg->serial>>16)&0xff;
	outbuf++;
	*outbuf = (scfg->serial>>24)&0xff;
	outbuf++;

	//reserveda[8];
	for (int i=0; i<8; i++)
		*outbuf++ = 0;

	/* bootstrap mailbox settings */
	*outbuf = scfg->bs_rec_mbox_offset&0xff;
	outbuf++;
	*outbuf = (scfg->bs_rec_mbox_offset>>8)&0xff;
	outbuf++;
	*outbuf = scfg->bs_rec_mbox_size&0xff;
	outbuf++;
	*outbuf = (scfg->bs_rec_mbox_size>>8)&0xff;
	outbuf++;
	*outbuf = scfg->bs_snd_mbox_offset&0xff;
	outbuf++;
	*outbuf = (scfg->bs_snd_mbox_offset>>8)&0xff;
	outbuf++;
	*outbuf = scfg->bs_snd_mbox_size&0xff;
	outbuf++;
	*outbuf = (scfg->bs_snd_mbox_size>>8)&0xff;
	outbuf++;

	/* standard mailbox settings */
	*outbuf = scfg->std_rec_mbox_offset&0xff;
	outbuf++;
	*outbuf = (scfg->std_rec_mbox_offset>>8)&0xff;
	outbuf++;
	*outbuf = scfg->std_rec_mbox_size&0xff;
	outbuf++;
	*outbuf = (scfg->std_rec_mbox_size>>8)&0xff;
	outbuf++;
	*outbuf = scfg->std_snd_mbox_offset&0xff;
	outbuf++;
	*outbuf = (scfg->std_snd_mbox_offset>>8)&0xff;
	outbuf++;
	*outbuf = scfg->std_snd_mbox_size&0xff;
	outbuf++;
	*outbuf = (scfg->std_snd_mbox_size>>8)&0xff;
	outbuf++;

	*outbuf = scfg->mailbox_protocol.word&0xff;
	outbuf++;
	*outbuf = (scfg->mailbox_protocol.word>>8)&0xff;
	outbuf++;

	//reservedb[66]; /* shall be zero */
	for (int i=0; i<66; i++)
		*outbuf++ = 0x00;

	*outbuf = scfg->eeprom_size&0xff;
	outbuf++;
	*outbuf = (scfg->eeprom_size>>8)&0xff;
	outbuf++;
	*outbuf = scfg->version&0xff;
	outbuf++;
	*outbuf = (scfg->version>>8)&0xff;
	outbuf++;

	sii->rawsize = (size_t)(outbuf-sii->rawbytes);

#if DEBUG == 1
	printf("DEBUG sii_write() wrote %lu bytes for preamble and std config\n", sii->rawsize);
#endif

	// - iterate through categories

	size_t sz = sii_cat_write(sii, skip_mask);
	outbuf += sz;
	sii->rawsize += sz;

	/* add end indicator */
	*outbuf = 0xff;
	outbuf++;
	*outbuf = 0xff;
	sii->rawsize += 2;
}


/*****************/
/* API functions */
/*****************/

SiiInfo *sii_init(void)
{
	SiiInfo *sii = calloc(1, sizeof(SiiInfo));
	return sii;
}

SiiInfo *sii_init_string(const unsigned char *eeprom, size_t size)
{
	if (eeprom == NULL) {
		fprintf(stderr, "No eeprom provided\n");
		return NULL;
	}

	SiiInfo *sii = calloc(1, sizeof(SiiInfo));

	parse_content(sii, eeprom, size);

	return sii;
}

SiiInfo *sii_init_file(const char *filename)
{
	if (filename == NULL) {
		fprintf(stderr, "Error no filename provided\n");
		return NULL;
	}

	SiiInfo *sii = calloc(1, sizeof(SiiInfo));
	unsigned char eeprom[1024];

	read_eeprom(stdin, eeprom, 1024);
	parse_content(sii, eeprom, 1024);

	return sii;
}


void sii_release(SiiInfo *sii)
{
	while (cat_rm(sii) != 1)
		;

	if (sii->rawbytes != NULL)
		free(sii->rawbytes);

	if (sii->outfile != NULL)
		free(sii->outfile);

	if (sii->preamble != NULL)
		free(sii->preamble);

	if (sii->config != NULL)
		free(sii->config);

	free(sii);
}

size_t sii_generate(SiiInfo *sii, unsigned int add_pdo_mapping)
{
	size_t maxsize = EE_TO_BYTES(sii->config->eeprom_size);
	sii->rawbytes = (uint8_t*) calloc(1, maxsize);
	sii->rawsize = 0;

	sii_write(sii, add_pdo_mapping);
	sii->rawvalid = 1; /* FIXME valid should be set in sii_write */

	return sii->rawsize;
}

void sii_print(SiiInfo *sii)
{
	printf("First print preamble and config\n");
	struct _sii_preamble *preamble = sii->preamble;

	if (preamble != NULL) {
		/* preamble */
		printf("Preamble:\n");
		printf("PDI Control: ................ 0x%.4x\n", preamble->pdi_ctrl);
		printf("PDI Config: ................. 0x%.4x\n", preamble->pdi_conf);
		printf("Sync Impulse Length: ........ %d ns (raw: 0x%.4x)\n", preamble->sync_impulse*10, preamble->sync_impulse);
		printf("PDI Config 2: ............... 0x%.4x\n", preamble->pdi_conf2);
		printf("Configured Station Alias: ... 0x%.4x\n", preamble->alias);
		printf("Checksum of Preamble: ....... 0x%.4x (%s)\n", preamble->checksum, (preamble->checksum_ok ? "ok" : "wrong"));
	}

	struct _sii_stdconfig *stdc = sii->config;

	if (stdc != NULL) {
		/* general information */
		printf("Identity:\n");
		printf("  Vendor ID: ................ 0x%08x\n", stdc->vendor_id);
		printf("  Product ID: ............... 0x%08x\n", stdc->product_id);
		printf("  Revision ID: .............. 0x%08x\n", stdc->revision_id);
		printf("  Serial Number: ............ 0x%08x\n", stdc->serial);

		/* mailbox settings */
		printf("\nDefault mailbox settings:\n");
		printf("  Bootstrap Mailbox:\n");
		printf("  Received Mailbox Offset: .. 0x%04x\n", stdc->bs_rec_mbox_offset);
		printf("  Received Mailbox Size: .... %d\n", stdc->bs_rec_mbox_size);
		printf("  Send Mailbox Offset: ...... 0x%04x\n", stdc->bs_snd_mbox_offset);
		printf("  Send Mailbox Size: ........ %d\n", stdc->bs_snd_mbox_size);

		printf("  Mailbox Settings:\n");
		printf("  Received Mailbox Offset: .. 0x%04x\n", stdc->std_rec_mbox_offset);
		printf("  Received Mailbox Size: .... %d\n", stdc->std_rec_mbox_size);
		printf("  Send Mailbox Offset: ...... 0x%04x\n", stdc->std_snd_mbox_offset);
		printf("  Send Mailbox Size: ........ %d\n", stdc->std_snd_mbox_size);

		printf("\nSupported Mailboxes:\n");
		printf("  CoE ....................... %s\n", (stdc->mailbox_protocol.word&MBOX_COE) ? "True" : "False");
		printf("  EoE ....................... %s\n", (stdc->mailbox_protocol.word&MBOX_EOE) ? "True" : "False");
		printf("  FoE ....................... %s\n", (stdc->mailbox_protocol.word&MBOX_FOE) ? "True" : "False");
		printf("  SoE ....................... %s\n", (stdc->mailbox_protocol.word&MBOX_SOE) ? "True" : "False");
		printf("  VoE ....................... %s\n", (stdc->mailbox_protocol.word&MBOX_VOE) ? "True" : "False");
		printf("\n");

		printf("EEPROM size: ................ %d bytes\n", EE_TO_BYTES(stdc->eeprom_size));
		printf("Version: .................... %d\n", stdc->version);
		printf("\n");
	}

	/* now print the categories */
	cat_rewind(sii);
	struct _sii_cat *cats = sii->cat_current;

	while (cats != NULL) {
		cat_print(cats);
		cats = cat_next(sii);
	}
}

int sii_check(SiiInfo *sii)
{
	fprintf(stderr, "Not yet implemented\n");
	return sii->rawvalid;
}

int sii_write_bin(SiiInfo *sii, const char *outfile)
{
	if (!sii->rawvalid) {
		fprintf(stderr, "Error, raw string is invalid\n");
		return -1;
	}

	FILE *fh;

	if (outfile == NULL)
		fh = stdout;
	else {
		// FIXME Currently existing files are silently overwritten, should ask to perform the action!
		struct stat fs;
		if (!stat(outfile, &fs)) {
			fprintf(stderr, "Warning, existing file %s is overwritten\n", outfile);
		}

		fh = fopen(outfile, "w");
		if (fh == NULL) {
			fprintf(stderr, "Error open file '%s' for writing\n", outfile);
			return -2;
		}
	}

	for (size_t i=0; i<sii->rawsize; i++)
		fputc(sii->rawbytes[i], fh);

	fclose(fh);

	return 0;
}


int sii_add_info(SiiInfo *sii, struct _sii_preamble *pre, struct _sii_stdconfig *cfg)
{
	if (sii->preamble != NULL) {
		fprintf(stderr, "Warning, sii->preamble not empty.\n");
		return -1;
	}

	if (sii->config != NULL) {
		fprintf(stderr, "Warning, sii->config not empty.\n");
		return -1;
	}

	sii->preamble = pre;
	sii->config = cfg;

	return 0;
}

int sii_category_add(SiiInfo *sii, struct _sii_cat *cat)
{
	return cat_add(sii, cat);
}

struct _sii_cat *sii_category_find(SiiInfo *sii, enum eSection category)
{
	for (struct _sii_cat *c = sii->cat_head; c; c = c->next) {
		if (c->type == category)
			return c;
	}

	return NULL;
}

int sii_strings_add(SiiInfo *sii, const char *entry)
{
	struct _sii_cat *strings = sii_category_find(sii, SII_CAT_STRINGS);

	if (strings == NULL)
		return -1;

	if (strings->data == NULL)
		return -1;

	return strings_add((struct _sii_strings *)strings->data, entry);
}

int strings_add(struct _sii_strings *strings, const char *entry)
{
	strings_entry_add(strings, string_new(entry, strlen(entry)));

	return strings->count;
}

const char *string_search_id(struct _sii_strings *strings, int id)
{
	for (struct _string *s = strings->head; s; s = s->next) {
		if (s->id == id)
			return s->data;
	}

	return NULL;
}

int string_search_string(struct _sii_strings *strings, const char *str)
{
	for (struct _string *s = strings->head; s; s = s->next) {
		if (strncmp(s->data, str, strlen(str)) == 0)
			return s->id;
	}

	return -1;
}

char *cat2string(enum eSection cat)
{
	switch (cat) {
	case SII_PREAMBLE:
		return "SII_PREAMBLE";
	case SII_STD_CONFIG:
		return "SII_STD_CONFIG";
	case SII_CAT_STRINGS:
		return "SII_CAT_STRINGS";
	case SII_CAT_DATATYPES:
		return "SII_CAT_DATATYPES";
	case SII_CAT_GENERAL:
		return "SII_CAT_GENERAL";
	case SII_CAT_FMMU:
		return "SII_CAT_FMMU";
	case SII_CAT_SYNCM:
		return "SII_CAT_SYNCM";
	case SII_CAT_TXPDO:
		return "SII_CAT_TXPDO";
	case SII_CAT_RXPDO:
		return "SII_CAT_RXPDO";
	case SII_CAT_DCLOCK:
		return "SII_CAT_DCLOCK";
	case SII_CAT_NOP:
	default:
		return "undefined";
	}
}

void sii_cat_sort(SiiInfo *sii)
{
	struct _sii_cat *head = sii->cat_head;
	struct _sii_cat *min = NULL;
	struct _sii_cat *new = NULL;
	struct _sii_cat *current = NULL;

	while ( (min = sii_cat_get_min(head)) != NULL ) {
		if (min == head)
			head = head->next;

		// cut out current min node
		if (min->prev != NULL)
			min->prev->next = min->next;

		if (min->next != NULL)
			min->next->prev = min->prev;

		min->next = NULL;
		min->prev = NULL;

		// paste into new
		if (new == NULL) {
			new = min;
			current = new;
		} else {
			current->next = min;
			min->prev = current;
			min->next = NULL;
			current = current->next;
		}
	}

	sii->cat_head = new;
}

struct _sii_dclock *dclock_get_default(void)
{
	static struct _sii_dclock dcproto = {
		.reserved1 = 0, /* shall be zero */
		.cyclic_op_enabled = 0,
		.sync0_active = 0,
		.sync1_active = 0,
		.reserved2 = 0,
		.sync_pulse = 0,
		.int0_status = 0,
		.reserved4 = 0,
		.int1_status = 0,
		.reserved5 = 0,
		.cyclic_op_starttime = 0,
		.sync0_cycle_time = 0,
		.sync1_cycle_time = 0x10000,
		.latch0_pos_edge = 1,
		.latch0_neg_edge = 0,
		.reserved6 = 0,
		.latch1_pos_edge = 0,
		.latch1_neg_edge = 0,
		.reserved7 = 0,
		.latch0_pos_event = 0,
		.latch0_neg_event = 0,
		.reserved9 = 0,
		.latch1_pos_event = 0,
		.latch1_neg_event = 0,
		.reserved10 = 0,
		.latch0_pos_edge_value = 0,
		.latch0_neg_edge_value = 0x01000000,
		.latch1_pos_edge_value = 0x00060001,
		.latch1_neg_edge_value = 0,
	};

	struct _sii_dclock *dc = malloc(sizeof(struct _sii_dclock));
	memmove(dc, &dcproto, sizeof(struct _sii_dclock));

	return dc;
}
