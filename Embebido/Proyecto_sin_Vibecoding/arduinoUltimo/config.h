// ============================================================================
// config.h — Constantes del sistema (pines, tiempos, MQTT, tareas).
// Son const con tipo y no #define: así el compilador controla el tipo.
// Ningún otro archivo usa números sueltos.
// ============================================================================
#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ============================================================================
// DEPURACIÓN
// ============================================================================
const bool DEBUG_ACTIVO = true;             // false = modo productivo, sin logs

// ============================================================================
// DEFINICIÓN DE PINES (ESP32 FÍSICO)
// ============================================================================
const uint8_t PIN_LCD_SDA      = 21;
const uint8_t PIN_LCD_SCL      = 22;
const uint8_t PIN_SERVO        = 27;
const uint8_t PIN_BUZZER       = 14;
const uint8_t PIN_POT_MIC      = 34;        // Entradas analógicas en ADC1 (el ADC2 falla con WiFi)
const uint8_t PIN_POT_OHM      = 35;
const uint8_t PIN_LED_PROG1    = 2;
const uint8_t PIN_LED_PROG2    = 4;
const uint8_t PIN_LED_PROG3    = 5;
const uint8_t PIN_LED_AMARILLO = 18;
const uint8_t PIN_BTN_RESET    = 23;

// ============================================================================
// PANTALLA Y SERVO
// ============================================================================
const int LCD_COLUMNAS = 16;
const int LCD_FILAS    = 2;
const int LCD_BRILLO   = 80;

const int SERVO_ANGULO_CERRADO = 15;
const int SERVO_ANGULO_ABIERTO = 90;
const int SERVO_FRECUENCIA_HZ  = 50;
const int SERVO_MIN_PULSO_US   = 500;
const int SERVO_MAX_PULSO_US   = 2400;

// ============================================================================
// PARÁMETROS DEL JUEGO
// ============================================================================
const int MAX_VIDAS      = 3;
const int TOTAL_DESAFIOS = 3;

// Desafío 1: tres aplausos dentro de una ventana de 5 s
const int D1_APLAUSOS_OBJETIVO = 3;
const int D1_PRIMER_APLAUSO    = 1;
const int UMBRAL_SONIDO_ALTO   = 2000;      // Nivel ADC que cuenta como aplauso
const int UMBRAL_SONIDO_BAJO   = 1400;      // Nivel ADC para rearmar el detector
const unsigned long TIEMPO_VENTANA_D1   = 5000;
const unsigned long DEBOUNCE_APLAUSO_MS = 200;
const unsigned long SEMIPERIODO_MAX_MS  = 300; // Parpadeo del LED amarillo (lento)
const unsigned long SEMIPERIODO_MIN_MS  = 60;  // Parpadeo del LED amarillo (rápido)

// Desafío 2: Ley de Ohm, V = I · R = 100 Ω · 20 mA = 2 V
const int R_OHM_VALOR          = 100;
const int I_CORRIENTE_MA       = 20;
const int V_OBJETIVO_MV        = R_OHM_VALOR * I_CORRIENTE_MA;
const int TOLERANCIA_MV        = 150;       // ±0,15 V (alineado con la tolerancia de la App Android)
const int UMBRAL_RUIDO_ADC     = 40;        // Cambio mínimo para considerar que se movió
const unsigned long TIEMPO_INACTIVIDAD_D2 = 3000; // Quieto este tiempo = valor elegido
const int ADC_MAXIMO           = 4095;      // ADC de 12 bits
const int ADC_MILIVOLTS_MAXIMO = 3300;      // 3,3 V
const float MILIVOLTS_POR_VOLT = 1000.0f;

// Tiempos de los estados
const unsigned long TIEMPO_LIMITE_DESAFIO    = 60000;  // D1 y D2 sin resolver
const unsigned long TIEMPO_LIMITE_D3         = 120000; // D3: la app tiene su juego de 30 s
const unsigned long TIEMPO_MOSTRAR_RESULTADO = 1500;
const unsigned long TIEMPO_PENALIZACION      = 60000;
const unsigned long TIEMPO_RESETEANDO        = 3000;
const unsigned long DEBOUNCE_BTN_MS          = 50;
const unsigned long MS_POR_SEGUNDO           = 1000;

