#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ESP32Servo.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include "rgb_lcd.h"

// ============================================================================
// DEFINICIÓN DE PINES (ESP32 FÍSICO)
// ============================================================================
#define PIN_LCD_SDA          21
#define PIN_LCD_SCL          22
#define LCD_COLUMNAS         16
#define LCD_FILAS             2
#define LCD_BRILLO           80

#define PIN_SERVO            27
#define SERVO_ANGULO_CERRADO           15
#define SERVO_ANGULO_ABIERTO           90
#define SERVO_FRECUENCIA_HZ            50
#define SERVO_MIN_PULSO_US            500
#define SERVO_MAX_PULSO_US           2400

#define PIN_BUZZER           14

#define PIN_POT_MIC          34
#define PIN_POT_OHM          35

#define PIN_LED_PROG1         2
#define PIN_LED_PROG2         4
#define PIN_LED_PROG3         5
#define PIN_LED_AMARILLO     18

#define PIN_BTN_RESET        23

// ============================================================================
// PARÁMETROS Y CONSTANTES
// ============================================================================
#define MAX_VIDAS             3
#define TOTAL_DESAFIOS        3

#define TMP_EVENTOS_MILI     20

#define D1_APLAUSOS_OBJETIVO  3
#define UMBRAL_SONIDO_ALTO 2000
#define UMBRAL_SONIDO_BAJO 1400
#define TIEMPO_VENTANA_D1  5000
#define DEBOUNCE_APLAUSO_MS 200
#define SEMIPERIODO_MAX_MS  300
#define SEMIPERIODO_MIN_MS   60

#define R_OHM_VALOR       100.0
#define I_CORRIENTE_VALOR 0.020
#define V_OBJETIVO        (R_OHM_VALOR * I_CORRIENTE_VALOR)
#define TOLERANCIA_VOLTAJE 0.15
#define TIEMPO_INACTIVIDAD_D2 3000
#define UMBRAL_RUIDO_ADC     40

#define TIEMPO_MOSTRAR_RESULTADO 1500
#define TIEMPO_PENALIZACION     60000
#define TIEMPO_RESETEANDO        3000
#define DEBOUNCE_BTN_MS            50

// ============================================================================
// PARÁMETROS WIFI Y MQTT (ESP32 FÍSICO)
// ============================================================================
#define WIFI_SSID            "TU_RED_WIFI"
#define WIFI_PASS            "TU_PASSWORD"

#define TOPICO_SEL_DESAFIO   "soa/escape/seleccion"
#define TOPICO_D3_CONFIRMAR  "soa/escape/d3"
#define TOPICO_TX_POT        "soa/escape/pot"
#define TOPICO_TX_DESAFIO    "soa/escape/desafio"
#define TOPICO_TX_ESTADO     "soa/escape/estado"
#define TOPICO_TX_VIDAS      "soa/escape/vidas"
#define MQTT_BROKER          "broker.hivemq.com" // O la IP local de tu PC: "192.168.1.50"
#define MQTT_PORT            1883

// ============================================================================
// CONFIGURACIÓN DE TAREAS FREERTOS
// ============================================================================
#define TAM_PILA_LED         2048
#define PRIORIDAD_LED           1

#define TAM_PILA_BUZZER      2048
#define PRIORIDAD_BUZZER        2

#define TAM_PILA_LCD         2560
#define PRIORIDAD_LCD           1

#define TAM_PILA_COM         2560
#define PRIORIDAD_COM           2

#define TAM_PILA_MQTT        4096
#define PRIORIDAD_MQTT          2

// ============================================================================
// TIPOS DE DATOS: ESTADOS Y EVENTOS (FSM PURA)
// ============================================================================
typedef enum {
    ESTADO_ESPERANDO_DESAFIO,
    ESTADO_D1_ESPERANDO_APLAUSO,
    ESTADO_D2_ESPERANDO_CAMBIO_POT,
    ESTADO_D3_ESPERANDO_OK,
    ESTADO_DESAFIO_FINALIZADO,
    ESTADO_PENALIZACION_BLOQUEADO,
    ESTADO_CAJA_DESBLOQUEADA,
    ESTADO_RESETEANDO
} estado_fsm_t;

typedef enum {
    EVENTO_CONTINUE = 0,
    EVENTO_TIMEOUT,
    EVENTO_TICK_1S,
    EVENTO_BTN_RESET,
    EVENTO_CMD_SEL_D1,
    EVENTO_CMD_SEL_D2,
    EVENTO_CMD_SEL_D3,
    EVENTO_CMD_D3_OK,
    EVENTO_APLAUSO_DETECTADO,
    EVENTO_POT_CAMBIO,
    EVENTO_POT_ESTABLE
} tipo_evento_t;

