#include <stdio.h>
#include "../compress_buffer.h"

// Author: Coleman Jenkins

union ftoi_t {
    float f;
    unsigned int i;
};

void print_float_binary(float f) {
    ftoi_t val;
    val.f = f;
    
    unsigned int temp = val.i;
    for( int i = 0; i < sizeof(float)*8; i++ ) {
        unsigned int shift = sizeof(float)*8 - i - 1;
        printf("%d", (temp & (0x1 << shift)) >> shift);
    }
}

void print_int_binary(unsigned int i) {
    unsigned int temp = i;
    for( int i = 0; i < sizeof(float)*8; i++ ) {
        unsigned int shift = sizeof(float)*8 - i - 1;
        printf("%d", (temp & (0x1 << shift)) >> shift);
    }
}

int leading_zeros(unsigned int val) {
    int ct = 0;
    for (int i = 0; i < sizeof(int)*8; i++){
        if (val & (0x01 << (sizeof(int)*8 - i - 1)))
            return ct;
        else
            ct++;
    }
    return ct;
}

int trailing_zeros(unsigned int val) {
    int ct = 0;
    for (int i = 0; i < sizeof(int)*8; i++){
        if (val & (0x01 << (i)))
            return ct;
        else
            ct++;
    }
    return ct;
}

void print_bit_section(unsigned int val, unsigned int start, unsigned int len) {
    for( int i = start; i < start + len; i++ ) {
        unsigned int shift = sizeof(float)*8 - i - 1;
        printf("%d", (val & (0x1 << shift)) >> shift);
    }
}

int main() {
    ftoi_t list[6];
    list[0].f = 100;
    list[1].f = 103;
    list[2].f = 103;
    list[3].f = 105;
    list[4].f = 103;
    list[5].f = 103;

    for (int i = 0; i < 6; i++) {
        printf("%d = %f : ", i, list[i].f);
        print_float_binary(list[i].f);
        printf("\n");
    }

    printf("Send values:\n");

    ftoi_t sent_msg_vals[6];
    sent_msg_vals[0] = list[0];
    print_int_binary(sent_msg_vals[0].i);
    printf("\n");

    for (int i = 1; i < 6; i++) {
        sent_msg_vals[i].i = list[i].i^list[i-1].i;
        print_int_binary(sent_msg_vals[i].i);
        printf("\n");
    }

    printf("Sending bits: \n");
    CompressBuffer buf;
    print_int_binary(sent_msg_vals[0].i);
    buf.add_int_val(sent_msg_vals[0].i, 32);
    printf("\n");

    for (int i = 1; i < 6; i++) {
        
        if(sent_msg_vals[i].i == 0) {
            printf("0\n");
            buf.add_bit(0);
        } else {
            unsigned int leading_i = leading_zeros(sent_msg_vals[i].i);
            unsigned int trailing_i = trailing_zeros(sent_msg_vals[i].i);
            unsigned int leading_i_1 = leading_zeros(sent_msg_vals[i-1].i);
            unsigned int trailing_i_1 = trailing_zeros(sent_msg_vals[i-1].i);
            
            if ((leading_i >= leading_i_1) && (trailing_i == trailing_i_1) && i != 1) {
                unsigned int n_meaningful_bits_prev = sizeof(float)*8 - leading_i_1 - trailing_i_1;
                printf("10 "); 
                buf.add_bit(1); // store
                buf.add_bit(0); // control
                print_bit_section(sent_msg_vals[i].i, leading_i_1, n_meaningful_bits_prev);
                buf.add_int_range(sent_msg_vals[i].i, trailing_i_1, n_meaningful_bits_prev);
                printf("\n");
            } else {
                unsigned int n_meaningful_bits = sizeof(float)*8 - leading_i - trailing_i;
                printf("11 ");
                buf.add_bit(1); // store
                buf.add_bit(1); // control

                print_bit_section(leading_i, 32 - 5, 5);
                printf(" ");
                buf.add_int_val(leading_i, 5);

                print_bit_section(n_meaningful_bits, 32 - 6, 6);
                printf(" ");
                buf.add_int_val(n_meaningful_bits, 6);

                print_bit_section(sent_msg_vals[i].i, leading_i, n_meaningful_bits);
                printf("\n");
                buf.add_int_range(sent_msg_vals[i].i, trailing_i, n_meaningful_bits);
            }
        }

    }

    buf.finish();
    printf("Buffer size: %d\n", buf.size());
    buf.print_buffer();

    return 0;
}