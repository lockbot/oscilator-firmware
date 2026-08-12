#ifndef HAL_DAC_H
#define HAL_DAC_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file hal_dac.h
 * @brief Interface para Conversor Digital-Analógico (DAC).
 */

/**
 * @brief Inicializa o driver DAC.
 * @return 0 em caso de sucesso.
 */
int hal_dac_init(void);

/**
 * @brief Escreve um valor na saída analógica.
 * @param value Valor a ser escrito (0-255).
 */
void hal_dac_write(uint8_t value);

#ifdef __cplusplus
}
#endif

#endif // HAL_DAC_H
