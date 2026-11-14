// ============================================================================
// eventos.h — Todo lo que GENERA eventos para la FSM, salvo Serial y MQTT
// (que están en comunicacion.h):
//  - Temporizadores de FreeRTOS: cuando vencen, encolan su evento.
//  - tarea_eventos: lee los sensores y encola un evento sólo si pasó algo.
// Acá no se conoce el estado de la FSM ni se tocan actuadores.
// ============================================================================
#ifndef EVENTOS_H
#define EVENTOS_H

#include "tipos.h"

// ============================================================================
// TEMPORIZADORES (software timers de FreeRTOS, reemplazan a millis())
//  - timer_timeout: tiempo límite de los desafíos -> EVENTO_TIMEOUT_DESAFIO
//  - timer_estado:  estados que salen solos (resultado, penalización, reset).
//    Al programarlo se le guarda en su ID el evento que tiene que encolar.
// Los callbacks corren en la tarea de timers de FreeRTOS: sólo encolan.
// ============================================================================
TimerHandle_t timer_timeout = NULL;
TimerHandle_t timer_estado  = NULL;

void callback_timeout(TimerHandle_t timer) {
    encolar_evento(EVENTO_TIMEOUT_DESAFIO, 0);
}

void callback_estado(TimerHandle_t timer) {
    encolar_evento((tipo_evento_t)(intptr_t)pvTimerGetTimerID(timer), 0);
}

// Se crean detenidos (pdFALSE = un solo disparo); el período se fija al iniciarlos.
void crear_timers() {
    timer_timeout = xTimerCreate("timer_timeout", pdMS_TO_TICKS(TIEMPO_LIMITE_DESAFIO), pdFALSE, NULL, callback_timeout);
    timer_estado  = xTimerCreate("timer_estado", pdMS_TO_TICKS(TIEMPO_RESETEANDO), pdFALSE, NULL, callback_estado);
}

// Arranca (o reinicia) la cuenta del timeout del desafío.
void iniciar_timeout(unsigned long duracion_ms) {
    xTimerChangePeriod(timer_timeout, pdMS_TO_TICKS(duracion_ms), SIN_ESPERA);
}

void detener_timeout() {
    xTimerStop(timer_timeout, SIN_ESPERA);
}

// Dentro de duracion_ms se encola "evento" (ej.: FIN_RESET dentro de 3 s).
void programar_evento(tipo_evento_t evento, unsigned long duracion_ms) {
    vTimerSetTimerID(timer_estado, (void *)(intptr_t)evento);
    xTimerChangePeriod(timer_estado, pdMS_TO_TICKS(duracion_ms), SIN_ESPERA);
}

void detener_timers() {
    xTimerStop(timer_timeout, SIN_ESPERA);
    xTimerStop(timer_estado, SIN_ESPERA);
}

// ============================================================================
// SENSORES: cada verificar_*() lee su sensor y devuelve true si generó un
// evento. Las variables de cada sensor son "estado del sensor", no del juego.
// ============================================================================
int aplausos_contados = 0;
unsigned long t_primer_aplauso = 0;
unsigned long t_ultimo_aplauso = 0;
bool mic_armado = true;

int adc_pot_ultimo = 0;
unsigned long t_ultimo_mov_pot = 0;
bool pot_empezo_a_mover = false;

int btn_reset_anterior = HIGH;
unsigned long t_ultimo_cambio_btn = 0;

// Tiempo actual según el reloj de FreeRTOS.
unsigned long ahora_ms() {
    return xTaskGetTickCount() * portTICK_PERIOD_MS;
}

int adc_a_milivolts(int lectura_adc) {
    return (lectura_adc * ADC_MILIVOLTS_MAXIMO) / ADC_MAXIMO;
}

void inicializar_sensores() {
    pinMode(PIN_POT_MIC, INPUT);
    pinMode(PIN_POT_OHM, INPUT);
    pinMode(PIN_BTN_RESET, INPUT_PULLUP);
    adc_pot_ultimo = analogRead(PIN_POT_OHM);
    btn_reset_anterior = digitalRead(PIN_BTN_RESET);
}

