/* ESI - EtherCAT Slave Information
 */
#include "esi.h"
#include "sii.h"

#include <stdio.h>
#include <string.h>

#include <libxml/parser.h>
#include <libxml/tree.h>

enum eFileType {
	UNKNOWN =0
	,SIIBIN
	,XML
};

struct _esi_data {
	SiiInfo *sii;
	char *siifile; /* also opt for sii->outfile */
	xmlDocPtr doc; /* do I need both? */
	xmlNode *xmlroot;
	char *xmlfile;
};

const char *get_filesuffix(const char *file)
{
	const char *suffix = file;
	const char *f = file;

	while (*f != '\0') {
		if (*f == '.')
			suffix = f+1;
		f++;
	}

	return suffix;
}

enum eFileType get_filetype(const char *file)
{
	enum eFileType type = UNKNOWN;

	// - file ending (.bin or .xml)
	// - first 4 bytes (?) -> "<?xml" or binary bits
	const char *suffix = get_filesuffix(file);
	if (suffix == NULL) {
		printf("Warning no suffix\n");
		type = UNKNOWN;
	} else if (strncmp(suffix, "xml", 3) == 0) {
		type = XML;
	} else if (strncmp(suffix, "bin", 3) == 0) {
		type = SIIBIN;
	} else {
		type = UNKNOWN;
	}

	FILE *fh = fopen(file, "r");
	char foo[7];
	fgets(foo, 5, fh);
	fclose(fh);

	if (strncmp(foo, "<?xml", 5) == 0) {
		if (type != XML)
			type = UNKNOWN;
	}

	return type;
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
