#include "oscillators.h"
#include <math.h>
#include <stdbool.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

extern volatile bool spwm_enabled;

uint8_t oscillator_get_sample(wave_type_t type, float phase, float amplitude_spread) {
    float val = 0.0f;

    switch (type) {
        case WAVE_SINE:
            val = (sinf(2.0f * M_PI * phase) + 1.0f) / 2.0f; // 0.0 a 1.0
            break;
        case WAVE_SAWTOOTH:
            val = phase; // 0.0 a 1.0
            break;
        case WAVE_SQUARE:
            val = (phase < 0.5f) ? 1.0f : 0.0f; // 0.0 a 1.0
            break;
        case WAVE_TRIANGLE:
            if (phase < 0.5f) {
                val = phase * 2.0f; // Sobe de 0.0 a 1.0
            } else {
                val = 1.0f - ((phase - 0.5f) * 2.0f); // Desce de 1.0 a 0.0
            }
            break;
        case WAVE_INV_SAWTOOTH:
            val = 1.0f - phase; // 1.0 a 0.0
            break;
        default:
            val = 0.0f;
            break;
    }

    if (!spwm_enabled) {
        // Mapeia a forma de onda (0.0 até 1.0) para os limites físicos (centro = 114).
        // val=0.0 -> (114 - amplitude_spread)
        // val=1.0 -> (114 + amplitude_spread)
        return (uint8_t)((114.0f - amplitude_spread) + (val * (amplitude_spread * 2.0f)));
    }

    /* 
     * ==========================================
     *  SPWM GENERATOR (Sinusoidal PWM)
     * ==========================================
     */
    float mf = 21.0f;
    float carrier = fmodf(phase * mf, 1.0f);

    uint8_t sample_spwm;
    if (val >= carrier) {
        /* HIGH: Teto dinâmico baseado na amplitude atual (max = 216) */
        sample_spwm = (uint8_t)(114.0f + amplitude_spread);
    } else {
        /* LOW: Chão dinâmico baseado na amplitude atual (min = 12) */
        sample_spwm = (uint8_t)(114.0f - amplitude_spread);
    }

    return sample_spwm;
}

uint8_t oscillator_get_fast_square(int speed_freq, float amplitude_spread) {
    static int counter = 0;
    static uint8_t current_state = 1; // 1 = HIGH, 0 = LOW
    
    counter++;
    if (counter > speed_freq) {
        counter = 1;
        current_state = !current_state;
    }
    
    if (current_state) {
        return (uint8_t)(114.0f + amplitude_spread);
    } else {
        return (uint8_t)(114.0f - amplitude_spread);
    }
}

