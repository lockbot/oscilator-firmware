#include "osal_input.h"
#include "osal_thread.h"
#include <signal.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <stdlib.h>

static struct termios oldt;
static int term_configured = 0;

static void restore_terminal(void) {
    if (term_configured) {
        tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    }
}

static void (*posix_shutdown_callback)(void) = NULL;

static void posix_sigint_handler(int sig) {
    (void)sig;
    printf("\n[SISTEMA] Interrupção recebida. Encerrando tarefas...\n");
    fflush(stdout);
    if (posix_shutdown_callback) posix_shutdown_callback();
}

void osal_input_set_nonblocking(void) {
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);

    // Disable canonical mode and echo so keys like arrows can be read instantly without Enter
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
    (void)event;    /* POSIX: joins fazem a espera, evento ignorado */
    (void)all_bits;
    posix_shutdown_callback = callback;
    signal(SIGINT, posix_sigint_handler);
}

void osal_shutdown(void) {
    /* POSIX: no-op — joins gerenciam o encerramento. */
}
