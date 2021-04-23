#ifndef _CELL_H_
#define _CELL_H_

#include <stdint.h>
#include <stdbool.h>

#define CELL_DATA_OFFSET 2
#define MAX_REFERENCES_COUNT 4

typedef struct Cell_t {
    uint8_t* cell_begin;
} Cell_t;

void Cell_init(struct Cell_t* self, uint8_t* cell_begin);
uint8_t Cell_get_d1(const struct Cell_t* self);
uint8_t Cell_get_d2(const struct Cell_t* self);
uint8_t Cell_get_data_size(const struct Cell_t* self);
uint8_t* Cell_get_data(const struct Cell_t* self);
uint8_t* Cell_get_refs(const struct Cell_t* self, uint8_t* refs_count);
uint16_t deserialize_cell(struct Cell_t* cell, const uint8_t cell_index, const uint8_t cells_count);
void calc_cell_hash(Cell_t* cell, const uint8_t cell_index);

#endif