// Pulsador: un evento por pulsación (flanco a LOW), con antirrebote.
bool verificar_boton_reset(evento_t &ev) {
    int btn_lectura = digitalRead(PIN_BTN_RESET);
    unsigned long ahora = ahora_ms();
    if (btn_lectura == btn_reset_anterior || ahora - t_ultimo_cambio_btn <= DEBOUNCE_BTN_MS) {
        return false;
    }
    t_ultimo_cambio_btn = ahora;
    btn_reset_anterior = btn_lectura;
    ev = { EVENTO_BTN_RESET, 0 };
    return btn_lectura == LOW;
}

// Micrófono: un aplauso es un pico sobre UMBRAL_SONIDO_ALTO; para contar otro
// el sonido tiene que bajar de UMBRAL_SONIDO_BAJO. Cuenta los aplausos de la
// ventana de 5 s: el tercero genera D1_CANT_APLAUSOS_ALCANZADA.
bool verificar_microfono(evento_t &ev) {
    int adc_mic = analogRead(PIN_POT_MIC);
    unsigned long ahora = ahora_ms();
    if (!mic_armado) {
        mic_armado = (adc_mic < UMBRAL_SONIDO_BAJO);
        return false;
    }
    if (adc_mic <= UMBRAL_SONIDO_ALTO || ahora - t_ultimo_aplauso <= DEBOUNCE_APLAUSO_MS) {
        return false;
    }
    mic_armado = false;
    t_ultimo_aplauso = ahora;
    if (aplausos_contados == 0 || ahora - t_primer_aplauso > TIEMPO_VENTANA_D1) {
        aplausos_contados = 0;                  // Empieza una ventana nueva
        t_primer_aplauso = ahora;
    }
    aplausos_contados++;
    bool alcanzado = (aplausos_contados >= D1_APLAUSOS_OBJETIVO);
    ev = { alcanzado ? EVENTO_D1_CANT_APLAUSOS_ALCANZADA : EVENTO_APLAUSO_DETECTADO, aplausos_contados };
    if (alcanzado) {
        aplausos_contados = 0;
    }
    return true;
}

// Potenciómetro: VALOR_POT_CAMBIO cada vez que se mueve, y
// D2_VALOR_POT_CORRECTO si queda quieto 3 s dentro de la tolerancia.
bool verificar_potenciometro(evento_t &ev) {
    int adc_pot = analogRead(PIN_POT_OHM);
    unsigned long ahora = ahora_ms();
    if (abs(adc_pot - adc_pot_ultimo) > UMBRAL_RUIDO_ADC) {
        adc_pot_ultimo = adc_pot;
        t_ultimo_mov_pot = ahora;
        pot_empezo_a_mover = true;
        ev = { EVENTO_VALOR_POT_CAMBIO, adc_a_milivolts(adc_pot) };
        return true;
    }
    if (!pot_empezo_a_mover || ahora - t_ultimo_mov_pot < TIEMPO_INACTIVIDAD_D2) {
        return false;
    }
    pot_empezo_a_mover = false;
    int milivolts = adc_a_milivolts(adc_pot_ultimo);
    ev = { EVENTO_D2_VALOR_POT_CORRECTO, milivolts };
    return abs(milivolts - V_OBJETIVO_MV) <= TOLERANCIA_MV;
}

// ============================================================================
// CAPTURA DE EVENTOS (tarea_eventos)
// Recorre los sensores con un vector de funciones e índice rotativo: en cada
// vuelta revisa UNO, así ninguno tapa a los demás.
// ============================================================================
typedef bool (*verificador_t)(evento_t &ev);

const verificador_t verificar_sensor[] = {
    verificar_boton_reset,
    verificar_microfono,
    verificar_potenciometro
};
const int CANT_SENSORES = sizeof(verificar_sensor) / sizeof(verificar_sensor[0]);

void tarea_eventos(void *parameter) {
    int indice = 0;
    evento_t ev;
    while (1) {
        if (verificar_sensor[indice](ev)) {
            xQueueSend(cola_eventos, &ev, SIN_ESPERA);
        }
        indice = (indice + 1) % CANT_SENSORES;
        vTaskDelay(pdMS_TO_TICKS(TMP_EVENTOS_MILI));
    }
}

#endif
