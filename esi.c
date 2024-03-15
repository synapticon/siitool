/* ESI - EtherCAT Slave Information
 */
#include "esi.h"
#include "esifile.h"
#include "sii.h"
#include "crc8.h"

#include <stdio.h>
#include <string.h>

#include <libxml/parser.h>
#include <libxml/tree.h>

#define Char2xmlChar(s)   ((xmlChar *)s)

#define MAX_BOOTSTRAP_STRING   18

struct _esi_data {
	SiiInfo *sii;
	char *siifile; /* also opt for sii->outfile */
	xmlDocPtr doc; /* do I need both? */
	xmlNode *xmlroot;
	char *xmlfile;
};

static inline void scan_hex_dec(const char *str, uint32_t *value)
{
	int ret = sscanf(str, "#x%x", value);
	if (ret != 1) {
		sscanf(str, "%u", value);
	}
}

static char *type2str(int type)
{
	switch (type) {
	case XML_ELEMENT_NODE:
		return "XML_ELEMENT_NODE";

	case XML_ATTRIBUTE_NODE:
		return "XML_ATTRIBUTE_NODE";

	case XML_TEXT_NODE:
		return "XML_TEXT_NODE";

	case XML_CDATA_SECTION_NODE:
		return "XML_CDATA_SECTION_NODE";

	case XML_ENTITY_REF_NODE:
		return "XML_ENTITY_REF_NODE";

	case XML_ENTITY_NODE:
		return "XML_ENTITY_NODE";

	case XML_PI_NODE:
		return "XML_PI_NODE";

	case XML_COMMENT_NODE:
		return "XML_COMMENT_NODE";

	case XML_DOCUMENT_NODE:
		return "XML_DOCUMENT_NODE";

	case XML_DOCUMENT_TYPE_NODE:
		return "XML_DOCUMENT_TYPE_NODE";

	case XML_DOCUMENT_FRAG_NODE:
		return "XML_DOCUMENT_FRAG_NODE";

	case XML_NOTATION_NODE:
		return "XML_NOTATION_NODE";

	case XML_HTML_DOCUMENT_NODE:
		return "XML_HTML_DOCUMENT_NODE";

	case XML_DTD_NODE:
		return "XML_DTD_NODE";

	case XML_ELEMENT_DECL:
		return "XML_ELEMENT_DECL";

	case XML_ATTRIBUTE_DECL:
		return "XML_ATTRIBUTE_DECL";

	case XML_ENTITY_DECL:
		return "XML_ENTITY_DECL";

	case XML_NAMESPACE_DECL:
		return "XML_NAMESPACE_DECL";

	case XML_XINCLUDE_START:
		return "XML_XINCLUDE_START";

	case XML_XINCLUDE_END:
		return "XML_XINCLUDE_END";

	case XML_DOCB_DOCUMENT_NODE:
		return "XML_DOCB_DOCUMENT_NODE";

	}

	return "empty";
}

#if DEBUG == 1
static void print_node(xmlNode *node)
{
	if ((node->type == XML_TEXT_NODE) && (0x0a == *(node->content)))
		return; // skip line feed node

	printf("%d: type: %s name = %s, content = '%s'\n",
		node->line, type2str(node->type), node->name, node->content);
}
#endif

static void print_all_nodes(xmlNode *root)
{
	//xmlNode *node = root;

	for (xmlNode *current = root; current; current = current->next) {
		if (strncmp((const char *)current->name, "Profile", 7) == 0) /* skip profile sektion */
			continue;
#if 0
		printf("%d: note type: %s name: %s\n", current->line, type2str(current->type), current->name);

#else
		if (current->type == XML_ELEMENT_NODE)
			printf("%d: type: %s name = %s, content = '%s'\n",
				       	current->line, type2str(current->type), current->name, current->content);

		if (current->type == XML_TEXT_NODE)
			printf("%d: type: %s name: %s content: %s\n",
					current->line, type2str(current->type), current->name, current->content);

		if (current->type == XML_COMMENT_NODE)
			continue;
#endif

		print_all_nodes(current->children);
	}
}

#if DEBUG == 1
static void parse_example(xmlNode *root)
{
	for (xmlNode *current = root; current; current = current->next) {
		/*
		printf("%d: note type: %s name: %s content: '%s'\n",
			       	current->line, type2str(current->type), current->name, current->content);
		 */
		if (current->type == XML_ELEMENT_NODE && strncmp((const char *)current->name, "Device", 6) == 0) {
			printf("Device found, now printing\n");
			print_all_nodes(current);
			printf("Finished\n");
			return;
		}

		parse_example(current->children);
	}
}
#endif

static int parse_boolean(xmlChar *boolean)
{
	if (xmlStrcmp(boolean, Char2xmlChar("1")) == 0 ||
	    xmlStrcmp(boolean, Char2xmlChar("true")) == 0)
		return 1;

	return 0;
}

