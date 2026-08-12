#include "osal_thread.h"
#include "osal_input.h"
#include "hal_adc.h"
#include "hal_dac.h"
#include "oscillators.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>

/* 
 * Variáveis de Controle Global 
 */
volatile bool keep_running = true; // Mantém o loop estrutural vivo (para POSIX poder limpar no final)
volatile bool called_to_end = false; // Flag mestre para iniciar o comportamento de encerramento
volatile bool spwm_enabled = true; // Controle Ativo/Inativo do Módulo de Chaveamento PWM

wave_type_t current_wave = WAVE_SINE; // Forma de onda selecionada
volatile float osc_step = 0.0025f;    // Passo de avanço da fase (Frequência)
volatile float osc_amplitude = 74.1f; // Spread de amplitude ao redor do centro 114
volatile uint32_t current_delay_us = 500; // Delay base global para generator e leitor
volatile int fast_sq_n = 1; // Quantidade N de amostras consecutivas na onda FAST\_SQUARE ("V")

#define OSAL_BIT_UDP (1 << 4) // Bit para o novo task streamer
#define UDP_RING_SIZE 4096
volatile uint8_t udp_ring_buffer[UDP_RING_SIZE];
volatile uint32_t udp_ring_head = 0;
volatile uint32_t udp_ring_tail = 0;

osal_mutex_t wave_mutex; // Mutex para proteger o acesso ao estado do oscilador
osal_event_t shutdown_event; // Event para encerramento de eventos

void shutdown_callback(void) {
    called_to_end = true;
}

void task_contador(void *arg) {
    (void)arg;
    int i = 1;
    bool finished = false;
    while (keep_running) {
        if (called_to_end) {
            if (!finished) {
                osal_event_set(shutdown_event, OSAL_BIT_CONTADOR);
                printf("[CONTADOR] Encerrado.\n");
                fflush(stdout);
                finished = true;
            }
            osal_delay_ms(10); // Não dá return! Fica "preso" vivo esperando o main() limpar tudo
            continue;
        }

        printf("[CONTADOR] Valor: %d\n", i++);
        if (i > 10) i = 1;
        osal_delay_ms(1000);
    }
}

