#include <stdio.h>
#include "compress_buffer.h"

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
    // declare data
    int times[6] = {3200, 4201, 5188, 6267, 7200, 8200};
    ftoi_t temperatures[6];
    temperatures[0].f = 21.3;
    temperatures[1].f = 21.4;
    temperatures[2].f = 21.35;
    temperatures[3].f = 21.55;
    temperatures[4].f = 21.23;
    temperatures[5].f = 22.01;

    // for storage
    ftoi_t sent_temp_msgs[6];

    // compress into buffer
    CompressBuffer buf;
    buf.set_print_status(true);
    bool full = false;
    for (int meas_n = 0; meas_n < 6 && !full; meas_n++){
        if (meas_n == 0) {
            sent_temp_msgs[0] = temperatures[0];
            printf("Add temp %f: ", temperatures[0].f);
            buf.add_int_val(sent_temp_msgs[0].i, 32); // temp

            printf("Add time %d: ", times[0]);
            buf.add_int_val(times[0], 32); // time
        } else {
            // check timestamp bits
            unsigned int timestamp_n_bits = 0;
            int dd;
            if (meas_n == 1) {
                timestamp_n_bits = 20;
            } else {
                dd = (times[meas_n] - times[meas_n - 1]) - (times[meas_n - 1] - times[meas_n - 2]);
                if (dd == 0) {
                    timestamp_n_bits = 1;
                } else if (dd >= -63 && dd <= 64) {
                    timestamp_n_bits = 2 + 7;
                } else if (dd >= -255 && dd <= 256) {
                    timestamp_n_bits = 3 + 9;
                } else if (dd >= -2047 && dd <= 2048) {
                    timestamp_n_bits = 4 + 12;
                } else {
                    timestamp_n_bits = 4 + 32; // this shouldn't happen often
                }
            }

            // compute sent temperature
            sent_temp_msgs[meas_n].i = temperatures[meas_n].i^temperatures[meas_n-1].i;
            printf("Add temp %f, encoded as %x:\n", temperatures[meas_n].f, sent_temp_msgs[meas_n].i);

            // check if can add temp, then add if so
            unsigned int leading_i = leading_zeros(sent_temp_msgs[meas_n].i);
            unsigned int trailing_i = trailing_zeros(sent_temp_msgs[meas_n].i);
            unsigned int leading_i_1 = leading_zeros(sent_temp_msgs[meas_n-1].i);
            unsigned int trailing_i_1 = trailing_zeros(sent_temp_msgs[meas_n-1].i);

            if (sent_temp_msgs[meas_n].i == 0) {
                if (!buf.check_add_bits(timestamp_n_bits + 1)) {
                    full = true;
                    break;
                }

                buf.add_bit(0);
            } else if ((leading_i >= leading_i_1) && (trailing_i == trailing_i_1) && meas_n != 1) { // same leading & trailing
                // check if can add
                unsigned int n_meaningful_bits_prev = sizeof(float)*8 - leading_i_1 - trailing_i_1;
                if (!buf.check_add_bits(timestamp_n_bits + 2 + n_meaningful_bits_prev)) {
                    full = true;
                    break;
                }

                // add temperature to buffer
                buf.add_bit(1); // store
                buf.add_bit(0); // control
                buf.add_int_range(sent_temp_msgs[meas_n].i, trailing_i_1, n_meaningful_bits_prev);
            } else { // different leading & traling, or it's the second entry
                // check if can add
                unsigned int n_meaningful_bits = sizeof(float)*8 - leading_i - trailing_i;
                if (!buf.check_add_bits(timestamp_n_bits + 2 + 5 + 6 + n_meaningful_bits)) {
                    full = true;
                    break;
                }

                // add temperature to buffer
                buf.add_bit(1); // store
                buf.add_bit(1); // control
                buf.add_int_val(leading_i, 5);
                buf.add_int_val(n_meaningful_bits, 6);
                buf.add_int_range(sent_temp_msgs[meas_n].i, trailing_i, n_meaningful_bits);
            }

            // add time
            if (meas_n == 1) {
                int diff = times[1] - times[0];
                printf("Adding time %d, encoded as %d:\n", times[1], diff);
                buf.add_int_val(diff, 20); // time. 20 is big enough for 17+ min interval
            } else {
                printf("Adding time %d, encoded as %d:\n", times[meas_n], dd);
                if (dd == 0) {
                    buf.add_int_val(0, 1);
                } else if (dd >= -63 && dd <= 64) {
                    buf.add_int_val(2, 2);
                    buf.add_int_val(dd, 7);
                } else if (dd >= -255 && dd <= 256) {
                    buf.add_int_val(6, 3);
                    buf.add_int_val(dd, 9);
                } else if (dd >= -2047 && dd <= 2048) {
                    buf.add_int_val(14, 4);
                    buf.add_int_val(dd, 12);
                } else {
                    buf.add_int_val(15, 4);
                    buf.add_int_val(dd, 32);
                }
            }
            
        }

    }

    buf.finish();
    printf("Buffer size: %d\n", buf.size());
    buf.print_buffer();

    return 0;
}