static uint16_t preamble_crc8(struct _sii_preamble *pa)
{
	if (pa == NULL)
		return 0;

	uint8_t crc = 0xff;

	crc8byte(&crc, pa->pdi_ctrl&0xff);
	crc8byte(&crc, (pa->pdi_ctrl>>8)&0xff);

	crc8byte(&crc, pa->pdi_conf&0xff);
	crc8byte(&crc, (pa->pdi_conf>>8)&0xff);

	crc8byte(&crc, pa->sync_impulse&0xff);
	crc8byte(&crc, (pa->sync_impulse>>8)&0xff);

	crc8byte(&crc, pa->pdi_conf2&0xff);
	crc8byte(&crc, (pa->pdi_conf2>>8)&0xff);

	crc8byte(&crc, pa->alias&0xff);
	crc8byte(&crc, (pa->alias>>8)&0xff);

	crc8byte(&crc, pa->reserved[0]&0xff);
	crc8byte(&crc, pa->reserved[1]&0xff);
	crc8byte(&crc, pa->reserved[2]&0xff);
	crc8byte(&crc, pa->reserved[3]&0xff);

	pa->checksum = crc;
	pa->checksum_ok = 1;

	return pa->checksum;
}

/* Searches the first occurence of node named 'name' */
static xmlNode *search_node(xmlNode *root, const char *name)
{
	xmlNode *tmp = NULL;

	for (xmlNode *curr = root; curr; curr = curr->next) {
		if (curr->type == XML_ELEMENT_NODE &&
			strncmp((const char *)curr->name, name, strlen((const char *)curr->name)) == 0)
			return curr;


		tmp = search_node(curr->children, name);
		if (tmp != NULL)
			return tmp;
	}

	return NULL;
}

static xmlNode *search_node_bfs(xmlNode *root, const char *name)
{
	xmlNode *tmp = NULL;

	for (xmlNode *curr = root; curr != NULL; curr = curr->next) {
		if (curr->type == XML_ELEMENT_NODE &&
			strncmp((const char *)curr->name, name, strlen((const char *)curr->name)) == 0) {
			return curr;
		}
	}

	for (xmlNode *curr = root; curr != NULL; curr = curr->next) {
		tmp = search_node_bfs(curr->children, name);

		if (tmp != NULL) {
			return tmp;
		}
	}

	return NULL;
}

static xmlNode *search_device(xmlNode *root, int device_number)
{
	xmlNode *d = search_node(search_node(root, "Devices"), "Device");
	while (device_number != 0 && d->next != NULL) {
		d = d->next;
		if (d->type == XML_ELEMENT_NODE) {
			device_number--;
		}
	}

	return (device_number > 0) ? NULL : d;
}

/* TODO: Add function to search for all nodes named by 'name' (e.g. multiple <Sm>-Tags */

/* functions to parse xml */
static struct _sii_preamble *parse_preamble(xmlNode *node)
{
	struct _sii_preamble *pa = calloc(1, sizeof(struct _sii_preamble));

	char string[1025];
	strncpy(string, (char *)node->children->content, sizeof(string) - 1);

	unsigned int lowbyte=0, highbyte=0;
	char *b = string;

	if (sscanf(b, "%2x%2x", &highbyte, &lowbyte) >= 2) {
		pa->pdi_ctrl = BYTES_TO_WORD(highbyte, lowbyte);
		b+=4;
	}

	if (sscanf(b, "%2x%2x", &highbyte, &lowbyte) >= 2) {
		pa->pdi_conf = BYTES_TO_WORD(highbyte, lowbyte);
		b+=4;
	}

	if (sscanf(b, "%2x%2x", &highbyte, &lowbyte) >= 2) {
		pa->sync_impulse = BYTES_TO_WORD(highbyte, lowbyte);
		b+=4;
	}

	if (sscanf(b, "%2x%2x", &highbyte, &lowbyte) >= 2) {
		pa->pdi_conf2 = BYTES_TO_WORD(highbyte, lowbyte);
		b+=4;
	}

	if (sscanf(b, "%2x%2x", &highbyte, &lowbyte) >= 2) {
		pa->alias = BYTES_TO_WORD(highbyte, lowbyte);
		//b+=4;
	}

	for (int i=0; i<4; i++)
		pa->reserved[i] = 0x00;

	pa->checksum = 0xff; /* set initial checksum */
	pa->checksum = preamble_crc8(pa);

	return pa;
}

static struct _sii_stdconfig *parse_config(xmlNode *root, xmlNode *device)
{
	xmlNode *n, *tmp;

	//xmlNode *n = search_node(root, "Vendor");
	n = search_node(root, "Vendor");
	if (n==NULL) {
		return NULL;
	}

	struct _sii_stdconfig *sc = calloc(1, sizeof(struct _sii_stdconfig));

	tmp = search_node(n, "Id");
	//char *vendoridstr = tmp->children->content;

	/* get id */
	// FIXME add some error and validty checking, esp. if node is set correctly and has the type
	scan_hex_dec((const char *)tmp->children->content, &(sc->vendor_id));

