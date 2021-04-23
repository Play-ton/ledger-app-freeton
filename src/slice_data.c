#include "slice_data.h"
#include "globals.h"
#include "utils.h"
#include "cell.h"

void SliceData_init(struct SliceData_t* self, uint8_t* data, uint16_t data_size_bytes) {
    VALIDATE(self && data, ERR_SLICE_IS_EMPTY);
    self->data = data;
    self->data_window_start = 0;
    self->data_window_end = data_size_bytes * 8;
    self->data_size_bytes = data_size_bytes;
}

void SliceData_from_cell(struct SliceData_t* self, struct Cell_t* cell) {
    uint8_t* cell_data = Cell_get_data(cell);
    uint8_t cell_data_size = Cell_get_data_size(cell);
    SliceData_init(self, cell_data, cell_data_size);
}

void SliceData_fill(struct SliceData_t* self, uint8_t value, uint16_t data_size_bytes) {
    VALIDATE(self && self->data, ERR_SLICE_IS_EMPTY);
    VALIDATE(self->data_size_bytes >= data_size_bytes, ERR_INVALID_DATA);
    os_memset(self->data, value, data_size_bytes);
}

void SliceData_truncate(struct SliceData_t* self, uint16_t size) {
    VALIDATE(self && self->data, ERR_SLICE_IS_EMPTY);
    VALIDATE(size <= self->data_window_end, ERR_CELL_UNDERFLOW);
    self->data_window_end = size;
}

uint16_t SliceData_remaining_bits(const struct SliceData_t* self) {
    VALIDATE(self && self->data, ERR_SLICE_IS_EMPTY);
    if (self->data_window_start > self->data_window_end) {
        return 0;
    }
    return self->data_window_end - self->data_window_start;
}

void SliceData_move_by(struct SliceData_t* self, uint16_t offset) {
    VALIDATE(self && self->data, ERR_SLICE_IS_EMPTY);
    VALIDATE(self->data_window_start + offset <= self->data_window_end, ERR_CELL_UNDERFLOW);
    self->data_window_start += offset;
}

uint8_t SliceData_get_bits(const struct SliceData_t* self, uint16_t offset, uint8_t bits) {
    VALIDATE(self && self->data, ERR_SLICE_IS_EMPTY);
    VALIDATE(offset + bits <= SliceData_remaining_bits(self), ERR_CELL_UNDERFLOW);
    VALIDATE(bits != 0 && bits <= 8, ERR_RANGE_CHECK);

    uint16_t index = self->data_window_start + offset;
    uint8_t q = index / 8;
    uint8_t r = index % 8;
    if (r == 0) {
        return self->data[q] >> (8 - r - bits);
    } else if (bits <= (8 - r)) {
        return self->data[q] >> (8 - r - bits) & ((1 << bits) - 1);
    } else {
        uint16_t ret = 0;
        if (q < self->data_size_bytes) {
            uint16_t byte = self->data[q];
            ret |= byte << 8;
        }
        if (q < self->data_size_bytes - 1) {
            ret |= self->data[q + 1];
        }

        ret = (ret >> (8 - r)) >> (8 - bits);
        return (uint8_t)ret;
    }
}

uint8_t SliceData_get_next_bit(struct SliceData_t* self) {
    uint8_t bit = SliceData_get_bits(self, 0, 1);
    SliceData_move_by(self, 1);
    return bit;
}

uint8_t SliceData_get_byte(const struct SliceData_t* self, uint16_t offset) {
    return SliceData_get_bits(self, offset, 8);
}

uint8_t SliceData_get_next_byte(struct SliceData_t* self) {
    uint8_t value = SliceData_get_byte(self, 0);
    SliceData_move_by(self, 8);
    return value;
}

