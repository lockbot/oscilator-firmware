#include "hal_adc.h"
#include "hal_dac.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __linux__
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#endif

#include "osal_thread.h"

/* Endereço I2C padrão do PCF8591 */
#define PCF8591_I2C_ADDR 0x48
#define I2C_BUS "/dev/i2c-1"

static int i2c_fd = -1;
static bool i2c_initialized = false;
static osal_mutex_t i2c_mutex;

/* 
 * Definições do Byte de Controle do PCF8591:
 * Bit 7: Reservado (0)
 * Bit 6: Analog Output Enable (1 = Habilita DAC)
 * Bit 5-4: Analog Input Programming (00 = 4 Canais Single-ended)
 * Bit 3: Reservado (0)
 * Bit 2: Auto-incremento (0 = Desabilitado)
 * Bit 1-0: Seleção de Canal (00 a 11)
 */
#define PCF8591_CTRL_DAC_ENABLE   0x40
#define PCF8591_CTRL_CH_MASK      0x03

/**
 * @brief Função interna para inicializar o barramento I2C
 */
static int init_i2c_bus(void) {
    if (i2c_initialized) return 0;

    osal_mutex_create(&i2c_mutex);

#ifdef __linux__
    i2c_fd = open(I2C_BUS, O_RDWR);
    if (i2c_fd < 0) {
        fprintf(stderr, "[HAL][ERRO] Falha ao abrir o barramento I2C (%s)\n", I2C_BUS);
        return -1;
    }

    if (ioctl(i2c_fd, I2C_SLAVE, PCF8591_I2C_ADDR) < 0) {
        fprintf(stderr, "[HAL][ERRO] Falha ao conectar ao dispositivo I2C no endereço 0x%02X\n", PCF8591_I2C_ADDR);
        close(i2c_fd);
        i2c_fd = -1;
        return -1;
    }

    i2c_initialized = true;
    printf("[HAL] Comunicação I2C com PCF8591 inicializada.\n");
    return 0;
#else
    fprintf(stderr, "[HAL][AVISO] Ambiente não-Linux. I2C não disponível.\n");
    return -1;
#endif
}

int hal_adc_init(void) {
    return init_i2c_bus();
}

uint8_t hal_adc_read(hal_adc_channel_t channel) {
    if (!i2c_initialized) {
        if (init_i2c_bus() != 0) return 0;
    }

#ifdef __linux__
    /* 
     * Monta o byte de controle:
     * Habilitamos o DAC (bit 6) e selecionamos o canal solicitado (bits 0-1)
     */
    uint8_t control_byte = PCF8591_CTRL_DAC_ENABLE | (channel & PCF8591_CTRL_CH_MASK); 
    // Lê o valor (geralmente a primeira leitura após mudar de canal é o valor antigo, 
    // então lemos duas vezes para garantir o valor atual, padrão do PCF8591)
    uint8_t val[2] = {0};
    
    osal_mutex_lock(i2c_mutex);
    
    if (write(i2c_fd, &control_byte, 1) != 1) {
        osal_mutex_unlock(i2c_mutex);
        return 0;
    }
    

    if (read(i2c_fd, val, 2) != 2) {
        osal_mutex_unlock(i2c_mutex);
        return 0;
    }
    
    osal_mutex_unlock(i2c_mutex);
    return val[1];
#else
    (void)channel;
    return 0;
#endif
}

int hal_dac_init(void) {
    return init_i2c_bus();
}

void hal_dac_write(uint8_t value) {
    if (!i2c_initialized) {
        if (init_i2c_bus() != 0) return;
    }

#ifdef __linux__
    uint8_t buffer[2];
    buffer[0] = PCF8591_CTRL_DAC_ENABLE; // Habilita saída analógica
    buffer[1] = value;                   // Valor da amostra (0-255)
    
    osal_mutex_lock(i2c_mutex);
    if (write(i2c_fd, buffer, 2) != 2) {
        fprintf(stderr, "[HAL][ERRO] Falha ao escrever dado I2C (DAC)\n");
    }
    osal_mutex_unlock(i2c_mutex);
#else
    (void)value;
#endif
}
