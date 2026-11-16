package com.example.escaperoom;

/**
 * Contrato de comunicación con el ESP32 para los desafíos interactivos.
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