typedef struct {
    tipo_evento_t tipo;
    int valor_aux;
    char str_aux[16];
} evento_t;

// ============================================================================
// TEMPORIZADORES GENÉRICOS (TOTALMENTE DESACOPLADOS DEL ESTADO)
// ============================================================================
bool timer_timeout_activo = false;
unsigned long t_inicio_timeout = 0;
unsigned long t_duracion_timeout = 0;

bool timer_tick_activo = false;
unsigned long t_ultimo_tick = 0;
int segs_restantes = 0;

void iniciar_timeout(unsigned long duracion_ms) {
    t_inicio_timeout = millis();
    t_duracion_timeout = duracion_ms;
    timer_timeout_activo = true;
}

void detener_timeout() {
    timer_timeout_activo = false;
}

void iniciar_tick_segundos(int segundos) {
    t_ultimo_tick = millis();
    segs_restantes = segundos;
    timer_tick_activo = true;
}

void detener_tick_segundos() {
    timer_tick_activo = false;
}

// ============================================================================
// VARIABLES GLOBALES
// ============================================================================
estado_fsm_t estado_actual;
evento_t evento;

int vidas = MAX_VIDAS;
bool desafio_resuelto[TOTAL_DESAFIOS + 1];
int total_resueltos = 0;

unsigned long t_ultimo_escaneo_sensores = 0;

int aplausos_contados = 0;
unsigned long t_ultimo_aplauso = 0;
bool mic_armado = true;

int adc_pot_ultimo = 0;
unsigned long t_ultimo_mov_pot = 0;
bool pot_empezo_a_mover = false;
float voltaje_leido = 0.0;
float voltaje_mostrado_anterior = -1.0;

int btn_reset_anterior = HIGH;
unsigned long t_ultimo_cambio_btn = 0;

rgb_lcd lcd;
Servo servo_traba;
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

// ============================================================================
// COLAS Y ESTRUCTURAS DE COMUNICACIÓN FREERTOS
// ============================================================================
typedef enum {
    SONIDO_BEEP,
    SONIDO_ACIERTO,
    SONIDO_ERROR
} tipo_sonido_t;

typedef struct {
    char linea1[17];
    char linea2[17];
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
    char topico[32];
    char payload[16];
} msg_mqtt_tx_t;

QueueHandle_t cola_buzzer = NULL;
QueueHandle_t cola_lcd = NULL;
QueueHandle_t cola_led = NULL;
QueueHandle_t cola_eventos_com = NULL;
QueueHandle_t cola_mqtt_tx = NULL;

// ============================================================================
// PROTOTIPOS DE FUNCIONES
// ============================================================================
void start();
void fsm();
void tomar_evento();

bool verificar_timers(unsigned long ahora);
bool verificar_boton_reset(unsigned long ahora);
bool verificar_eventos_comunicacion();
void muestrear_microfono(unsigned long ahora);
void muestrear_potenciometro(unsigned long ahora);

void act_lcd_menu_principal();
void act_lcd_reseteando();
void act_lcd_d1_intro();
void act_lcd_d2_intro();
void act_lcd_d3_intro();
void act_lcd_resultado(bool exito, const char* msg);
void act_lcd_penalizacion(int segs_restantes);
void act_lcd_victoria();

void act_led_desafio_completado(int num_desafio, bool encender);
void act_led_amarillo_apagar();
void act_led_amarillo_progreso(int actual, int total);

void act_servo_abrir();
void act_servo_cerrar();

void act_buzzer_beep();
void act_buzzer_acierto();
void act_buzzer_error();

void tarea_led_ritmo(void *parameter);
void tarea_buzzer(void *parameter);
void tarea_lcd(void *parameter);
void imprimir_linea_lcd(uint8_t fila, const char* texto);
void tarea_comunicacion(void *parameter);
void tarea_mqtt(void *parameter);
void reconectar_mqtt();
void callback_mqtt(char* topic, byte* payload, unsigned int length);
void enviar_a_lcd(const char* l1, const char* l2);
void enviar_a_mqtt(const char* topico, const char* payload);
void enviar_vidas_mqtt();

void registrar_acierto_desafio(int num);
void registrar_fallo_vida();
void iniciar_penalizacion();
void restaurar_vidas();
void reiniciar_juego();

// ============================================================================
// SETUP Y LOOP
// ============================================================================
void setup() {
    start();
}

void loop() {
    fsm();
    delay(10);
}