void task_input_keyboard(void *arg) {
    (void)arg;
    printf("[TECLADO] Comandos:\n 'f' - Respect\n 'z' - Seno\n 'a' - Serrote | 'd' - Serrote Inv.\n 'q' - Quadrada | 't' - Triangular\n 'v' - Quadrada SUPER Alta Freq (VMAX/VMIN puro)\n 'r' - Modo 'Real' (PWM) \n 'c' - Encerrar\n");
    printf("[TECLADO] Tuning:\n");
    printf("  Seta DIR/ESQ: Frequencia   |  's' - Reset Freq.\n");
    printf("  Seta CIMA/BAIXO: Amplitude |  'w' - Reset Amp.\n");
    printf("                             |  'x' - Reset Tudo\n");
    osal_input_set_nonblocking();

    bool finished = false;
    while (keep_running) {
        if (called_to_end) {
            if (!finished) {
                osal_event_set(shutdown_event, OSAL_BIT_KEYBOARD);
                printf("[TECLADO] Encerrado.\n");
                fflush(stdout);
                finished = true;

                /* Teclado puxa a responsabilidade de esperar todo mundo e abaixar a energia (FreeRTOS) */
                osal_event_wait_all(shutdown_event, OSAL_BIT_CONTADOR | OSAL_BIT_KEYBOARD | OSAL_BIT_SENSOR | OSAL_BIT_WAVEGEN);
                printf("\n[SISTEMA] Todos os modulos prumptos. Desligando...\n");
                fflush(stdout);

                osal_shutdown(); // No FreeRTOS, mata o scheduler aqui. No POSIX, não faz nada.
            }
            osal_delay_ms(10); 
            continue;
        }

        char c;
        // Leitura unbuffered não-bloqueante byte a byte (raw mode configurado pelo osal)
        while (read(STDIN_FILENO, &c, 1) > 0) {
            if (c == 27) { // 27 = ESC (Início de sequência ANSI para Setas)
                char seq[2];
                // Tentamos capturar as próximas 2 posições da sequência '[' e a direção
                int timeout = 50; 
                while (read(STDIN_FILENO, &seq[0], 1) <= 0 && timeout > 0) { osal_delay_ms(1); timeout--; }
                if (timeout > 0 && seq[0] == '[') {
                    timeout = 50;
                    while (read(STDIN_FILENO, &seq[1], 1) <= 0 && timeout > 0) { osal_delay_ms(1); timeout--; }
                    if (timeout > 0) {
                        osal_mutex_lock(wave_mutex);
                        if (seq[1] == 'A') { // UP
                            osc_amplitude += 5.1f;
                            if (osc_amplitude > 102.0f) osc_amplitude = 102.0f;
                            printf("\r[TECLADO] Amplitude aumentada (%.0f%%)       \n", (osc_amplitude / 102.0f) * 100.0f);
                        } else if (seq[1] == 'B') { // DOWN
                            osc_amplitude -= 5.1f;
                            if (osc_amplitude < 0.0f) osc_amplitude = 0.0f;
                            printf("\r[TECLADO] Amplitude reduzida (%.0f%%)       \n", (osc_amplitude / 102.0f) * 100.0f);
                        } else if (seq[1] == 'C') { // RIGHT (Aumentar Frequência)
                            if (current_wave == WAVE_FAST_SQUARE) {
                                fast_sq_n--; // Diminuir N -> Aumentar Frequência
                                if (fast_sq_n < 1) fast_sq_n = 1; // Limite MÁXIMO de velocidade
                                printf("\r[TECLADO] Frequencia aumentada (V Mode: N=%d)       \n", fast_sq_n);
                            } else {
                                osc_step += 0.0005f;
                                if (osc_step > 0.05f) osc_step = 0.05f;
                                printf("\r[TECLADO] Frequencia aumentada (passo %.4f)       \n", osc_step);
                            }
                        } else if (seq[1] == 'D') { // LEFT (Diminuir Frequência)
                            if (current_wave == WAVE_FAST_SQUARE) {
                                fast_sq_n++; // Aumentar N -> Diminuir Frequência
                                if (fast_sq_n > 50) fast_sq_n = 50; 
                                printf("\r[TECLADO] Frequencia reduzida (V Mode: N=%d)       \n", fast_sq_n);
                            } else {
                                osc_step -= 0.0005f;
                                if (osc_step < 0.0005f) osc_step = 0.0005f;
                                printf("\r[TECLADO] Frequencia reduzida (passo %.4f)       \n", osc_step);
                            }
                        }
                        osal_mutex_unlock(wave_mutex);
                    }
                }
            } else {
                // Interceptação das teclas alfanuméricas simples
                char key = c;
                if (key == 'f' || key == 'F') {
                    printf("\r[TECLADO] Respect Paid!\n");
                } else if (key == 'z' || key == 'Z') {
                    osal_mutex_lock(wave_mutex);
                    current_wave = WAVE_SINE; current_delay_us = 500;
                    osal_mutex_unlock(wave_mutex);
                    printf("\r[TECLADO] Onda alterada para SENO (Delay: 500us)       \n");
                } else if (key == 'a' || key == 'A') {
                    osal_mutex_lock(wave_mutex);
                    current_wave = WAVE_SAWTOOTH; current_delay_us = 500;
                    osal_mutex_unlock(wave_mutex);
                    printf("\r[TECLADO] Onda alterada para SERROTE (Delay: 500us)       \n");
                } else if (key == 'd' || key == 'D') {
                    osal_mutex_lock(wave_mutex);
                    current_wave = WAVE_INV_SAWTOOTH; current_delay_us = 500;
                    osal_mutex_unlock(wave_mutex);
                    printf("\r[TECLADO] Onda alterada para SERROTE INVERTIDA (Delay: 500us) \n");
                } else if (key == 'q' || key == 'Q') {
                    osal_mutex_lock(wave_mutex);
                    current_wave = WAVE_SQUARE; current_delay_us = 500;
                    osal_mutex_unlock(wave_mutex);
                    printf("\r[TECLADO] Onda alterada para QUADRADA (Delay: 500us)       \n");
                } else if (key == 't' || key == 'T') {
                    osal_mutex_lock(wave_mutex);
                    current_wave = WAVE_TRIANGLE; current_delay_us = 500;
                    osal_mutex_unlock(wave_mutex);
                    printf("\r[TECLADO] Onda alterada para TRIANGULAR (Delay: 500us)       \n");
                } else if (key == 'v' || key == 'V') {
                    osal_mutex_lock(wave_mutex);
                    current_wave = WAVE_FAST_SQUARE; 
                    current_delay_us = 300; // Alta velocidade pedida
                    fast_sq_n = 1;
                    osal_mutex_unlock(wave_mutex);
                    printf("\r[TECLADO] Onda VMAX/VMIN RAPIDA! Delay 100us, N=1!       \n");
                } else if (key == 'x' || key == 'X') {
                    osal_mutex_lock(wave_mutex);
                    if (current_wave == WAVE_FAST_SQUARE) {
                        fast_sq_n = 1;
                    } else {
                        osc_step = 0.0025f;
                    }
                    osc_amplitude = 74.1f;
                    osal_mutex_unlock(wave_mutex);
                    printf("\r[TECLADO] Valores padrao RESTAURADOS: Frequencia e Amplitude\n");
                } else if (key == 's' || key == 'S') {
                    osal_mutex_lock(wave_mutex);
                    if (current_wave == WAVE_FAST_SQUARE) {
                        fast_sq_n = 1;
                    } else {
                        osc_step = 0.0025f;
                    }
                    osal_mutex_unlock(wave_mutex);
                    printf("\r[TECLADO] Frequencia RESTAURADA para o padrao\n");
                } else if (key == 'w' || key == 'W') {
                    osal_mutex_lock(wave_mutex);
                    osc_amplitude = 74.1f;
                    osal_mutex_unlock(wave_mutex);
                    printf("\r[TECLADO] Amplitude RESTAURADA para o padrao\n");
                } else if (key == 'r' || key == 'R') {
                    spwm_enabled = !spwm_enabled;
                    if (spwm_enabled) {
                        printf("\r[TECLADO] Modulo SPWM: ATIVADO (Sinal Chaveado de Alta Frequencia)\n");
                    } else {
                        printf("\r[TECLADO] Modulo SPWM: DESATIVADO (Modo 'Real': Onda Analogica Pura)\n");
                    }
                } else if (key == 'c' || key == 'C') {
                    printf("\n[SISTEMA] Comando 'C' recebido...\n");
                    called_to_end = true;
                }
            }
        }
        osal_delay_ms(200);
    }
}

