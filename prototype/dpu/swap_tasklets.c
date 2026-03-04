#include <stdint.h>
#include <mram.h>
#include <defs.h>
#include <barrier.h>

// Buffer MRAM pour swap (64KB max)
__mram_noinit uint8_t mram_buffer[65536];

// Barrier pour synchronisation tasklets
BARRIER_INIT(my_barrier, NR_TASKLETS);

int main() {
    uint32_t tasklet_id = me();
    
    // Chaque tasklet traite sa portion
    // Pour swap pur, on fait juste une lecture minimale
    uint8_t dummy[8];
    
    // Calcul offset par tasklet
    uint32_t offset = tasklet_id * 8;
    
    // Lecture minimale pour forcer le symbole
    if (offset < 65536) {
        mram_read((__mram_ptr void const *)(mram_buffer + offset), dummy, 8);
    }
    
    // Synchronisation
    barrier_wait(&my_barrier);
    
    return 0;
}
