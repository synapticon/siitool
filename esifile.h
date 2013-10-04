/* esifile - functions for filehandling
 */

#ifndef ESIFILE_H
#define ESIFILE_H

enum eFileType {
	UNKNOWN =0
	,SIIBIN
	,XML
};

const char *efile_suffix(const char *file);

enum eFileType efile_type(const char *file);

#endif /* ESIFILE_H */
