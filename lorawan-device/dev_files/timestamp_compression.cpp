#include <stdio.h>
#include <iostream>
#include "../compress_buffer.h"

// Author: Richard Wang

using namespace std;

int main() {
    int times[6] = {100, 101, 103, 105, 108, 109};
    CompressBuffer buf;
    for (int i = 0; i < 6; i++) {
        if (i == 0) {
            buf.add_int_val(times[i], 32);
        } else if (i == 1) {
            int diff = times[1] - times[0];
            buf.add_int_val(diff, 14);
        } else {
            int dd = (times[i] - times[i - 1]) - (times[i - 1] - times[i - 2]);
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
    buf.finish();
    buf.print_buffer();
    return 0;
}