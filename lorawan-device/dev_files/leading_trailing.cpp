#include <stdio.h>
using namespace std;

// Author: Coleman Jenkins

union ftoi_t {
    float f;
    unsigned int i;
};

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

int main() {
    int i = 0xaa11c10;
    printf("binary rep: ");
    print_int_binary(i);
    printf("\n");
    printf("trailing: %d\n", trailing_zeros(i));
    printf("leading: %d\n", leading_zeros(i));
    return 0;
}