// ============================================================================
// INICIALIZACIÓN
// ============================================================================
void start() {
    Serial.begin(115200);
    Serial.setTimeout(50);
    Wire.begin(PIN_LCD_SDA, PIN_LCD_SCL);

    Serial.println("\n===================================");
    Serial.println("  INICIANDO SISTEMA ESCAPE ROOM  ");
    Serial.println("====================================");

    pinMode(PIN_POT_MIC, INPUT);
    pinMode(PIN_POT_OHM, INPUT);
    adc_pot_ultimo = analogRead(PIN_POT_OHM);

    pinMode(PIN_LED_PROG1, OUTPUT);
    pinMode(PIN_LED_PROG2, OUTPUT);
    pinMode(PIN_LED_PROG3, OUTPUT);
    pinMode(PIN_LED_AMARILLO, OUTPUT);

    pinMode(PIN_BTN_RESET, INPUT_PULLUP);
    btn_reset_anterior = digitalRead(PIN_BTN_RESET);

    if (PIN_BUZZER < 34) {
        pinMode(PIN_BUZZER, OUTPUT);
    }

    act_led_desafio_completado(1, false);
    act_led_desafio_completado(2, false);
    act_led_desafio_completado(3, false);

    servo_traba.setPeriodHertz(SERVO_FRECUENCIA_HZ);
    servo_traba.attach(PIN_SERVO, SERVO_MIN_PULSO_US, SERVO_MAX_PULSO_US);
    act_servo_cerrar();

    cola_buzzer = xQueueCreate(5, sizeof(tipo_sonido_t));
    cola_lcd = xQueueCreate(8, sizeof(msg_lcd_t));
    cola_led = xQueueCreate(4, sizeof(cmd_led_amarillo_t));
    cola_eventos_com = xQueueCreate(5, sizeof(evento_t));
    cola_mqtt_tx = xQueueCreate(10, sizeof(msg_mqtt_tx_t));

    lcd.begin(LCD_COLUMNAS, LCD_FILAS);
    lcd.setRGB(LCD_BRILLO, LCD_BRILLO, LCD_BRILLO);
    lcd.clear();

    xTaskCreate(tarea_led_ritmo, "Task_LED", TAM_PILA_LED, NULL, PRIORIDAD_LED, NULL);
    xTaskCreate(tarea_buzzer, "Task_Buzzer", TAM_PILA_BUZZER, NULL, PRIORIDAD_BUZZER, NULL);
    xTaskCreate(tarea_lcd, "Task_LCD", TAM_PILA_LCD, NULL, PRIORIDAD_LCD, NULL);
    xTaskCreate(tarea_comunicacion, "Task_Com", TAM_PILA_COM, NULL, PRIORIDAD_COM, NULL);
    xTaskCreate(tarea_mqtt, "Task_MQTT", TAM_PILA_MQTT, NULL, PRIORIDAD_MQTT, NULL);

    act_led_amarillo_apagar();

    vidas = MAX_VIDAS;
    total_resueltos = 0;
    for (int i = 1; i <= TOTAL_DESAFIOS; i++) {
        desafio_resuelto[i] = false;
    }

    estado_actual = ESTADO_ESPERANDO_DESAFIO;
    evento.tipo = EVENTO_CONTINUE;

    act_lcd_menu_principal();
    act_buzzer_beep();
    enviar_vidas_mqtt();
}

// ============================================================================
// SUBFUNCIONES DE CAPTURA DE EVENTOS (PURAS, SIN CONOCIMIENTO DE ESTADO)
// ============================================================================
bool verificar_timers(unsigned long ahora) {
    if (timer_timeout_activo && (ahora - t_inicio_timeout >= t_duracion_timeout)) {
        timer_timeout_activo = false;
        evento.tipo = EVENTO_TIMEOUT;
        return true;
    }

    if (timer_tick_activo && (ahora - t_ultimo_tick >= 1000)) {
        t_ultimo_tick = ahora;
        if (segs_restantes > 0) segs_restantes--;
        evento.tipo = EVENTO_TICK_1S;
        evento.valor_aux = segs_restantes;
        return true;
    }

    return false;
}

bool verificar_boton_reset(unsigned long ahora) {
    int btn_lectura = digitalRead(PIN_BTN_RESET);
    if (btn_lectura != btn_reset_anterior) {
        if (ahora - t_ultimo_cambio_btn > DEBOUNCE_BTN_MS) {
            t_ultimo_cambio_btn = ahora;
            btn_reset_anterior = btn_lectura;
            if (btn_lectura == LOW) {
                evento.tipo = EVENTO_BTN_RESET;
                return true;
            }
        }
    }
    return false;
}

