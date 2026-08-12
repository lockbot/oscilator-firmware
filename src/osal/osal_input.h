#ifndef OSAL_INPUT_H
#define OSAL_INPUT_H

#include "osal_thread.h"  // defines osal_event_t
#include <stdint.h>       // defines uint32_t

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file osal_input.h
 * @brief Camada de abstração para entrada do usuário e sinais do sistema.
 *
 * Esconde detalhes de plataforma relacionados a leitura de teclado
 * e tratamento de sinais de encerramento, mantendo o código de
 * aplicação livre de #ifdefs e includes específicos de SO.
 */

/**
 * @brief Configura o terminal para leitura não-bloqueante.
 *
 * No Linux: usa fcntl() para setar O_NONBLOCK no stdin, permitindo
 * que fgets() retorne imediatamente caso não haja entrada disponível.
 * Em outros backends: no-op ou implementação equivalente.
 *
 * Deve ser chamada uma vez no início da task de leitura de teclado,
 * antes do loop principal.
 */
void osal_input_set_nonblocking(void);

/**
 * @brief Registra um callback e o evento de shutdown a ser aguardado
 * antes de encerrar o scheduler.
 *
 * No POSIX: instala um signal handler via signal(). O evento e bits
 * são ignorados — os joins fazem o trabalho de espera.
 * No FreeRTOS: cria uma pthread dedicada fora do scheduler que aguarda
 * SIGINT via sigwait(). Quando recebido, seta keep_running = false via
 * callback, aguarda todos os bits do evento de shutdown (sinalizados
 * por cada task antes de encerrar), e só então chama vTaskEndScheduler().
 *
 * @param callback   Função a ser chamada no recebimento do sinal.
 *                   Deve apenas setar flags — não alocar memória.
 * @param event      Handle do evento de shutdown criado em main.c.
 * @param all_bits   Máscara com todos os bits que devem estar setados
 *                   para considerar que todas as tasks encerraram.
 */
void osal_input_register_shutdown(void (*callback)(void),
                                  osal_event_t event,
                                  uint32_t all_bits);

/**
 * @brief Encerra o scheduler e sinaliza o fim do sistema.
 * No FreeRTOS: chama vTaskEndScheduler() de dentro de uma task —
 * deve ser chamada apenas de contexto de task, nunca de ISR ou
 * signal handler.
 * No POSIX: no-op — o encerramento é gerenciado pelos joins.
 */
void osal_shutdown(void);

#ifdef __cplusplus
}
#endif

#endif /* OSAL_INPUT_H */