uint64_t SliceData_get_int(const struct SliceData_t* self, uint8_t bits) {
    VALIDATE(bits <= SliceData_remaining_bits(self), ERR_CELL_UNDERFLOW);
    VALIDATE(bits <= 64, ERR_RANGE_CHECK);
    if (bits == 0) {
        return 0;
    }

    uint64_t value = 0;
    uint8_t bytes = bits / 8;
    for (uint8_t i = 0; i < bytes; ++i) {
        uint64_t byte = SliceData_get_byte(self, 8 * i);
        value |= byte << (8 * (7 - i));
    }

    volatile uint8_t remainder = bits % 8;
    if (remainder != 0) {
        uint64_t r = SliceData_get_bits(self, bytes * 8, remainder);
        value |= r << (8 * (7 - bytes) + (8 - remainder));
    }

    return value >> (64 - bits);
}

uint64_t SliceData_get_next_int(struct SliceData_t* self, uint8_t bits) {
    uint64_t value = SliceData_get_int(self, bits);
    SliceData_move_by(self, bits);
    return value;
}

uint64_t SliceData_get_next_size(struct SliceData_t* self, uint16_t max_value) {
    if (max_value == 0) {
        return 0;
    }
    uint8_t bits = 16 - leading_zeros(max_value);
    uint64_t res = SliceData_get_next_int(self, bits);
    return res;
}

bool SliceData_is_empty(const struct SliceData_t* self) {
    VALIDATE(self && self->data, ERR_SLICE_IS_EMPTY);
    return self->data_window_start >= self->data_window_end;
}

bool SliceData_equal(const struct SliceData_t* self, const struct SliceData_t* other) {
    uint32_t self_rb = SliceData_remaining_bits(self);
    uint32_t other_rb = SliceData_remaining_bits(other);
    if (self_rb != other_rb) {
        return false;
    }

    return SliceData_get_int(self, self_rb) == SliceData_get_int(other, other_rb);
}

uint16_t SliceData_get_cursor(const struct SliceData_t* self) {
    VALIDATE(self && self->data, ERR_SLICE_IS_EMPTY);
    return self->data_window_start;
}

uint8_t* SliceData_begin(const struct SliceData_t* self) {
    VALIDATE(self && self->data, ERR_SLICE_IS_EMPTY);
    return self->data;
}

uint16_t SliceData_data_size(const struct SliceData_t* self) {
    VALIDATE(self && self->data, ERR_SLICE_IS_EMPTY);
    return self->data_size_bytes;
}

void SliceData_append(struct SliceData_t* self, uint8_t* in, uint16_t bits, bool append_tag) {
    uint8_t bytes = bits / 8;
    VALIDATE(bytes <= SliceData_remaining_bits(self) * 8, ERR_CELL_UNDERFLOW);

    uint16_t offset = self->data_window_start;
    if (offset % 8 == 0 || bytes == 0) {
        os_memcpy(self->data + offset / 8, in, bytes ? bytes : 1);
    }
    else {
        uint8_t shift = offset % 8;
        uint8_t first_data_byte = offset / 8;
        uint8_t prev = 0;
        for (uint16_t i = first_data_byte, j = 0; j < bytes; ++i, ++j) {
            VALIDATE(i == (j + first_data_byte) && i < self->data_size_bytes, ERR_INVALID_DATA);

            uint8_t cur = in[j] >> shift;
            if (j == 0) {
                uint8_t first_byte = self->data[i] >> (8 - shift);
                first_byte <<= 8 - shift;
                self->data[i] = first_byte | cur;
            } else {
                self->data[i] = prev | cur;
            }

            prev = in[j] << (8 - shift);
            if (j == bytes - 1) {
                uint8_t last_byte = prev;
                if (append_tag) {
                    if (shift != 7) {
                        last_byte >>= 7 - shift;
                    }
                    last_byte |= 1;
                    if (shift != 7) {
                        last_byte <<= 7 - shift;
                    }

                    bits += 8 - shift;
                }
                self->data[i + 1] = last_byte;
            }
        }
    }
    
    self->data_window_start += bits;
}