	n = device; //search_node(search_node(root, "Devices"), "Device");

	tmp = search_node(n, "Type");
	xmlAttr *prop = tmp->properties;

	while (prop != NULL) {
		if (xmlStrncmp(prop->name, Char2xmlChar("ProductCode"), xmlStrlen(prop->name)) == 0) {
			scan_hex_dec((const char *)prop->children->content, &(sc->product_id));
		}

		if (xmlStrncmp(prop->name, Char2xmlChar("RevisionNo"), xmlStrlen(prop->name)) == 0) {
			scan_hex_dec((const char *)prop->children->content, &(sc->revision_id));
		}

		prop = prop->next;
	}

	sc->serial = 0; /* FIXME the serial number is not in the esi? */

	sc->bs_rec_mbox_offset = 0;
	sc->bs_rec_mbox_size = 0;
	sc->bs_snd_mbox_offset = 0;
	sc->bs_snd_mbox_size = 0;

	sc->std_rec_mbox_offset = 0;
	sc->std_rec_mbox_size = 0;
	sc->std_snd_mbox_offset = 0;
	sc->std_snd_mbox_size = 0;

	/* filter the std mailbox configuration; from <Sm> description */

	tmp = n->children;
	while (tmp != NULL) {
		if (xmlStrncmp(tmp->name, Char2xmlChar("Sm"), xmlStrlen(tmp->name)) == 0) {
			xmlNode *smchild = tmp->children;
			while (smchild != NULL) {
				uint32_t atmp = 0;
				if (xmlStrncmp(smchild->content, Char2xmlChar("MBoxOut"), xmlStrlen(smchild->content))== 0) {
					xmlAttr *p = tmp->properties;
					while (p!=NULL) {
						if (xmlStrncmp(p->name, Char2xmlChar("DefaultSize"), xmlStrlen(p->name)) == 0) {
							scan_hex_dec((const char *)p->children->content, &atmp);
							sc->std_rec_mbox_size = (uint16_t)atmp;
						}

						if (xmlStrncmp(p->name, Char2xmlChar("StartAddress"), xmlStrlen(p->name)) == 0) {
							scan_hex_dec((const char *)p->children->content, &atmp);
							sc->std_rec_mbox_offset = atmp;
						}

						p = p->next;
					}

				}

				if (xmlStrncmp(smchild->content, Char2xmlChar("MBoxIn"), xmlStrlen(smchild->content))== 0) {
					xmlAttr *p = tmp->properties;
					while (p!=NULL) {
						if (xmlStrncmp(p->name, Char2xmlChar("DefaultSize"), xmlStrlen(p->name)) == 0) {
							scan_hex_dec((const char *)p->children->content, &atmp);
							sc->std_snd_mbox_size = (uint16_t)atmp;
						}

						if (xmlStrncmp(p->name, Char2xmlChar("StartAddress"), xmlStrlen(p->name)) == 0) {
							scan_hex_dec((const char *)p->children->content, &atmp);
							sc->std_snd_mbox_offset = atmp;
						}

						p = p->next;
					}

				}

				smchild = smchild->next;
			}
		}

		tmp = tmp->next;
	}

	/* get the supported mailboxes - these also occure again in the general section */

	tmp = search_node_bfs(n, "Mailbox");
	xmlNode *mbox;

	mbox = search_node(tmp, "CoE");
	if (mbox != NULL)
		sc->mailbox_protocol.bit.coe = 1;

	mbox = search_node(tmp, "EoE");
	if (mbox != NULL)
		sc->mailbox_protocol.bit.eoe = 1;

	mbox = search_node(tmp, "FoE");
	if (mbox != NULL)
		sc->mailbox_protocol.bit.foe = 1;

	mbox = search_node(tmp, "VoE");
	if (mbox != NULL)
		sc->mailbox_protocol.bit.voe = 1;

	/* fetch eeprom size */
	tmp = search_node(n, "ByteSize");
	/* convert byte -> kbyte */
	sc->eeprom_size = BYTES_TO_EE(atoi((char *)tmp->children->content));
	sc->version = 1; /* also not in Esi */

	tmp = search_node_bfs(n, "Eeprom");
	if (tmp == NULL) {
		fprintf(stderr, "Warning <Eeprom> tag not found");
	} else {
		xmlNode *bootstrap = search_node(tmp, "BootStrap");
		if (bootstrap != NULL) {
			char bsraw[MAX_BOOTSTRAP_STRING] = { 0 };
			memmove(bsraw, (char *)bootstrap->children->content, MAX_BOOTSTRAP_STRING);

			unsigned int braw[MAX_BOOTSTRAP_STRING] = { 0 };
			sscanf(bsraw, "%2x%2x%2x%2x%2x%2x%2x%2x",
					&braw[0], &braw[1], &braw[2], &braw[3],
					&braw[4], &braw[5], &braw[6], &braw[7]);

			sc->bs_rec_mbox_offset = braw[1] << 8 | braw[0];
			sc->bs_rec_mbox_size =   braw[3] << 8 | braw[2];
			sc->bs_snd_mbox_offset = braw[5] << 8 | braw[4];
			sc->bs_snd_mbox_size =   braw[7] << 8 | braw[6];
		}
	}

