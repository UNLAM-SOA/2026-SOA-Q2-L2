package com.example.escaperoom;

import android.content.Context;
import android.hardware.Sensor;
import android.hardware.SensorEvent;
import android.hardware.SensorEventListener;
import android.hardware.SensorManager;

/**
 * Detecta sacudidas del telefono usando el acelerometro.
 *
 * Calcula la fuerza G total de los tres ejes; si supera un umbral (y paso el
 * tiempo de enfriamiento desde la ultima deteccion) avisa al listener.
 */
public class ShakeDetector implements SensorEventListener {

    /** Se dispara cuando se detecta una sacudida. */
    public interface OnShakeListener {
        void onShake();
    }

    // Fuerza (en g) a partir de la cual se considera una sacudida. 1g = reposo.
    private static final float SHAKE_THRESHOLD_G = 3.0f;
    // Tiempo minimo entre sacudidas, para no disparar varias de golpe.
    private static final long SHAKE_COOLDOWN_MS = 1000;
    // Cantidad de picos de fuerza seguidos necesarios para contar como sacudida.
    private static final int REQUIRED_SHAKES = 3;
    // Tiempo maximo entre picos, si pasa mas, se reinicia la cuenta.
    private static final long SHAKE_WINDOW_MS = 800;
    // Separacion minima entre picos, para no contar el mismo movimiento dos veces.
    private static final long MIN_PEAK_GAP_MS = 150;

    private final SensorManager sensorManager;
    private final Sensor accelerometer;
    private final OnShakeListener listener;
    private long lastShakeTime;
    private long lastPeakTime;
    private int shakeCount;

    public ShakeDetector(Context context, OnShakeListener listener) {
        this.listener = listener;
        sensorManager = (SensorManager) context.getSystemService(Context.SENSOR_SERVICE);
        accelerometer = sensorManager != null
                ? sensorManager.getDefaultSensor(Sensor.TYPE_ACCELEROMETER)
                : null;
    }

    /** true si el dispositivo tiene acelerometro disponible. */
    public boolean isAvailable() {
        return accelerometer != null;
    }

    /** Empieza a escuchar el acelerometro. */
    public void start() {
        if (accelerometer != null) {
            shakeCount = 0;
            sensorManager.registerListener(this, accelerometer, SensorManager.SENSOR_DELAY_GAME);
        }
    }

    /** Deja de escuchar el acelerometro (importante para no gastar bateria). */
    public void stop() {
        if (sensorManager != null) {
            sensorManager.unregisterListener(this);
        }
    }

    @Override
    public void onSensorChanged(SensorEvent event) {
        float gx = event.values[0] / SensorManager.GRAVITY_EARTH;
        float gy = event.values[1] / SensorManager.GRAVITY_EARTH;
        float gz = event.values[2] / SensorManager.GRAVITY_EARTH;

        // Magnitud del vector de aceleracion, en multiplos de g.
        double gForce = Math.sqrt(gx * gx + gy * gy + gz * gz);

        if (gForce > SHAKE_THRESHOLD_G) {
            long now = System.currentTimeMillis();
            if (now - lastShakeTime < SHAKE_COOLDOWN_MS) {
                return;
            }
            // Ignora picos pegados al anterior (es el mismo movimiento).
            if (now - lastPeakTime < MIN_PEAK_GAP_MS) {
                return;
            }
            // Si paso mucho desde el ultimo pico, empieza la cuenta de cero.
            if (now - lastPeakTime > SHAKE_WINDOW_MS) {
                shakeCount = 0;
            }
            lastPeakTime = now;
            shakeCount++;

            if (shakeCount >= REQUIRED_SHAKES) {
                shakeCount = 0;
                lastShakeTime = now;
                if (listener != null) {
                    listener.onShake();
                }
            }
        }
    }

    @Override
    public void onAccuracyChanged(Sensor sensor, int accuracy) {
        // No necesitamos reaccionar a los cambios de precision.
    }
}