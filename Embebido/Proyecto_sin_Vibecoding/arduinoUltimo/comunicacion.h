// ============================================================================
// comunicacion.h — Monitor serie (simula la app en Wokwi) y MQTT con la app.
// Lo que llega NO ejecuta acciones: se traduce a un evento y se encola para
// la FSM. Lo que la FSM quiere publicar lo deja en cola_mqtt_tx y lo publica
// tarea_mqtt, la única que usa el cliente MQTT.
//
// Comandos (por Serial o MQTT):
//   soa/escape/seleccion  "1" | "2" | "3"   -> EVENTO_DESAFIO_1/2/3
//   soa/escape/d3         "OK" | "ERROR"    -> EVENTO_D3_CODIGO_OK / _ERROR
// Publica: soa/escape/desafio ("1,OK"), vidas, pot ("1.85"), estado.
// ============================================================================
#ifndef COMUNICACION_H
#define COMUNICACION_H

#include <WiFi.h>
#include <PubSubClient.h>
#include "tipos.h"
#include "secrets.h"

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

// ============================================================================
// RECEPCIÓN: texto -> evento
// ============================================================================

// "1", "2" o "3": elegir desafío. Devuelve true si era uno de ésos.
bool encolar_seleccion(const String &texto) {
    if (texto == "1")      encolar_evento(EVENTO_DESAFIO_1, 0);
    else if (texto == "2") encolar_evento(EVENTO_DESAFIO_2, 0);
    else if (texto == "3") encolar_evento(EVENTO_DESAFIO_3, 0);
    else return false;
    return true;
}

// "OK" o "ERROR": resultado del código del desafío 3 (lo valida la app).
bool encolar_codigo_d3(const String &texto) {
    if (texto.equalsIgnoreCase("OK"))         encolar_evento(EVENTO_D3_CODIGO_OK, 0);
    else if (texto.equalsIgnoreCase("ERROR")) encolar_evento(EVENTO_D3_CODIGO_ERROR, 0);
    else return false;
    return true;
}

// Saca espacios y el '#' opcional del final.
void limpiar_comando(String &linea) {
    linea.trim();
    if (linea.endsWith("#")) {
        linea.remove(linea.length() - 1);
        linea.trim();
    }
}

void tarea_comunicacion(void *parameter) {
    while (1) {
        if (Serial.available()) {
            String linea = Serial.readStringUntil('\n');
            limpiar_comando(linea);
            if (linea.length() > 0 && !encolar_seleccion(linea) && !encolar_codigo_d3(linea)) {
                LOG_DEBUG("[SERIAL] Comando desconocido: %s\n", linea.c_str());
            }
        }
        vTaskDelay(pdMS_TO_TICKS(PERIODO_SERIAL));
    }
}

// Lo llama mqttClient.loop() (dentro de tarea_mqtt). Copia el mensaje con
// su largo: el original copiaba en char[8] sin mirar el largo y un mensaje
// largo escribía fuera del arreglo.
void callback_mqtt(char *topic, byte *payload, unsigned int length) {
    String msg;
    for (unsigned int i = 0; i < length; i++) {
        msg += (char)payload[i];
    }
    limpiar_comando(msg);

    if (strcmp(topic, TOPICO_SEL_DESAFIO) == 0) {
        encolar_seleccion(msg);
    } else if (strcmp(topic, TOPICO_D3_CODIGO) == 0) {
        encolar_codigo_d3(msg);
    }
}

// ============================================================================
// ENVÍO (la FSM deja el mensaje en la cola, no espera)
// ============================================================================
void enviar_a_mqtt(const char *topico, const char *payload) {
    msg_mqtt_tx_t msg;
    strncpy(msg.topico, topico, sizeof(msg.topico) - 1);
    msg.topico[sizeof(msg.topico) - 1] = '\0';
    strncpy(msg.payload, payload, sizeof(msg.payload) - 1);
    msg.payload[sizeof(msg.payload) - 1] = '\0';
    xQueueSend(cola_mqtt_tx, &msg, SIN_ESPERA);
}

void enviar_vidas_mqtt(int vidas) {
    char buf_v[TAM_BUFFER_TEXTO];
    snprintf(buf_v, sizeof(buf_v), "%d", vidas);
    enviar_a_mqtt(TOPICO_TX_VIDAS, buf_v);
}

void enviar_resultado_mqtt(int num_desafio, bool exito) {
    char buf[TAM_BUFFER_TEXTO];
    snprintf(buf, sizeof(buf), "%d,%s", num_desafio, exito ? "OK" : "ERROR");
    enviar_a_mqtt(TOPICO_TX_DESAFIO, buf);
}

void enviar_tension_mqtt(int milivolts) {
    char buf[TAM_BUFFER_TEXTO];
    snprintf(buf, sizeof(buf), "%.2f", milivolts / MILIVOLTS_POR_VOLT);
    enviar_a_mqtt(TOPICO_TX_POT, buf);
}

// ============================================================================
// TAREA MQTT
// ============================================================================
void reconectar_mqtt() {
    String cid = "ESP32_SOA_" + String((uint32_t)ESP.getEfuseMac(), HEX);
    if (mqttClient.connect(cid.c_str())) {
        mqttClient.subscribe(TOPICO_SEL_DESAFIO);
        mqttClient.subscribe(TOPICO_D3_CODIGO);
        LOG_DEBUG("[MQTT] Conectado a %s\n", MQTT_BROKER);
        enviar_vidas_mqtt(vidas); // Enviar vidas actuales al conectar/reconectar
    }
}

void tarea_mqtt(void *parameter) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    while (WiFi.status() != WL_CONNECTED) {
        vTaskDelay(pdMS_TO_TICKS(ESPERA_WIFI));
    }
    LOG_DEBUG("[MQTT] WiFi conectado\n");
    mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
    mqttClient.setCallback(callback_mqtt);

    while (1) {
        if (!mqttClient.connected()) {
            reconectar_mqtt();
            vTaskDelay(pdMS_TO_TICKS(ESPERA_RECONEXION));
        } else {
            msg_mqtt_tx_t tx_msg;
            while (xQueueReceive(cola_mqtt_tx, &tx_msg, SIN_ESPERA) == pdTRUE) {
                mqttClient.publish(tx_msg.topico, tx_msg.payload);
            }
            mqttClient.loop();
            vTaskDelay(pdMS_TO_TICKS(PERIODO_MQTT));
        }
    }
}

#endif
