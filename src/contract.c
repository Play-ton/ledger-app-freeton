#include "contract.h"
#include "slice_data.h"
#include "byte_stream.h"
#include "cell.h"
#include "utils.h"
#include "errors.h"
#include "hashmap_label.h"

#define BOC_GENERIC_TAG 0xb5ee9c72
#define MAX_ROOTS_COUNT 1
#define HASH_SIZE 32

uint16_t deserialize_cell(struct Cell_t* cell, const uint8_t cell_index, const uint8_t cells_count) {
    uint8_t d1 = Cell_get_d1(cell);
    uint8_t l = d1 >> 5; // level
    uint8_t h = (d1 & 16) == 16; // with hashes
    uint8_t s = (d1 & 8) == 8; // exotic
    uint8_t r = d1 & 7;	// refs count
    uint8_t absent = r == 7 && h;
    UNUSED(l);
    UNUSED(absent);

    VALIDATE(!h, ERR_INVALID_DATA);
    VALIDATE(!s, ERR_INVALID_DATA); // only ordinary cells are valid
    VALIDATE(r <= MAX_REFERENCES_COUNT, ERR_INVALID_DATA);

    uint8_t data_size = Cell_get_data_size(cell);
    uint8_t refs_count = 0;
    uint8_t* refs = Cell_get_refs(cell, &refs_count);
    for (uint8_t i = 0; i < refs_count; ++i) {
        uint8_t ref = refs[i];
        VALIDATE(ref <= cells_count && ref > cell_index, ERR_INVALID_DATA);
    }

    return CELL_DATA_OFFSET + data_size + refs_count; // cell size
}

void deserialize_cells_tree(struct ByteStream_t* src) {
    {
        uint64_t magic = ByteStream_read_u32(src);
        VALIDATE(BOC_GENERIC_TAG == magic, ERR_INVALID_DATA);
    }
    
    uint8_t first_byte = ByteStream_read_byte(src);
    {
        bool index_included = (first_byte & 0x80) != 0;
        bool has_crc = (first_byte & 0x40) != 0;
        bool has_cache_bits = (first_byte & 0x20) != 0;
        uint8_t flags = (first_byte & 0x18) >> 3;
        UNUSED(flags);
        VALIDATE(!index_included && !has_crc && !has_cache_bits, ERR_INVALID_DATA);
    }

    uint8_t ref_size = first_byte & 0x7; // size in bytes
    VALIDATE(ref_size == 1, ERR_INVALID_DATA);
    
    uint8_t offset_size = ByteStream_read_byte(src);
    VALIDATE(offset_size != 0 && offset_size <= 8, ERR_INVALID_DATA);

    uint8_t cells_count = ByteStream_read_byte(src);
    uint8_t roots_count = ByteStream_read_byte(src);
    VALIDATE(roots_count == MAX_ROOTS_COUNT, ERR_INVALID_DATA);
    VALIDATE(cells_count <= MAX_CELLS_COUNT, ERR_INVALID_DATA);
    contract_context.cells_count = cells_count;

    {
        uint8_t absent_count = ByteStream_read_byte(src);
        UNUSED(absent_count);
        uint8_t* total_cells_size = ByteStream_read_data(src, offset_size);
        UNUSED(total_cells_size);
        uint8_t* buf = ByteStream_read_data(src, roots_count * ref_size);
        UNUSED(buf);
    }

    Cell_t cell;
    for (uint8_t i = 0; i < cells_count; ++i) {
        uint8_t* cell_begin = ByteStream_get_cursor(src);
        Cell_init(&cell, cell_begin);
        uint16_t offset = deserialize_cell(&cell, i, cells_count);
        contract_context.cells[i] = cell;
        ByteStream_read_data(src, offset);
    }
}

void get_cell_hash(Cell_t* cell, uint8_t* hash, const uint8_t cell_index) {
    uint8_t hash_buffer[262]; // d1(1) + d2(1) + data(128) + 4 * (depth(1) + hash(32))

    uint16_t hash_buffer_offset = 0;
    hash_buffer[0] = Cell_get_d1(cell);
    hash_buffer[1] = Cell_get_d2(cell);
    hash_buffer_offset += 2;
    uint8_t data_size = Cell_get_data_size(cell);
    if (contract_context.public_key_cell_index && cell_index == contract_context.public_key_cell_index) {
        os_memcpy(hash_buffer + hash_buffer_offset, contract_context.public_key_cell_data, data_size);
    } else {
        os_memcpy(hash_buffer + hash_buffer_offset, Cell_get_data(cell), data_size);
    }
    hash_buffer_offset += data_size;

    uint8_t refs_count = 0;
    uint8_t* refs = Cell_get_refs(cell, &refs_count);
    VALIDATE(refs_count >= 0 && refs_count <= MAX_REFERENCES_COUNT, ERR_INVALID_DATA);
    for (uint8_t child = 0; child < refs_count; ++child) {
        uint8_t* depth = &contract_context.cell_depth[cell_index];
        uint8_t child_depth = contract_context.cell_depth[refs[child]];
        *depth = (*depth > child_depth + 1) ? *depth : (child_depth + 1);
        uint8_t buf[2];
        buf[0] = 0;
        buf[1] = child_depth;
        os_memcpy(hash_buffer + hash_buffer_offset, buf, sizeof(buf));
        hash_buffer_offset += sizeof(buf);
    }
    
    for (uint8_t child = 0; child < refs_count; ++child) {
        uint8_t* cell_hash = data_context.addr_context.hashes + refs[child] * HASH_SIZE;
        os_memcpy(hash_buffer + hash_buffer_offset, cell_hash, HASH_SIZE);
        hash_buffer_offset += HASH_SIZE;
    }
    
    int result = cx_hash_sha256(hash_buffer, hash_buffer_offset, hash, HASH_SIZE);
    VALIDATE(result == HASH_SIZE, ERR_INVALID_HASH);
}

