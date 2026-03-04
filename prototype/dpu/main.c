#include <stdint.h>
#include <mram.h>
__mram_noinit uint8_t mram_buffer[8192];
int main() {
    uint8_t dummy[8];
    mram_read((__mram_ptr void const *)mram_buffer, dummy, 8);
    return 0;
}
