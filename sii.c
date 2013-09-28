/* sii.c
 *
 * 2013 Frank Jeschke <fjeschke@synapticon.de>
 */

#include "sii.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#define BYTES_TO_WORD(x,y)          ((((int)y<<8)&0xff00) | (x&0xff))
#define BYTES_TO_DWORD(a,b,c,d)     ((unsigned int)(d&0xff)<<24)  | \
	                            ((unsigned int)(c&0xff)<<16) | \
				    ((unsigned int)(b&0xff)<<8)  | \
				     (unsigned int)(a&0xff)

#if 0 /* set in sii.h */
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
#endif

static char **strings; /* all strings */
static int g_print_offsets = 0;

/* category functions */
static struct _sii_cat *cat_new(uint16_t type, uint16_t size);
static void cat_data_cleanup(void *data, uint16_t type);
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
static void sii_cat_write(struct _sii *sii);
static void sii_write(SiiInfo *sii);

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
	struct _sii_preamble *preamble = malloc(sizeof(struct _sii_preamble));
	size_t count = 0;

	/* pdi control */
	preamble->pdi_ctrl = ((int)buffer[1]<<8) | buffer[0];

	/* pdi config */
	preamble->pdi_conf = ((int)buffer[3]<<8) | buffer[2];

	count = 4;

	/* sync impulse len (multiples of 10ns), stored as nano seconds */
	preamble->sync_impulse = BYTES_TO_WORD(buffer[count], buffer[count+1]) * 10;
	count+=2;

	/* PDI config 2 */
	preamble->pdi_conf2 = BYTES_TO_WORD(buffer[count], buffer[count+1]);
	count+=2;

	/* configured station alias */
	preamble->alias = BYTES_TO_WORD(buffer[count], buffer[count+1]);
	count+=2;

	count+=4; /* next 4 bytes are reserved */

	/* checksum FIXME add checksum test */
	preamble->checksum = BYTES_TO_WORD(buffer[count], buffer[count+1]);
	count += 2;

#if 0
	/* DEBUG print out */
	printf("Preamble:\n");
	printf("PDI Control: %.4x\n", preamble->pdi_ctrl);
	printf("PDI config: %.4x\n", preamble->pdi_conf);
	printf("Sync Impulse length = %d ns (raw: %.4x)\n", preamble->sync_impulse*10, preamble->sync_impulse);
	printf("PDI config 2: %.4x\n", preamble->pdi_conf2);
	printf("Configured station alias: %.4x\n", preamble->alias);
	printf("Checksum of preamble: %.4x\n", preamble->checksum);
#endif
	if (size != count)
		printf("%s: Warning counter differs from size\n", __func__);

	return preamble;
}

#if 0 /* defined in sii.h */
#define MBOX_EOE    0x0002
#define MBOX_COE    0x0004
#define MBOX_FOE    0x0008
#define MBOX_SOE    0x0010
#define MBOX_VOE    0x0020
#endif

static struct _sii_stdconfig *parse_stdconfig(const unsigned char *buffer, size_t size)
{
	size_t count =0;
	const unsigned char *b = buffer;
	struct _sii_stdconfig *stdc = malloc(sizeof(struct _sii_stdconfig));
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

#if 0
	/* DEBUG print */
	printf("General Information:\n");
	printf("Vendor ID: ....... 0x%08x\n", stdc->vendor_id);
	printf("Product ID: ...... 0x%08x\n", stdc->product_id);
	printf("Revision ID: ..... 0x%08x\n", stdc->revision_id);
	printf("Serial Number: ... 0x%08x\n", stdc->serial);

	/* mailbox settings */
	printf("\nDefault mailbox settings:\n");
	printf("Bootstrap received mailbox offset: 0x%04x\n", stdc->bs_rec_mbox_offset);
	printf("Bootstrap received mailbox size:   %d\n", stdc->bs_rec_mbox_size);
	printf("Bootstrap send mailbox offset:     0x%04x\n", stdc->bs_snd_mbox_offset);
	printf("Bootstrap send mailbox size:       %d\n", stdc->bs_snd_mbox_size);

	printf("Standard received mailbox offset:  0x%04x\n", stdc->std_rec_mbox_offset);
	printf("Standard received mailbox size:    %d\n", stdc->std_rec_mbox_size);
	printf("Standard send mailbox offset:      0x%04x\n", stdc->std_snd_mbox_offset);
	printf("Standard send mailbox size:        %d\n", stdc->std_snd_mbox_size);

	printf("\nSupported Mailboxes: ");
	if (stdc->mailbox_protocol.word&MBOX_EOE)
		printf("EoE, ");
	if (stdc->mailbox_protocol.word&MBOX_COE)
		printf("CoE, ");
	if (stdc->mailbox_protocol.word&MBOX_FOE)
		printf("FoE, ");
	if (stdc->mailbox_protocol.word&MBOX_SOE)
		printf("SoE, ");
	if (stdc->mailbox_protocol.word&MBOX_VOE)
		printf("VoE, ");
	printf("\n");

	printf("EEPROM size: %d kbit\n", stdc->eeprom_size);
	printf("Version: %d\n", stdc->version);
	printf("\n");
#endif
	count = b-buffer;
	if (size != count)
		printf("Warning counter differs from size\n");

	return stdc;
}

static char **parse_stringsection(const unsigned char *buffer, size_t size)
{
	const unsigned char *pos = buffer;
	unsigned index = 0;
	unsigned strcount = 0;
	char str[1024];
	size_t len = 0;
	memset(str, '\0', 1024);

	//printf("String section:\n");
	strcount = *pos++;
	//printf("Number of Strings: %d\n", strcount+1);

	strings = (char **)malloc((strcount+1) * sizeof(char *));

	for (index=0; index<strcount; index++) {
		len = *pos++;
		memmove(str, pos, len);
		pos += len;
		//printf("Index: %d, length: %u = '%s'\n", index, len, str);
		strings[index] = malloc(len+1);
		memmove(strings[index], str, len+1);
		memset(str, '\0', 1024);
	}

	strings[index] = NULL;

	if (size != strcount)
		printf("Warning counter differs from size\n");

	return strings;
}

