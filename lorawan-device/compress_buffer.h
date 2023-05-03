#include <stdint.h>
#include <stdio.h>
#include <cmath>

// important note: must call finish() before sending
// Author: Coleman Jenkins

class CompressBuffer {
private:
    bool print_additions;
    
public:
    uint8_t buf[47]; // max packet size allowed is 47 bytes
    uint16_t next_write_bit; // next bit FROM THE LEFT to be filled

    CompressBuffer() {
        printf("[buf] Constructor\n");
        next_write_bit = 3; // first 3 bits reserved for empty bits at end
        for (int i = 0; i < 47; i++) {
            buf[i] = 0;
        }
        print_additions = false;
    }

    // create buffer with existing buffer
    CompressBuffer(uint8_t* b, int size) {
        if (size > 47) {
            size = 47;
        }
        for (int i = 0; i < size; i++) {
            buf[i] = b[i];
        }
        next_write_bit = 8*size;
        print_additions = false;
    } 

    // have each addition get printed
    void set_print_status(bool new_val) {
        print_additions = new_val;
    }

    // add a single bit to the buffer at the next spot
    // return values: 
    //      0 -> success
    //      1 -> buffer full, not added
    //      2 -> input not 1 or 0
    int add_bit(uint8_t bit) {
        if (bit != 0 && bit != 1) {
            printf("[buf] Must pass 1 or 0 to add_bit. Not added to buffer.\n");
            return 2;
        }
        uint8_t byte = next_write_bit/8;
        if (byte >= 47) {
            printf("[buf] Buffer full, not added.\n");
            return 1;
        }
        buf[byte] = buf[byte] | (bit << (7-(next_write_bit%8)));
        next_write_bit++;

        if (print_additions) {
            printf("[buf] Add bit %d\n", bit);
        }

        return 0;
    }

    // adds bits from an int in a specific range of indices
    // from the int. `pad` is number of bits from the right to exclude
    // Ex. add_int_range(44, 2, 3) adds '011' because 44 is
    //      '101100', so the lowest '00' are ignored and it
    //      takes the next 3 bits
    // return values
    //      0 -> success
    //      1 -> buffer full, parttially not added
    //      2 -> pad out of range
    //      3 -> n_bits out of range
    int add_int_range(unsigned int bits, uint8_t pad, uint8_t n_bits) {
        if (pad >= 32 || pad < 0) {
            printf("[buf] pad must be in the range [0, 31] inclusive.\n");
            return 2;
        }
        if (n_bits > 32 || n_bits < 0) {
            printf("[buf] n_bits must be in the range [0, 32] inclusive.\n");
            return 3;
        }
        unsigned int content_bits = bits >> pad;
        // clear out any values above the requested
        unsigned int keep_bits = 0;
        for (uint8_t i = 0; i < n_bits; i++) {
            keep_bits = keep_bits | (0x1 << i);
        }
        content_bits = content_bits & keep_bits;

        uint8_t bits_remaining = n_bits;
        while (bits_remaining > 0) {
            int byte = next_write_bit/8;
            if (byte >= 47) {
                printf("[buf] Buffer full, part not added.\n");
                return 1;
            }

            uint8_t bits_to_add;
            uint8_t selected_bits;
            uint8_t open_bits_in_byte = 8 - (next_write_bit % 8);
            if (bits_remaining >= open_bits_in_byte) {
                bits_to_add = open_bits_in_byte; 
                selected_bits = uint8_t(content_bits >> (bits_remaining - bits_to_add));
            } else {
                bits_to_add = bits_remaining; 
                selected_bits = uint8_t(content_bits >> (bits_remaining - bits_to_add));
                selected_bits = selected_bits << (8 - bits_to_add - (next_write_bit % 8));
            }
            buf[byte] |= selected_bits;
            next_write_bit += bits_to_add;
            bits_remaining -= bits_to_add;
        }

        if (print_additions) {
            printf("[buf] Add %d (%x)\n", content_bits, content_bits);
        }

        return 0;
    }

    // add bits from an int. specify the number of bits
    // FROM THE RIGHT to add. 
    // Ex. add_int_val(12,3) would add '100' because 12 is
    //      '1100' and it takes the lowest 3 bits
    int add_int_val(unsigned int bits, uint8_t n_bits) {
        return add_int_range(bits, 0, n_bits);
    }

    // print n_bytes of the buffer
    void print_buffer(int n_bytes) {
        if (n_bytes > 47) {
            n_bytes = 47;
        }
        printf(" ****** buffer ****** \n");
        for (int byte = 0; byte < n_bytes; byte++) {
            printf("%2d : ", byte);
            for (int i = 0; i < 8; i++) {
                unsigned int shift = 7 - i;
                printf("%d", (buf[byte] & (0x1 << shift)) >> shift);
            }
            printf(" (0x%x)\n", buf[byte]);
        }
        printf("Python buffer: b'");
        for (int byte = 0; byte < size(); byte++) {
            printf("\\x%02x", buf[byte]);
        }
        printf("'\n");
        printf(" ****** buffer ****** \n");
    }

    // print the whole buffer
    void print_buffer() {
        print_buffer(47);
    }

    // number of used bytes in the buffer
    int size() {
        return ceil(next_write_bit/8.0);
    }

    // check if n_bits can be added to the buffer
    bool check_add_bits(unsigned int n_bits) {
        if (next_write_bit + n_bits <= 47*8) return true;
        return false;
    }

    // since there may be extra bits at the end,
    // set the first 3 bits to represent the number
    // of empty bits in the last byte
    void finish() {
        int empty = 8 - (next_write_bit % 8);
        int temp_NWB = next_write_bit;
        next_write_bit = 0; // write at the beginning
        add_int_val(empty, 3);
        next_write_bit = temp_NWB;
    }
};