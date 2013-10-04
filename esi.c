
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

	enum eFileType type = get_filetype(file);
	switch (type) {
	case SIIBIN:
		/* init with sii bin */
		esi->sii = sii_init_file(file);
		break;
	case XML:
		/* init with xml */
		esi->sii = sii_init();
		esi->doc = xmlReadFile(file, NULL, 0);
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
