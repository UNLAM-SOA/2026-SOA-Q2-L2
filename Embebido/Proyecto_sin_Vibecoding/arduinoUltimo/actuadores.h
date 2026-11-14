// ============================================================================
// actuadores.h — Funciones act_*() que usa la FSM y tareas de los actuadores.
// Servo y LEDs verdes se manejan directo. Pantalla, buzzer y LED amarillo
// tardan, así que la FSM les deja la orden en una cola y sigue: lo hace su
// propia tarea, sin trabar a la máquina de estados.
// ============================================================================
#ifndef ACTUADORES_H
#define ACTUADORES_H

#include <Wire.h>
#include <ESP32Servo.h>
#include "rgb_lcd.h"
#include "tipos.h"

rgb_lcd lcd;
Servo servo_traba;

const uint8_t PINES_LED_PROG[TOTAL_DESAFIOS + 1] = { 0, PIN_LED_PROG1, PIN_LED_PROG2, PIN_LED_PROG3 };

void inicializar_actuadores() {
    for (int i = 1; i <= TOTAL_DESAFIOS; i++) {
        pinMode(PINES_LED_PROG[i], OUTPUT);
        digitalWrite(PINES_LED_PROG[i], LOW);
    }
    pinMode(PIN_LED_AMARILLO, OUTPUT);
    pinMode(PIN_BUZZER, OUTPUT);

    Wire.begin(PIN_LCD_SDA, PIN_LCD_SCL);
    lcd.begin(LCD_COLUMNAS, LCD_FILAS);
    lcd.setRGB(LCD_BRILLO, LCD_BRILLO, LCD_BRILLO);
    lcd.clear();

    servo_traba.setPeriodHertz(SERVO_FRECUENCIA_HZ);
    servo_traba.attach(PIN_SERVO, SERVO_MIN_PULSO_US, SERVO_MAX_PULSO_US);
    servo_traba.write(SERVO_ANGULO_CERRADO);
}

// ============================================================================
// PANTALLA
// ============================================================================
void enviar_a_lcd(const char *l1, const char *l2, int cuenta_regresiva = 0) {
    msg_lcd_t msg;
    memset(&msg, 0, sizeof(msg));
    strncpy(msg.linea1, l1, LCD_COLUMNAS);
    strncpy(msg.linea2, l2, LCD_COLUMNAS);
    msg.cuenta_regresiva = cuenta_regresiva;
    xQueueSend(cola_lcd, &msg, SIN_ESPERA);
}

void act_lcd_menu_principal(int vidas) {
    char buf[TAM_BUFFER_TEXTO];
    snprintf(buf, sizeof(buf), "Vidas:%d Use App", vidas);
    enviar_a_lcd("ESCAPE ROOM SOA", buf);
}

void act_lcd_reseteando() {
    enviar_a_lcd("SISTEMA ESCAPE", "Reseteando...");
}

void act_lcd_d1_intro() {
    enviar_a_lcd("La clave no se", "lee, se escucha.");
}

void act_lcd_d2_intro() {
    char buf[TAM_BUFFER_TEXTO];
    snprintf(buf, sizeof(buf), "D2:R=%d I=%dmA", R_OHM_VALOR, I_CORRIENTE_MA);
    enviar_a_lcd(buf, "V=...");
}

void act_lcd_d3_intro() {
    enviar_a_lcd("D3: App Android", "Esperando cod..");
}

void act_lcd_resultado(bool exito, const char *msg) {
    enviar_a_lcd(exito ? "** ACIERTO **" : "* ERROR/TIMEOUT *", msg);
}

// La cuenta regresiva la actualiza tarea_lcd sola, segundo a segundo.
void act_lcd_penalizacion(int segs_restantes) {
    char buf[TAM_BUFFER_TEXTO];
    snprintf(buf, sizeof(buf), "Espera: %ds", segs_restantes);
    enviar_a_lcd("!PENALIZACION!", buf, segs_restantes);
}

void act_lcd_victoria() {
    enviar_a_lcd("!CAJA ABIERTA!", "");
}

// ============================================================================
// LEDS Y SERVO
// ============================================================================
void act_led_desafio_completado(int num_desafio, bool encender) {
    digitalWrite(PINES_LED_PROG[num_desafio], encender ? HIGH : LOW);
}

void act_led_amarillo_apagar() {
    cmd_led_amarillo_t cmd = { LED_CMD_APAGAR, 0 };
    xQueueSend(cola_led, &cmd, SIN_ESPERA);
}

