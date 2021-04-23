#include "message.h"
#include "utils.h"
#include "error.h"
#include "slice_data.h"
#include "byte_stream.h"
#include "contract.h"

#define ADDRESS_CELL_INDEX 0
#define AMOUNT_CELL_INDEX 1

void deserialize_array(uint8_t* in, uint8_t in_size, uint16_t offset, uint8_t* out, uint8_t out_size) {
    uint8_t shift = offset % 8;
    uint8_t first_data_byte = offset / 8;
    for (uint16_t i = first_data_byte, j = 0; j < out_size; ++i, ++j) {
        VALIDATE(i == (j + first_data_byte) && (i + 1) <= in_size, ERR_INVALID_DATA);

        uint8_t cur = in[i] << shift;
        out[j] = cur;

        if (j == out_size - 1) {
            out[j] |= in[i + 1] >> (8 - shift);
        }

        if (i != first_data_byte) {
            out[j - 1] |= in[i] >> (8 - shift);
        }
    }
}

void deserialize_address(struct SliceData_t* slice, uint8_t* address, int8_t* wc) {
    uint8_t* data = SliceData_begin(slice);
    uint16_t data_size = SliceData_data_size(slice);
    {
        uint64_t address_type = SliceData_get_next_int(slice, 2);
        VALIDATE(address_type == 2, ERR_INVALID_DATA);

        uint8_t anycast = SliceData_get_next_bit(slice);
        UNUSED(anycast);
    }

    *wc = SliceData_get_next_byte(slice);
    uint16_t offset = SliceData_get_cursor(slice);
    deserialize_array(data, data_size, offset, address, ADDRESS_LENGTH);
    SliceData_move_by(slice, ADDRESS_LENGTH * 8);
}

void set_dst_address(struct SliceData_t* slice, struct SliceData_t* to_sign) {
    int8_t wc;
    uint8_t address[ADDRESS_LENGTH];
    deserialize_address(slice, address, &wc);

    uint8_t prefix = 0x80; // ((type(2) << 1) | anycast(0)) << 5
    SliceData_append(to_sign, &prefix, 3, false);
    SliceData_append(to_sign, (uint8_t*)&wc, 8, false);
    SliceData_append(to_sign, address, ADDRESS_LENGTH * 8, true);

    char* address_str = data_context.sign_tr_context.address_str;
    char wc_temp[6]; // snprintf always returns zero
    snprintf(wc_temp, sizeof(wc_temp), "%d:", wc);
    int wc_len = strlen(wc_temp);
    os_memcpy(address_str, wc_temp, wc_len);
    address_str += wc_len;
    snprintf(address_str, sizeof(data_context.sign_tr_context.address_str) - wc_len, "%.*h", sizeof(address), address);
}

void deserialize_amount(struct SliceData_t* slice, uint8_t* amount, uint8_t* amount_length) {
    uint8_t* data = SliceData_begin(slice);
    uint16_t data_size = SliceData_data_size(slice);
    uint16_t offset = SliceData_get_cursor(slice);
    for (uint8_t i = 0; i < data_size; ++i) {
        if (data[i] != 0) {
            break;
        }
        offset += 8;
    }

    *amount_length = MAX_AMOUNT_LENGTH - offset / 8;
    deserialize_array(data, data_size, offset, amount, *amount_length);
    SliceData_move_by(slice, *amount_length * 8 + offset);
}

void set_amount(struct SliceData_t* slice) {
    uint8_t amount[MAX_AMOUNT_LENGTH];
    os_memset(amount, 0, sizeof(amount));
    uint8_t amount_length = 0;
    deserialize_amount(slice, amount, &amount_length);

    char* amount_str = data_context.sign_tr_context.amount_str;
    uint8_t text_size = convert_hex_amount_to_displayable(amount, amount_length, amount_str);
    strcpy(amount_str + text_size, " TON");
}

void le_to_be(uint8_t* le, uint8_t* be, uint8_t size) {
    for (uint8_t i = 0; i < size; ++i) {
        be[i] = le[size - 1 - i];
    }
}

void deserialize_header(struct SliceData_t* slice, struct SliceData_t* to_sign) {
    uint64_t time = SliceData_get_next_int(slice, sizeof(time) * 8);
    uint32_t expire = SliceData_get_next_int(slice, sizeof(expire) * 8);
    uint32_t input_id = SliceData_get_next_int(slice, sizeof(input_id) * 8);
    VALIDATE(input_id == INPUT_ID, ERR_INVALID_INPUT_ID);

    uint8_t buff[16];
    le_to_be((uint8_t*)&time, buff, sizeof(time));
    le_to_be((uint8_t*)&expire, buff + sizeof(time), sizeof(expire));
    le_to_be((uint8_t*)&input_id, buff + sizeof(time) + sizeof(expire), sizeof(input_id));
    SliceData_append(to_sign, buff, sizeof(buff) * 8, false);
}

void prepare_to_sign(struct ByteStream_t* src, uint8_t* src_address) {
    deserialize_cells_tree(src);
    BocContext_t* bc = &boc_context;

    SliceData_t to_sign;
    SliceData_init(&to_sign, data_context.sign_tr_context.to_sign_buffer, sizeof(data_context.sign_tr_context.to_sign_buffer));

    uint8_t buf[2];
    buf[0] = 0x01;
    buf[1] = 0x63;
    SliceData_append(&to_sign, buf, sizeof(buf) * 8, false);

    {
        Cell_t* cell = &bc->cells[ADDRESS_CELL_INDEX];
        SliceData_t slice;
        SliceData_from_cell(&slice, cell);
        SliceData_move_by(&slice, 4);
        int8_t wc;
        uint8_t address[ADDRESS_LENGTH];
        deserialize_address(&slice, address, &wc);
        VALIDATE(os_memcmp(src_address, address, ADDRESS_LENGTH) == 0, ERR_INVALID_SRC_ADDRESS);
        SliceData_move_by(&slice, 6);
        deserialize_header(&slice, &to_sign);
        set_dst_address(&slice, &to_sign);
        SliceData_move_by(&to_sign, 2 * 8);
    }

    {
        Cell_t* cell = &bc->cells[AMOUNT_CELL_INDEX];
        SliceData_t slice;
        SliceData_from_cell(&slice, cell);
        set_amount(&slice);
        bool bounce = SliceData_get_next_bit(&slice) != 0;
        UNUSED(bounce);

        calc_cell_hash(cell, AMOUNT_CELL_INDEX);
        SliceData_append(&to_sign, bc->hashes + HASH_SIZE, HASH_SIZE * 8, false);
    }

    int result = cx_hash_sha256(SliceData_begin(&to_sign), SliceData_get_cursor(&to_sign) / 8, data_context.sign_tr_context.to_sign, HASH_SIZE);
    VALIDATE(result == HASH_SIZE, ERR_INVALID_HASH);
}
