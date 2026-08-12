#ifndef OSAL_THREAD_H
#define OSAL_THREAD_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file osal.h
 * @brief Camada de Abstração do Sistema Operacional (OSAL).
 * 
 * Fornece uma interface unificada para threads, mutexes e tempo, permitindo
 * que o código de aplicação rode tanto em Linux (POSIX) quanto em RTOS (FreeRTOS).
 */

#define OSAL_BIT_CONTADOR  (1 << 0)
#define OSAL_BIT_KEYBOARD  (1 << 1)
#define OSAL_BIT_SENSOR    (1 << 2)
#define OSAL_BIT_WAVEGEN   (1 << 3)

/* 
 * Tipos Opacos (Handles) 
 * Usamos void* para esconder a implementação real (pthread_t ou TaskHandle_t)
 */
typedef void* osal_thread_t;
typedef void* osal_mutex_t;

/* Assinatura da função da thread: recebe um ponteiro e não retorna nada */
typedef void (*osal_thread_func_t)(void *arg);

/**
 * @brief Inicia o scheduler (FreeRTOS) ou bloqueia aguardando encerramento (POSIX).
 * No FreeRTOS: chama vTaskStartScheduler() e nunca retorna.
 * No POSIX: simplesmente retorna, pois as threads já estão rodando.
 */
void osal_run(void);

/**
 * @brief Handle opaco para um grupo de eventos (Event Group).
 * Usado para sincronização entre tasks: cada task sinaliza um bit
 * ao encerrar, permitindo que o main() aguarde o término de todas.
 */
typedef void* osal_event_t;

/**
 * @brief Cria um novo grupo de eventos com todos os bits zerados.
 * @param event Ponteiro para armazenar o handle criado.
 * @return 0 em caso de sucesso, negativo caso contrário.
 */
int osal_event_create(osal_event_t *event);

/**
 * @brief Seta um ou mais bits no grupo de eventos.
 * Chamado por uma task antes de encerrar para sinalizar sua conclusão.
 * @param event Handle do grupo de eventos.
 * @param bits Máscara de bits a setar (ex: BIT_0, BIT_1, BIT_0 | BIT_2).
 */
void osal_event_set(osal_event_t event, uint32_t bits);

/**
 * @brief Bloqueia até que todos os bits da máscara estejam setados.
 * Chamado pelo main() após osal_run() para aguardar o encerramento
 * de todas as tasks de forma portável entre POSIX e FreeRTOS.
 * @param event Handle do grupo de eventos.
 * @param bits Máscara com todos os bits que devem estar setados para desbloquear.
 */
void osal_event_wait_all(osal_event_t event, uint32_t bits);

/**
 * @brief Destrói o grupo de eventos e libera os recursos associados.
 * @param event Handle do grupo de eventos a destruir.
 */
void osal_event_destroy(osal_event_t event);

/**
 * @brief Cria e inicia uma nova thread/task.
 * @param thread Ponteiro para armazenar o handle da thread criada.
 * @param name Nome da thread (útil para debug).
 * @param func Função a ser executada.
 * @param arg Argumentos a serem passados para a função.
 * @param priority Prioridade da thread (maior o número, maior a prioridade, adaptado para freertos ou linux).
 * @param stack_size Tamanho da pilha (usado mais para FreeRTOS).
 * @return 0 em caso de sucesso, negativo caso contrário.
 */
int osal_thread_create(osal_thread_t *thread, const char *name, osal_thread_func_t func, void *arg, int priority, uint32_t stack_size);

/**
 * @brief Aguarda o término da thread (Join). Pode não ser aplicável em FreeRTOS.
 * @param thread A thread a aguardar.
 */
void osal_thread_join(osal_thread_t thread);

/**
 * @brief Cria um Mutex para controle de concorrência.
 * @param mutex Ponteiro para armazenar o handle do mutex criado.
 * @return 0 em caso de sucesso.
 */
int osal_mutex_create(osal_mutex_t *mutex);

/**
 * @brief Bloqueia (lock) um mutex.
 * @param mutex Mutex a ser bloqueado.
 * @return 0 em caso de sucesso.
 */
int osal_mutex_lock(osal_mutex_t mutex);

/**
 * @brief Desbloqueia (unlock) um mutex.
 * @param mutex Mutex a ser desbloqueado.
 * @return 0 em caso de sucesso.
 */
int osal_mutex_unlock(osal_mutex_t mutex);

/**
 * @brief Destrói um mutex.
 * @param mutex Mutex a ser destruído.
 */
void osal_mutex_destroy(osal_mutex_t mutex);

/**
 * @brief Bloqueia a thread/task atual por um determinado número de milissegundos.
 * @param ms Tempo em milissegundos.
 */
void osal_delay_ms(uint32_t ms);

/**
 * @brief Bloqueia a thread/task atual por um determinado número de microssegundos.
 * @param us Tempo em microssegundos (mínimo de 100us no FreeRTOS configurado).
 */
void osal_delay_us(uint32_t us);

/**
 * @brief Retorna o tempo em microssegundos desde a inicialização do sistema.
 * @return Tempo em microssegundos.
 */
uint64_t osal_get_time_us(void);

#ifdef __cplusplus
}
#endif

#endif // OSAL_THREAD_H