bool verificar_eventos_comunicacion() {
    evento_t ev_com;
    if (xQueueReceive(cola_eventos_com, &ev_com, 0) == pdTRUE) {
        evento = ev_com;
        return true;
    }
    return false;
}

void muestrear_microfono(unsigned long ahora) {
    int adc_mic = analogRead(PIN_POT_MIC);

    if (mic_armado && adc_mic > UMBRAL_SONIDO_ALTO) {
        if (ahora - t_ultimo_aplauso > DEBOUNCE_APLAUSO_MS) {
            mic_armado = false;
            t_ultimo_aplauso = ahora;
            evento.tipo = EVENTO_APLAUSO_DETECTADO;
            return;
        }
    }

    if (!mic_armado && adc_mic < UMBRAL_SONIDO_BAJO) {
        mic_armado = true;
    }
}

void muestrear_potenciometro(unsigned long ahora) {
    int adc_pot = analogRead(PIN_POT_OHM);

    if (abs(adc_pot - adc_pot_ultimo) > UMBRAL_RUIDO_ADC) {
        adc_pot_ultimo = adc_pot;
        t_ultimo_mov_pot = ahora;
        pot_empezo_a_mover = true;
        voltaje_leido = (adc_pot * 3.3f) / 4095.0f;
        evento.tipo = EVENTO_POT_CAMBIO;
        return;
    }

    if (pot_empezo_a_mover && (ahora - t_ultimo_mov_pot >= TIEMPO_INACTIVIDAD_D2)) {
        pot_empezo_a_mover = false;
        voltaje_leido = (adc_pot_ultimo * 3.3f) / 4095.0f;
        evento.tipo = EVENTO_POT_ESTABLE;
        return;
    }
}

// ============================================================================
// CAPTURA DE EVENTOS (tomar_evento)
// ============================================================================
void tomar_evento() {
    evento.tipo = EVENTO_CONTINUE;
    evento.valor_aux = 0;
    evento.str_aux[0] = '\0';
    unsigned long ahora = millis();

    if (verificar_timers(ahora)) return;
    if (verificar_boton_reset(ahora)) return;
    if (verificar_eventos_comunicacion()) return;

    if (ahora - t_ultimo_escaneo_sensores >= TMP_EVENTOS_MILI) {
        t_ultimo_escaneo_sensores = ahora;
        muestrear_microfono(ahora);
        if (evento.tipo != EVENTO_CONTINUE) return;
        muestrear_potenciometro(ahora);
        if (evento.tipo != EVENTO_CONTINUE) return;
    }
}

