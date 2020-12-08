// part of hashmap interface intended to insert a public key
#ifndef _HASHMAP_LABEL_H_
#define _HASHMAP_LABEL_H_

#include <stdint.h>

struct SliceData_t;
// expects that key is 64 bit of zeros
void put_to_node(uint8_t cell_index, uint16_t bit_len, struct SliceData_t* key);

#endif
