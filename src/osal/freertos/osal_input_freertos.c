#include "osal_input.h"
#include "osal_thread.h"
#include <signal.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <termios.h>
#include <stdlib.h>
#include "FreeRTOS.h"
#include "task.h"

static struct termios oldt;
static int term_configured = 0;

static void restore_terminal(void) {
    if (term_configured) {
        tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    }
}

/* Estrutura para passar múltiplos argumentos para a pthread */
typedef struct {
    void (*callback)(void);
    osal_event_t event;
    uint32_t all_bits;
} shutdown_args_t;

static shutdown_args_t shutdown_args;

static void * prvShutdownThread(void *arg) {
    shutdown_args_t *args = (shutdown_args_t *)arg;

    /* Não precisamos bloquear SIGINT aqui de novo se já foi feito 
     * no main thread pela osal_input_register_shutdown, 
     * mas não tem problema fazer por margem de segurança. */
    sigset_t xSignals;
    sigemptyset(&xSignals);
    sigaddset(&xSignals, SIGINT);
    pthread_sigmask(SIG_BLOCK, &xSignals, NULL);

    int iSignal;
    sigwait(&xSignals, &iSignal);

    printf("\n[SISTEMA] Interrupção recebida. Encerrando tarefas...\n");
    fflush(stdout);

    /* Seta keep_running = false. 
     * Não podemos chamar xEventGroupWaitBits (osal_event_wait_all)
     * daqui de dentro pois este é um pthread nativo (fora do array do 
     * FreeRTOS), o que causaria memory crash se invocado. A espera será
     * feita em osal_shutdown, chamado nativamente por task_input_teclado. */
    if (args->callback) args->callback();

    return NULL;
}

void osal_input_set_nonblocking(void) {
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);

    // Disable canonical mode and echo for arrow keys (raw input)
    struct termios newt;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    
    if (!term_configured) {
        atexit(restore_terminal);
        term_configured = 1;
    }
}

void osal_input_register_shutdown(void (*callback)(void),
                                  osal_event_t event,
                                  uint32_t all_bits) {
    shutdown_args.callback = callback;
    shutdown_args.event    = event;
    shutdown_args.all_bits = all_bits;

    /* 
     * Bloqueia SIGINT na thread principal (antes do scheduler iniciar).
     * Como as tarefas do FreeRTOS derivam da principal, todas as tasks 
     * herdarão este bloqueio. Dessa forma, o sinal SIGINT nunca vai 
     * abortar o software repentinamente pela action "default kill", 
     * e nosso sigwait lá em cima tem a chance de apanhar o sinal sossegadamente.
     */
    sigset_t xSignals;
    sigemptyset(&xSignals);
    sigaddset(&xSignals, SIGINT);
    pthread_sigmask(SIG_BLOCK, &xSignals, NULL);

    /* Criação da thread de escuta nativa Unix que vai interceptar os sinais do OS */
    pthread_t thread;
    pthread_create(&thread, NULL, prvShutdownThread, NULL);
    pthread_detach(thread);
}

void osal_shutdown(void) {
    /* O Teclado já coordenou todo mundo. O scheduler pode pousar. */
    vTaskEndScheduler();
}
