#ifndef _CONTRACT_H_
#define _CONTRACT_H_

#include "cell.h"

#include <stdint.h>
#include <stdbool.h>

struct ByteStream_t;
void deserialize_cells_tree(struct ByteStream_t* src);
void get_address(const uint32_t account_number, uint8_t* address);

#endif