	return sc;
}

static struct _sii_general *parse_general(SiiInfo *sii, xmlNode *root, xmlNode *device)
{
	xmlNode *parent;
	xmlNode *node;
	xmlNode *tmp;
	struct _sii_general *general = calloc(1, sizeof(struct _sii_general));

	/* Search group-,image-, order- and namestring and store these strings
	 * to the corresponding *index.
	 *
	 * Note, these strings are Vendor specific.
	 */

	parent = search_node(root, "Groups");
	node = search_node(parent, "Group");
	tmp = search_node(node, "Type");
	general->groupindex = sii_strings_add(sii, (const char *)tmp->children->content);

	general->imageindex = 0;
	general->orderindex = 0;

	tmp = search_node(device, "Name"); /* FIXME check language id and use the english version LcId="1033" */
	general->nameindex = sii_strings_add(sii, (const char *)tmp->children->content);

	/* reset temporial nodes */
	parent = NULL;
	node = NULL;
	tmp = NULL;

	/* fetch CoE details */
	parent = device;

	for (xmlAttr *attr = parent->properties; attr; attr = attr->next) {
		if (xmlStrcmp(attr->name, Char2xmlChar("Physics")) == 0) {
			char *phys = malloc(xmlStrlen(attr->children->content)+1);
			memmove(phys, attr->children->content, xmlStrlen(attr->children->content)+1);
			for (char *c = phys, port=0; *c != '\0'; c++, port++) {
				int val = 0;
				switch (*c) {
				case 'Y': /* MII */
					val = 0x01;
					break;
				case 'K': /* EBUS */
					val = 0x03;
					break;
				default:
					val = 0x00;
					break;
				}

				switch (port) {
				case 0:
					general->phys_port_0 = val&0xf;
					break;
				case 1:
					general->phys_port_1 = val&0xf;
					break;
				case 2:
					general->phys_port_2 = val&0xf;
					break;
				case 3:
					general->phys_port_3 = val&0xf;
					break;
				default:
					fprintf(stderr, "Error invalid port %d\n", port);
					break;
				}
			}

			free(phys);
		}
	}

	node = search_node_bfs(parent, "Mailbox");
	tmp = search_node(node, "CoE");
	if (tmp != NULL) {
		general->coe_enable_sdo = 1;
		/* parse the attributes */
		for (xmlAttr *attr = tmp->properties; attr; attr = attr->next) {
			if (xmlStrcmp(attr->name, Char2xmlChar("SdoInfo")) == 0)
				general->coe_enable_sdo_info = parse_boolean(attr->children->content);

			if (xmlStrcmp(attr->name, Char2xmlChar("PdoAssign")) == 0)
				general->coe_enable_pdo_assign = parse_boolean(attr->children->content);

			if (xmlStrcmp(attr->name, Char2xmlChar("PdoConfig")) == 0)
				general->coe_enable_pdo_conf = parse_boolean(attr->children->content);

			if (xmlStrcmp(attr->name, Char2xmlChar("PdoUpload")) == 0)
				general->coe_enable_upload_start = parse_boolean(attr->children->content);

			if (xmlStrcmp(attr->name, Char2xmlChar("??sdoComplete")) == 0)
				general->coe_enable_sdo_complete = parse_boolean(attr->children->content);

			//attr = attr->next;
		}
	}

	tmp = search_node(node, "EoE");
	if (tmp != NULL)
		general->eoe_enabled = 1;

	tmp = search_node(node, "FoE");
	if (tmp != NULL)
		general->foe_enabled = 1;

	return general;
}

static void parse_fmmu(xmlNode *current, SiiInfo *sii)
{
	struct _sii_cat *cat = sii_category_find(sii, SII_CAT_FMMU);
	if (cat == NULL) { /* create new category */
		cat = calloc(1, sizeof(struct _sii_cat));
		cat->type = SII_CAT_FMMU;
		sii_category_add(sii, cat);
	}

	if (cat->data == NULL) { /* if category exists but doesn't contain any data */
		cat->data = (void *)calloc(1, sizeof(struct _sii_fmmu));
	}

	struct _sii_fmmu *fmmu = (struct _sii_fmmu *)cat->data;

	/* now fetch the data */
	xmlChar *content = current->children->content;
	if (xmlStrncmp(content, Char2xmlChar("Inputs"), xmlStrlen(content)) == 0)
		fmmu_add_entry(fmmu, FMMU_INPUTS);
	else if (xmlStrncmp(content, Char2xmlChar("Outputs"), xmlStrlen(content)) == 0)
		fmmu_add_entry(fmmu, FMMU_OUTPUTS);
	else if (xmlStrncmp(content, Char2xmlChar("MBoxState"), xmlStrlen(content)) == 0)
		fmmu_add_entry(fmmu, FMMU_SYNCMSTAT);
	else
		fmmu_add_entry(fmmu, FMMU_UNUSED);

	/* add entry to fmmu struct */
	cat->size += 8; /* add size of new fmmu entry */
}