void find_public_key_cell() {
    ContractContext_t* cc = &contract_context;
    VALIDATE(Cell_get_data(&cc->cells[0])[0] & 0x20, ERR_INVALID_DATA); // has data branch

    uint8_t refs_count = 0;
    uint8_t* refs = Cell_get_refs(&cc->cells[0], &refs_count);
    VALIDATE(refs_count > 0 && refs_count <= 2, ERR_INVALID_DATA);

    uint8_t data_root = refs[refs_count - 1];
    VALIDATE(data_root != 0 && data_root <= MAX_CELLS_COUNT, ERR_INVALID_DATA);
    refs = Cell_get_refs(&cc->cells[data_root], &refs_count);
    VALIDATE(refs_count != 0 && refs_count <= MAX_REFERENCES_COUNT, ERR_INVALID_DATA);
    
    uint8_t key_buffer[8];
    SliceData_t key;
    os_memset(key_buffer, 0, sizeof(key_buffer));
    SliceData_init(&key, key_buffer, sizeof(key_buffer));

    uint16_t bit_len = SliceData_remaining_bits(&key);
    put_to_node(refs[0], bit_len, &key);
}

void get_address(const uint32_t account_number, uint8_t* address) {
    ContractContext_t* cc = &contract_context;
    VALIDATE(cc->cells_count != 0, ERR_INVALID_DATA);
    find_public_key_cell(); // sets public key cell index to contract_context

    VALIDATE(cc->public_key_cell_index && cc->public_key_label_size_bits, ERR_CELL_IS_EMPTY);
    Cell_t* cell = &cc->cells[cc->public_key_cell_index];
    uint8_t cell_data_size = Cell_get_data_size(cell);
    VALIDATE(cell_data_size != 0 && cell_data_size <= MAX_PUBLIC_KEY_CELL_DATA_SIZE, ERR_INVALID_DATA);
    uint8_t* cell_data = Cell_get_data(cell);

    os_memcpy(cc->public_key_cell_data, cell_data, cell_data_size);
    uint8_t* public_key = data_context.pk_context.public_key;
    get_public_key(account_number, public_key);
    
    uint8_t offset = cc->public_key_label_size_bits;
    uint8_t* data = cc->public_key_cell_data;
    if (offset % 8 == 0) { // lucky day
        os_memcpy(data + offset / 8, public_key, PUBLIC_KEY_LENGTH);
    }
    else {
        uint8_t shift = offset % 8;
        uint8_t first_data_byte = offset / 8;
        for (uint16_t i = first_data_byte, j = 0; j < PUBLIC_KEY_LENGTH; ++i, ++j) {
            VALIDATE(i == (j + first_data_byte) && i < cell_data_size, ERR_INVALID_DATA);

            uint8_t pk_cur = public_key[j] >> shift;
            if (i == first_data_byte) {
                uint8_t first_byte = data[i] >> (8 - shift);
                first_byte <<= 8 - shift;
                data[i] = first_byte | pk_cur;
                continue;
            }

            uint8_t pk_prev = public_key[j - 1] << (8 - shift);
            data[i] = pk_prev | pk_cur;
            if (j == PUBLIC_KEY_LENGTH - 1) { // append tag
                uint8_t last_byte = public_key[j] << (8 - shift);
                last_byte = pk_cur | last_byte;
                if (shift != 7) {
                    last_byte >>= 7 - shift;
                }
                last_byte |= 1;
                if (shift != 7) {
                    last_byte <<= 7 - shift;
                }
                data[i + 1] = last_byte;
            }
        }
    }
    
    uint8_t* hash = address;
    for (int16_t i = cc->cells_count - 1; i >= 0; --i) {
        Cell_t* cell = &cc->cells[i];
        os_memset(hash, 0, sizeof(hash));
        get_cell_hash(cell, hash, i);
        if (i != 0) {
            os_memcpy(data_context.addr_context.hashes + i * HASH_SIZE, hash, HASH_SIZE);
        }
    }
}