// ============================================================================
// MÁQUINA DE ESTADOS FINITOS (fsm: ÚNICO LUGAR DE DECISIÓN Y LÓGICA DE JUEGO)
// ============================================================================
void fsm() {
    tomar_evento();

    switch (estado_actual) {
        case ESTADO_ESPERANDO_DESAFIO:
            switch (evento.tipo) {
                case EVENTO_CMD_SEL_D1:
                    if (!desafio_resuelto[1]) {
                        aplausos_contados = 0;
                        act_led_amarillo_apagar();
                        act_lcd_d1_intro();
                        estado_actual = ESTADO_D1_ESPERANDO_APLAUSO;
                    }
                    break;

                case EVENTO_CMD_SEL_D2:
                    if (!desafio_resuelto[2]) {
                        pot_empezo_a_mover = false;
                        t_ultimo_mov_pot = millis();
                        voltaje_mostrado_anterior = -1.0;
                        act_lcd_d2_intro();
                        estado_actual = ESTADO_D2_ESPERANDO_CAMBIO_POT;
                    }
                    break;

                case EVENTO_CMD_SEL_D3:
                    if (!desafio_resuelto[3]) {
                        act_lcd_d3_intro();
                        estado_actual = ESTADO_D3_ESPERANDO_OK;
                    }
                    break;

                case EVENTO_BTN_RESET:
                    act_lcd_reseteando();
                    act_buzzer_beep();
                    iniciar_timeout(TIEMPO_RESETEANDO);
                    estado_actual = ESTADO_RESETEANDO;
                    break;

                default:
                    break;
            }
            break;

        case ESTADO_D1_ESPERANDO_APLAUSO:
            switch (evento.tipo) {
                case EVENTO_APLAUSO_DETECTADO:
                    if (aplausos_contados == 0) {
                        iniciar_timeout(TIEMPO_VENTANA_D1);
                    }
                    aplausos_contados++;
                    act_buzzer_beep();
                    act_led_amarillo_progreso(aplausos_contados, D1_APLAUSOS_OBJETIVO);

                    if (aplausos_contados >= D1_APLAUSOS_OBJETIVO) {
                        detener_timeout();
                        enviar_a_mqtt(TOPICO_TX_DESAFIO, "1,OK");
                        registrar_acierto_desafio(1);
                        act_lcd_resultado(true, "RITMO LOGRADO");
                        act_buzzer_acierto();
                        iniciar_timeout(TIEMPO_MOSTRAR_RESULTADO);
                        estado_actual = ESTADO_DESAFIO_FINALIZADO;
                    }
                    break;

                case EVENTO_TIMEOUT:
                    act_led_amarillo_apagar();
                    enviar_a_mqtt(TOPICO_TX_DESAFIO, "1,ERROR");
                    registrar_fallo_vida();
                    act_lcd_resultado(false, "TIEMPO AGOTADO");
                    act_buzzer_error();
                    iniciar_timeout(TIEMPO_MOSTRAR_RESULTADO);
                    estado_actual = ESTADO_DESAFIO_FINALIZADO;
                    break;

                default:
                    break;
            }
            break;

        case ESTADO_D2_ESPERANDO_CAMBIO_POT:
            switch (evento.tipo) {
                case EVENTO_POT_CAMBIO: {
                    char buf_pot[8];
                    snprintf(buf_pot, sizeof(buf_pot), "%.2f", voltaje_leido);
                    enviar_a_mqtt(TOPICO_TX_POT, buf_pot);
                    break;
                }

                case EVENTO_POT_ESTABLE:
                    if (abs(voltaje_leido - V_OBJETIVO) <= TOLERANCIA_VOLTAJE) {
                        enviar_a_mqtt(TOPICO_TX_DESAFIO, "2,OK");
                        registrar_acierto_desafio(2);
                        act_lcd_resultado(true, "VALOR EXACTO");
                        act_buzzer_acierto();
                    } else {
                        enviar_a_mqtt(TOPICO_TX_DESAFIO, "2,ERROR");
                        registrar_fallo_vida();
                        act_lcd_resultado(false, "VALOR ERRADO");
                        act_buzzer_error();
                    }
                    iniciar_timeout(TIEMPO_MOSTRAR_RESULTADO);
                    estado_actual = ESTADO_DESAFIO_FINALIZADO;
                    break;

                default:
                    break;
            }
            break;

        case ESTADO_D3_ESPERANDO_OK:
            switch (evento.tipo) {
                case EVENTO_CMD_D3_OK:
                    enviar_a_mqtt(TOPICO_TX_DESAFIO, "3,OK");
                    registrar_acierto_desafio(3);
                    act_lcd_resultado(true, "JUEGO SUPERADO");
                    act_buzzer_acierto();
                    iniciar_timeout(TIEMPO_MOSTRAR_RESULTADO);
                    estado_actual = ESTADO_DESAFIO_FINALIZADO;
                    break;

                default:
                    break;
            }
            break;

        case ESTADO_DESAFIO_FINALIZADO:
            switch (evento.tipo) {
                case EVENTO_TIMEOUT:
                    act_led_amarillo_apagar();
                    if (total_resueltos >= TOTAL_DESAFIOS) {
                        estado_actual = ESTADO_CAJA_DESBLOQUEADA;
                        act_lcd_victoria();
                        act_servo_abrir();
                        act_buzzer_acierto();
                        enviar_a_mqtt(TOPICO_TX_ESTADO, "VICTORIA");
                    } else if (vidas == 0) {
                        iniciar_penalizacion();
                        enviar_a_mqtt(TOPICO_TX_ESTADO, "BLOQUEADO");
                        estado_actual = ESTADO_PENALIZACION_BLOQUEADO;
                    } else {
                        estado_actual = ESTADO_ESPERANDO_DESAFIO;
                        act_lcd_menu_principal();
                        act_buzzer_beep();
                        enviar_vidas_mqtt();
                    }
                    break;

                default:
                    break;
            }
            break;

        case ESTADO_PENALIZACION_BLOQUEADO:
            switch (evento.tipo) {
                case EVENTO_TICK_1S:
                    act_lcd_penalizacion(evento.valor_aux);
                    break;

                case EVENTO_TIMEOUT:
                    detener_tick_segundos();
                    restaurar_vidas();
                    estado_actual = ESTADO_ESPERANDO_DESAFIO;
                    act_lcd_menu_principal();
                    act_buzzer_beep();
                    enviar_vidas_mqtt();
                    break;

                default:
                    break;
            }
            break;

        case ESTADO_CAJA_DESBLOQUEADA:
            switch (evento.tipo) {
                case EVENTO_BTN_RESET:
                    act_lcd_reseteando();
                    act_buzzer_beep();
                    iniciar_timeout(TIEMPO_RESETEANDO);
                    estado_actual = ESTADO_RESETEANDO;
                    break;

                default:
                    break;
            }
            break;

        case ESTADO_RESETEANDO:
            switch (evento.tipo) {
                case EVENTO_TIMEOUT:
                    reiniciar_juego();
                    estado_actual = ESTADO_ESPERANDO_DESAFIO;
                    act_lcd_menu_principal();
                    act_buzzer_beep();
                    enviar_vidas_mqtt();
                    break;
                    act_buzzer_beep();
                    break;

                default:
                    break;
            }
            break;
    }
}