static void parse_syncm(xmlNode *current, SiiInfo *sii)
{
	struct _sii_cat *cat = sii_category_find(sii, SII_CAT_SYNCM);
	if (cat == NULL) {
		cat = calloc(1, sizeof(struct _sii_cat));
		cat->type = SII_CAT_SYNCM;
		sii_category_add(sii, cat);
	}

	if (cat->data == NULL) {
		cat->data = (void *)calloc(1, sizeof(struct _sii_syncm));
	}

	/* now fetch the data */
	//size_t smsize = 0;
	struct _sii_syncm *sm = (struct _sii_syncm *)cat->data;
	struct _syncm_entry *entry = calloc(1, sizeof(struct _syncm_entry));
	entry->id = -1;

	xmlAttr *args = current->properties;
	for (xmlAttr *a = args; a ; a = a->next) {
		if (xmlStrncmp(a->name, Char2xmlChar("DefaultSize"), xmlStrlen(a->name)) == 0) {
			entry->length = atoi((char *)a->children->content);
		} else if (xmlStrncmp(a->name, Char2xmlChar("StartAddress"), xmlStrlen(a->name)) == 0) {
			uint32_t tmp = 0;
			scan_hex_dec((char *)a->children->content, &tmp);
			entry->phys_address = tmp&0xffff;
		} else if (xmlStrncmp(a->name, Char2xmlChar("ControlByte"), xmlStrlen(a->name)) == 0) {
			uint32_t tmp = 0;
			scan_hex_dec((char *)a->children->content, &tmp);
			entry->control = tmp&0xff;
		} else if (xmlStrncmp(a->name, Char2xmlChar("Enable"), xmlStrlen(a->name)) == 0) {
			entry->enable = atoi((char *)a->children->content);
		}
	}

	entry->status = 0; /* don't care */

	/* type is encoded in the value of the node */
	xmlChar *type = current->children->content;
	if (xmlStrncmp(type, Char2xmlChar("MBoxIn"), xmlStrlen(type)) == 0)
		entry->type = SMT_MBOXIN;
	else if (xmlStrncmp(type, Char2xmlChar("MBoxOut"), xmlStrlen(type)) == 0)
		entry->type = SMT_MBOXOUT;
	else if (xmlStrncmp(type, Char2xmlChar("Inputs"), xmlStrlen(type)) == 0)
		entry->type = SMT_INPUTS;
	else if (xmlStrncmp(type, Char2xmlChar("Outputs"), xmlStrlen(type)) == 0)
		entry->type = SMT_OUTPUTS;
	else
		entry->type = SMT_UNUSED;

	syncm_entry_add(sm, entry);
	cat->size += 8; /* a syncmanager entry is 8 bytes */
}

static void get_dc_proto(SiiInfo *sii)
{
	struct _sii_cat *cat = calloc(1, sizeof(struct _sii_cat));
	cat->type = SII_CAT_DCLOCK;

	/* now fetch the data */
	struct _sii_dclock *dc = dclock_get_default();

	cat->data = (void *)dc;
	cat->size = 48;
	sii_category_add(sii, cat);
}

#if 0 // FIXME this is a future feature - don't remove
static void parse_dclock(xmlNode *current, SiiInfo *sii)
{
	struct _sii_cat *cat = calloc(1, sizeof(struct _sii_cat));
	cat->type = SII_CAT_DCLOCK;

	/* now fetch the data */
	size_t dcsize = 0;
	struct _sii_dclock *dc = calloc(1, sizeof(struct _sii_dclock));

	/* FIXME add entries, only use the first (default) OpMode */
	printf("[DEBUG %s] FIXME implementation of dclock parsing is incomplete!\n", __func__);

	xmlNode *op = search_node(current, "OpMode");
	for (xmlNode *vals = op->children; vals; vals = vals->next) {
		if (xmlStrncmp(vals->name, Char2xmlChar("Name"), xmlStrlen(vals->name)) == 0) {
			//fprintf(stderr, "[DistributedClock] Warning, unknown handling of '%s'\n", (char *)vals->name);
		} else if (xmlStrncmp(vals->name, Char2xmlChar("Desc"), xmlStrlen(vals->name)) == 0) {
			//fprintf(stderr, "[DistributedClock] Warning, unknown handling of '%s'\n", (char *)vals->name);
		} else if (xmlStrncmp(vals->name, Char2xmlChar("AssignActivate"), xmlStrlen(vals->name)) == 0) {
			//fprintf(stderr, "[DistributedClock] Warning, unknown handling of '%s'\n", (char *)vals->name);
		} else if (xmlStrncmp(vals->name, Char2xmlChar("CycleTimeSync0"), xmlStrlen(vals->name)) == 0) {
			int tmp = atoi((char *)vals->children->content);
			dc->sync0_cycle_time = (uint32_t)tmp;
		} else if (xmlStrncmp(vals->name, Char2xmlChar("ShiftTimeSync0"), xmlStrlen(vals->name)) == 0) {
			//fprintf(stderr, "[DistributedClock] Warning, unknown handling of '%s'\n", (char *)vals->name);
		} else if (xmlStrncmp(vals->name, Char2xmlChar("CycleTimeSync1"), xmlStrlen(vals->name))) {
			int tmp = atoi((char *)vals->children->content);
			dc->sync1_cycle_time = (uint32_t)tmp;
		} else if (xmlStrncmp(vals->name, Char2xmlChar("ShiftTimeSync1"), xmlStrlen(vals->name)) == 0) {
			//fprintf(stderr, "[DistributedClock] Warning, unknown handling of '%s'\n", (char *)vals->name);
		} else {
			//fprintf(stderr, "[DistributedClock] Warning, unknown handling of '%s'\n", (char *)vals->name);
		}
	}

	cat->data = (void *)dc;
	cat->size = dcsize;
	sii_category_add(sii, cat);
}
#endif

