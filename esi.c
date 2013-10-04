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

static void print_all_nodes(xmlNode *root)
{
	//xmlNode *node = root;

	for (xmlNode *current = root; current; current = current->next) {
		if (current->type == XML_ELEMENT_NODE)
			printf("node type: element, name = %s\n", current->name);

		print_all_nodes(current->children);
	}
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

void esi_print_xml(EsiData *esi)
{
	xmlNode *root = xmlDocGetRootElement(esi->doc);
	print_all_nodes(root);
	xmlDocDump(stdout, esi->doc);
}
