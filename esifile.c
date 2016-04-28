#include "esifile.h"
#include <stdio.h>
#include <string.h>


const char *efile_suffix(const char *file)
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

enum eFileType efile_type(const char *file)
{
	enum eFileType type = UNKNOWN;

	// - file ending (.bin or .xml)
	// - first 4 bytes (?) -> "<?xml" or binary bits
	const char *suffix = efile_suffix(file);
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
	char *ret = fgets(foo, 5, fh);
	fclose(fh);

	if ((ret != NULL) && (strncmp(foo, "<?xml", 5) == 0)) {
		if (type != XML)
			type = UNKNOWN;
	} else {
		type = UNKNOWN;
	}

	return type;
}

