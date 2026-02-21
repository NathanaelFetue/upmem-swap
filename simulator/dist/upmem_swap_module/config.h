#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>

/* ===== Configuration Globale ===== */

/* Paramètres de simulation */
#define PAGE_SIZE 4096                    /* Taille d'une page (4KB) */
#define DEFAULT_RAM_SIZE_MB 32            /* RAM simulée par défaut (32 MB) */
#define DEFAULT_NR_DPUS 16                /* Nombre de DPUs par défaut */
#define DEFAULT_NR_ACCESSES 10000         /* Nombre d'accès mémoire par défaut */
#define DEFAULT_WORKING_SET 10000         /* Working set par défaut */

/* DEBUG activé pour logs verbeux */
#define DEBUG 0

#if DEBUG
#define DEBUG_PRINT(fmt, ...) \
    printf("[DEBUG] " fmt "\n", ##__VA_ARGS__)
#else
#define DEBUG_PRINT(fmt, ...)
#endif

/* Capacité MRAM par DPU (en bytes) */
#define DPU_MRAM_SIZE (64 * 1024 * 1024)  /* 64 MB par DPU */

/* Types de workload */
typedef enum {
    WORKLOAD_RANDOM,
    WORKLOAD_SEQUENTIAL,
    WORKLOAD_MIXED
} workload_type_t;

#endif /* CONFIG_H */
