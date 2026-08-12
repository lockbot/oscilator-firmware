#ifndef OSCILLATORS_H
#define OSCILLATORS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Tipos de ondas suportados pelo gerador.
 */
typedef enum {
    WAVE_SINE = 0,     // Onda Senoidal
    WAVE_SAWTOOTH,    // Onda Dente de Serra
    WAVE_SQUARE,      // Onda Quadrada
    WAVE_TRIANGLE,    // Onda Triangular Normal
    WAVE_INV_SAWTOOTH,// Onda Serrote Invertida (Desce)
    WAVE_FAST_SQUARE  // Onda Alternante de Alta Velocidade (Triangulo/Quadrada rapida dependente de N)
} wave_type_t;

/**
 * @brief Calcula o próximo valor da amostra para uma onda.
 * @param type Tipo da onda.
 * @param phase Fase atual (0.0 a 1.0).
 * @param amplitude_spread Afastamento percentual/físico do centro (114) — máx 102.
 * @return Valor da amostra (12-216).
 */
uint8_t oscillator_get_sample(wave_type_t type, float phase, float amplitude_spread);
uint8_t oscillator_get_fast_square(int speed_freq, float amplitude_spread);

#ifdef __cplusplus
}
#endif

#endif // OSCILLATORS_H
