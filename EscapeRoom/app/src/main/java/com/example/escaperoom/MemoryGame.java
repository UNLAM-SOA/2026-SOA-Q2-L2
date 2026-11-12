package com.example.escaperoom;

import android.content.Context;
import android.os.CountDownTimer;
import android.view.View;
import android.widget.LinearLayout;

import java.util.Random;

/**
 * Desafío 3: simulación de gestión de memoria (heap) con PRODUCIR / CONSUMIR.
 *
 * El sistema solicita en cada paso una operación (reservar o liberar un bloque).
 * El jugador debe ejecutar la operación correcta. Gana al completar TOTAL_OPS
 * operaciones antes de que se agote el tiempo; pierde por desbordamiento,
 * subdesbordamiento, operación incorrecta o tiempo agotado.
 */
public class MemoryGame {

    public interface Callback {
        /** Actualiza la UI tras cada cambio de estado. */
        void onProgress(int opsDone, int totalOps, int heap, int capacity,
                        int secondsLeft, boolean requestIsProduce);

        /** El juego terminó. failCode == FAIL_NONE si ganó. */
        void onFinished(boolean won, int failCode);
    }

    public static final int CAPACITY = 8;
    public static final int TOTAL_OPS = 15;
    public static final int DURATION_SECONDS = 30;

    public static final int FAIL_NONE = 0;
    public static final int FAIL_OVERFLOW = 1;
    public static final int FAIL_UNDERFLOW = 2;
    public static final int FAIL_WRONG = 3;
    public static final int FAIL_TIMEOUT = 4;

    private final View[] cells = new View[CAPACITY];
    private final Callback callback;
    private final Random random = new Random();

    private CountDownTimer timer;
    private boolean running;
    private int heap;
    private int opsDone;
    private int secondsLeft;
    private boolean requestIsProduce;

    public MemoryGame(LinearLayout container, Callback callback) {
        this.callback = callback;
        Context context = container.getContext();
        int margin = dp(context, 4);
        int height = dp(context, 40);

        container.removeAllViews();
        for (int i = 0; i < CAPACITY; i++) {
            View cell = new View(context);
            LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(0, height, 1f);
            lp.setMargins(margin, 0, margin, 0);
            cell.setLayoutParams(lp);
            cell.setBackgroundResource(R.drawable.bg_tile_idle);
            container.addView(cell);
            cells[i] = cell;
        }
    }

    public void start() {
        stop();
        heap = 0;
        opsDone = 0;
        secondsLeft = DURATION_SECONDS;
        running = true;
        nextRequest();
        renderHeap();
        notifyProgress();

        timer = new CountDownTimer(DURATION_SECONDS * 1000L, 250) {
            @Override
            public void onTick(long millisLeft) {
                int s = (int) Math.ceil(millisLeft / 1000.0);
                if (s != secondsLeft) {
                    secondsLeft = s;
                    notifyProgress();
                }
            }

            @Override
            public void onFinish() {
                finish(false, FAIL_TIMEOUT);
            }
        }.start();
    }

    public void stop() {
        running = false;
        if (timer != null) {
            timer.cancel();
            timer = null;
        }
    }

    public void produce() {
        act(true);
    }

    public void consume() {
        act(false);
    }

    private void act(boolean produce) {
        if (!running) return;

        // ¿El jugador siguió la solicitud del sistema?
        if (produce != requestIsProduce) {
            if (produce && heap >= CAPACITY) {
                finish(false, FAIL_OVERFLOW);
            } else if (!produce && heap <= 0) {
                finish(false, FAIL_UNDERFLOW);
            } else {
                finish(false, FAIL_WRONG);
            }
            return;
        }

        // Ejecuta la operación solicitada (válida por construcción, con guardas).
        if (produce) {
            if (heap >= CAPACITY) {
                finish(false, FAIL_OVERFLOW);
                return;
            }
            heap++;
        } else {
            if (heap <= 0) {
                finish(false, FAIL_UNDERFLOW);
                return;
            }
            heap--;
        }

        opsDone++;
        renderHeap();

        if (opsDone >= TOTAL_OPS) {
            finish(true, FAIL_NONE);
            return;
        }

        nextRequest();
        notifyProgress();
    }

    /** Genera la siguiente solicitud, siempre válida para el estado actual. */
    private void nextRequest() {
        if (heap <= 0) {
            requestIsProduce = true;
        } else if (heap >= CAPACITY) {
            requestIsProduce = false;
        } else {
            requestIsProduce = random.nextBoolean();
        }
    }

    private void renderHeap() {
        for (int i = 0; i < CAPACITY; i++) {
            cells[i].setBackgroundResource(
                    i < heap ? R.drawable.bg_tile_active : R.drawable.bg_tile_idle);
        }
    }

    private void notifyProgress() {
        callback.onProgress(opsDone, TOTAL_OPS, heap, CAPACITY, secondsLeft, requestIsProduce);
    }

    private void finish(boolean won, int failCode) {
        if (!running) return;
        stop();
        callback.onFinished(won, failCode);
    }

    private static int dp(Context context, int value) {
        return Math.round(value * context.getResources().getDisplayMetrics().density);
    }
}