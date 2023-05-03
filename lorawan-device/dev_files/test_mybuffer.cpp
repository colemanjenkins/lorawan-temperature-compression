#include <stdio.h>
#include "../compress_buffer.h"
using namespace std;

int main() {
    unsigned int i = 0xaa11c10;

    CompressBuffer buf;
    // buf.add_bit(1);
    buf.add_int_val(0x10203041,32);
    buf.add_bit(1); // store
    buf.add_bit(1); // store
    buf.add_int_val(0xd, 5);
    // buf.add_int_val(0x2, 7);
    // buf.add_int_range(0xAF0, 4, 8);
    buf.print_buffer();
    return 0;
}