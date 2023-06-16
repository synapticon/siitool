/* sii.h
 *
 * 2013-09-04 Frank Jeschke <fjeschke@synapticon.com>
 */

#ifndef SII_H
#define SII_H

#include <unistd.h>
#include <stdint.h>

#define SII_VERSION_MAJOR  0
#define SII_VERSION_MINOR  0

#define MBOX_EOE    0x0002
#define MBOX_COE    0x0004
#define MBOX_FOE    0x0008
#define MBOX_SOE    0x0010
#define MBOX_VOE    0x0020

#define BYTES_TO_WORD(x,y)          ((((int)y<<8)&0xff00) | (x&0xff))
#define BYTES_TO_DWORD(a,b,c,d)     ((unsigned int)(d&0xff)<<24)  | \
	                            ((unsigned int)(c&0xff)<<16) | \
	                            ((unsigned int)(b&0xff)<<8)  | \
	                            (unsigned int)(a&0xff)

#define EE_TO_BYTES(x) ((x << 7) + 0x80)
#define BYTES_TO_EE(x) ((x - 0x80) >> 7)

enum eSection {
	SII_PREAMBLE = -1
	,SII_STD_CONFIG = -2
	,SII_CAT_NOP = 0
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

struct _sii_preamble {
	uint16_t pdi_ctrl;
	uint16_t pdi_conf;
	uint16_t sync_impulse;   /* impulse length in multiple of 10 ns */
	uint16_t pdi_conf2;
	uint16_t alias;
	uint8_t reserved[4]; /* shall be zero */
	uint16_t checksum; /* checksum with x^8 + x^2 + x^1 + 1; initial value 0xff */
	int checksum_ok;
};

struct _sii_stdconfig {
	uint32_t vendor_id;
	uint32_t product_id;
	uint32_t revision_id;
	uint32_t serial;
	uint8_t reserveda[8]; /* shall be zero */
	/* bootstrap mailbox settings */
	uint16_t bs_rec_mbox_offset;
	uint16_t bs_rec_mbox_size;
	uint16_t bs_snd_mbox_offset;
	uint16_t bs_snd_mbox_size;
	/* standard mailbox settings */
	uint16_t std_rec_mbox_offset;
	uint16_t std_rec_mbox_size;
	uint16_t std_snd_mbox_offset;
	uint16_t std_snd_mbox_size;
	/*uint16_t mailbox_protocol;  / * bitmask of supported mailbox protocols */
	union {
		uint16_t word;
		struct {
			uint16_t res1:1;
			uint16_t eoe:1;
			uint16_t coe:1;
			uint16_t foe:1;
			uint16_t soe:1;
			uint16_t voe:1;
			uint16_t res2:10;
		} bit;
	} mailbox_protocol;
	uint8_t reservedb[66]; /* shall be zero */
	uint16_t eeprom_size;
	uint16_t version;
};

struct _string {
	uint8_t length;
	char *data;
	/* misc information */
	int id;
	struct _string *next;
	struct _string *prev;
};

struct _sii_strings {
	uint8_t count;
	size_t size;
	struct _string *head;
	struct _string *current;
};

struct _sii_general {
	uint8_t groupindex; /* index to string in string section */
	uint8_t imageindex;
	uint8_t orderindex;
	uint8_t nameindex;
	uint8_t reserved1;
	/* CoE Details: each 0 - not enabled, 1 enabled */
	uint8_t coe_enable_sdo:1;
	uint8_t coe_enable_sdo_info:1;
	uint8_t coe_enable_pdo_assign:1;
	uint8_t coe_enable_pdo_conf:1;
	uint8_t coe_enable_upload_start:1;
	uint8_t coe_enable_sdo_complete:1;
	uint8_t coe_detail_reserved1:2;
	uint8_t foe_enabled;
	uint8_t eoe_enabled;
	uint8_t soe_channels; /* reserved */
	uint8_t ds402_channels; /* reserved */
	uint8_t sysman_class; /* reserved */
	uint8_t flag_safe_op:1;
	uint8_t flag_notLRW:1;
	uint8_t flag_MBoxDataLinkLayer:1;
	uint8_t flag_IdentALSts:1;
	uint8_t flag_IdentPhyM:1;
	uint8_t flagreseved:3;
	int16_t current_ebus;
	uint8_t reserved2[2];
	/* physical ports
	 * values: 0x00  not used
	 *         0x01  MII
	 *         0x02  reserved
	 *         0x03  EBUS
	 */
	uint16_t phys_port_0:4;
	uint16_t phys_port_1:4;
	uint16_t phys_port_2:4;
	uint16_t phys_port_3:4;
	uint16_t physical_address;
	uint8_t reservedb[12]; /* shall be zero */
};

enum eFmmuUsage {
	FMMU_UNUSED     = 0
	,FMMU_OUTPUTS   = 0x01
	,FMMU_INPUTS    = 0x02
	,FMMU_SYNCMSTAT = 0x03
};

struct _fmmu_entry {
	uint8_t usage;
	/* no content of sii entry */
	int id;
	struct _fmmu_entry *next;
	struct _fmmu_entry *prev;
};

struct _sii_fmmu {
	int count;
	struct _fmmu_entry *list;
};

enum eSyncmType {
	SMT_UNUSED = 0x0
	,SMT_MBOXOUT
	,SMT_MBOXIN
	,SMT_OUTPUTS
	,SMT_INPUTS
};

struct _syncm_entry {
	uint16_t phys_address;
	uint16_t length;
	uint8_t control;
	uint8_t status; /* don't care */
	uint8_t enable;
	uint8_t type;
	/* no content of sii entry */
	int id;
	struct _syncm_entry *next;
	struct _syncm_entry *prev;
};

struct _sii_syncm {
	int count;
	struct _syncm_entry *list;
};


#define SII_UNDEF_PDO  0
#define SII_RX_PDO     1
#define SII_TX_PDO     2

enum ePdoType {
	RxPDO  = SII_RX_PDO
	,TxPDO = SII_TX_PDO
};

struct _pdo_entry {
	uint16_t index;
	uint8_t subindex;
	uint8_t string_index;
	uint8_t data_type;
	uint8_t bit_length;
	uint16_t flags; /* for future use - set to 0 */
	/* no content of sii entry */
	int id;
	struct _pdo_entry *next;
	struct _pdo_entry *prev;
};

struct _sii_pdo {
	uint16_t index;
	uint8_t entries;
	uint8_t syncmanager; /* related syncmanager */
	uint8_t dcsync;
	uint8_t name_index; /* index to string */
	uint16_t flags; /* for future use - set to 0 */
	/* no content of sii entry */
	int type; /* rx or tx pdo */
	struct _pdo_entry *list;
};

/* FIXME currently this struct is a assumption how the
 * distributed clock could look like.
 * Further versions of the ETG specs should clearify
 * this issue. */
struct _sii_dclock {
	uint8_t reserved1; /* shall be zero */
	uint8_t cyclic_op_enabled:1;
	uint8_t sync0_active:1;
	uint8_t sync1_active:1;
	uint8_t reserved2:5;
	uint16_t sync_pulse;
	uint8_t int0_status:1;
	uint8_t reserved4:7;
	uint8_t int1_status:1;
	uint8_t reserved5:7;
	uint32_t cyclic_op_starttime;
	uint32_t sync0_cycle_time;
	uint32_t sync1_cycle_time;
	uint16_t latch0_pos_edge:1;
	uint16_t latch0_neg_edge:1;
	uint16_t reserved6:6;
	uint16_t latch1_pos_edge:1;
	uint16_t latch1_neg_edge:1;
	uint16_t reserved7:6;
	uint8_t latch0_pos_event:1;
	uint8_t latch0_neg_event:1;
	uint8_t reserved9:6;
	uint8_t latch1_pos_event:1;
	uint8_t latch1_neg_event:1;
	uint8_t reserved10:6;
	uint8_t reservedc[10];
	uint32_t latch0_pos_edge_value;
	uint32_t latch0_neg_edge_value;
	uint32_t latch1_pos_edge_value;
	uint32_t latch1_neg_edge_value;
};

/* a single category */
struct _sii_cat {
	uint16_t type:15;
	uint16_t vendor:1;
	uint16_t size;
	void *data;
	struct _sii_cat *next;
	struct _sii_cat *prev;
};

struct _sii {
	struct _sii_preamble *preamble;
	struct _sii_stdconfig *config;
	struct _sii_cat *cat_head;
	struct _sii_cat *cat_current;
	/* meta information */
	char *outfile;
	uint8_t *rawbytes;
	int rawvalid;
	size_t rawsize;
};

typedef struct _sii SiiInfo;

/**
 * @brief Init a empty data structure
 *
 * @return empty sii object
 */
SiiInfo *sii_init(void);

SiiInfo *sii_init_string(const unsigned char *eeprom, size_t size);
SiiInfo *sii_init_file(const char *filename);
void sii_release(SiiInfo *sii);

/**
 * \brief Generate binary sii file
 *
 * \param *sii  pointer to sii structure
 * \param outfile  output filename
 * \return number of bytes written
 */
size_t sii_generate(SiiInfo *sii, unsigned int add_pdo_mapping);

void sii_print(SiiInfo *sii);

//void sii_print_bin(SiiInfo *sii); - ???

/* wirte binary to file */
int sii_write_bin(SiiInfo *sii, const char *outfile);

/**
 * \brief Sanity check of current config
 */
int sii_check(SiiInfo *sii);


int sii_add_info(SiiInfo *sii, struct _sii_preamble *pre, struct _sii_stdconfig *cfg);

/* functions to handle categories */
int sii_category_add(SiiInfo *sii, struct _sii_cat *cat);
struct _sii_cat *sii_category_find(SiiInfo *sii, enum eSection category);

/* Add new string if only SiiInfo is available */
int sii_strings_add(SiiInfo *sii, const char *entry);

/* functions for specific substructures */
void fmmu_add_entry(struct _sii_fmmu *fmmu, int usage);
void syncm_entry_add(struct _sii_syncm *sm, struct _syncm_entry *entry);
void pdo_entry_add(struct _sii_pdo *pdo, struct _pdo_entry *entry);

/**
 * Add new string if category string is available.
 *
 * @return the index of the newly added string
 */
int strings_add(struct _sii_strings *strings, const char *entry);

const char *string_search_id(struct _sii_strings *strings, int id);
int string_search_string(struct _sii_strings *strings, const char *str);

/* misc functions */
char *cat2string(enum eSection cat);

/* sort the categories in increasing order of cathegories type */
void sii_cat_sort(SiiInfo *sii);

/* Attention: Since the documentation of the distributed clock is not very
 * clear and marked as "for future use" (as to date 2013-10-22) the category
 * is fixed with a default value found on the EEPROM file of one of our
 * evaluation boards. As soon as the descirption is fixed and the future use
 * note disappeared the need for the default setting will become obsolete.
 */
struct _sii_dclock *dclock_get_default(void);
#endif /* SII_H */
