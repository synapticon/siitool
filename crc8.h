#ifndef CRC8_H
#define CRC8_H

#include <stdlib.h>
#include <stdint.h>

/* calculates crc-8 sum for each byte given by msg */
uint8_t crc8byte(uint8_t *crc, uint8_t msg);

/* calculates crc-8 sum for array of bytes */
uint8_t crc8(const uint8_t *msg, size_t blen);

#endif /* CRC8_H */
