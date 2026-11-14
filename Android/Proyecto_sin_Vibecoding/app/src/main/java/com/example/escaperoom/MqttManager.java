package com.example.escaperoom;

import android.content.Context;
import android.content.SharedPreferences;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;

import org.eclipse.paho.client.mqttv3.IMqttDeliveryToken;
import org.eclipse.paho.client.mqttv3.MqttCallbackExtended;
import org.eclipse.paho.client.mqttv3.MqttClient;
import org.eclipse.paho.client.mqttv3.MqttConnectOptions;
import org.eclipse.paho.client.mqttv3.MqttException;
import org.eclipse.paho.client.mqttv3.MqttMessage;
import org.eclipse.paho.client.mqttv3.persist.MemoryPersistence;

import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.UUID;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

/**
 * Gestor central de comunicacion MQTT para la aplicacion Escape Room.
 * Conecta con el broker (por defecto broker.hivemq.com:1883),
 * gestiona reconexiones, suscripciones a los topicos del ESP32 y despacho de eventos en la UI thread.
 */
public class MqttManager {

    private static final String TAG = "MqttManager";

    public static final String DEFAULT_BROKER_HOST = "broker.hivemq.com";
    public static final int DEFAULT_BROKER_PORT = 1883;

    // Topicos suscritos
    public static final String TOPIC_POT     = "soa/escape/pot";
    public static final String TOPIC_DESAFIO = "soa/escape/desafio";
    public static final String TOPIC_VIDAS   = "soa/escape/vidas";
    public static final String TOPIC_ESTADO  = "soa/escape/estado";

    // Topicos publicados
    public static final String TOPIC_SELECCION = "soa/escape/seleccion";
    public static final String TOPIC_D3        = "soa/escape/d3";

    private static final String PREFS_NAME = "mqtt_prefs";
    private static final String KEY_HOST = "broker_host";
    private static final String KEY_PORT = "broker_port";

    public interface MqttListener {
        default void onPotentiometerUpdated(float voltage) {}
        default void onDesafioUpdated(int challenge, boolean success) {}
        default void onVidasUpdated(int vidas) {}
        default void onEstadoUpdated(String estado) {}
        default void onConnectionChanged(boolean connected) {}
    }

    private static MqttManager instance;

    private final Context appContext;
    private final Handler mainHandler = new Handler(Looper.getMainLooper());
    private final ExecutorService executor = Executors.newSingleThreadExecutor();
    private final List<MqttListener> listeners = new ArrayList<>();

    private MqttClient mqttClient;
    private String brokerHost;
    private int brokerPort;
    private boolean isConnecting = false;

    // Estado en memoria
    private int currentVidas = 3;
    private float lastVoltage = 0.0f;
    private String currentEstado = "NORMAL";
    private final Map<Integer, Boolean> desafioStatus = new HashMap<>();

    public static synchronized MqttManager getInstance(Context context) {
        if (instance == null) {
            instance = new MqttManager(context.getApplicationContext());
        }
        return instance;
    }