// ============================================================================
// FUNCIONES DE CONTROL DE JUEGO
// ============================================================================
void registrar_acierto_desafio(int num) {
    if (num >= 1 && num <= TOTAL_DESAFIOS) {
        if (!desafio_resuelto[num]) {
            desafio_resuelto[num] = true;
            total_resueltos++;
            act_led_desafio_completado(num, true);
        }
    }
}

void registrar_fallo_vida() {
    if (vidas > 0) vidas--;
}

void iniciar_penalizacion() {
    iniciar_timeout(TIEMPO_PENALIZACION);
    iniciar_tick_segundos(TIEMPO_PENALIZACION / 1000);
    act_lcd_penalizacion(TIEMPO_PENALIZACION / 1000);
}

void restaurar_vidas() {
    vidas = MAX_VIDAS;
    act_buzzer_beep();
}

void reiniciar_juego() {
    vidas = MAX_VIDAS;
    total_resueltos = 0;
    for (int i = 1; i <= TOTAL_DESAFIOS; i++) {
        desafio_resuelto[i] = false;
        act_led_desafio_completado(i, false);
    }

    act_servo_cerrar();
    act_led_amarillo_apagar();
    detener_timeout();
    detener_tick_segundos();

    aplausos_contados = 0;
    t_ultimo_aplauso = 0;
    mic_armado = true;

    adc_pot_ultimo = analogRead(PIN_POT_OHM);
    t_ultimo_mov_pot = 0;
    pot_empezo_a_mover = false;
    voltaje_leido = 0.0;
    voltaje_mostrado_anterior = -1.0;

    btn_reset_anterior = digitalRead(PIN_BTN_RESET);
}

// ============================================================================
// ACTUADORES
// ============================================================================
void enviar_a_lcd(const char* l1, const char* l2) {
    if (cola_lcd != NULL) {
        msg_lcd_t msg;
        memset(&msg, 0, sizeof(msg));
        if (l1) strncpy(msg.linea1, l1, 16);
        if (l2) strncpy(msg.linea2, l2, 16);
        xQueueSend(cola_lcd, &msg, 0);
    }
}

void act_lcd_menu_principal() {
    enviar_a_lcd("ESCAPE ROOM SOA", "Esperando App...");
}

void act_lcd_reseteando() {
    enviar_a_lcd("SISTEMA ESCAPE", "Reseteando...");
}

void act_lcd_d1_intro() {
    enviar_a_lcd("La clave no se", "lee, se escucha.");
}

void act_lcd_d2_intro() {
    enviar_a_lcd("D2:R=100 I=20mA", "V=...");
}

void act_lcd_d3_intro() {
    enviar_a_lcd("D3: App Android", "Esperando OK...");
}

void act_lcd_resultado(bool exito, const char* msg) {
    enviar_a_lcd(exito ? "** ACIERTO **" : "* ERROR/TIMEOUT *", msg);
}

void act_lcd_penalizacion(int segs_restantes) {
    char buf[17];
    snprintf(buf, sizeof(buf), "Espera: %ds", segs_restantes);
    enviar_a_lcd("!PENALIZACION!", buf);
}

void act_lcd_victoria() {
    enviar_a_lcd("!CAJA ABIERTA!", "");
}

void act_led_desafio_completado(int num_desafio, bool encender) {
    int pin = (num_desafio == 1) ? PIN_LED_PROG1 :
              (num_desafio == 2) ? PIN_LED_PROG2 : PIN_LED_PROG3;
    digitalWrite(pin, encender ? HIGH : LOW);
}

void act_led_amarillo_apagar() {
    cmd_led_amarillo_t cmd = { LED_CMD_APAGAR, 0 };
    if (cola_led) xQueueSend(cola_led, &cmd, 0);
}

