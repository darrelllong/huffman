#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

#define BYTE  256
#define KBYTE 1024

uint64_t number = 0, count[BYTE] = { 0 };

uint8_t buffer[KBYTE] = { 0 };

void tally(int file) {
    int length;
    while ((length = read(file, buffer, KBYTE)) > 0) {
        number += length;
        for (int i = 0; i < length; i += 1) {
            count[buffer[i]] += 1;
        }
    }
    return;
}

double entropy(int file) {
    tally(file);
    double sum = 0.0;
    for (int i = 0; i < BYTE; i += 1) {
        double p = (double) count[i] / (double) number;
        if (p > 0) {
            sum += p * log2(p);
        }
    }
    return -sum;
}

int main(void) {
    printf("%lf\n", entropy(STDIN_FILENO));
    return 0;
}