static void parse_datatype_section(const unsigned char *buffer, size_t size)
{
	printf("\n+++ parsing of datatype section not yet implemented (first byte: 0x%.2x, size: %d)\n",
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
	}

	return NULL;
}

static struct _sii_general *parse_general_section(const unsigned char *buffer, size_t size)
{
	const unsigned char *b = buffer;
	struct _sii_general *siig = malloc(sizeof(struct _sii_general));

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

	siig->flag_safe_op = (*b & 0x01) == 0 ? 0 : 1;
	siig->flag_notLRW = (*b & 0x02) == 0 ? 0 : 1;
	b++;

	siig->current_ebus = BYTES_TO_WORD(*b, *(b+1));
	b+=2;
	b+=2;

	siig->phys_port_0 = *b&0x07;
	siig->phys_port_1 = (*b>>4)&0x7;
	b++;
	siig->phys_port_2 = *b&0x7;
	siig->phys_port_3 = (*b>>4)&0x7;
	b+=14;

#if 0
	/* DEBUG printout */
	printf("General:\n");

	printf("  Group Index: %d (Vendor Specific, index of Strings): %s\n", siig->groupindex, "tba");
	printf("  Image Index: %d (Vendor Specific, index to Strings): %s\n", siig->imageindex, "tba");
	printf("  Order Index: %d (Vendor Specific, index to Strings): %s\n", siig->orderindex, "tba");
	printf("  Name  Index: %d (Vendor Specific, index to Strings): %s\n", siig->nameindex, "tba");

	printf("  CoE Details:\n");
	printf("    Enable SDO: .................. %s\n", siig->coe_enable_sdo == 0 ? "no" : "yes");
	printf("    Enable SDO Info: ............. %s\n", siig->coe_enable_sdo_info == 0 ? "no" : "yes");
	printf("    Enable PDO Assign: ........... %s\n", siig->coe_enable_pdo_assign == 0 ? "no" : "yes");
	printf("    Enable PDO Configuration: .... %s\n", siig->coe_enable_pdo_conf == 0 ? "no" : "yes");
	printf("    Enable Upload at Startup: .... %s\n", siig->coe_enable_upload_start == 0 ? "no" : "yes");
	printf("    Enable SDO complete access: .. %s\n", siig->coe_enable_sdo_complete == 0 ? "no" : "yes");

	printf("  FoE Details: %s\n", siig->foe_enabled == 0 ? "not enabled" : "enabled");
	printf("  EoE Details: %s\n", siig->eoe_enabled == 0 ? "not enabled" : "enabled");


	printf("  Flag SafeOp: %s\n", siig->flag_safe_op == 0 ? "not enabled" : "enabled");
	printf("  Flag notLRW: %s\n", siig->flag_notLRW == 0 ? "not enabled" : "enabled");

	printf("  CurrentOnEBus: %d mA\n", siig->current_ebus);

	printf("  Physical Ports:\n");
	printf("     Port 0: %s\n", physport_type(siig->phys_port_0));
	printf("     Port 1: %s\n", physport_type(siig->phys_port_1));
	printf("     Port 2: %s\n", physport_type(siig->phys_port_2));
	printf("     Port 3: %s\n", physport_type(siig->phys_port_3));
#endif
	size_t count = b-buffer;
	if (size != count)
		printf("Warning counter differs from size\n");

	return siig;
}

static void fmmu_rm_entry(struct _sii_fmmu *fmmu)
{
	struct _fmmu_entry *fe = fmmu->list;
	while (fe->next != NULL)
		fe = fe->next;

	if (fe->prev != NULL) {
		fe->prev->next = NULL;
		fmmu->list = NULL;
	}
	free(fe);
}

static void fmmu_add_entry(struct _sii_fmmu *fmmu, int usage)
{
	struct _fmmu_entry *new = malloc(sizeof(struct _fmmu_entry));
	new->prev = NULL;
	new->next = NULL;
	new->id = -1;
	new->usage = usage;

	if (fmmu->list == NULL) {
		new->id = 0;
		fmmu->list = new;
	} else {
		struct _fmmu_entry *f = fmmu->list;
		while (f->next != NULL)
			f = f->next;

		f->next = new;
		new->prev = f;
		new->id = f->id+1;
	}
}

static struct _sii_fmmu *parse_fmmu_section(const unsigned char *buffer, size_t secsize)
{
	//int fmmunbr = 0;
	//size_t count=0;
	const unsigned char *b = buffer;

	struct _sii_fmmu *fmmu = malloc(sizeof(struct _sii_fmmu));
	fmmu->count = 0;
	fmmu->list = NULL;

