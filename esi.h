/* ESI - EtherCAT Slave Information
 *
 * 2013-10-04 Frank Jeschke <fjeschke@synapticon.com>
 */

#ifndef ESI_H
#define ESI_H

#include <unistd.h>

typedef struct _esi_data EsiData;

EsiData *esi_init(const char *file);
EsiData *esi_init_file(const char *file);
EsiData *esi_init_string(const unsigned char *file, size_t size);

void esi_release(EsiData *esi);

void esi_print_xml(EsiData *esi);

int esi_parse(EsiData *esi);

#endif /* ESI_H */
