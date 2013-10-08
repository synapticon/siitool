/* ESI - EtherCAT Slave Information
 */
#include "esi.h"
#include "esifile.h"
#include "sii.h"
#include "stringlist.h"

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
	StringList *strings;
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
	printf("%d: type: %s name = %s, content = '%s'\n",
		node->line, type2str(node->type), node->name, node->content);
}

static void print_all_nodes(xmlNode *root)
{
	//xmlNode *node = root;

	for (xmlNode *current = root; current; current = current->next) {
		if (current->type == XML_ELEMENT_NODE)
			printf("node type: element, name = %s\n", current->name);

		print_all_nodes(current->children);
	}
}

static void parse_example(xmlNode *root, StringList *strings)
{
	if (strings != NULL)
		strings = NULL;

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

		parse_example(current->children, NULL);
	}
}

static uint16_t preamble_crc8(struct _sii_preamble *pa)
{
	if (pa == NULL)
		return 0;

	/* FIXME add crc calculation! */
	return 0x2f;
}

static xmlNode *searchNode(xmlNode *root, const char *name)
{
	xmlNode *tmp = NULL;

	for (xmlNode *curr = root; curr; curr = curr->next) {
		if (curr->type == XML_ELEMENT_NODE &&
			strncmp((const char *)curr->name, name, strlen(name)) == 0)
			return curr;


		tmp = searchNode(curr->children, name);
		if (tmp != NULL)
			return tmp;
	}

	return NULL;
}

/* parse xml functions */
static struct _sii_preamble *parse_preamble(xmlNode *root, StringList *strings)
{
	struct _sii_preamble *pa = malloc(sizeof(struct _sii_preamble));

	return pa;
}

static struct _sii_stdconfig *parse_config(xmlNode *root, StringList *strings)
{
	struct _sii_stdconfig *sc = malloc(sizeof(struct _sii_stdconfig));

	return sc;
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

	xmlNode *n = searchNode(root, "ConfigData");
	struct _sii_preamble *preamble = parse_preamble(n);
	//struct _sii_stdconfig *stdconfig = parse_config(root, esi->strings);


	return -1;
}

void esi_print_xml(EsiData *esi)
{
	xmlNode *root = xmlDocGetRootElement(esi->doc);
	//parse_example(root, NULL);
	xmlNode *node = searchNode(root, "Device");
	printf("\n+++ Printing all nodes +++\n");
	print_all_nodes(node);
	//xmlDocDump(stdout, esi->doc);
}
