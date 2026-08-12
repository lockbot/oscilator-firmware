#include "osal_thread.h"
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <stdio.h>

/**
 * @file osal_thread_posix.c
 * @brief Implementação OSAL para sistemas Linux/POSIX usando pthreads.
 */

/**
 * Estrutura auxiliar para passar argumentos para pthreads e manter
 * a assinatura osal_thread_func_t correta.
 */
typedef struct {
    osal_thread_func_t func;
    void *arg;
} posix_thread_arg_t;

static void* posix_thread_wrapper(void* arg) {
    posix_thread_arg_t *pt_arg = (posix_thread_arg_t*)arg;
    pt_arg->func(pt_arg->arg);
    free(pt_arg);
    return NULL;
}

void osal_run(void) {
    /* POSIX: threads já estão rodando, não precisa fazer nada. */
}

typedef struct {
    uint32_t bits;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} posix_event_t;

int osal_event_create(osal_event_t *event) {
    posix_event_t *pe = (posix_event_t*)malloc(sizeof(posix_event_t));
    if (!pe) return -1;
    pe->bits = 0;
    pthread_mutex_init(&pe->mutex, NULL);
    pthread_cond_init(&pe->cond, NULL);
    *event = (osal_event_t)pe;
    return 0;
}

void osal_event_set(osal_event_t event, uint32_t bits) {
    posix_event_t *pe = (posix_event_t *)event;
    if (pe) {
        pthread_mutex_lock(&pe->mutex);
        pe->bits |= bits;
        pthread_cond_broadcast(&pe->cond);
        pthread_mutex_unlock(&pe->mutex);
    }
}

void osal_event_wait_all(osal_event_t event, uint32_t bits) {
    posix_event_t *pe = (posix_event_t *)event;
    if (pe) {
        pthread_mutex_lock(&pe->mutex);
        while ((pe->bits & bits) != bits) {
            pthread_cond_wait(&pe->cond, &pe->mutex);
        }
        pthread_mutex_unlock(&pe->mutex);
    }
}

void osal_event_destroy(osal_event_t event) {
    posix_event_t *pe = (posix_event_t *)event;
    if (pe) {
        pthread_mutex_destroy(&pe->mutex);
        pthread_cond_destroy(&pe->cond);
        free(pe);
    }
}

int osal_thread_create(osal_thread_t *thread, const char *name, osal_thread_func_t func, void *arg, int priority, uint32_t stack_size) {
    /* 
     * Nota: No Linux, prioridades de tempo real (SCHED_FIFO) requerem privilégios de root.
     * Estamos ignorando parâmetros de nome e prioridade para manter esta versão simples e segura.
     */
    (void)name; 
    (void)priority; 
    (void)stack_size;

    pthread_t *pt = (pthread_t*)malloc(sizeof(pthread_t));
    if (!pt) return -1;

    posix_thread_arg_t *pt_arg = (posix_thread_arg_t*)malloc(sizeof(posix_thread_arg_t));
    if (!pt_arg) {
        free(pt);
        return -1;
    }

    pt_arg->func = func;
    pt_arg->arg = arg;

    if (pthread_create(pt, NULL, posix_thread_wrapper, pt_arg) != 0) {
        free(pt);
        free(pt_arg);
        return -1;
    }

    *thread = (osal_thread_t)pt;
    return 0;
}

void osal_thread_join(osal_thread_t thread) {
    pthread_t *pt = (pthread_t*)thread;
    if (pt) {
        pthread_join(*pt, NULL);
        free(pt);
    }
}

int osal_mutex_create(osal_mutex_t *mutex) {
    pthread_mutex_t *pm = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
    if (!pm) return -1;

    if (pthread_mutex_init(pm, NULL) != 0) {
        free(pm);
        return -1;
    }

    *mutex = (osal_mutex_t)pm;
    return 0;
}

int osal_mutex_lock(osal_mutex_t mutex) {
    pthread_mutex_t *pm = (pthread_mutex_t*)mutex;
    if (!pm) return -1;
    return pthread_mutex_lock(pm);
}

int osal_mutex_unlock(osal_mutex_t mutex) {
    pthread_mutex_t *pm = (pthread_mutex_t*)mutex;
    if (!pm) return -1;
    return pthread_mutex_unlock(pm);
}

void osal_mutex_destroy(osal_mutex_t mutex) {
    pthread_mutex_t *pm = (pthread_mutex_t*)mutex;
    if (pm) {
        pthread_mutex_destroy(pm);
        free(pm);
    }
}

void osal_delay_ms(uint32_t ms) {
    usleep(ms * 1000); // usleep requer microsegundos
}

void osal_delay_us(uint32_t us) {
    usleep(us);
}

uint64_t osal_get_time_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)(ts.tv_sec * 1000000ULL + ts.tv_nsec / 1000ULL);
}