struct _coe_datatypes {
	int index;
	char *coename;
	char *esiname;
};

static int parse_pdo_get_data_type(char *xmldatatype)
{
	/* FIXME make match of CoE datatypes and xml description of types
	 * see table in document ETG 2000.S Page 18
	 */
	struct _coe_datatypes datatypes[] = {
		{ 0, "UNDEF", "UNDEF" },
		{ 0x0001, "BOOLEAN", "BOOL" },
		{ 0x0002, "INTEGER8", "SINT" },
		{ 0x0003, "INTEGER16", "INT" },
		{ 0x0004, "INTEGER32", "DINT" },
		{ 0x0005, "UNSIGNED8", "USINT" },
		{ 0x0006, "UNSIGNED16", "UINT" },
		{ 0x0007, "UNSIGNED32", "UDINT" },
		{ 0x0008, "REAL32", "REAL" },
		{ 0x0009, "VISIBLE_STRING", "STRING" },
		{ 0x000A, "OCTET_STRING", "OF BYTE" },
		{ 0x000B, "UNICODE_STRING", "OF UINT" },
		{ 0x0010, "INTEGER24", "INT24" },
		{ 0x0011, "REAL64", "LREAL" },
		{ 0x0012, "INTEGER40", "INT40" },
		{ 0x0013, "INTEGER48", "INT48" },
		{ 0x0014, "INTEGER56", "INT56" },
		{ 0x0015, "INTEGER64", "INT64" },
		{ 0x0016, "UNSIGNED24", "UINT24" },
		//{ 0x0017, "Reserved", "" },
		{ 0x0018, "UNSIGNED40", "UINT40" },
		{ 0x0019, "UNSIGNED48", "UINT48" },
		{ 0x001A, "UNSIGNED56", "UINT56" },
		{ 0x001B, "UNSIGNED64", "UINT64" },
		//{ 0x001C-0x001F, "reserved", "" },
		{ 0, NULL, NULL }
	};

	for (struct _coe_datatypes *dt = datatypes; dt->esiname != NULL; dt++) {
		if (strncmp(dt->esiname, xmldatatype, strlen(dt->esiname)) == 0)
			return dt->index;
	}

	return -1; /* unrecognized */
}

static struct _pdo_entry *parse_pdo_entry(xmlNode *val, SiiInfo *sii, int include_pdo_strings)
{
	struct _pdo_entry *entry = calloc(1, sizeof(struct _pdo_entry));

	int tmp = 0;

	for (xmlNode *child = val->children; child; child = child->next) {
		if (xmlStrncmp(child->name, Char2xmlChar("Index"), xmlStrlen(child->name)) == 0) {
			scan_hex_dec((char *)child->children->content, (uint32_t *)&tmp);
			entry->index = tmp&0xffff;
			tmp = 0;
		} else if (xmlStrncmp(child->name, Char2xmlChar("SubIndex"), xmlStrlen(child->name)) == 0) {
			tmp = atoi((char *)child->children->content);
			entry->subindex = tmp&0xff;
			tmp = 0;
		} else if (xmlStrncmp(child->name, Char2xmlChar("BitLen"), xmlStrlen(child->name)) == 0) {
			entry->bit_length = atoi((char *)child->children->content);
		} else if (xmlStrncmp(child->name, Char2xmlChar("Name"), xmlStrlen(child->name)) == 0) {
			/* again, write this to the string category and store index to string here. */
			if (child->children == NULL) {
				fprintf(stderr, "[WARNING] Reading child content of size 0 (line: %d)\n", child->line);
				tmp = -1;
			} else {
				if (include_pdo_strings)
					tmp = sii_strings_add(sii, (char *)child->children->content);
				else
					tmp = 0;
			}

			if (tmp < 0) {
				fprintf(stderr, "Error creating input string!\n");
				entry->string_index = 0;
			} else {
				entry->string_index = (uint8_t)tmp&0xff;
			}
		} else if (xmlStrncmp(child->name, Char2xmlChar("DataType"), xmlStrlen(child->name)) == 0) {
			if (child->children == NULL) {
				fprintf(stderr, "Warning unspecified datatype found. Please check your ESI, datatype is set to 0\n");
				continue;
			}

			int dt = parse_pdo_get_data_type((char *)child->children->content);
			if (dt <= 0)
				fprintf(stderr, "Warning unrecognized esi data type '%s'\n", (char *)child->children->content);
			else
				entry->data_type = (uint8_t)dt;
#if 0 /* doesn't help much, except for debugging */
		} else {
			fprintf(stderr, "Warning, unrecognized pdo setting: '%s'\n",
					(char *)child->name);
#endif
		}
	}