	printf("FMMU Settings:\n");
	while ((unsigned int)(b-buffer)<secsize) {
		//printf("  FMMU%d: ", fmmunbr++);
		fmmu_add_entry(fmmu, *b);
		/*
		switch (*b) {
		case 0x00:
		case 0xff:
			printf("not used\n");
			break;
		case 0x01:
			printf("used for Outputs\n");
			break;
		case 0x02:
			printf("used for Inputs\n");
			break;
		case 0x03:
			printf("used for SyncM status\n");
			break;
		default:
			printf("WARNING: undefined behavior\n");
			break;
		}
		*/
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

	if (entry->prev != NULL) {
		entry->prev->next = NULL;
		sm->list = NULL;
	}

	free(entry);
}

static void syncm_add_entry(struct _sii_syncm *sm,
		int phys_address, int length, int control, int status, int enable, int type)
{
	struct _syncm_entry *entry = malloc(sizeof(struct _syncm_entry));
	entry->id = -1;
	entry->next = NULL;
	entry->prev = NULL;
	entry->phys_address = phys_address;
	entry->length = length;
	entry->control = control;
	entry->status = status;
	entry->enable = enable;
	entry->type = type;

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

	struct _sii_syncm *sm = malloc(sizeof(struct _sii_syncm));
	sm->list = NULL;
	sm->count = 0;

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

#if 0
		printf("SyncManager SM%d\n", smnbr);
		printf("  Physical Startaddress: 0x%04x\n", physadr);
		printf("  Length: %d\n", length);
		printf("  Control Register: 0x%02x\n", control);
		printf("  Status Register: 0x%02x\n", status);
		printf("  Enable byte: 0x%02x\n", enable);
		printf("  SM Type: ");
		switch (type) {
		case 0x00:
			printf("not used or unknown\n");
			break;
		case 0x01:
			printf("Mailbox Out\n");
			break;
		case 0x02:
			printf("Mailbox In\n");
			break;
		case 0x03:
			printf("Process Data Out\n");
			break;
		case 0x04:
			printf("Process Data In\n");
			break;
		default:
			printf("undefined\n");
			break;
		}
#endif
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

static void pdo_add_entry(struct _sii_pdo *pdo,
		int index, int subindex, int string_index, int data_type,
		int bit_length, int flags)
{
	struct _pdo_entry *entry = malloc(sizeof(struct _pdo_entry));

	entry->id = -1;
	entry->index = index;
	entry->subindex = subindex;
	entry->string_index = string_index;
	entry->data_type = data_type;
	entry->bit_length = bit_length;
	entry->flags = flags;
	entry->next = NULL;
	entry->prev = NULL;

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
	char *pdostr;
	//int pdonbr=0;
	//int entries = 0;
	int entry = 0;

	struct _sii_pdo *pdo = malloc(sizeof(struct _sii_pdo));

	switch (t) {
	case RxPDO:
		pdo->type = SII_RX_PDO;
		pdostr = "RxPDO";
		break;
	case TxPDO:
		pdo->type = SII_TX_PDO;
		pdostr = "TxPDO";
		break;
	default:
		pdo->type = SII_UNDEF_PDO;
		pdostr = "undefined";
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

#if 0
	printf("%s%d:\n", pdostr, pdonbr);
	printf("  PDO Index: 0x%04x\n", pdo->index);
	printf("  Entries: %d\n", pdo->entries);
	printf("  SyncM: %d\n", pdo->syncmanager);
	printf("  Synchronization: 0x%02x\n", pdo->dcsync);
	printf("  Name Index: %d\n", pdo->name_index);
	printf("  Flags for future use: 0x%04x\n", pdo->flags);
#endif

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

#if 0
		printf("\n    Entry %d:\n", entry);
		printf("    Entry Index: 0x%04x\n", index);
		printf("    Subindex: 0x%02x\n", subindex);
		printf("    String Index: %d (%s)\n", string_index, strings[string_index]);
		printf("    Data Type: 0x%02x (Index in CoE Object Dictionary)\n", data_type);
		printf("    Bitlength: %d\n", bit_length);
		printf("     Flags (for future use): 0x%04x\n", flags);
#endif
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
	const unsigned char *b = buffer+1; /* first byte is reserved */

	struct _sii_dclock *dc = malloc(sizeof(struct _sii_dclock));

	//printf("DC Sync Parameter\n");

	dc->cyclic_op_enabled = (*b & 0x01) == 0 ? 0 : 1;
	dc->sync0_active = (*b & 0x02) == 0 ? 0 : 1;
	dc->sync1_active = (*b & 0x04) == 0 ? 0 : 1;
	b++; /* next 5 bit reserved */

	dc->sync_pulse = BYTES_TO_WORD(*b, *(b+1));
	b+=2;

	b+=10; /* skipped reserved */

	dc->int0_status = (*b & 0x01) == 0 ? 0 : 1;
	b++;
	dc->int1_status = (*b & 0x01) == 0 ? 0 : 1;
	b++;
	b+=12; /* skipped reserved */

	dc->cyclic_op_starttime = BYTES_TO_DWORD(*b, *(b+1), *(b+2), *(b+3));
	b+=4;
	dc->sync0_cycle_time = BYTES_TO_DWORD(*b, *(b+1), *(b+2), *(b+3));
	b+=4;
	dc->sync1_cycle_time = BYTES_TO_DWORD(*b, *(b+1), *(b+2), *(b+3));
	b+=4;

	dc->latch0_pos_edge = (*b & 0x01) == 0 ? 0 : 1;
	dc->latch0_neg_edge = (*b & 0x02) == 0 ? 0 : 1;
	b+=2;

	dc->latch1_pos_edge = (*b & 0x01) == 0 ? 0 : 1;
	dc->latch1_neg_edge = (*b & 0x02) == 0 ? 0 : 1;
	b+=2;
	b+=4; /* another reserved block */

	dc->latch0_pos_event = (*b & 0x01) == 0 ? 0 : 1;
	dc->latch0_neg_event = (*b & 0x02) == 0 ? 0 : 1;
	b+=1; /* the follwing 14 bits are reserved */

	dc->latch1_pos_event = (*b & 0x01) == 0 ? 0 : 1;
	dc->latch1_neg_event = (*b & 0x02) == 0 ? 0 : 1;
	b+=1; /* the follwing 14 bits are reserved */

	dc->latch0_pos_edge_value = BYTES_TO_DWORD(*b, *(b+1), *(b+2), *(b+3));
	b+=4;
	b+=4;

	dc->latch0_neg_edge_value = BYTES_TO_DWORD(*b, *(b+1), *(b+2), *(b+3));
	b+=4;
	b+=4;

	dc->latch1_pos_edge_value = BYTES_TO_DWORD(*b, *(b+1), *(b+2), *(b+3));
	b+=4;
	b+=4;

	dc->latch1_neg_edge_value = BYTES_TO_DWORD(*b, *(b+1), *(b+2), *(b+3));
	b+=4;
	b+=4;

#if 0
	/* DEBUG printout */
	printf("  Cyclic Operation Enable: %s\n", dc->cyclic_op_enabled == 0 ? "no" : "yes");
	printf("  SYNC0 activate: %s\n", dc->sync0_active == 0 ? "no" : "yes");
	printf("  SYNC1 activate: %s\n", dc->sync1_active == 0 ? "no" : "yes");

	printf("  SYNC Pulse: %d (ns?)\n", dc->sync_pulse);

	printf("  Interrupt 0 Status: %s\n",  dc->int0_status == 0 ? "not active" : "active");
	printf("  Interrupt 1 Status: %s\n",  dc->int1_status == 0 ? "not active" : "active");

	printf("  Cyclic Operation Startime: %d ns\n", dc->cyclic_op_starttime);
	printf("  SYNC0 Cycle Time: %d (ns?)\n", dc->sync0_cycle_time);
	printf("  SYNC0 Cycle Time: %d (ns?)\n", dc->sync1_cycle_time);

	printf("\nLatch Description\n");
	printf("  Latch 0 PosEdge: %s\n", dc->latch0_pos_edge == 0 ? "continous" : "single");
	printf("  Latch 0 NegEdge: %s\n", dc->latch0_neg_edge == 0 ? "continous" : "single");

	printf("  Latch 1 PosEdge: %s\n", dc->latch1_pos_edge == 0 ? "continous" : "single");
	printf("  Latch 1 NegEdge: %s\n", dc->latch1_neg_edge == 0 ? "continous" : "single");

	printf("  Latch 0 PosEvnt: %s\n", dc->latch0_pos_event == 0 ? "no Event" : "Event stored");
	printf("  Latch 0 NegEvnt: %s\n", dc->latch0_neg_event == 0 ? "no Event" : "Event stored");

	printf("  Latch 1 PosEvnt: %s\n", dc->latch1_pos_event == 0 ? "no Event" : "Event stored");
	printf("  Latch 1 NegEvnt: %s\n", dc->latch1_neg_event == 0 ? "no Event" : "Event stored");

	printf("  Latch0PosEdgeValue: 0x%08x\n", dc->latch0_pos_edge_value);
	printf("  Latch0NegEdgeValue: 0x%08x\n", dc->latch0_neg_edge_value);
	printf("  Latch1PosEdgeValue: 0x%08x\n", dc->latch1_pos_edge_value);
	printf("  Latch1NegEdgeValue: 0x%08x\n", dc->latch1_neg_edge_value);
#endif
	size_t count = b-buffer;
	if (size != count)
		printf("Warning counter differs from size\n");

	return dc;
}

static void print_offsets(const unsigned char *start, const unsigned char *current)
{
	if (!g_print_offsets) {
		printf("\n");
		return;
	}

	printf("\n[Offset: 0x%0x (%d)] ", current-start, current-start);
}

static int parse_content(struct _sii *sii, const unsigned char *eeprom, size_t maxsize)
{
	enum eSection section = SII_PREAMBLE;
	//size_t count = 0;
	size_t secsize = 0;
	const unsigned char *buffer = eeprom;
	const unsigned char *secstart = eeprom;

	struct _sii_cat *newcat;

	//struct _sii_preamble *preamble;
	//struct _sii_stdconfig *stdconfig;
	char **strings;
	struct _sii_general *general;
	struct _sii_fmmu *fmmu;
	struct _sii_syncm *syncmanager;
	struct _sii_pdo *txpdo;
	struct _sii_pdo *rxpdo;
	struct _sii_dclock *distributedclock;

	while (1) {
		print_offsets(eeprom, secstart);
		switch (section) {
		case SII_CAT_NOP:
			break;

		case SII_PREAMBLE:
			sii->preamble = parse_preamble(buffer, 16);
			buffer = eeprom+16;
			secstart = buffer;
			section = SII_STD_CONFIG;
			break;

		case SII_STD_CONFIG:
			printf("Print std config:\n");
			sii->config = parse_stdconfig(buffer, 46+66);
			buffer = buffer+46+66;
			secstart = buffer;
			section = get_next_section(buffer, &secsize);
			buffer += 4;
			break;

		case SII_CAT_STRINGS:
			newcat = cat_new((uint16_t)(section&0xffff), (uint16_t)(secsize&0xffff));
			strings = parse_stringsection(buffer, secsize);
			newcat->data = (void *)strings;
			cat_add(sii, newcat);
			printf("DEBUG Added string section\n");

			buffer+=secsize;
			secstart = buffer;
			section = get_next_section(buffer, &secsize);
			buffer+=4;
			break;

		case SII_CAT_DATATYPES:
			parse_datatype_section(buffer, secsize);
			buffer+=secsize;
			secstart = buffer;
			section = get_next_section(buffer, &secsize);
			buffer+=4;
			break;

		case SII_CAT_GENERAL:
			newcat = cat_new((uint16_t)(section&0xffff), (uint16_t)(secsize&0xffff));
			general = parse_general_section(buffer, secsize);
			newcat->data = (void *)general;
			cat_add(sii, newcat);
			printf("DEBUG Added general section\n");

			buffer+=secsize;
			secstart = buffer;
			section = get_next_section(buffer, &secsize);
			buffer+=4;
			break;

		case SII_CAT_FMMU:
			newcat = cat_new((uint16_t)(section&0xffff), (uint16_t)(secsize&0xffff));
			fmmu = parse_fmmu_section(buffer, secsize);
			newcat->data = (void *)fmmu;
			cat_add(sii, newcat);
			printf("DEBUG Added fmmu section\n");

			buffer+=secsize;
			secstart = buffer;
			section = get_next_section(buffer, &secsize);
			buffer+=4;
			break;

		case SII_CAT_SYNCM:
			newcat = cat_new((uint16_t)(section&0xffff), (uint16_t)(secsize&0xffff));
			syncmanager = parse_syncm_section(buffer, secsize);
			newcat->data = (void *)syncmanager;
			cat_add(sii, newcat);
			printf("DEBUG Added syncm section\n");

			buffer+=secsize;
			secstart = buffer;
			section = get_next_section(buffer, &secsize);
			buffer+=4;
			break;

		case SII_CAT_TXPDO:
			newcat = cat_new((uint16_t)(section&0xffff), (uint16_t)(secsize&0xffff));
			txpdo = parse_pdo_section(buffer, secsize, TxPDO);
			newcat->data = (void *)txpdo;
			cat_add(sii, newcat);
			printf("DEBUG Added txpdo section\n");

			buffer+=secsize;
			secstart = buffer;
			section = get_next_section(buffer, &secsize);
			buffer+=4;
			break;

		case SII_CAT_RXPDO:
			newcat = cat_new((uint16_t)(section&0xffff), (uint16_t)(secsize&0xffff));
			rxpdo = parse_pdo_section(buffer, secsize, RxPDO);
			newcat->data = (void *)rxpdo;
			cat_add(sii, newcat);
			printf("DEBUG Added rxpdo section\n");

			buffer+=secsize;
			secstart = buffer;
			section = get_next_section(buffer, &secsize);
			buffer+=4;
			break;

		case SII_CAT_DCLOCK:
			newcat = cat_new((uint16_t)(section&0xffff), (uint16_t)(secsize&0xffff));
			distributedclock = parse_dclock_section(buffer, secsize);
			newcat->data = (void *)distributedclock;
			cat_add(sii, newcat);
			printf("DEBUG Added dclock section\n");

			buffer+=secsize;
			secstart = buffer;
			section = get_next_section(buffer, &secsize);
			buffer+=4;
			break;

		case SII_END:
			goto finish;
			break;
		}
	}

	if ((size_t)(buffer-eeprom) > maxsize)
		printf("Warning, read more bytes than are available\n");
finish:

	return 0;
}

/*** categroy list handling ***/

static void cat_data_cleanup_fmmu(struct _sii_fmmu *fmmu)
{
	printf("FIXME: implement fmmu cleanup\n");

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

static void free_strings(char **strings)
{
	int i = 0;
	while (strings[i] == NULL) {
		free(strings[i]);
		i++;
	}

	free(strings);
}

static void cat_data_cleanup(void *data, uint16_t type)
{
	/* clean up type specific data */
	switch (type) {
	case SII_CAT_STRINGS:
		free_strings((char **) data);
		break;

	case SII_CAT_DATATYPES:
		fprintf(stderr, "The Datatype categroie isn't implemented.\n");
		break;

	case SII_CAT_GENERAL:
		free(data); /* section general is simple */
		break;

	case SII_CAT_FMMU:
		cat_data_cleanup_fmmu((struct _sii_fmmu *)data);
		break;

	case SII_CAT_SYNCM:
		cat_data_cleanup_syncm((struct _sii_syncm *)data);
		break;

	case SII_CAT_TXPDO:
	case SII_CAT_RXPDO:
		cat_data_cleanup_pdo((struct _sii_pdo *)data);
		break;

	case SII_CAT_DCLOCK:
		free(data); /* section dc is simple */
		break;

	default:
		printf("Warning: cleanup received unknown category\n");
		break;
	}
}

static struct _sii_cat *cat_new(uint16_t type, uint16_t size)
{
	struct _sii_cat *new = malloc(sizeof(struct _sii_cat));

	new->type   = type&0x7fff;
	new->vendor = (type>>16)&0x1;
	new->size   = size;
	new->data   = NULL;
	new->next   = NULL;
	new->prev   = NULL;

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

	cat_data_cleanup(curr->data, curr->type);
	curr->data = NULL; /* FIXME lost pointer */
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

static void cat_print(struct _sii_cat *cat)
{
	/* preamble and std config should printed here */
	printf("Print categorie: 0x%x\n", cat->type);
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
	printf("printing categorie strings (0x%x size: %d)\n",
			cat->type, cat->size);

	char **strings = (char **)cat->data;

	int i=0;
	while (strings[i] != NULL) {
		printf("%i: %s\n", i, strings[i]);
		i++;
	}
}

static void cat_print_datatypes(struct _sii_cat *cat)
{
	printf("printing categorie datatypes (0x%x size: %d) - not yet implemented\n",
			cat->type, cat->size);
}

static void cat_print_general(struct _sii_cat *cat)
{
	printf("Printing Categorie General 0x%x (byte size: %d)\n", cat->type, cat->size);
	struct _sii_general *gen = (struct _sii_general *)cat->data;

	//printf("General:\n");

	printf("  Group Index: %d (Vendor Specific, index of Strings): %s\n", gen->groupindex, "tba");
	printf("  Image Index: %d (Vendor Specific, index to Strings): %s\n", gen->imageindex, "tba");
	printf("  Order Index: %d (Vendor Specific, index to Strings): %s\n", gen->orderindex, "tba");
	printf("  Name  Index: %d (Vendor Specific, index to Strings): %s\n", gen->nameindex, "tba");

	printf("  CoE Details:\n");
	printf("    Enable SDO: .................. %s\n", gen->coe_enable_sdo == 0 ? "no" : "yes");
	printf("    Enable SDO Info: ............. %s\n", gen->coe_enable_sdo_info == 0 ? "no" : "yes");
	printf("    Enable PDO Assign: ........... %s\n", gen->coe_enable_pdo_assign == 0 ? "no" : "yes");
	printf("    Enable PDO Configuration: .... %s\n", gen->coe_enable_pdo_conf == 0 ? "no" : "yes");
	printf("    Enable Upload at Startup: .... %s\n", gen->coe_enable_upload_start == 0 ? "no" : "yes");
	printf("    Enable SDO complete access: .. %s\n", gen->coe_enable_sdo_complete == 0 ? "no" : "yes");

	printf("  FoE Details: %s\n", gen->foe_enabled == 0 ? "not enabled" : "enabled");
	printf("  EoE Details: %s\n", gen->eoe_enabled == 0 ? "not enabled" : "enabled");


	printf("  Flag SafeOp: %s\n", gen->flag_safe_op == 0 ? "not enabled" : "enabled");
	printf("  Flag notLRW: %s\n", gen->flag_notLRW == 0 ? "not enabled" : "enabled");

	printf("  CurrentOnEBus: %d mA\n", gen->current_ebus);

	printf("  Physical Ports:\n");
	printf("     Port 0: %s\n", physport_type(gen->phys_port_0));
	printf("     Port 1: %s\n", physport_type(gen->phys_port_1));
	printf("     Port 2: %s\n", physport_type(gen->phys_port_2));
	printf("     Port 3: %s\n", physport_type(gen->phys_port_3));
}

static void cat_print_fmmu(struct _sii_cat *cat)
{
	printf("printing categorie fmmu (0x%x size: %d)\n",
			cat->type, cat->size);

	struct _sii_fmmu *fmmus = cat->data;
	printf("Number of FMMUs: %d\n", fmmus->count);

	struct _fmmu_entry *fmmu = fmmus->list;

	while (fmmu != NULL) {
		printf("  FMMU%d: ", fmmu->id);
		switch (fmmu->usage) {
		case 0x00:
		case 0xff:
			printf("not used\n");
			break;
		case 0x01:
			printf("used for Outputs\n");
			break;
		case 0x02:
			printf("used for Inputs\n");
			break;
		case 0x03:
			printf("used for SyncM status\n");
			break;
		default:
			printf("WARNING: undefined behavior\n");
			break;
		}

		fmmu = fmmu->next;
	}
}

static void cat_print_syncm_entries(struct _syncm_entry *sme)
{
	struct _syncm_entry *e = sme;
	int smnbr = 0;

	while (e != NULL) {
		printf("SyncManager SM%d\n", smnbr);
		printf("  Physical Startaddress: 0x%04x\n", e->phys_address);
		printf("  Length: %d\n", e->length);
		printf("  Control Register: 0x%02x\n", e->control);
		printf("  Status Register: 0x%02x\n", e->status);
		printf("  Enable byte: 0x%02x\n", e->enable);
		printf("  SM Type: ");
		switch (e->type) {
		case 0x00:
			printf("not used or unknown\n");
			break;
		case 0x01:
			printf("Mailbox Out\n");
			break;
		case 0x02:
			printf("Mailbox In\n");
			break;
		case 0x03:
			printf("Process Data Out\n");
			break;
		case 0x04:
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
	printf("printing categorie syncm (0x%x size: %d)\n",
			cat->type, cat->size);

	struct _sii_syncm *sm = (struct _sii_syncm *)cat->data;

	printf("Number of SyncManager: %d\n", sm->count);
	cat_print_syncm_entries(sm->list);
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
	printf("printing categorie rx-/txpdo (0x%x size: %d)\n",
			cat->type, cat->size);

	struct _sii_pdo *pdo = (struct _sii_pdo *)cat->data;

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
	printf("%s:\n", pdostr);
	printf("  PDO Index: 0x%04x\n", pdo->index);
	printf("  Entries: %d\n", pdo->entries);
	printf("  SyncM: %d\n", pdo->syncmanager);
	printf("  Synchronization: 0x%02x\n", pdo->dcsync);
	printf("  Name Index: %d\n", pdo->name_index);
	printf("  Flags for future use: 0x%04x\n", pdo->flags);

	struct _pdo_entry *list = pdo->list;
	while (list != NULL) {
		printf("\n    Entry %d:\n", list->id);
		printf("    Entry Index: 0x%04x\n", list->index);
		printf("    Subindex: 0x%02x\n", list->subindex);
		printf("    String Index: %d (%s)\n", list->string_index, "*unavailable*");
		printf("    Data Type: 0x%02x (Index in CoE Object Dictionary)\n", list->data_type);
		printf("    Bitlength: %d\n", list->bit_length);
		printf("     Flags (for future use): 0x%04x\n", list->flags);

		list = list->next;
	}
}


static void cat_print_dc(struct _sii_cat *cat)
{
	printf("printing categorie distributed clock (dc) (0x%x size: %d)\n",
			cat->type, cat->size);
	struct _sii_dclock *dc = (struct _sii_dclock *)cat->data;

	printf("  Cyclic Operation Enable: %s\n", dc->cyclic_op_enabled == 0 ? "no" : "yes");
	printf("  SYNC0 activate: %s\n", dc->sync0_active == 0 ? "no" : "yes");
	printf("  SYNC1 activate: %s\n", dc->sync1_active == 0 ? "no" : "yes");

	printf("  SYNC Pulse: %d (ns?)\n", dc->sync_pulse);

	printf("  Interrupt 0 Status: %s\n",  dc->int0_status == 0 ? "not active" : "active");
	printf("  Interrupt 1 Status: %s\n",  dc->int1_status == 0 ? "not active" : "active");

	printf("  Cyclic Operation Startime: %d ns\n", dc->cyclic_op_starttime);
	printf("  SYNC0 Cycle Time: %d (ns?)\n", dc->sync0_cycle_time);
	printf("  SYNC0 Cycle Time: %d (ns?)\n", dc->sync1_cycle_time);

	printf("\nLatch Description\n");
	printf("  Latch 0 PosEdge: %s\n", dc->latch0_pos_edge == 0 ? "continous" : "single");
	printf("  Latch 0 NegEdge: %s\n", dc->latch0_neg_edge == 0 ? "continous" : "single");

	printf("  Latch 1 PosEdge: %s\n", dc->latch1_pos_edge == 0 ? "continous" : "single");
	printf("  Latch 1 NegEdge: %s\n", dc->latch1_neg_edge == 0 ? "continous" : "single");

	printf("  Latch 0 PosEvnt: %s\n", dc->latch0_pos_event == 0 ? "no Event" : "Event stored");
	printf("  Latch 0 NegEvnt: %s\n", dc->latch0_neg_event == 0 ? "no Event" : "Event stored");

	printf("  Latch 1 PosEvnt: %s\n", dc->latch1_pos_event == 0 ? "no Event" : "Event stored");
	printf("  Latch 1 NegEvnt: %s\n", dc->latch1_neg_event == 0 ? "no Event" : "Event stored");

	printf("  Latch0PosEdgeValue: 0x%08x\n", dc->latch0_pos_edge_value);
	printf("  Latch0NegEdgeValue: 0x%08x\n", dc->latch0_neg_edge_value);
	printf("  Latch1PosEdgeValue: 0x%08x\n", dc->latch1_pos_edge_value);
	printf("  Latch1NegEdgeValue: 0x%08x\n", dc->latch1_neg_edge_value);
}

/* write sii binary data */

static uint16_t sii_cat_write_strings(struct _sii_cat *cat, unsigned char *buf)
{
	unsigned char *b = buf;
	printf("TODO: binary write of string section (0x%x)\n", cat->type);
	// FIXME check what ist the string separator
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
	unsigned char *b = buf;
	printf("TODO: binary write of general section (0x%x)\n", cat->type);
	return (uint16_t)(b-buf);
}

static uint16_t sii_cat_write_fmmu(struct _sii_cat *cat, unsigned char *buf)
{
	unsigned char *b = buf;
	printf("TODO: binary write of fmmu section (0x%x)\n", cat->type);
	return (uint16_t)(b-buf);
}

static uint16_t sii_cat_write_syncm(struct _sii_cat *cat, unsigned char *buf)
{
	unsigned char *b = buf;
	printf("TODO: binary write of syncm section (0x%x)\n", cat->type);
	return (uint16_t)(b-buf);
}

static uint16_t sii_cat_write_pdo(struct _sii_cat *cat, unsigned char *buf)
{
	unsigned char *b = buf;
	printf("TODO: binary write of pdo section (0x%x)\n", cat->type);
	return (uint16_t)(b-buf);
}

static uint16_t sii_cat_write_dc(struct _sii_cat *cat, unsigned char *buf)
{
	unsigned char *b = buf;
	printf("TODO: binary write of dc section (0x%x)\n", cat->type);
	return (uint16_t)(b-buf);
}

static void sii_cat_write(struct _sii *sii)
{
	unsigned char *buf = sii->rawbytes;
	size_t *bufsize = &sii->rawsize;
	struct _sii_cat *cat = sii->cat_head;
	uint16_t catsize = 0;

	// for each category:
	// buf[0], buf[1] <- type
	// buf[2], buf[3] <- size
	// buf[N>3] category data

	while (cat != NULL) {

		switch (cat->type) {
		case SII_CAT_STRINGS:
			*buf = cat->type&0xff;
			buf++;
			*buf = (cat->type>>8)&0xff;
			buf++;

			catsize = sii_cat_write_strings(cat, buf);
			buf += catsize;
			*buf = catsize&0xff;
			buf++;
			*buf = (catsize>>8)&0xff;
			catsize = 0;
			break;

		case SII_CAT_DATATYPES:
			*buf = cat->type&0xff;
			buf++;
			*buf = (cat->type>>8)&0xff;
			buf++;

			catsize = sii_cat_write_datatypes(cat, buf);
			buf += catsize;
			*buf = catsize&0xff;
			buf++;
			*buf = (catsize>>8)&0xff;
			catsize = 0;
			break;

		case SII_CAT_GENERAL:
			*buf = cat->type&0xff;
			buf++;
			*buf = (cat->type>>8)&0xff;
			buf++;

			catsize = sii_cat_write_general(cat, buf);
			buf += catsize;
			*buf = catsize&0xff;
			buf++;
			*buf = (catsize>>8)&0xff;
			catsize = 0;
			break;

		case SII_CAT_FMMU:
			*buf = cat->type&0xff;
			buf++;
			*buf = (cat->type>>8)&0xff;
			buf++;

			catsize = sii_cat_write_fmmu(cat, buf);
			buf += catsize;
			*buf = catsize&0xff;
			buf++;
			*buf = (catsize>>8)&0xff;
			catsize = 0;
			break;

		case SII_CAT_SYNCM:
			*buf = cat->type&0xff;
			buf++;
			*buf = (cat->type>>8)&0xff;
			buf++;

			catsize = sii_cat_write_syncm(cat, buf);
			buf += catsize;
			*buf = catsize&0xff;
			buf++;
			*buf = (catsize>>8)&0xff;
			catsize = 0;
			break;

		case SII_CAT_TXPDO:
		case SII_CAT_RXPDO:
			*buf = cat->type&0xff;
			buf++;
			*buf = (cat->type>>8)&0xff;
			buf++;

			catsize = sii_cat_write_pdo(cat, buf);
			buf += catsize;
			*buf = catsize&0xff;
			buf++;
			*buf = (catsize>>8)&0xff;
			catsize = 0;
			break;

		case SII_CAT_DCLOCK:
			*buf = cat->type&0xff;
			buf++;
			*buf = (cat->type>>8)&0xff;
			buf++;

			catsize = sii_cat_write_dc(cat, buf);
			buf += catsize;
			*buf = catsize&0xff;
			buf++;
			*buf = (catsize>>8)&0xff;
			catsize = 0;
			break;

		default:
			printf("Not yet available\n");
			break;
		}

		cat = cat->next;
	}
}

static void sii_write(SiiInfo *sii)
{
	unsigned char *outbuf = sii->rawbytes;

	// - write preamble
	struct _sii_preamble *pre = sii->preamble;
	*outbuf = pre->pdi_ctrl&0xff;
	outbuf++;
	*outbuf = (pre->pdi_ctrl>>8)&0xff;
	outbuf++;
	*outbuf = pre->pdi_conf&0xff;
	outbuf++;
	*outbuf = (pre->pdi_conf>>8)&0xff;
	outbuf++;
	*outbuf = pre->sync_impulse&0xff;
	outbuf++;
	*outbuf = (pre->sync_impulse>>8)&0xff;
	outbuf++;
	*outbuf = pre->pdi_conf2&0xff;
	outbuf++;
	*outbuf = (pre->pdi_conf2>>8)&0xff;
	outbuf++;
	*outbuf = pre->alias&0xff;
	outbuf++;
	*outbuf = (pre->alias>>8)&0xff;
	outbuf++;

	// reserved[4]; /* shall be zero */
	*outbuf = 0;
	outbuf++;
	*outbuf = 0;
	outbuf++;
	*outbuf = 0;
	outbuf++;
	*outbuf = 0;
	outbuf++;

	/* FIXME calculate checksum */
	*outbuf = pre->checksum&0xff;
	outbuf++;
	*outbuf = (pre->checksum>>8)&0xff;
	outbuf++;

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

	//reserveda[8]; /* shall be zero */
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

	// - iterate through categories

	sii->rawsize = (size_t)(outbuf-sii->rawbytes);
	printf("DEBUG sii_write() wrote %d bytes for preamble and std config\n", sii->rawsize);
	sii_cat_write(sii);
}


/*****************/
/* API functions */
/*****************/

SiiInfo *sii_init(void)
{
	SiiInfo *sii = malloc(sizeof(SiiInfo));

	sii->cat_head = NULL;
	sii->cat_current = NULL;
	sii->rawbytes = NULL;
	sii->rawvalid = 0;

	return sii;
}

SiiInfo *sii_init_string(const unsigned char *eeprom, size_t size)
{
	SiiInfo *sii = malloc(sizeof(SiiInfo));

	if (eeprom == NULL) {
		fprintf(stderr, "No eeprom provided\n");
		free(sii);
		return NULL;
	}

	sii->cat_head = NULL;
	sii->cat_current = NULL;
	sii->rawbytes = NULL;
	sii->rawvalid = 0;

	parse_content(sii, eeprom, size);

	return sii;
}

SiiInfo *sii_init_file(const char *filename)
{
	SiiInfo *sii = malloc(sizeof(SiiInfo));
	unsigned char eeprom[1024];

	if (filename != NULL)
		read_eeprom(stdin, eeprom, 1024);
	else {
		fprintf(stderr, "Error no filename provided\n");
		free(sii);
		return NULL;
	}

	sii->cat_head = NULL;
	sii->cat_current = NULL;
	sii->rawbytes = NULL;
	sii->rawvalid = 0;

	parse_content(sii, eeprom, 1024);

	return sii;
}


void sii_release(SiiInfo *sii)
{
	while (cat_rm(sii) != 1)
		;

	if (sii->rawbytes != NULL)
		free(sii->rawbytes);

	free(sii);
}

size_t sii_generate(SiiInfo *sii)
{
	size_t maxsize = sii->config->eeprom_size * 1024;
	sii->rawbytes = (uint8_t*) malloc(maxsize);
	memset(sii->rawbytes, 0, maxsize);

	sii_write(sii);
	sii->rawvalid = 1; /* FIXME valid should be set in sii_write */

	return sii->rawsize;
}

void sii_print(SiiInfo *sii)
{
	printf("First print preamble and config\n");
	struct _sii_preamble *preamble = sii->preamble;

	/* DEBUG print out */
	printf("Preamble:\n");
	printf("PDI Control: %.4x\n", preamble->pdi_ctrl);
	printf("PDI config: %.4x\n", preamble->pdi_conf);
	printf("Sync Impulse length = %d ns (raw: %.4x)\n", preamble->sync_impulse*10, preamble->sync_impulse);
	printf("PDI config 2: %.4x\n", preamble->pdi_conf2);
	printf("Configured station alias: %.4x\n", preamble->alias);
	printf("Checksum of preamble: %.4x\n", preamble->checksum);

	struct _sii_stdconfig *stdc = sii->config;

	/* DEBUG print */
	printf("General Information:\n");
	printf("Vendor ID: ....... 0x%08x\n", stdc->vendor_id);
	printf("Product ID: ...... 0x%08x\n", stdc->product_id);
	printf("Revision ID: ..... 0x%08x\n", stdc->revision_id);
	printf("Serial Number: ... 0x%08x\n", stdc->serial);

	/* mailbox settings */
	printf("\nDefault mailbox settings:\n");
	printf("Bootstrap received mailbox offset: 0x%04x\n", stdc->bs_rec_mbox_offset);
	printf("Bootstrap received mailbox size:   %d\n", stdc->bs_rec_mbox_size);
	printf("Bootstrap send mailbox offset:     0x%04x\n", stdc->bs_snd_mbox_offset);
	printf("Bootstrap send mailbox size:       %d\n", stdc->bs_snd_mbox_size);

	printf("Standard received mailbox offset:  0x%04x\n", stdc->std_rec_mbox_offset);
	printf("Standard received mailbox size:    %d\n", stdc->std_rec_mbox_size);
	printf("Standard send mailbox offset:      0x%04x\n", stdc->std_snd_mbox_offset);
	printf("Standard send mailbox size:        %d\n", stdc->std_snd_mbox_size);

	printf("\nSupported Mailboxes: ");
	if (stdc->mailbox_protocol.word&MBOX_EOE)
		printf("EoE, ");
	if (stdc->mailbox_protocol.word&MBOX_COE)
		printf("CoE, ");
	if (stdc->mailbox_protocol.word&MBOX_FOE)
		printf("FoE, ");
	if (stdc->mailbox_protocol.word&MBOX_SOE)
		printf("SoE, ");
	if (stdc->mailbox_protocol.word&MBOX_VOE)
		printf("VoE, ");
	printf("\n");

	printf("EEPROM size: %d kbit\n", stdc->eeprom_size);
	printf("Version: %d\n", stdc->version);
	printf("\n");

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
	if (sii->rawvalid)
		return -1;

	// FIXME check if file exists and ask for overwrite
	// unless force is set.
	//struct stat fs;
	//if (!stat(outfile, &fs))
	FILE *fh = fopen(outfile, "w");

	for (size_t i=0; i<sii->rawsize; i++)
		fputc(sii->rawbytes[i], fh);

	fclose(fh);

	return 0;
}
