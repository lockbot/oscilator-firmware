#ifndef HAL_ADC_H
#define HAL_ADC_H

#include <stdint.h>

/**
 * @file hal_adc.h
 * @brief Interface para Conversor Analógico-Digital (ADC).
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Canais Analógicos do PCF8591.
 */
typedef enum {
    HAL_ADC_CH0 = 0, // Entrada Externa 0
    HAL_ADC_CH1 = 1, // Entrada Externa 1
    HAL_ADC_CH2 = 2, // LDR / Sensor de Luz (em muitas placas)
    HAL_ADC_CH3 = 3  // Trimpot / Potenciômetro (em muitas placas)
} hal_adc_channel_t;

/**
 * @brief Inicializa o driver ADC.
 * @return 0 em caso de sucesso.
 */
int hal_adc_init(void);

/**
 * @brief Lê um valor de um canal analógico.
 * @param channel Canal a ser lido.
 * @return Valor lido (0-255 para o PCF8591).
 */
uint8_t hal_adc_read(hal_adc_channel_t channel);

#ifdef __cplusplus
}
#endif

#endif // HAL_ADC_H
