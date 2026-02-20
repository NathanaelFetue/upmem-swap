/*
 * UPMEM DPU Program
 * 
 * Pour Axe 1: Pas de compression, juste stockage passif
 * Pour Axe 2 (futur): Implémentate compression RLE/LZ4
 */

#include <mram.h>
#include <defs.h>

/* Très simple pour maintenant */
int main() {
    /* En Axe 1: DPUs n'exécutent rien, juste serve comme MRAM */
    /* Host gère tous les transferts via SDK */
    return 0;
}