    private MqttManager(Context context) {
        this.appContext = context;
        SharedPreferences prefs = appContext.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE);
        this.brokerHost = prefs.getString(KEY_HOST, DEFAULT_BROKER_HOST);
        this.brokerPort = prefs.getInt(KEY_PORT, DEFAULT_BROKER_PORT);
        connect();
    }

    public synchronized void addListener(MqttListener listener) {
        if (!listeners.contains(listener)) {
            listeners.add(listener);
        }
    }

    public synchronized void removeListener(MqttListener listener) {
        listeners.remove(listener);
    }

    public String getBrokerHost() {
        return brokerHost;
    }

    public int getBrokerPort() {
        return brokerPort;
    }

    public int getVidas() {
        return currentVidas;
    }

    public float getLastVoltage() {
        return lastVoltage;
    }

    public String getCurrentEstado() {
        return currentEstado;
    }

    public Boolean getDesafioStatus(int challenge) {
        return desafioStatus.get(challenge);
    }

    public boolean isConnected() {
        return mqttClient != null && mqttClient.isConnected();
    }

    /**
     * Actualiza la configuracion del broker y reconecta si cambio.
     */
    public void updateBrokerConfig(String host, int port) {
        if (host == null || host.trim().isEmpty()) host = DEFAULT_BROKER_HOST;
        if (port <= 0) port = DEFAULT_BROKER_PORT;

        if (!host.equals(brokerHost) || port != brokerPort) {
            this.brokerHost = host;
            this.brokerPort = port;

            SharedPreferences.Editor editor = appContext.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE).edit();
            editor.putString(KEY_HOST, brokerHost);
            editor.putInt(KEY_PORT, brokerPort);
            editor.apply();

            disconnectAndReconnect();
        }
    }

    public void connect() {
        if (isConnected() || isConnecting) return;

        executor.execute(() -> {
            isConnecting = true;
            notifyConnectionChanged(false);

            String serverUri = "tcp://" + brokerHost + ":" + brokerPort;
            String clientId = "Android_Escape_" + UUID.randomUUID().toString().substring(0, 8);

            try {
                if (mqttClient != null) {
                    try {
                        if (mqttClient.isConnected()) mqttClient.disconnectForcibly();
                        mqttClient.close();
                    } catch (Exception ignored) {}
                }

                mqttClient = new MqttClient(serverUri, clientId, new MemoryPersistence());
                MqttConnectOptions options = new MqttConnectOptions();
                options.setAutomaticReconnect(true);
                options.setCleanSession(true);
                options.setConnectionTimeout(10);
                options.setKeepAliveInterval(30);

                mqttClient.setCallback(new MqttCallbackExtended() {
                    @Override
                    public void connectComplete(boolean reconnect, String serverURI) {
                        Log.i(TAG, "Conectado a MQTT broker: " + serverURI + " (reconnect=" + reconnect + ")");
                        subscribeToTopics();
                        isConnecting = false;
                        notifyConnectionChanged(true);
                    }

                    @Override
                    public void connectionLost(Throwable cause) {
                        Log.w(TAG, "Conexion MQTT perdida", cause);
                        isConnecting = false;
                        notifyConnectionChanged(false);
                    }

                    @Override
                    public void messageArrived(String topic, MqttMessage message) {
                        handleIncomingMessage(topic, message);
                    }

                    @Override
                    public void deliveryComplete(IMqttDeliveryToken token) {
                    }
                });

                Log.i(TAG, "Conectando a " + serverUri + "...");
                mqttClient.connect(options);

            } catch (Exception e) {
                Log.e(TAG, "Error al conectar con MQTT: " + e.getMessage());
                isConnecting = false;
                notifyConnectionChanged(false);
            }
        });
    }

    private void disconnectAndReconnect() {
        executor.execute(() -> {
            try {
                if (mqttClient != null && mqttClient.isConnected()) {
                    mqttClient.disconnectForcibly();
                }
            } catch (Exception ignored) {}
            isConnecting = false;
            connect();
        });
    }

    private void subscribeToTopics() {
        if (mqttClient == null || !mqttClient.isConnected()) return;

        try {
            String[] topics = {TOPIC_POT, TOPIC_DESAFIO, TOPIC_VIDAS, TOPIC_ESTADO};
            int[] qos = {1, 1, 1, 1};
            mqttClient.subscribe(topics, qos);
            Log.i(TAG, "Suscrito con exito a los topicos de Escape Room");
        } catch (MqttException e) {
            Log.e(TAG, "Error al suscribirse a topicos", e);
        }
    }

    private void handleIncomingMessage(String topic, MqttMessage message) {
        String payload = new String(message.getPayload(), StandardCharsets.UTF_8).trim();
        Log.d(TAG, "Mensaje MQTT recibido -> [" + topic + "]: " + payload);

        try {
            if (TOPIC_POT.equals(topic)) {
                float voltage = Float.parseFloat(payload);
                lastVoltage = voltage;
                notifyPotentiometer(voltage);

            } else if (TOPIC_DESAFIO.equals(topic)) {
                // Formato esperado: "1,OK", "1,ERROR", "2,OK", etc.
                String[] parts = payload.split(",");
                if (parts.length >= 2) {
                    int ch = Integer.parseInt(parts[0].trim());
                    boolean success = parts[1].trim().equalsIgnoreCase("OK");
                    desafioStatus.put(ch, success);
                    notifyDesafio(ch, success);
                }

            } else if (TOPIC_VIDAS.equals(topic)) {
                int vidas = Integer.parseInt(payload);
                currentVidas = vidas;
                // Si se restauraron las vidas, el estado ya no puede ser BLOQUEADO
                if (vidas > 0 && "BLOQUEADO".equalsIgnoreCase(currentEstado)) {
                    currentEstado = "NORMAL";
                    notifyEstado("NORMAL");
                }
                notifyVidas(vidas);

            } else if (TOPIC_ESTADO.equals(topic)) {
                currentEstado = payload;
                if ("BLOQUEADO".equalsIgnoreCase(payload)) {
                    currentVidas = 0;
                    notifyVidas(0);
                }
                notifyEstado(payload);
            }
        } catch (Exception e) {
            Log.e(TAG, "Error al procesar mensaje de " + topic + " (" + payload + ")", e);
        }
    }

    // --- Publicaciones requeridas ---

    /**
     * Publica en soa/escape/seleccion con "1", "2" o "3" al pulsar los botones del menu.
     */
    public void publishSeleccion(int challengeNumber) {
        publish(TOPIC_SELECCION, String.valueOf(challengeNumber));
    }

    /**
     * Publica en soa/escape/d3 con "OK" cuando el jugador gana el minijuego de la Activity 2.
     */
    public void publishD3Ok() {
        publish(TOPIC_D3, "OK");
    }

    /**
     * Publica en soa/escape/d3 con "ERROR" cuando el jugador falla el minijuego de la Activity 2.
     */
    public void publishD3Error() {
        publish(TOPIC_D3, "ERROR");
    }

    public void publish(String topic, String payload) {
        executor.execute(() -> {
            if (mqttClient != null && mqttClient.isConnected()) {
                try {
                    MqttMessage msg = new MqttMessage(payload.getBytes(StandardCharsets.UTF_8));
                    msg.setQos(1);
                    mqttClient.publish(topic, msg);
                    Log.i(TAG, "Mensaje publicado -> [" + topic + "]: " + payload);
                } catch (MqttException e) {
                    Log.e(TAG, "Error al publicar en " + topic, e);
                }
            } else {
                Log.w(TAG, "No se puede publicar: MQTT desconectado. Intentando reconectar...");
                connect();
            }
        });
    }

    // --- Despacho seguro en UI Thread ---

    private synchronized List<MqttListener> copyListeners() {
        return new ArrayList<>(listeners);
    }

    private void notifyConnectionChanged(boolean connected) {
        mainHandler.post(() -> {
            for (MqttListener l : copyListeners()) {
                l.onConnectionChanged(connected);
            }
        });
    }

    private void notifyPotentiometer(float voltage) {
        mainHandler.post(() -> {
            for (MqttListener l : copyListeners()) {
                l.onPotentiometerUpdated(voltage);
            }
        });
    }

    private void notifyDesafio(int challenge, boolean success) {
        mainHandler.post(() -> {
            for (MqttListener l : copyListeners()) {
                l.onDesafioUpdated(challenge, success);
            }
        });
    }

    private void notifyVidas(int vidas) {
        mainHandler.post(() -> {
            for (MqttListener l : copyListeners()) {
                l.onVidasUpdated(vidas);
            }
        });
    }

    private void notifyEstado(String estado) {
        mainHandler.post(() -> {
            for (MqttListener l : copyListeners()) {
                l.onEstadoUpdated(estado);
            }
        });
    }
}

