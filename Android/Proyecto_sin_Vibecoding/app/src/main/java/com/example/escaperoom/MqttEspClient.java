package com.example.escaperoom;

import android.content.Context;

/**
 * Implementacion real de EspClient basada en MQTT.
 * Al arrancar un desafio publica en soa/escape/seleccion y escucha
 * el resultado en soa/escape/desafio.
 */
public class MqttEspClient implements EspClient, MqttManager.MqttListener {

    private final MqttManager mqttManager;
    private Listener listener;
    private int waitingChallenge = -1;

    public MqttEspClient(Context context) {
        this.mqttManager = MqttManager.getInstance(context);
        this.mqttManager.addListener(this);
    }

    @Override
    public void setListener(Listener listener) {
        this.listener = listener;
    }

    @Override
    public void startChallenge(int challenge) {
        this.waitingChallenge = challenge;
        mqttManager.publishSeleccion(challenge);
    }

    @Override
    public void cancel() {
        this.waitingChallenge = -1;
    }

    @Override
    public void onDesafioUpdated(int challenge, boolean success) {
        if (waitingChallenge == challenge) {
            waitingChallenge = -1;
            if (listener != null) {
                listener.onResult(success);
            }
        }
    }

    @Override
    public void onConnectionChanged(boolean connected) {
        if (!connected && waitingChallenge != -1) {
            // Se perdio conexion mientras esperaba
            if (listener != null) {
                listener.onError();
            }
        }
    }

    public void cleanup() {
        cancel();
        mqttManager.removeListener(this);
    }
}

