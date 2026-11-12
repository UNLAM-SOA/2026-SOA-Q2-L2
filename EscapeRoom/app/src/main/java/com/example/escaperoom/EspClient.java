package com.example.escaperoom;

/**
 * Contrato de comunicación con el ESP32.
 *
 * La app no sabe si por debajo es BLE, Wi-Fi o Bluetooth clásico: para cambiar el transporte
 * basta con crear otra clase que implemente esta interfaz (ver FakeEspClient como ejemplo).
 *
 * Protocolo propuesto (texto simple):
 *   App  -> ESP32 : "START:1" o "START:2"  (arrancar desafío)
 *   ESP32 -> App  : "RESULT:OK" o "RESULT:FAIL"
 */
public interface EspClient {

    interface Listener {
        /** El ESP32 informó el resultado del desafío. */
        void onResult(boolean success);

        /** No se pudo enviar el mensaje o se perdió la conexión. */
        void onError();
    }

    void setListener(Listener listener);

    /** Envía la señal para que el ESP32 arranque el desafío indicado. */
    void startChallenge(int challenge);

    /** Deja de esperar el resultado del desafío en curso. */
    void cancel();
}