void act_led_amarillo_progreso(int actual, int total) {
    cmd_led_amarillo_t cmd;
    if (actual <= 0) {
        cmd.modo = LED_CMD_APAGAR;
        cmd.semiperiodo_ms = 0;
    } else if (actual >= total) {
        cmd.modo = LED_CMD_FIJO;
        cmd.semiperiodo_ms = 0;
    } else {
        cmd.modo = LED_CMD_RITMO;
        float ratio = (float)actual / (float)total;
        cmd.semiperiodo_ms = (unsigned long)(SEMIPERIODO_MAX_MS - ratio * (SEMIPERIODO_MAX_MS - SEMIPERIODO_MIN_MS));
    }
    if (cola_led) xQueueSend(cola_led, &cmd, 0);
}

void act_servo_abrir() {
    servo_traba.write(SERVO_ANGULO_ABIERTO);
}

void act_servo_cerrar() {
    servo_traba.write(SERVO_ANGULO_CERRADO);
}

void act_buzzer_beep() {
    if (cola_buzzer != NULL) {
        tipo_sonido_t s = SONIDO_BEEP;
        xQueueSend(cola_buzzer, &s, 0);
    }
}

void act_buzzer_acierto() {
    if (cola_buzzer != NULL) {
        tipo_sonido_t s = SONIDO_ACIERTO;
        xQueueSend(cola_buzzer, &s, 0);
    }
}

void act_buzzer_error() {
    if (cola_buzzer != NULL) {
        tipo_sonido_t s = SONIDO_ERROR;
        xQueueSend(cola_buzzer, &s, 0);
    }
}

// ============================================================================
// TAREAS FREERTOS
// ============================================================================
void tarea_led_ritmo(void *parameter) {
    cmd_led_amarillo_t cmd = { LED_CMD_APAGAR, 0 };
    cmd_led_amarillo_t nuevo_cmd;

    while (1) {
        if (xQueueReceive(cola_led, &nuevo_cmd, 0) == pdTRUE) {
            cmd = nuevo_cmd;
        }

        switch (cmd.modo) {
            case LED_CMD_APAGAR:
                digitalWrite(PIN_LED_AMARILLO, LOW);
                xQueueReceive(cola_led, &cmd, portMAX_DELAY);
                break;

            case LED_CMD_FIJO:
                digitalWrite(PIN_LED_AMARILLO, HIGH);
                xQueueReceive(cola_led, &cmd, portMAX_DELAY);
                break;

            case LED_CMD_RITMO:
                if (cmd.semiperiodo_ms > 0) {
                    digitalWrite(PIN_LED_AMARILLO, HIGH);
                    vTaskDelay(pdMS_TO_TICKS(cmd.semiperiodo_ms));
                    if (xQueueReceive(cola_led, &nuevo_cmd, 0) == pdTRUE) {
                        cmd = nuevo_cmd;
                        break;
                    }
                    digitalWrite(PIN_LED_AMARILLO, LOW);
                    vTaskDelay(pdMS_TO_TICKS(cmd.semiperiodo_ms));
                } else {
                    digitalWrite(PIN_LED_AMARILLO, LOW);
                    vTaskDelay(pdMS_TO_TICKS(50));
                }
                break;
        }
    }
}

void tarea_buzzer(void *parameter) {
    tipo_sonido_t sonido;
    while (1) {
        if (xQueueReceive(cola_buzzer, &sonido, portMAX_DELAY) == pdTRUE) {
            switch (sonido) {
                case SONIDO_BEEP:
                    tone(PIN_BUZZER, 2000, 100);
                    vTaskDelay(pdMS_TO_TICKS(120));
                    noTone(PIN_BUZZER);
                    break;

                case SONIDO_ACIERTO:
                    tone(PIN_BUZZER, 1500, 150);
                    vTaskDelay(pdMS_TO_TICKS(180));
                    tone(PIN_BUZZER, 2500, 300);
                    vTaskDelay(pdMS_TO_TICKS(350));
                    noTone(PIN_BUZZER);
                    break;

                case SONIDO_ERROR:
                    tone(PIN_BUZZER, 400, 400);
                    vTaskDelay(pdMS_TO_TICKS(450));
                    noTone(PIN_BUZZER);
                    break;
            }
        }
    }
}

