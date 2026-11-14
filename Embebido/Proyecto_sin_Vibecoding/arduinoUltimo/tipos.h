// ============================================================================
// tipos.h — Estados, eventos, mensajes de las colas y log de depuración.
// Los nombres de estados y eventos son los del diagrama del informe
// (Informes/FSM.png) con el prefijo ESTADO_ / EVENTO_.
// ============================================================================
#ifndef TIPOS_H
#define TIPOS_H

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/timers.h>
#include "config.h"

// ============================================================================
// TIPOS DE DATOS: ESTADOS Y EVENTOS (FSM PURA)
// ============================================================================
typedef enum {
    ESTADO_ESPERANDO_DESAFIO,
    ESTADO_D1_ESPERANDO_APLAUSO,
    ESTADO_D2_ESPERANDO_CAMBIO_POT,
    ESTADO_D3_ESPERANDO_CODIGO,
    ESTADO_DESAFIO_FINALIZADO,
    ESTADO_PENALIZACION_BLOQUEADO,
    ESTADO_CAJA_DESBLOQUEADA,
    ESTADO_RESETEANDO,
    CANT_ESTADOS
} estado_fsm_t;

typedef enum {
    EVENTO_CONTINUE,                    // "No pasó nada": nunca se encola
    EVENTO_DESAFIO_1,
    EVENTO_DESAFIO_2,
    EVENTO_DESAFIO_3,
    EVENTO_APLAUSO_DETECTADO,
    EVENTO_D1_CANT_APLAUSOS_ALCANZADA,
    EVENTO_VALOR_POT_CAMBIO,
    EVENTO_D2_VALOR_POT_CORRECTO,
    EVENTO_D3_CODIGO_OK,
    EVENTO_D3_CODIGO_ERROR,
    EVENTO_TIMEOUT_DESAFIO,
    EVENTO_CON_VIDAS,
    EVENTO_SIN_VIDAS,
    EVENTO_DESAFIOS_COMPLETADOS,
    EVENTO_TIMEOUT_FIN_PENALIZACION,
    EVENTO_BTN_RESET,
    EVENTO_FIN_RESET,
    CANT_EVENTOS
} tipo_evento_t;

// valor_aux: dato del evento (número de aplauso, tensión en mV)
typedef struct {
    tipo_evento_t tipo;
    int valor_aux;
} evento_t;

// ============================================================================
// MENSAJES DE LAS COLAS DE ACTUADORES
// ============================================================================
typedef enum {
    SONIDO_BEEP,
    SONIDO_ACIERTO,
    SONIDO_ERROR
} tipo_sonido_t;

// Si cuenta_regresiva > 0, tarea_lcd descuenta sola un segundo por vez
typedef struct {
    char linea1[LCD_COLUMNAS + 1];
    char linea2[LCD_COLUMNAS + 1];
    int cuenta_regresiva;
} msg_lcd_t;

typedef enum {
    LED_CMD_APAGAR,
    LED_CMD_FIJO,
    LED_CMD_RITMO
} modo_led_t;

typedef struct {
    modo_led_t modo;
    unsigned long semiperiodo_ms;
} cmd_led_amarillo_t;

typedef struct {
    char topico[TAM_TOPICO];
    char payload[TAM_PAYLOAD];
} msg_mqtt_tx_t;

// ============================================================================
// COLAS (se crean en start())
// ============================================================================
QueueHandle_t cola_eventos = NULL;      // Sensores, Serial, MQTT y timers -> tarea_fsm
QueueHandle_t cola_buzzer  = NULL;      // tarea_fsm -> tarea_buzzer
QueueHandle_t cola_lcd     = NULL;      // tarea_fsm -> tarea_lcd
QueueHandle_t cola_led     = NULL;      // tarea_fsm -> tarea_led_ritmo
QueueHandle_t cola_mqtt_tx = NULL;      // tarea_fsm -> tarea_mqtt

// Variable global de vidas compartida con comunicacion.h
extern int vidas;

// Encola un evento para la FSM sin bloquear (lo usan todos los productores).
void encolar_evento(tipo_evento_t tipo, int valor_aux) {
    evento_t ev = { tipo, valor_aux };
    xQueueSend(cola_eventos, &ev, SIN_ESPERA);
}

// ============================================================================
// LOG DE DEPURACIÓN
// Escriben dos tareas (FSM y MQTT): el mutex evita que se mezclen las líneas.
// Se usa igual que Serial.printf. Con DEBUG_ACTIVO = false no imprime nada.
// ============================================================================
SemaphoreHandle_t mutex_serial = NULL;

#define LOG_DEBUG(...)                                  \
    do {                                                \
        if (DEBUG_ACTIVO) {                             \
            xSemaphoreTake(mutex_serial, portMAX_DELAY);\
            Serial.printf(__VA_ARGS__);                 \
            xSemaphoreGive(mutex_serial);               \
        }                                               \
    } while (0)

#endif