	return entry;
}

static void parse_pdo(xmlNode *current, SiiInfo *sii, int include_pdo_strings)
{
	enum eSection type;
	if (xmlStrcmp(current->name, Char2xmlChar("RxPdo")) == 0)
		type = SII_CAT_RXPDO;
	else if(xmlStrcmp(current->name, Char2xmlChar("TxPdo")) == 0)
		type = SII_CAT_TXPDO;
	else {
		fprintf(stderr, "[%s] Error, no PDO type\n", __func__);
		return;
	}

	struct _sii_cat *cat = calloc(1, sizeof(struct _sii_cat));
	cat->type = type;
	sii_category_add(sii, cat);

	if (cat->data == NULL) {
		cat->data = (void *)calloc(1, sizeof(struct _sii_pdo));
	}

	struct _sii_pdo *pdo = (struct _sii_pdo *)cat->data;
	if (type == SII_CAT_RXPDO)
		pdo->type = RxPDO; //SII_RX_PDO;
	else if (type == SII_CAT_TXPDO)
		pdo->type = TxPDO; //SII_TX_PDO;
	else
		pdo->type = 0;

	size_t pdosize = 8; /* base size of pdo */

	/* get Arguments for syncmanager */
	for (xmlAttr *attr = current->properties; attr; attr = attr->next) {
		if (xmlStrncmp(attr->name, Char2xmlChar("Sm"), xmlStrlen(attr->name)) == 0) {
			pdo->syncmanager = atoi((char *)attr->children->content);
		}
		/* currently fixed is unhandled - check where this setting should reside
		else if (xmlStrncmp(attr->name, Char2xmlChar("Fixed"), xmlStrlen(attr->name))) {
		}
		 */
	}

	/* set uncrecognized values to default */
	pdo->dcsync = 0;
	pdo->flags = 0;

	/* get node Name and node Index */
	/* then parse the pdo list - all <Entry> children */
	for (xmlNode *val = current->children; val; val = val->next) {
		if (xmlStrncmp(val->name, Char2xmlChar("Name"), xmlStrlen(val->name)) == 0) {
			int tmp = 0;
            if (include_pdo_strings)
			    tmp = sii_strings_add(sii, (char *)val->children->content);
			if (tmp < 0) {
				fprintf(stderr, "Error creating input string!\n");
				pdo->name_index = 0;
			} else {
				pdo->name_index = (uint8_t)tmp&0xff;
			}
		} else if (xmlStrncmp(val->name, Char2xmlChar("Index"), xmlStrlen(val->name)) == 0) {
			uint32_t tmp = 0;
			scan_hex_dec((char *)val->children->content, &tmp);
			pdo->index = tmp&0xffff;
		} else if (xmlStrncmp(val->name, Char2xmlChar("Entry"), xmlStrlen(val->name)) == 0) {
			/* add new pdo entry */
			pdo->entries += 1;
			struct _pdo_entry *entry = parse_pdo_entry(val, sii, include_pdo_strings);
			pdo_entry_add(pdo, entry);
			pdosize += 8; /* size of every pdo entry */
		}
	}

	cat->size = pdosize;
}


/* API function */

struct _esi_data *esi_init(const char *file)
{
	if (file == NULL) {
		fprintf(stderr, "Warning, init with empty filename\n");
		return NULL;
	}

	struct _esi_data *esi = (struct _esi_data *)calloc(1, sizeof(struct _esi_data));

	enum eFileType type = efile_type(file);
	switch (type) {
	case SIIBIN:
		/* init with sii bin */
		esi->sii = sii_init_file(file);
		if (esi->sii == NULL) {
			fprintf(stderr, "Failed to read SII bin file '%s'\n", file);
			free(esi);
			return NULL;
		}
		break;

	case XML:
		/* init with xml */
		LIBXML_TEST_VERSION

		esi->sii = sii_init();
		esi->doc = xmlReadFile(file, NULL, 0);
		if (esi->doc == NULL) {
			fprintf(stderr, "Failed to parse XML file '%s'\n", file);
			free(esi->sii);
			free(esi);
			return NULL;
		}
		break;

	case UNKNOWN:
	default:
		/* init empty ??? */
		free(esi);
		esi = NULL;
		break;
	}