void task_pcf8591_monitor(void *arg) {
    (void)arg;
    hal_adc_init();

    printf("[SENSOR] Monitorando AIN3. Alimentando Ring Buffer...\n");
    
    bool finished = false;
    while (keep_running) {
        if (called_to_end) {
            if (!finished) {
                osal_event_set(shutdown_event, OSAL_BIT_SENSOR);
                printf("[SENSOR] Encerrado.\n");
                fflush(stdout);
                finished = true;
            }
            osal_delay_ms(10);
            continue;
        }

        uint8_t leitura = hal_adc_read(HAL_ADC_CH3);

        /* Insere no Ring Buffer SPSC (Produtor) */
        uint32_t next_head = (udp_ring_head + 1) % UDP_RING_SIZE;
        if (next_head != udp_ring_tail) {
            udp_ring_buffer[udp_ring_head] = leitura;
            udp_ring_head = next_head; // Operação atômica em ARM 32-bit
        }
        
        osal_delay_us(current_delay_us);
    }
}

void task_udp_streamer(void *arg) {
    (void)arg;
    
    int udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in dest_addr;
    memset(&dest_addr, 0, sizeof(dest_addr)); // CRITICO: Zera o sin_zero (padding) para evitar rejeicao do Kernel Linux (EINVAL)
    
    if (udp_socket >= 0) {
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_port = htons(8888); // Porta definida para o streaming
        // dest_addr.sin_addr.s_addr = inet_addr("255.255.255.255"); // Pode trocar para IP direto se o modem persistir no block
        // IP de Broadcast isolado para a sub-rede do cabo Ethernet (192.168.10.x). 
        // Garante que o pacote sai fisicamente pelo cabo Cat6 ignorando a antena Wi-Fi.
        // dest_addr.sin_addr.s_addr = inet_addr("192.168.10.255");
        // IP de Broadcast isolado para a sub-rede wi-fi (192.168.0.x). 
        dest_addr.sin_addr.s_addr = inet_addr("192.168.0.255");
        
        int broadcastEnable = 1; 
        setsockopt(udp_socket, SOL_SOCKET, SO_BROADCAST, &broadcastEnable, sizeof(broadcastEnable));
        printf("[NET] UDP Socket 8888 Criado com SUCESSO!\n");
    } else {
        perror("[ERRO FATAL] Falha absurda ao criar o socket na task_udp_streamer!");
    }

    printf("[NET] UDP Streamer iniciado (Modo Agrupamento Inteligente)\n");
    uint8_t local_buf[1500]; // Expande buffer local para caber seguramente a maior Quota Capped possivel (1470) sem C Stack-Buff Overflow!

    bool finished = false;
    while (keep_running) {
        if (called_to_end) {
            if (!finished) {
                osal_event_set(shutdown_event, OSAL_BIT_UDP);
                printf("[NET] UDP Streamer Encerrado.\n");
                fflush(stdout);
                finished = true;
            }
            osal_delay_ms(10);
            continue;
        }

        uint32_t quota = 147000 / current_delay_us; 
        
        // A "Menina dos Olhos": Garante que todas as configurações de velocidade resultem 
        // no exato mesmo pacing temporal absoluto da rede de 147ms (6.8Hz de disparo)!
        // Limite crítico travado na beleza matemática do MTU sem fragmentar: 1470.
        if (quota > 1470) quota = 1470; 
        if (quota < 1) quota = 1;

        // Verifica o tamanho da fila
        uint32_t head = udp_ring_head;
        uint32_t tail = udp_ring_tail;
        uint32_t count = (head >= tail) ? (head - tail) : (UDP_RING_SIZE - tail + head);

        if (count >= quota) {
            // Consome 'quota' itens da fila
            for (uint32_t i = 0; i < quota; i++) {
                local_buf[i] = udp_ring_buffer[udp_ring_tail];
                udp_ring_tail = (udp_ring_tail + 1) % UDP_RING_SIZE;
            }
            
            // Envia tudo em um único pacote consolidado! O modem perdoa.
            if (udp_socket >= 0) {
                int res = sendto(udp_socket, local_buf, quota, 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
                if (res < 0) {
                    perror("\r[ERRO UDP] Falha no sendto (kernel rejeitou pacote)");
                    osal_delay_ms(100); // Previne flood de prints
                }
            }
        } else {
            // Espera fila encher para prevenir busy-wait excessivo
            osal_delay_ms(1);
        }
    }

    if (udp_socket >= 0) {
        close(udp_socket);
    }
}


void task_wave_generator(void *arg) {
    (void)arg;
    hal_dac_init();
    float phase = 0.0f;

    printf("[WAVEGEN] Iniciando geração real de sinal no AOUT...\n");

    bool finished = false;
    while (keep_running) {
        if (called_to_end) {
            if (!finished) {
                osal_event_set(shutdown_event, OSAL_BIT_WAVEGEN);
                printf("[WAVEGEN] Encerrado.\n");
                fflush(stdout);
                finished = true;
            }
            osal_delay_ms(10);
            continue;
        }

        wave_type_t wave;
        float current_step;
        float current_amplitude;

        osal_mutex_lock(wave_mutex);
        wave = current_wave;
        current_step = osc_step;
        current_amplitude = osc_amplitude;
        osal_mutex_unlock(wave_mutex);

        uint8_t sample;
        if (wave == WAVE_FAST_SQUARE) {
            // Em modo V (FAST SQUARE), usamos o contador isolado em oscillators.c
            // NUNCA tocamos ou lemos o osc_step original!
            sample = oscillator_get_fast_square(fast_sq_n, current_amplitude);
        } else {
            // Qualquer outra onda opera via phase shift
            sample = oscillator_get_sample(wave, phase, current_amplitude);
            phase += current_step;
            if (phase >= 1.0f) phase -= 1.0f; // Evita zerar duro para manter continuidade no float
        }
        
        hal_dac_write(sample);

        osal_delay_us(current_delay_us);
    }
}

int main(void) {
    printf("--- SOFIA FIRMWARE: Teste de Arquitetura e Ondas ---\n");
    printf("--- Pressione Ctrl+C para sair ---\n");
    
    osal_mutex_create(&wave_mutex);
    osal_event_create(&shutdown_event);

    osal_input_register_shutdown(
        shutdown_callback,
        shutdown_event,
        OSAL_BIT_CONTADOR | OSAL_BIT_KEYBOARD | OSAL_BIT_SENSOR | OSAL_BIT_WAVEGEN | OSAL_BIT_UDP
    );

    osal_thread_t th1, th2, th3, th4, th5;
    
    if (osal_thread_create(&th1, "Contador", task_contador, NULL, 5, 4096) != 0) {
        fprintf(stderr, "[ERRO] Falha ao criar a tarefa: Contador\n");
    }
    if (osal_thread_create(&th2, "Keyboard", task_input_keyboard, NULL, 4, 4096) != 0) {
        fprintf(stderr, "[ERRO] Falha ao criar a tarefa: Keyboard\n");
    }
    if (osal_thread_create(&th3, "Sensor", task_pcf8591_monitor, NULL, 3, 4096) != 0) {
        fprintf(stderr, "[ERRO] Falha ao criar a tarefa: Sensor (PCF8591)\n");
    }
    if (osal_thread_create(&th4, "WaveGen", task_wave_generator, NULL, 1, 4096) != 0) {
        fprintf(stderr, "[ERRO] Falha ao criar a tarefa: WaveGen\n");
    }
    if (osal_thread_create(&th5, "UDPStream", task_udp_streamer, NULL, 2, 4096) != 0) {
        fprintf(stderr, "[ERRO] Falha ao criar a tarefa: UDP Streamer\n");
    }

    /* Inicia o scheduler (FreeRTOS) ou retorna imediatamente (POSIX via joins) */
    osal_run();

    /* Ponto de convergência mestre */
    osal_event_wait_all(shutdown_event, OSAL_BIT_CONTADOR | OSAL_BIT_KEYBOARD | OSAL_BIT_SENSOR | OSAL_BIT_WAVEGEN | OSAL_BIT_UDP);

    printf("\n[SISTEMA] Interrupção recebida. Encerrando tarefas...\n");
    fflush(stdout);

    /* Agora podemos libertar as pthreads presas do POSIX do seu delay e deixar elas retornarem em paz */
    keep_running = false;

    osal_thread_join(th1);
    osal_thread_join(th2);
    osal_thread_join(th3);
    osal_thread_join(th4);
    osal_thread_join(th5);
    
    osal_mutex_destroy(wave_mutex);
    osal_event_destroy(shutdown_event);

    printf("--- Firmware SOFIA encerrado ---\n");
    fflush(stdout);
    return 0;
}
