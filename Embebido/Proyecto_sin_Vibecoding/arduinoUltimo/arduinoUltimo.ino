// ============================================================================
// Sistema de Desafíos Interactivos (Proyecto Escape Room) — SOA, Lunes Grupo 2
//
// Patrón de la cátedra (FSM + FreeRTOS, productor-consumidor):
//   tarea_eventos (sensores) ─┐
//   tarea_comunicacion (Serial)├─ cola_eventos ─▶ tarea_fsm ─▶ colas ─▶ tarea_lcd
//   tarea_mqtt (app Android) ─┤                                     ├─▶ tarea_buzzer
//   timers de FreeRTOS ───────┘                                     └─▶ tarea_led_ritmo
//
// Archivos:
//   config.h        constantes (pines, tiempos, MQTT, tareas, debug)
//   tipos.h         estados, eventos, colas y log
//   eventos.h       timers y lectura de sensores (generan eventos)
//   comunicacion.h  Serial y MQTT
//   actuadores.h    pantalla, LEDs, servo, buzzer y sus tareas
//   fsm.h           máquina de estados y lógica del juego
//   secrets.h       WiFi (no se sube al repositorio)
// ============================================================================
#include "config.h"
#include "tipos.h"
#include "eventos.h"
#include "actuadores.h"
#include "comunicacion.h"
#include "fsm.h"

void crear_colas() {
    cola_eventos = xQueueCreate(LARGO_COLA_EVENTOS, sizeof(evento_t));
    cola_buzzer  = xQueueCreate(LARGO_COLA_BUZZER, sizeof(tipo_sonido_t));
    cola_lcd     = xQueueCreate(LARGO_COLA_LCD, sizeof(msg_lcd_t));
    cola_led     = xQueueCreate(LARGO_COLA_LED, sizeof(cmd_led_amarillo_t));
    cola_mqtt_tx = xQueueCreate(LARGO_COLA_MQTT, sizeof(msg_mqtt_tx_t));
}

// Las tareas se crean una sola vez acá, nunca en loop().
void crear_tareas() {
    xTaskCreate(tarea_fsm, "Task_FSM", TAM_PILA_FSM, NULL, PRIORIDAD_FSM, NULL);
    xTaskCreate(tarea_eventos, "Task_Eventos", TAM_PILA_EVENTOS, NULL, PRIORIDAD_EVENTOS, NULL);
    xTaskCreate(tarea_led_ritmo, "Task_LED", TAM_PILA_LED, NULL, PRIORIDAD_LED, NULL);
    xTaskCreate(tarea_buzzer, "Task_Buzzer", TAM_PILA_BUZZER, NULL, PRIORIDAD_BUZZER, NULL);
    xTaskCreate(tarea_lcd, "Task_LCD", TAM_PILA_LCD, NULL, PRIORIDAD_LCD, NULL);
    xTaskCreate(tarea_comunicacion, "Task_Com", TAM_PILA_COM, NULL, PRIORIDAD_COM, NULL);
    xTaskCreate(tarea_mqtt, "Task_MQTT", TAM_PILA_MQTT, NULL, PRIORIDAD_MQTT, NULL);
}

// Inicializa todo y asigna el estado inicial de la FSM.
void start() {
    Serial.begin(VELOCIDAD_SERIAL);
    Serial.setTimeout(TIMEOUT_SERIAL);
    mutex_serial = xSemaphoreCreateMutex();
    LOG_DEBUG("\n=== INICIANDO SISTEMA ESCAPE ROOM ===\n");

    crear_colas();
    crear_timers();
    inicializar_sensores();
    inicializar_actuadores();

    estado_actual = ESTADO_ESPERANDO_DESAFIO;
    reiniciar_juego();                  // Vidas, LEDs, servo y menú en pantalla
    crear_tareas();
}

void setup() {
    start();
}

// Todo lo hacen las tareas de FreeRTOS: la tarea de Arduino que ejecuta
// loop() se elimina para que no gaste CPU (y no hay delay()).
void loop() {
    vTaskDelete(NULL);
}
