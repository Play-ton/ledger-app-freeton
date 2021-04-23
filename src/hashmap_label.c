#include "hashmap_label.h"
#include "cell.h"
#include "slice_data.h"
#include "utils.h"
#include "errors.h"

uint8_t get_label_same(uint8_t max, struct SliceData_t* slice, struct SliceData_t* label) {
    uint8_t value = SliceData_get_next_bit(slice) ? 0xff : 0;
    uint8_t length = SliceData_get_next_size(slice, max);
    VALIDATE(length <= 64, ERR_RANGE_CHECK);

    uint8_t length_bytes = length / 8 + (length % 8 ? 1 : 0);
    SliceData_fill(label, value,  length_bytes);
    SliceData_truncate(label, length);
    return length >= max ? 0 : (max - length);
}

uint8_t get_label(uint8_t max, struct SliceData_t* slice, struct SliceData_t* label) {
    VALIDATE(SliceData_is_empty(slice) == false, ERR_SLICE_IS_EMPTY);
    VALIDATE(SliceData_get_next_bit(slice) == 1, ERR_WRONG_LABEL); // label short
    VALIDATE(SliceData_get_next_bit(slice) == 1, ERR_WRONG_LABEL); // label long

    return get_label_same(max, slice, label);
}

void put_to_node(uint8_t cell_index, uint16_t bit_len, struct SliceData_t* key) {
    VALIDATE(cell_index != 0 && cell_index <= MAX_CONTRACT_CELLS_COUNT, ERR_INVALID_DATA);
    static const uint8_t key_len_bytes = 8;
    VALIDATE(key && key->data_size_bytes == key_len_bytes, ERR_RANGE_CHECK);
    for (uint8_t i = 0; i < key_len_bytes; ++i) {
        VALIDATE(key->data[i] == 0, ERR_INVALID_KEY);
    }

    Cell_t* cell = &boc_context.cells[cell_index];
    SliceData_t slice;
    SliceData_init(&slice, Cell_get_data(cell), Cell_get_data_size(cell));

    SliceData_t label;
    uint8_t label_data[8];
    os_memset(label_data, 0, sizeof(label_data));
    SliceData_init(&label, label_data, sizeof(label_data));
    get_label(bit_len, &slice, &label);

    if (SliceData_equal(&label, key)) {
        uint8_t len = 16 - leading_zeros(bit_len);
        uint8_t label_size_bits = 2 + 1 + len; // prefix + key bit + len
        boc_context.public_key_label_size_bits = label_size_bits;
        boc_context.public_key_cell_index = cell_index;
        return;
    }

    // common prefix
    {
        uint8_t max_prefix_len = MIN(SliceData_remaining_bits(&label), SliceData_remaining_bits(key));
        VALIDATE(max_prefix_len <= 64, ERR_RANGE_CHECK);
        uint8_t i = 0;
        while (i < max_prefix_len && SliceData_get_bits(&label, i, 1) == SliceData_get_bits(key, i, 1)) {
            i += 1;
        }

        SliceData_move_by(key, i);
        SliceData_truncate(&label, i);
        uint8_t label_rb = SliceData_remaining_bits(&label);
        VALIDATE(bit_len >= label_rb, ERR_CELL_UNDERFLOW);
        bit_len -= label_rb;
    }

    VALIDATE(bit_len >= 1, ERR_CELL_UNDERFLOW);
    uint8_t next_index = SliceData_get_next_bit(key);
    VALIDATE(next_index == 0, ERR_INVALID_KEY);

    uint8_t refs_count = 0;
    uint8_t* refs = Cell_get_refs(&boc_context.cells[cell_index], &refs_count);
    VALIDATE(refs_count > 0 && refs_count <= MAX_REFERENCES_COUNT, ERR_INVALID_DATA);
    uint8_t next_cell = refs[next_index];
    VALIDATE(next_cell != 0 && next_cell <= MAX_CONTRACT_CELLS_COUNT, ERR_INVALID_DATA);
    bit_len -= 1;

    return put_to_node(next_cell, bit_len, key);
}