// ============================================================================
// SONIDOS DEL BUZZER (frecuencia en Hz, duración en ms)
// ============================================================================
const unsigned int  FREC_BEEP     = 2000;
const unsigned long DUR_BEEP      = 100;
const unsigned int  FREC_ACIERTO1 = 1500;
const unsigned long DUR_ACIERTO1  = 150;
const unsigned int  FREC_ACIERTO2 = 2500;
const unsigned long DUR_ACIERTO2  = 300;
const unsigned int  FREC_ERROR    = 400;
const unsigned long DUR_ERROR     = 400;
const unsigned long PAUSA_SONIDO  = 30;     // Silencio después de cada nota

// ============================================================================
// SERIAL, WIFI Y MQTT (usuario y contraseña del WiFi en secrets.h)
// ============================================================================
const unsigned long VELOCIDAD_SERIAL = 115200;
const unsigned long TIMEOUT_SERIAL   = 50;
const unsigned long PERIODO_SERIAL   = 20;

const char *const TOPICO_SEL_DESAFIO = "soa/escape/seleccion"; // App -> ESP32: "1", "2", "3"
const char *const TOPICO_D3_CODIGO   = "soa/escape/d3";        // App -> ESP32: "OK", "ERROR"
const char *const TOPICO_TX_POT      = "soa/escape/pot";
const char *const TOPICO_TX_DESAFIO  = "soa/escape/desafio";
const char *const TOPICO_TX_ESTADO   = "soa/escape/estado";
const char *const TOPICO_TX_VIDAS    = "soa/escape/vidas";
const char *const MQTT_BROKER        = "broker.hivemq.com";
const uint16_t    MQTT_PORT          = 1883;
const unsigned long ESPERA_WIFI      = 500;
const unsigned long ESPERA_RECONEXION = 2000;
const unsigned long PERIODO_MQTT     = 20;
const size_t TAM_TOPICO       = 32;      // Largo máximo de un tópico MQTT
const size_t TAM_PAYLOAD      = 16;      // Largo máximo de un mensaje MQTT
const size_t TAM_BUFFER_TEXTO = 32;      // Buffer para armar textos con snprintf

// ============================================================================
// CONFIGURACIÓN DE TAREAS FREERTOS (pila múltiplo de 1024)
// ============================================================================
const uint32_t TAM_PILA_FSM     = 1024 * 4;
const uint32_t TAM_PILA_EVENTOS = 1024 * 2;
const uint32_t TAM_PILA_LED     = 1024 * 2;
const uint32_t TAM_PILA_BUZZER  = 1024 * 2;
const uint32_t TAM_PILA_LCD     = 1024 * 3;
const uint32_t TAM_PILA_COM     = 1024 * 3;
const uint32_t TAM_PILA_MQTT    = 1024 * 8;

const UBaseType_t PRIORIDAD_FSM     = 3;
const UBaseType_t PRIORIDAD_EVENTOS = 2;
const UBaseType_t PRIORIDAD_LED     = 1;
const UBaseType_t PRIORIDAD_BUZZER  = 2;
const UBaseType_t PRIORIDAD_LCD     = 1;
const UBaseType_t PRIORIDAD_COM     = 2;
const UBaseType_t PRIORIDAD_MQTT    = 2;

const unsigned long TMP_EVENTOS_MILI = 5;   // Un sensor por vuelta: cada uno cada 15 ms

// Largo de las colas
const UBaseType_t LARGO_COLA_EVENTOS = 10;
const UBaseType_t LARGO_COLA_BUZZER  = 5;
const UBaseType_t LARGO_COLA_LCD     = 8;
const UBaseType_t LARGO_COLA_LED     = 4;
const UBaseType_t LARGO_COLA_MQTT    = 10;
const TickType_t  SIN_ESPERA         = 0;   // Enviar sin bloquear

#endif
