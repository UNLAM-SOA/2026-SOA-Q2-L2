package com.example.escaperoom;

import android.os.Handler;
import android.os.Looper;

/**
 * Simulación del ESP32 para probar la interfaz sin hardware.
 * Responde a los 3 segundos y alterna entre ganar y fallar en cada intento.
 * Reemplazar por la implementación real (BLE / Wi-Fi) cuando esté lista.
 */
public class FakeEspClient implements EspClient {

    private static final long RESPONSE_DELAY_MS = 3000;
    private static int attempts = 0;

    private final Handler handler = new Handler(Looper.getMainLooper());
    private Listener listener;
    private Runnable pending;

    @Override
    public void setListener(Listener listener) {
        this.listener = listener;
    }

    @Override
    public void startChallenge(int challenge) {
        cancel();
        final boolean success = (attempts++ % 2 == 0);
        pending = () -> {
            pending = null;
            if (listener != null) listener.onResult(success);
        };
        handler.postDelayed(pending, RESPONSE_DELAY_MS);
    }

    @Override
    public void cancel() {
        if (pending != null) {
            handler.removeCallbacks(pending);
            pending = null;
        }
    }
}