// Parpadea más rápido cuantos más aplausos hay; fijo al llegar al objetivo.
void act_led_amarillo_progreso(int actual, int total) {
    cmd_led_amarillo_t cmd = { LED_CMD_FIJO, 0 };
    if (actual < total) {
        cmd.modo = LED_CMD_RITMO;
        cmd.semiperiodo_ms = SEMIPERIODO_MAX_MS - (SEMIPERIODO_MAX_MS - SEMIPERIODO_MIN_MS) * actual / total;
    }
    xQueueSend(cola_led, &cmd, SIN_ESPERA);
}

void act_servo_abrir() {
    servo_traba.write(SERVO_ANGULO_ABIERTO);
}

void act_servo_cerrar() {
    servo_traba.write(SERVO_ANGULO_CERRADO);
}

// ============================================================================
// BUZZER
// ============================================================================
void act_buzzer(tipo_sonido_t s) {
    xQueueSend(cola_buzzer, &s, SIN_ESPERA);
}

void act_buzzer_beep()    { act_buzzer(SONIDO_BEEP); }
void act_buzzer_acierto() { act_buzzer(SONIDO_ACIERTO); }
void act_buzzer_error()   { act_buzzer(SONIDO_ERROR); }

// ============================================================================
// TAREAS DE LOS ACTUADORES
// ============================================================================

// Si no hay orden nueva durante un semiperíodo, invierte el LED (modo ritmo).
// Apagado o fijo, espera sin límite la próxima orden.
void tarea_led_ritmo(void *parameter) {
    cmd_led_amarillo_t cmd = { LED_CMD_APAGAR, 0 };
    bool encendido = false;
    while (1) {
        TickType_t espera = (cmd.modo == LED_CMD_RITMO) ? pdMS_TO_TICKS(cmd.semiperiodo_ms) : portMAX_DELAY;
        if (xQueueReceive(cola_led, &cmd, espera) == pdTRUE) {
            encendido = (cmd.modo != LED_CMD_APAGAR);
        } else {
            encendido = !encendido;
        }
        digitalWrite(PIN_LED_AMARILLO, encendido ? HIGH : LOW);
    }
}

// vTaskDelay bloquea sólo esta tarea: la FSM sigue funcionando mientras suena.
void tocar_nota(unsigned int frecuencia, unsigned long duracion) {
    tone(PIN_BUZZER, frecuencia);
    vTaskDelay(pdMS_TO_TICKS(duracion));
    noTone(PIN_BUZZER);
    vTaskDelay(pdMS_TO_TICKS(PAUSA_SONIDO));
}

void tarea_buzzer(void *parameter) {
    tipo_sonido_t sonido;
    while (1) {
        if (xQueueReceive(cola_buzzer, &sonido, portMAX_DELAY) == pdTRUE) {
            switch (sonido) {
                case SONIDO_BEEP:
                    tocar_nota(FREC_BEEP, DUR_BEEP);
                    break;
                case SONIDO_ACIERTO:
                    tocar_nota(FREC_ACIERTO1, DUR_ACIERTO1);
                    tocar_nota(FREC_ACIERTO2, DUR_ACIERTO2);
                    break;
                case SONIDO_ERROR:
                    tocar_nota(FREC_ERROR, DUR_ERROR);
                    break;
            }
        }
    }
}

void imprimir_linea_lcd(uint8_t fila, const char *texto) {
    char linea[LCD_COLUMNAS + 1];
    memset(linea, ' ', LCD_COLUMNAS);
    linea[LCD_COLUMNAS] = '\0';
    memcpy(linea, texto, strnlen(texto, LCD_COLUMNAS));
    lcd.setCursor(0, fila);
    lcd.print(linea);
}

// Con cuenta regresiva espera como máximo 1 s: si no llega nada, descuenta.
void tarea_lcd(void *parameter) {
    msg_lcd_t msg;
    char buf[TAM_BUFFER_TEXTO];
    int segundos = 0;
    while (1) {
        TickType_t espera = (segundos > 0) ? pdMS_TO_TICKS(MS_POR_SEGUNDO) : portMAX_DELAY;
        if (xQueueReceive(cola_lcd, &msg, espera) == pdTRUE) {
            segundos = msg.cuenta_regresiva;
            imprimir_linea_lcd(0, msg.linea1);
            imprimir_linea_lcd(1, msg.linea2);
        } else {
            segundos--;
            snprintf(buf, sizeof(buf), "Espera: %ds", segundos);
            imprimir_linea_lcd(1, buf);
        }
    }
}

#endif