void imprimir_linea_lcd(uint8_t fila, const char* texto) {
    if (fila >= LCD_FILAS) return;
    char linea[LCD_COLUMNAS + 1];
    memset(linea, ' ', LCD_COLUMNAS);
    linea[LCD_COLUMNAS] = '\0';

    if (texto != nullptr) {
        size_t len = strlen(texto);
        if (len > LCD_COLUMNAS) len = LCD_COLUMNAS;
        memcpy(linea, texto, len);
    }
    lcd.setCursor(0, fila);
    lcd.print(linea);
}

void tarea_lcd(void *parameter) {
    msg_lcd_t msg;
    while (1) {
        if (xQueueReceive(cola_lcd, &msg, portMAX_DELAY) == pdTRUE) {
            imprimir_linea_lcd(0, msg.linea1);
            imprimir_linea_lcd(1, msg.linea2);
        }
    }
}

void tarea_comunicacion(void *parameter) {
    while (1) {
        if (Serial.available()) {
            String linea = Serial.readStringUntil('\n');
            linea.trim();
            if (linea.length() > 0) {
                if (linea.endsWith("#")) {
                    linea.remove(linea.length() - 1);
                    linea.trim();
                }

                evento_t ev;
                ev.valor_aux = 0;
                memset(ev.str_aux, 0, sizeof(ev.str_aux));

                if (linea == "1") {
                    ev.tipo = EVENTO_CMD_SEL_D1;
                    xQueueSend(cola_eventos_com, &ev, 0);
                } else if (linea == "2") {
                    ev.tipo = EVENTO_CMD_SEL_D2;
                    xQueueSend(cola_eventos_com, &ev, 0);
                } else if (linea == "3") {
                    ev.tipo = EVENTO_CMD_SEL_D3;
                    xQueueSend(cola_eventos_com, &ev, 0);
                } else if (linea.equalsIgnoreCase("OK")) {
                    ev.tipo = EVENTO_CMD_D3_OK;
                    xQueueSend(cola_eventos_com, &ev, 0);
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void callback_mqtt(char* topic, byte* payload, unsigned int length) {
    char msg[8];
    memcpy(msg, payload, length);
    msg[length] = '\0';

    evento_t ev;

    if (strcmp(topic, TOPICO_SEL_DESAFIO) == 0) {
        if (strcmp(msg, "1") == 0)      ev.tipo = EVENTO_CMD_SEL_D1;
        else if (strcmp(msg, "2") == 0) ev.tipo = EVENTO_CMD_SEL_D2;
        else if (strcmp(msg, "3") == 0) ev.tipo = EVENTO_CMD_SEL_D3;
        else return;
        xQueueSend(cola_eventos_com, &ev, 0);
    } 
    else if (strcmp(topic, TOPICO_D3_CONFIRMAR) == 0) {
        if (strcasecmp(msg, "OK") == 0) {
            ev.tipo = EVENTO_CMD_D3_OK;
            xQueueSend(cola_eventos_com, &ev, 0);
        }
    }
}

void enviar_a_mqtt(const char* topico, const char* payload) {
    if (cola_mqtt_tx != NULL && topico != NULL && payload != NULL) {
        msg_mqtt_tx_t msg;
        strncpy(msg.topico, topico, sizeof(msg.topico) - 1);
        msg.topico[sizeof(msg.topico) - 1] = '\0';
        strncpy(msg.payload, payload, sizeof(msg.payload) - 1);
        msg.payload[sizeof(msg.payload) - 1] = '\0';
        xQueueSend(cola_mqtt_tx, &msg, 0);
    }
}

void enviar_vidas_mqtt() {
    char buf_v[4];
    snprintf(buf_v, sizeof(buf_v), "%d", vidas);
    enviar_a_mqtt(TOPICO_TX_VIDAS, buf_v);
}

void reconectar_mqtt() {
    String cid = "ESP32_SOA_" + String((uint32_t)ESP.getEfuseMac(), HEX);
    if (mqttClient.connect(cid.c_str())) {
        mqttClient.subscribe(TOPICO_SEL_DESAFIO);
        mqttClient.subscribe(TOPICO_D3_CONFIRMAR);
    }
}

void tarea_mqtt(void *parameter) {
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    while (WiFi.status() != WL_CONNECTED) {
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
    mqttClient.setCallback(callback_mqtt);

    while (1) {
        if (!mqttClient.connected()) {
            reconectar_mqtt();
            vTaskDelay(pdMS_TO_TICKS(2000));
        } else {
            msg_mqtt_tx_t tx_msg;
            while (xQueueReceive(cola_mqtt_tx, &tx_msg, 0) == pdTRUE) {
                mqttClient.publish(tx_msg.topico, tx_msg.payload);
            }
            mqttClient.loop();
            vTaskDelay(pdMS_TO_TICKS(20));
        }
    }
}
