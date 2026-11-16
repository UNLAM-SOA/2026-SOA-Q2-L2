// ============================================================================
// comunicacion.h — Serial y MQTT (traduce comandos a eventos y publica cola TX)
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

// "1", "2", "3" o "ABANDONAR": elegir o abandonar desafío. Devuelve true si era uno de ésos.
bool encolar_seleccion(const String &texto) {
    if (texto == "1")      encolar_evento(EVENTO_DESAFIO_1, 0);
    else if (texto == "2") encolar_evento(EVENTO_DESAFIO_2, 0);
    else if (texto == "3") encolar_evento(EVENTO_DESAFIO_3, 0);
    else if (texto.equalsIgnoreCase("ABANDONAR")) encolar_evento(EVENTO_ABANDONAR, 0);
    else return false;
    return true;
}

// Resultado del desafío 3 enviado por la app ("OK" o "ERROR").
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

void enviar_estado_desafio_mqtt(int num_desafio, const char *estado) {
    char buf[TAM_BUFFER_TEXTO];
    snprintf(buf, sizeof(buf), "%d,%s", num_desafio, estado);
    enviar_a_mqtt(TOPICO_TX_DESAFIO, buf);
}

void enviar_resultado_mqtt(int num_desafio, bool exito) {
    enviar_estado_desafio_mqtt(num_desafio, exito ? "OK" : "ERROR");
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