	return esi;
}

/* FIXME Input type is already given in the calling function parse_xml_input() */
EsiData *esi_init_string(const unsigned char *buf, size_t size)
{
	EsiData *esi = calloc(1, sizeof(struct _esi_data));

	enum eFileType type = UNKNOWN;

	if (strncmp((const char *)buf, "<?xml", 5) == 0)
		type = XML;
	else
		type = SIIBIN;

	switch (type) {
	case SIIBIN:
		/* init with sii bin */
		esi->sii = sii_init_string(buf, size);
		if (esi->sii == NULL) {
			fprintf(stderr, "Failed to read SII bin.\n");
			free(esi);
			return NULL;
		}
		break;

	case XML:
		/* init with xml */
		LIBXML_TEST_VERSION

		esi->sii = sii_init();
		esi->doc = xmlReadMemory((const char *)buf, size, "noname.xml", NULL, 0);
		if (esi->doc == NULL) {
			fprintf(stderr, "Failed to parse XML.\n");
			sii_release(esi->sii);
			esi_release(esi);
			return NULL;
		}
		break;

	case UNKNOWN:
	default:
		/* init empty ??? */
		free(esi);
		esi = NULL;
		break;
	}

	return esi;
}

void esi_release(struct _esi_data *esi)
{
	xmlFreeDoc(esi->doc);

	if (esi->sii != NULL)
		sii_release(esi->sii);

	if (esi->siifile != NULL)
		free(esi->siifile);

	if (esi->xmlroot != NULL)
		free(esi->xmlroot);

	if (esi->xmlfile != NULL)
		free(esi->xmlfile);

	free(esi);
}

int esi_parse(EsiData *esi, int device_number, int include_pdo_strings)
{
	xmlNode *root = xmlDocGetRootElement(esi->doc);

	/* first, prepare category strings, since this is always needed */
	struct _sii_cat *strings = sii_category_find(esi->sii, SII_CAT_STRINGS);
	if (strings == NULL) {
		strings = calloc(1, sizeof(struct _sii_cat));
		strings->type = SII_CAT_STRINGS;
		struct _sii_strings *strdata = calloc(1, sizeof(struct _sii_strings));
		strings->data = strdata;
		sii_category_add(esi->sii, strings);
	}

	xmlNode *device = search_device(root, device_number);
	if (!device) {
		fprintf(stderr, "Error, invalid device number %d\n", device_number);
		return -1;
	}
	xmlNode *n = search_node(device, "ConfigData");
	esi->sii->preamble = parse_preamble(n);
	esi->sii->config = parse_config(root, device);

	struct _sii_general *general = parse_general(esi->sii, root, device);
	struct _sii_cat *gencat = calloc(1, sizeof(struct _sii_cat));
	gencat->type = SII_CAT_GENERAL;
	gencat->size = sizeof(struct _sii_general);
	gencat->data = (void *)general;
	sii_category_add(esi->sii, gencat);

	/* iterate through children of node 'Device' and get the necessary informations */
	for (xmlNode *current = device->children; current; current = current->next) {
		//printf("[DEBUG %s] start parsing of %s\n", __func__, current->name);
		if (xmlStrncmp(current->name, Char2xmlChar("Fmmu"), xmlStrlen(current->name)) == 0) {
			parse_fmmu(current, esi->sii);
		} else if (xmlStrncmp(current->name, Char2xmlChar("Sm"), xmlStrlen(current->name)) == 0) {
			parse_syncm(current, esi->sii);
#if 0 /* FIXME DC should be added again! */
		} else if (xmlStrncmp(current->name, Char2xmlChar("Dc"), xmlStrlen(current->name)) == 0) {
			parse_dclock(current, esi->sii);
#endif
		} else if (xmlStrncmp(current->name, Char2xmlChar("RxPdo"), xmlStrlen(current->name)) == 0) {
			parse_pdo(current, esi->sii, include_pdo_strings);
		} else if (xmlStrncmp(current->name, Char2xmlChar("TxPdo"), xmlStrlen(current->name)) == 0) {
			parse_pdo(current, esi->sii, include_pdo_strings);
		}
	}

	/* as last action, add the distributed clock settings. */
	get_dc_proto(esi->sii); /* FIXME see comment on sii.c:get_dc_proto() */

	return 0;
}

void esi_print_xml(EsiData *esi)
{
	xmlNode *root = xmlDocGetRootElement(esi->doc);
	//parse_example(root);
	xmlNode *node = search_node(root, "Device");
	printf("\n+++ Printing all nodes +++\n");
	print_all_nodes(node);
	//xmlDocDump(stdout, esi->doc);
}

void esi_print_sii(EsiData *esi)
{
	SiiInfo *sii = esi->sii;
	sii_print(sii);
}

SiiInfo *esi_get_sii(EsiData *esi)
{
	return esi->sii;
}
