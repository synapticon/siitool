/* ESI - EtherCAT Slave Information
 */
#include "esi.h"
#include "esifile.h"
#include "sii.h"

#include <stdio.h>
#include <string.h>

#include <libxml/parser.h>
#include <libxml/tree.h>

struct _esi_data {
	SiiInfo *sii;
	char *siifile; /* also opt for sii->outfile */
	xmlDocPtr doc; /* do I need both? */
	xmlNode *xmlroot;
	char *xmlfile;
};

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

static void print_node(xmlNode *node)
{
	if ((node->type == XML_TEXT_NODE) && (0x0a == *(node->content)))
		return; // skip line feed node

	printf("%d: type: %s name = %s, content = '%s'\n",
		node->line, type2str(node->type), node->name, node->content);
}

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

static uint16_t preamble_crc8(struct _sii_preamble *pa)
{
	if (pa == NULL)
		return 0;

	/* FIXME add crc calculation! */
	return 0x2f;
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

static xmlNode *search_children(xmlNode *root, const char *name)
{
	for (xmlNode *curr = root->children; curr; curr = curr->next) {
		if (curr->type == XML_ELEMENT_NODE &&
				xmlStrncmp(curr->name, (xmlChar *)name, xmlStrlen(curr->name)) == 0)
			return curr;
	}

	return NULL;
}

/* TODO: Add function to search for all nodes named by 'name' (e.g. multiple <Sm>-Tags */

/* functions to parse xml */
static struct _sii_preamble *parse_preamble(xmlNode *node)
{
	struct _sii_preamble *pa = malloc(sizeof(struct _sii_preamble));

	char string[1024];
	strncpy(string, (char *)node->children->content, 1024);

	unsigned int lowbyte=0, highbyte=0;
	char *b = string;
	sscanf(b, "%2x%2x", &highbyte, &lowbyte);
	pa->pdi_ctrl = BYTES_TO_WORD(highbyte, lowbyte);
	b+=4;

	sscanf(b, "%2x%2x", &highbyte, &lowbyte);
	pa->pdi_conf = BYTES_TO_WORD(highbyte, lowbyte);
	b+=4;

	sscanf(b, "%2x%2x", &highbyte, &lowbyte);
	pa->sync_impulse = BYTES_TO_WORD(highbyte, lowbyte);
	b+=4;

	sscanf(b, "%2x%2x", &highbyte, &lowbyte);
	pa->pdi_conf2 = BYTES_TO_WORD(highbyte, lowbyte);
	b+=4;

	sscanf(b, "%2x%2x", &highbyte, &lowbyte);
	pa->alias = BYTES_TO_WORD(highbyte, lowbyte);
	b+=4;

	for (int i=0; i<4; i++)
		pa->reserved[i] = 0x00;

	pa->checksum = 0xff; /* set initial checksum */
	pa->checksum = preamble_crc8(pa);

	return pa;
}

static struct _sii_stdconfig *parse_config(xmlNode *root)
{
	xmlNode *n, *tmp;

	struct _sii_stdconfig *sc = malloc(sizeof(struct _sii_stdconfig));

	//xmlNode *n = search_node(root, "Vendor");
	n = search_node(root, "Vendor");
	if (n==NULL)
		return NULL;

	tmp = search_node(n, "Id");
	//char *vendoridstr = tmp->children->content;

	/* get id */
	// FIXME add some error and validty checking, esp. if node is set correctly and has the type
	sscanf((const char *)tmp->children->content, "#x%x", &(sc->vendor_id));

	n = search_node(search_node(root, "Devices"), "Device");

	tmp = search_node(n, "Type");
	xmlAttr *prop = tmp->properties;

	while (prop != NULL) {
		if (xmlStrncmp(prop->name, xmlCharStrdup("ProductCode"), xmlStrlen(prop->name))) {
			sscanf((const char *)prop->children->content, "#x%x", &(sc->product_id));
		}

		if (xmlStrncmp(prop->name, xmlCharStrdup("RevisionNo"), xmlStrlen(prop->name))) {
			sscanf((const char *)prop->children->content, "#x%x", &(sc->revision_id));
		}

		prop = prop->next;
	}

	sc->serial = 0; /* FIXME the serial number is not in the esi? */

	/* FIXME where are the bootstrap settings? */
	sc->bs_rec_mbox_offset = 0x1000;
	sc->bs_rec_mbox_size = 532;
	sc->bs_snd_mbox_offset = 0x1800;
	sc->bs_snd_mbox_size = 532;

	sc->std_rec_mbox_offset = 0x000a;
	sc->std_rec_mbox_size = 0xf;
	sc->std_snd_mbox_offset = 0x000b;
	sc->std_snd_mbox_size = 0xf;

	/* filter the std mailbox configuration; from <Sm> description */

	tmp = n->children;
	while (tmp != NULL) {
		if (xmlStrncmp(tmp->name, xmlCharStrdup("Sm"), xmlStrlen(tmp->name)) == 0) {
			xmlNode *smchild = tmp->children;
			while (smchild != NULL) {
				if (xmlStrncmp(smchild->content, xmlCharStrdup("MBoxOut"), xmlStrlen(smchild->content))== 0) {
					xmlAttr *p = tmp->properties;
					while (p!=NULL) {
						if (xmlStrncmp(p->name, xmlCharStrdup("DefaultSize"), xmlStrlen(p->name)) == 0) {
							sc->std_rec_mbox_size = (uint16_t)atoi((const char *)p->children->content);
						}

						if (xmlStrncmp(p->name, xmlCharStrdup("StartAddress"), xmlStrlen(p->name)) == 0) {
							unsigned int atmp = 0;
							sscanf((const char *)p->children->content, "#x%x", &atmp);
							sc->std_rec_mbox_offset = atmp;
						}

						p = p->next;
					}

				}

				if (xmlStrncmp(smchild->content, xmlCharStrdup("MBoxIn"), xmlStrlen(smchild->content))== 0) {
					xmlAttr *p = tmp->properties;
					while (p!=NULL) {
						if (xmlStrncmp(p->name, xmlCharStrdup("DefaultSize"), xmlStrlen(p->name)) == 0) {
							sc->std_snd_mbox_size = (uint16_t)atoi((const char *)p->children->content);
						}

						if (xmlStrncmp(p->name, xmlCharStrdup("StartAddress"), xmlStrlen(p->name)) == 0) {
							unsigned int atmp = 0;
							sscanf((const char *)p->children->content, "#x%x", &atmp);
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

	tmp = search_node(n, "Mailbox");
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
	sc->eeprom_size = atoi((char *)tmp->children->content)/1024;
	sc->version = 1; /* also not in Esi */

	return sc;
}

static struct _sii_general *parse_general(xmlNode *root)
{
	xmlNode *parent;
	xmlNode *node;
	xmlNode *tmp;
	struct _sii_general *general = malloc(sizeof(struct _sii_general));

	/* FIXME handle string indexes */

	/* fetch CoE details */
	//printf("[DEBUG %s] get CoE details\n", __func__);
	parent = search_node(root, "Device");

	node = search_children(parent, "Mailbox");
	tmp = search_node(node, "CoE");
	if (tmp != NULL) {
		general->coe_enable_sdo = 1;
		/* parse the attributes */
		for (xmlAttr *attr = tmp->properties; attr; attr = attr->next) {
			if (xmlStrcmp(attr->name, xmlCharStrdup("SdoInfo")) == 0)
				general->coe_enable_sdo_info = atoi((char *)attr->children->content);

			if (xmlStrcmp(attr->name, xmlCharStrdup("PdoAssign")) == 0)
				general->coe_enable_pdo_assign = atoi((char *)attr->children->content);

			if (xmlStrcmp(attr->name, xmlCharStrdup("PdoConfig")) == 0)
				general->coe_enable_pdo_conf = atoi((char *)attr->children->content);

			if (xmlStrcmp(attr->name, xmlCharStrdup("PdoUpload")) == 0)
				general->coe_enable_upload_start = atoi((char *)attr->children->content);

			if (xmlStrcmp(attr->name, xmlCharStrdup("??sdoComplete")) == 0)
				general->coe_enable_sdo_complete = atoi((char *)attr->children->content);

			//attr = attr->next;
		}
	}

	tmp = search_node(node, "EoE");
	if (tmp != NULL)
		general->eoe_enabled = 1;

	tmp = search_node(node, "FoE");
	if (tmp != NULL)
		general->foe_enabled = 1;

	/* FIXME where are the physical port settings? */

	return general;
}

static void parse_fmmu(xmlNode *current, SiiInfo *sii)
{
	struct _sii_cat *cat = sii_category_find(sii, SII_CAT_FMMU);
	if (cat == NULL) { /* create new category */
		cat = malloc(sizeof(struct _sii_cat));
		cat->next = NULL;
		cat->prev = NULL;
		cat->data = NULL;
		cat->type = SII_CAT_FMMU;
		sii_category_add(sii, cat);
	}

	if (cat->data == NULL) { /* if category exists but doesn't contain any data */
		cat->data = (void *)malloc(sizeof(struct _sii_fmmu));
		memset(cat->data, 0, sizeof(struct _sii_fmmu));
	}

	struct _sii_fmmu *fmmu = (struct _sii_fmmu *)cat->data;

	/* now fetch the data */
	xmlChar *content = current->children->content;
	if (xmlStrncmp(content, xmlCharStrdup("Inputs"), xmlStrlen(content)))
		fmmu_add_entry(fmmu, FMMU_INPUTS);
	else if (xmlStrncmp(content, xmlCharStrdup("Outputs"), xmlStrlen(content)))
		fmmu_add_entry(fmmu, FMMU_OUTPUTS);
	else if (xmlStrncmp(content, xmlCharStrdup("SynmanagerStat"), xmlStrlen(content))) //FIXME what is the valid content?
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
		cat = malloc(sizeof(struct _sii_cat));
		cat->next = NULL;
		cat->prev = NULL;
		cat->data = NULL;
		cat->type = SII_CAT_SYNCM;
		sii_category_add(sii, cat);
	}

	if (cat->data == NULL) {
		cat->data = (void *)malloc(sizeof(struct _sii_syncm));
		memset(cat->data, 0, sizeof(struct _sii_syncm));
	}

	/* now fetch the data */
	//size_t smsize = 0;
	struct _sii_syncm *sm = (struct _sii_syncm *)cat->data;
	struct _syncm_entry *entry = malloc(sizeof(struct _syncm_entry));

	/* FIXME add entries */
	xmlAttr *args = current->properties;
	for (xmlAttr *a = args; a ; a = a->next) {
		if (xmlStrncmp(a->name, xmlCharStrdup("DefaultSize"), xmlStrlen(a->name)) == 0) {
			entry->length = atoi((char *)a->children->content);
		} else if (xmlStrncmp(a->name, xmlCharStrdup("StartAddress"), xmlStrlen(a->name)) == 0) {
			int tmp = 0;
			sscanf((char *)a->children->content, "#x%x", &tmp);
			entry->phys_address = tmp&0xffff;
		} else if (xmlStrncmp(a->name, xmlCharStrdup("ControlByte"), xmlStrlen(a->name)) == 0) {
			int tmp = 0;
			sscanf((char *)a->children->content, "#x%x", &tmp);
			entry->control = tmp&0xff;
		} else if (xmlStrncmp(a->name, xmlCharStrdup("Enable"), xmlStrlen(a->name)) == 0) {
			entry->enable = atoi((char *)a->children->content);
		}
	}

	entry->status = 0; /* don't care */

	/* type is encoded in the value of the node */
	xmlChar *type = current->children->content;
	if (xmlStrncmp(type, xmlCharStrdup("MBoxIn"), xmlStrlen(type)) == 0)
		entry->type = SMT_MBOXIN;
	else if (xmlStrncmp(type, xmlCharStrdup("MBoxOut"), xmlStrlen(type)) == 0)
		entry->type = SMT_MBOXOUT;
	else if (xmlStrncmp(type, xmlCharStrdup("Inputs"), xmlStrlen(type)) == 0)
		entry->type = SMT_INPUTS;
	else if (xmlStrncmp(type, xmlCharStrdup("Outputs"), xmlStrlen(type)) == 0)
		entry->type = SMT_OUTPUTS;
	else
		entry->type = SMT_UNUSED;

	syncm_entry_add(sm, entry);
	cat->size += 8; /* a syncmanager entry is 8 bytes */
}

static void parse_dclock(xmlNode *current, SiiInfo *sii)
{
	struct _sii_cat *cat = malloc(sizeof(struct _sii_cat));
	cat->next = NULL;
	cat->prev = NULL;
	cat->type = SII_CAT_DCLOCK;

	/* now fetch the data */
	size_t dcsize = 0;
	struct _sii_dclock *dc = malloc(sizeof(struct _sii_dclock));
	memset(dc, 0, sizeof(struct _sii_dclock));

	/* FIXME add entries, only use the first (default) OpMode */
	printf("[DEBUG %s] FIXME implementation of dclock parsing is incomplete!\n", __func__);

	xmlNode *op = search_node(current, "OpMode");
	for (xmlNode *vals = op->children; vals; vals = vals->next) {
		if (xmlStrncmp(vals->name, xmlCharStrdup("Name"), xmlStrlen(vals->name))) {
			//fprintf(stderr, "[DistributedClock] Warning, unknown handling of '%s'\n", (char *)vals->name);
		} else if (xmlStrncmp(vals->name, xmlCharStrdup("Desc"), xmlStrlen(vals->name))) {
			//fprintf(stderr, "[DistributedClock] Warning, unknown handling of '%s'\n", (char *)vals->name);
		} else if (xmlStrncmp(vals->name, xmlCharStrdup("AssignActivate"), xmlStrlen(vals->name))) {
			//fprintf(stderr, "[DistributedClock] Warning, unknown handling of '%s'\n", (char *)vals->name);
		} else if (xmlStrncmp(vals->name, xmlCharStrdup("CycleTimeSync0"), xmlStrlen(vals->name))) {
			int tmp = atoi((char *)vals->children->content);
			dc->sync0_cycle_time = (uint32_t)tmp;
		} else if (xmlStrncmp(vals->name, xmlCharStrdup("ShiftTimeSync0"), xmlStrlen(vals->name))) {
			//fprintf(stderr, "[DistributedClock] Warning, unknown handling of '%s'\n", (char *)vals->name);
		} else if (xmlStrncmp(vals->name, xmlCharStrdup("CycleTimeSync1"), xmlStrlen(vals->name))) {
			int tmp = atoi((char *)vals->children->content);
			dc->sync1_cycle_time = (uint32_t)tmp;
		} else if (xmlStrncmp(vals->name, xmlCharStrdup("ShiftTimeSync1"), xmlStrlen(vals->name))) {
			//fprintf(stderr, "[DistributedClock] Warning, unknown handling of '%s'\n", (char *)vals->name);
		} else {
			//fprintf(stderr, "[DistributedClock] Warning, unknown handling of '%s'\n", (char *)vals->name);
		}
	}

	cat->data = (void *)dc;
	cat->size = dcsize;
	sii_category_add(sii, cat);
}

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
		{ 0x0001, "BOOLEAN", "" },
		{ 0x0002, "INTEGER8", "" },
		{ 0x0003, "INTEGER16", "" },
		{ 0x0004, "INTEGER32", "" },
		{ 0x0005, "UNSIGNED8", "USINT" },
		{ 0x0006, "UNSIGNED16", "UINT" },
		{ 0x0007, "UNSIGNED32", "UDINT" },
		{ 0x0008, "REAL32", "" },
		{ 0x0009, "VISIBLE_STRING", "" },
		{ 0x000A, "OCTET_STRING", "" },
		{ 0x000B, "UNICODE_STRING", "" },
		{ 0x000C, "TIME_OF_DAY", "" },
		{ 0x000D, "TIME_DIFFERENCE", "" },
		//{ 0x000E, "Reserved", "" },
		{ 0x000F, "DOMAIN", "" },
		{ 0x0010, "INTEGER24", "" },
		{ 0x0011, "REAL64", "" },
		{ 0x0012, "INTEGER40", "" },
		{ 0x0013, "INTEGER48", "" },
		{ 0x0014, "INTEGER56", "" },
		{ 0x0015, "INTEGER64", "" },
		{ 0x0016, "UNSIGNED24", "" },
		//{ 0x0017, "Reserved", "" },
		{ 0x0018, "UNSIGNED40", "" },
		{ 0x0019, "UNSIGNED48", "" },
		{ 0x001A, "UNSIGNED56", "" },
		{ 0x001B, "UNSIGNED64", "" },
		//{ 0x001C-0x001F, "reserved", "" },
		{ 0, NULL, NULL }
	};

	for (struct _coe_datatypes *dt = datatypes; dt->esiname != NULL; dt++) {
		if (strncmp(dt->esiname, xmldatatype, strlen(dt->esiname)) == 0)
			return dt->index;
	}

	return -1; /* unrecognized */
}

static struct _pdo_entry *parse_pdo_entry(xmlNode *val, SiiInfo *sii)
{
	struct _pdo_entry *entry = malloc(sizeof(struct _pdo_entry));
	int tmp = 0;

	for (xmlNode *child = val->children; child; child = child->next) {
		if (xmlStrncmp(child->name, xmlCharStrdup("Index"), xmlStrlen(child->name)) == 0) {
			sscanf((char *)child->children->content, "#x%x", &tmp);
			entry->index = tmp&0xffff;
			tmp = 0;
		} else if (xmlStrncmp(child->name, xmlCharStrdup("SubIndex"), xmlStrlen(child->name)) == 0) {
			tmp = atoi((char *)child->children->content);
			entry->subindex = tmp&0xff;
			tmp = 0;
		} else if (xmlStrncmp(child->name, xmlCharStrdup("BitLen"), xmlStrlen(child->name)) == 0) {
			entry->bit_length = atoi((char *)child->children->content);
		} else if (xmlStrncmp(child->name, xmlCharStrdup("Name"), xmlStrlen(child->name)) == 0) {
			/* again, write this to the string category and store index to string here. */
			sii_strings_add(sii, (char *)child->name);
		} else if (xmlStrncmp(child->name, xmlCharStrdup("DataType"), xmlStrlen(child->name)) == 0) {
			int dt = parse_pdo_get_data_type((char *)child->children->content);
			if (dt <= 0)
				fprintf(stderr, "Warning unrecognized esi data type '%s'\n", (char *)child->children->content);
			else
				entry->data_type = (uint8_t)dt;
		} else {
			fprintf(stderr, "Warning, unrecognized pdo setting: '%s'\n",
					(char *)child->name);
		}
	}

	return entry;
}

static void parse_pdo(xmlNode *current, SiiInfo *sii)
{
	enum ePdoType type;
	if (xmlStrcmp(current->name, xmlCharStrdup("RxPdo")) == 0)
		type = SII_CAT_RXPDO;
	else if(xmlStrcmp(current->name, xmlCharStrdup("TxPdo")) == 0)
		type = SII_CAT_TXPDO;
	else {
		fprintf(stderr, "[%s] Error, no PDO type\n", __func__);
		return;
	}

	struct _sii_cat *cat = sii_category_find(sii, type);
	if (cat == NULL) {
		cat = malloc(sizeof(struct _sii_cat));
		cat->next = NULL;
		cat->prev = NULL;
		cat->data = NULL;
		cat->type = type;
		sii_category_add(sii, cat);
	}

	if (cat->data == NULL) {
		cat->data = (void *)malloc(sizeof(struct _sii_pdo));
		memset(cat->data, 0, sizeof(struct _sii_pdo));
	}

	struct _sii_pdo *pdo = (struct _sii_pdo *)cat->data;

	size_t pdosize = 0;

	/* get Arguments for syncmanager */
	for (xmlAttr *attr = current->properties; attr; attr = attr->next) {
		if (xmlStrncmp(attr->name, xmlCharStrdup("Sm"), xmlStrlen(attr->name)) == 0) {
			pdo->syncmanager = atoi((char *)attr->children->content);
		}
		/* currently fixed is unhandled - check where this setting should reside
		else if (xmlStrncmp(attr->name, xmlCharStrdup("Fixed"), xmlStrlen(attr->name))) {
		}
		 */
	}

	/* set uncrecognized values to default */
	pdo->dcsync = 0;
	pdo->flags = 0;

	/* get node Name and node Index */
	/* then parse the pdo list - all <Entry> children */
	for (xmlNode *val = current->children; val; val = val->next) {
		if (xmlStrncmp(val->name, xmlCharStrdup("Name"), xmlStrlen(val->name)) == 0) {
			/* add string to category strings and store index to pdo->name_index */
		} else if (xmlStrncmp(val->name, xmlCharStrdup("Index"), xmlStrlen(val->name)) == 0) {
			int tmp = 0;
			sscanf((char *)val->children->content, "#x%x", &tmp);
			pdo->index = tmp&0xffff;
		} else if (xmlStrncmp(val->name, xmlCharStrdup("Entry"), xmlStrlen(val->name)) == 0) {
			/* add new pdo entry */
			pdo->entries += 1;
			struct _pdo_entry *entry = parse_pdo_entry(val, sii);
			pdo_entry_add(pdo, entry);
		}
	}

	cat->size = pdosize;
}


/* API function */

struct _esi_data *esi_init(const char *file)
{
	struct _esi_data *esi = (struct _esi_data *)malloc(sizeof(struct _esi_data));

	if (file == NULL) {
		fprintf(stderr, "Warning, init with empty filename\n");
		free(esi);
		return NULL;
	}

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

EsiData *esi_init_string(const unsigned char *buf, size_t size)
{
	EsiData *esi = malloc(sizeof(struct _esi_data));

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
		esi->doc = xmlReadMemory((const char *)buf, size, "newsii.xml", NULL, 0);
		if (esi->doc == NULL) {
			fprintf(stderr, "Failed to parse XML.\n");
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

void esi_release(struct _esi_data *esi)
{
	sii_release(esi->sii);
	xmlFreeDoc(esi->doc);
	free(esi);
}

int esi_parse(EsiData *esi)
{
	xmlNode *root = xmlDocGetRootElement(esi->doc);

	/* first, prepare category strings, since this is always needed */
	struct _sii_cat *strings = sii_category_find(esi->sii, SII_CAT_STRINGS);
	if (strings == NULL) {
		strings = malloc(sizeof(struct _sii_cat));
		strings->next = NULL;
		strings->prev = NULL;
		strings->size = 0;
		strings->type = SII_CAT_STRINGS;
		strings->data = NULL;
		sii_category_add(esi->sii, strings);
	}

	xmlNode *n = search_node(root, "ConfigData");
	esi->sii->preamble = parse_preamble(n);
	esi->sii->config = parse_config(root);

	struct _sii_general *general = parse_general(root);
	struct _sii_cat *gencat = malloc(sizeof(struct _sii_cat));
	gencat->next = NULL;
	gencat->prev = NULL;
	gencat->type = SII_CAT_GENERAL;
	gencat->size = sizeof(struct _sii_general); /* FIXME CHECK is this size valid? */
	gencat->data = (void *)general;
	sii_category_add(esi->sii, gencat);

	/* get the first device */
	xmlNode *device = search_node(search_node(root, "Devices"), "Device");

	/* iterate through children of node 'Device' and get the necessary informations */
	for (xmlNode *current = device->children; current; current = current->next) {
		//printf("[DEBUG %s] start parsing of %s\n", __func__, current->name);
		if (xmlStrncmp(current->name, xmlCharStrdup("Fmmu"), xmlStrlen(current->name)) == 0) {
			parse_fmmu(current, esi->sii);
		} else if (xmlStrncmp(current->name, xmlCharStrdup("Sm"), xmlStrlen(current->name)) == 0) {
			parse_syncm(current, esi->sii);
		} else if (xmlStrncmp(current->name, xmlCharStrdup("Dc"), xmlStrlen(current->name)) == 0) {
			parse_dclock(current, esi->sii);
		} else if (xmlStrncmp(current->name, xmlCharStrdup("RxPdo"), xmlStrlen(current->name)) == 0) {
			parse_pdo(current, esi->sii);
		} else if (xmlStrncmp(current->name, xmlCharStrdup("TxPdo"), xmlStrlen(current->name)) == 0) {
			parse_pdo(current, esi->sii);
		}
	}

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
