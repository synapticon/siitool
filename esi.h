/* ESI - EtherCAT Slave Information
 *
 * 2013-10-04 Frank Jeschke <fjeschke@synapticon.com>
 */

#ifndef ESI_H
#define ESI_H

typedef struct _esi_data EsiData;

EsiData *esi_init(const char *file);
void esi_release(EsiData *esi);

#endif /* ESI_H */
