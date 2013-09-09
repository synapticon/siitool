/* sii.h
 *
 * 2013-09-04 Frank Jeschke <jeschke@fjes.de>
 */

#ifndef SII_H
#define SII_H

#include <unistd.h>

#define SII_VERSION_MAJOR  0
#define SII_VERSION_MINOR  0

#define MBOX_EOE    0x0002
#define MBOX_COE    0x0004
#define MBOX_FOE    0x0008
#define MBOX_SOE    0x0010
#define MBOX_VOE    0x0020

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

struct _sii_preamble {
	int pdi_ctrl;
	int pdi_conf;
	int sync_impulse;
	int pdi_conf2;
	int alias;
	int checksum;
};

struct _sii_stdconfig {
	unsigned vendor_id;
	unsigned product_id;
	unsigned revision_id;
	unsigned serial;
	/* bootstrap mailbox settings */
	int bs_rec_mbox_offset;
	int bs_rec_mbox_size;
	int bs_snd_mbox_offset;
	int bs_snd_mbox_size;
	/* standard mailbox settings */
	int std_rec_mbox_offset;
	int std_rec_mbox_size;
	int std_snd_mbox_offset;
	int std_snd_mbox_size;
	int supported_mailbox;  /* bitmask of supported mailbox protocols */
	int eeprom_size;
	int version;
};

struct _sii_general {
	int groupindex; /* index to string in string section */
	int imageindex;
	int orderindex;
	int nameindex;
	/* CoE Details: each 0 - not enabled, 1 enabled */
	int coe_enable_sdo;
	int coe_enable_sdo_info;
	int coe_enable_pdo_assign;
	int coe_enable_pdo_conf;
	int coe_enable_upload_start;
	int coe_enable_sdo_complete;
	int foe_enabled;
	int eoe_enabled;
	int flag_safe_op;
	int flag_notLRW;
	int current_ebus;
	/* physical ports */
	int phys_port_0;
	int phys_port_1;
	int phys_port_2;
	int phys_port_3;
};

struct _fmmu_entry {
	int id;
	int usage;
	struct _fmmu_entry *next;
	struct _fmmu_entry *prev;
};

struct _sii_fmmu {
	int count;
	struct _fmmu_entry *list;
};

struct _syncm_entry {
	int id;
	int phys_address;
	int length;
	int control;
	int status;
	int enable;
	int type;
	struct _syncm_entry *next;
	struct _syncm_entry *prev;
};

struct _sii_syncm {
	int count;
	struct _syncm_entry *list;
};

struct _pdo_entry {
	int count;
	int index;
	int subindex;
	int string_index;
	int data_type;
	int bit_length;
	int flags;
	struct _pdo_entry *next;
	struct _pdo_entry *prev;
};

#define SII_UNDEF_PDO  0
#define SII_RX_PDO     1
#define SII_TX_PDO     2

struct _sii_pdo {
	int type; /* rx or tx pdo */
	int index;
	int entries;
	int syncmanager;
	int sync;
	int name_index; /* index to string */
	int flags; /* future use */
	struct _pdo_entry *list;
};

struct _sii_dclock {
	int cyclic_op_enabled;
	int sync0_active;
	int sync1_active;
	int sync_pulse;
	int int0_status;
	int int1_status;
	unsigned cyclic_starttime;
	unsigned sync0_cycle_time;
	unsigned sync1_cycle_time;
	int latch0_pos_edge;
	int latch0_neg_edge;
	int latch1_pos_edge;
	int latch1_neg_edge;
	int latch0_pos_event;
	int latch0_neg_event;
	int latch1_pos_event;
	int latch1_neg_event;
	unsigned latch0_pos_edge_value;
	unsigned latch0_neg_edge_value;
	unsigned latch1_pos_edge_value;
	unsigned latch1_neg_edge_value;
};

typedef struct _sii_info {
	struct _sii_preamble *preamble;
	struct _sii_stdconfig *stdconfig;
	char **strings; /* array of strings from string section */
	/* struct _sii_datatypes; -- not yet available */
	struct _sii_general *general;
	struct _sii_fmmu *fmmu; /* contains linked list with each fmmu */
	struct _sii_syncm *syncmanager; /* contains linked list with each syncmanager */
	struct _sii_pdo *txpdo; /* contains list of pdoentries */
	struct _sii_pdo *rxpdo; /* contains list of pdoentries */
	struct _sii_dclock *distributedclock;
} SiiInfo;

SiiInfo *sii_init_string(const unsigned char *eeprom, size_t size);
SiiInfo *sii_init_file(const char *filename);
void sii_release(SiiInfo *sii);

/**
 * \brief Generate binary sii file
 *
 * \param *sii  pointer to sii structure
 * \param outfile  output filename
 * \return 0 success; otherwise failure
 */
int sii_generate(SiiInfo *sii, const char *outfile);

/**
 * \brief Sanity check of current config
 */
int sii_check(SiiInfo *sii);

#endif /* SII_H */
