package com.example.escaperoom;

import android.os.Handler;
import android.os.Looper;

/**
 * Simulación local del ESP32 para pruebas sin hardware.
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
