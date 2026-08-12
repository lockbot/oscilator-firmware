#include "osal_thread.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "event_groups.h"

#include <stdbool.h>

/**
 * @file osal_thread_freertos.c
 * @brief Implementação OSAL para FreeRTOS (Real).
 * 
 * Este arquivo utiliza os headers reais do FreeRTOS.
 * Para compilar, os arquivos do kernel FreeRTOS devem estar no seu include path.
 */

static bool is_scheduler_running = false;

void osal_run(void) {
    is_scheduler_running = true;
    vTaskStartScheduler();
    is_scheduler_running = false;
    /* vTaskEndScheduler() faz retornar aqui — encerramento normal. */
}

int osal_event_create(osal_event_t *event) {
    EventGroupHandle_t eg = xEventGroupCreate();
    if (eg == NULL) return -1;
    *event = (osal_event_t)eg;
    return 0;
}

void osal_event_set(osal_event_t event, uint32_t bits) {
    if (!is_scheduler_running) return;
    xEventGroupSetBits((EventGroupHandle_t)event, (EventBits_t)bits);
}

void osal_event_wait_all(osal_event_t event, uint32_t bits) {
    /* 
     * Se o usuário chamar osal_event_wait_all no main.c APÓS 
     * a saída do vTaskEndScheduler(), esta proteção impede que 
     * o FreeRTOS cause um SEGFAULT por usar APIs extintas 
     */
    if (!is_scheduler_running) return;
    xEventGroupWaitBits(
        (EventGroupHandle_t)event,
        (EventBits_t)bits,
        pdFALSE,    /* não limpa os bits após desbloquear */
        pdTRUE,     /* espera TODOS os bits — wait for all */
        portMAX_DELAY
    );
}

void osal_event_destroy(osal_event_t event) {
    vEventGroupDelete((EventGroupHandle_t)event);
}

int osal_thread_create(osal_thread_t *thread, const char *name, osal_thread_func_t func, void *arg, int priority, uint32_t stack_size) {
    TaskHandle_t handle;
    /* No FreeRTOS, stack_size é em palavras (words). Convertendo bytes para words. */
    if (xTaskCreate((TaskFunction_t)func, name,
                (uint16_t)(stack_size / sizeof(StackType_t)),
                arg, (UBaseType_t)priority, &handle) == pdPASS) {
        *thread = (osal_thread_t)handle;
        return 0;
    }
    return -1;
}

void osal_thread_join(osal_thread_t thread) {
    /* FreeRTOS não possui 'join' nativo. Tarefas são independentes. 
       Se necessário, a sincronização deve ser feita via EventGroups ou Semáforos. */
    (void)thread;
}

int osal_mutex_create(osal_mutex_t *mutex) {
    SemaphoreHandle_t m = xSemaphoreCreateMutex();
    if (m != NULL) {
        *mutex = (osal_mutex_t)m;
        return 0;
    }
    return -1;
}

int osal_mutex_lock(osal_mutex_t mutex) {
    if (xSemaphoreTake((SemaphoreHandle_t)mutex, portMAX_DELAY) == pdPASS) {
        return 0;
    }
    return -1;
}

int osal_mutex_unlock(osal_mutex_t mutex) {
    if (xSemaphoreGive((SemaphoreHandle_t)mutex) == pdPASS) {
        return 0;
    }
    return -1;
}

void osal_mutex_destroy(osal_mutex_t mutex) {
    vSemaphoreDelete((SemaphoreHandle_t)mutex);
}

void osal_delay_ms(uint32_t ms) {
    if (!is_scheduler_running) return;
    vTaskDelay(pdMS_TO_TICKS(ms));
}

void osal_delay_us(uint32_t us) {
    if (!is_scheduler_running) return;
    // Com configTICK_RATE_HZ = 10000, 1 tick = 100us
    TickType_t ticks = (TickType_t)(((uint64_t)us * configTICK_RATE_HZ) / 1000000ULL);
    if (ticks == 0 && us > 0) {
        ticks = 1; // Garante bloqueio mínimo se a conta falhar por baixo
    }
    vTaskDelay(ticks);
}

uint64_t osal_get_time_us(void) {
    if (!is_scheduler_running) return 0;
    /* Implementação depende de um hardware timer para precisão real de microsegundos. 
       Usando tick count mapeado (precisão de 100us na config atual). */
    return (uint64_t)xTaskGetTickCount() * (1000000ULL / configTICK_RATE_HZ);
}

void vApplicationMallocFailedHook(void) {
    /* Trava em loop — indica heap insuficiente. Aumente configTOTAL_HEAP_SIZE */
    for(;;);
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName) {
    (void)xTask;
    (void)pcTaskName;
    for(;;); /* Stack overflow — aumente stack_size da task problemática */
}