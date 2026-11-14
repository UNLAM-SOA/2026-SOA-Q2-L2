package com.example.escaperoom;

import android.content.Intent;
import android.content.res.ColorStateList;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.RippleDrawable;
import android.os.Bundle;
import android.text.Editable;
import android.text.TextWatcher;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.StringRes;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.content.ContextCompat;

import java.util.Random;

/**
 * Flujo de un desafio:
 *   explicacion -> Comenzar -> (desafios 1 y 2: esperando al ESP32 | desafio 3: minijuego) -> resultado
 *
 * En el desafio 3, al ganar el minijuego se revela un codigo de 4 caracteres. El jugador
 * lo escribe en el campo de entrada y, al completar los 4 caracteres, debe SACUDIR el
 * telefono. El acelerometro detecta la sacudida y recien ahi se compara el codigo:
 * si coincide, el minijuego se da por ganado y se publica "OK" en soa/escape/d3.
 */
public class ChallengeActivity extends AppCompatActivity implements EspClient.Listener, MqttManager.MqttListener {

    public static final String EXTRA_CHALLENGE = "challenge";
    private static final int MINIGAME_CHALLENGE = 3;

    private static final String CODE_CHARS = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
    private static final int CODE_LENGTH = 4;

    private static final int[] TITLES = {
            R.string.challenge_1_title, R.string.challenge_2_title, R.string.challenge_3_title};
    private static final int[] INTROS = {
            R.string.challenge_1_intro, R.string.challenge_2_intro, R.string.challenge_3_intro};
    private static final int[] COLORS = {
            R.color.challenge_1, R.color.challenge_2, R.color.challenge_3};

    private int challenge;
    private MqttEspClient esp;
    private MqttManager mqttManager;
    private MemoryGame game;
    private ShakeDetector shakeDetector;
    private final Random codeRandom = new Random();
    private String winCode;
    private boolean codeReady; // true cuando hay 4 caracteres escritos y falta sacudir

    private View panelIntro, panelWaiting, panelGame, panelResult;
    private View containerD1Waiting, containerD2Gauge;
    private PotentiometerGaugeView potGaugeView;

    private ImageView heart1, heart2, heart3;
    private View codeEntry;
    private TextView gameOps, gameTime, heapCapacity, requestLabel, resultTitle, resultMessage;
    private TextView codeReveal, codeError;
    private EditText codeInput;
    private Button btnProduce, btnConsume;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_challenge);

        challenge = getIntent().getIntExtra(EXTRA_CHALLENGE, 1);
        mqttManager = MqttManager.getInstance(this);

        panelIntro = findViewById(R.id.panelIntro);
        panelWaiting = findViewById(R.id.panelWaiting);
        panelGame = findViewById(R.id.panelGame);
        panelResult = findViewById(R.id.panelResult);

        containerD1Waiting = findViewById(R.id.containerD1Waiting);
        containerD2Gauge = findViewById(R.id.containerD2Gauge);
        potGaugeView = findViewById(R.id.potGaugeView);

        heart1 = findViewById(R.id.challengeHeart1);
        heart2 = findViewById(R.id.challengeHeart2);
        heart3 = findViewById(R.id.challengeHeart3);

        codeEntry = findViewById(R.id.codeEntry);
        gameOps = findViewById(R.id.gameHits);
        gameTime = findViewById(R.id.gameTime);
        heapCapacity = findViewById(R.id.heapCapacity);
        requestLabel = findViewById(R.id.requestLabel);
        resultTitle = findViewById(R.id.resultTitle);
        resultMessage = findViewById(R.id.resultMessage);
        codeReveal = findViewById(R.id.codeReveal);
        codeError = findViewById(R.id.codeError);
        codeInput = findViewById(R.id.codeInput);

        ((TextView) findViewById(R.id.challengeTitle)).setText(TITLES[challenge - 1]);
        ((TextView) findViewById(R.id.introText)).setText(INTROS[challenge - 1]);

        int color = ContextCompat.getColor(this, COLORS[challenge - 1]);
        stylePrimaryButton((Button) findViewById(R.id.btnStart), color);
        stylePrimaryButton((Button) findViewById(R.id.btnRetry), color);

        // Botones del minijuego de memoria (desafio 3).
        btnProduce = findViewById(R.id.btnProduce);
        btnConsume = findViewById(R.id.btnConsume);
        stylePrimaryButton(btnProduce, ContextCompat.getColor(this, R.color.result_win));
        stylePrimaryButton(btnConsume, ContextCompat.getColor(this, R.color.piece_amber));

        // Cliente real MQTT para comunicacion con el ESP32
        esp = new MqttEspClient(this);
        esp.setListener(this);

        // Detector de sacudidas (acelerometro) para confirmar el codigo.
        shakeDetector = new ShakeDetector(this, this::onShake);

        game = new MemoryGame((LinearLayout) findViewById(R.id.heapContainer), new MemoryGame.Callback() {
            @Override
            public void onProgress(int opsDone, int totalOps, int heap, int capacity,
                                   int secondsLeft, boolean requestIsProduce) {
                gameOps.setText(getString(R.string.game_ops, opsDone, totalOps));
                gameTime.setText(getString(R.string.game_time, secondsLeft));
                heapCapacity.setText(getString(R.string.game_heap, heap, capacity));
                requestLabel.setText(requestIsProduce
                        ? R.string.request_produce : R.string.request_consume);
            }

            @Override
            public void onFinished(boolean won, int failCode) {
                if (won) {
                    revealCode();
                } else {
                    // Si falla el minijuego de memoria, avisamos al ESP32 para descontar vida
                    mqttManager.publishD3Error();
                    showResult(false, failMessage(failCode));
                }
            }
        });

        // Al escribir, solo marcamos si el codigo esta completo; la comparacion
        // se hace recien cuando el jugador sacude el telefono.
        codeInput.addTextChangedListener(new TextWatcher() {
            @Override public void beforeTextChanged(CharSequence s, int st, int c, int a) { }
            @Override public void onTextChanged(CharSequence s, int st, int b, int c) { }
            @Override public void afterTextChanged(Editable s) { onCodeChanged(s.toString()); }
        });

        btnProduce.setOnClickListener(v -> game.produce());
        btnConsume.setOnClickListener(v -> game.consume());

        findViewById(R.id.btnBack).setOnClickListener(v -> finish());
        findViewById(R.id.btnStart).setOnClickListener(v -> startChallenge());
        findViewById(R.id.btnRetry).setOnClickListener(v -> startChallenge());
        findViewById(R.id.btnMenu).setOnClickListener(v -> volverAlMenuPrincipal());
        findViewById(R.id.btnCancel).setOnClickListener(v -> {
            esp.cancel();
            showPanel(panelIntro);
        });

        updateVidasUi(mqttManager.getVidas());
        showPanel(panelIntro);
    }

    private void volverAlMenuPrincipal() {
        Intent intent = new Intent(this, MainActivity.class);
        intent.addFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP | Intent.FLAG_ACTIVITY_SINGLE_TOP);
        startActivity(intent);
        finish();
    }

    private void startChallenge() {
        if (challenge == MINIGAME_CHALLENGE) {
            // Avisar al ESP32 que arranca D3 para que entre en ESTADO_D3_ESPERANDO_CODIGO
            mqttManager.publishSeleccion(3);
            resetGameUi();
            showPanel(panelGame);
            game.start();
        } else {
            showPanel(panelWaiting);

            // Ajustar subpaneles segun sea D1 o D2
            if (challenge == 2) {
                if (containerD1Waiting != null) containerD1Waiting.setVisibility(View.GONE);
                if (containerD2Gauge != null) {
                    containerD2Gauge.setVisibility(View.VISIBLE);
                    potGaugeView.setVoltage(mqttManager.getLastVoltage());
                }
            } else {
                if (containerD1Waiting != null) containerD1Waiting.setVisibility(View.VISIBLE);
                if (containerD2Gauge != null) containerD2Gauge.setVisibility(View.GONE);
            }

            esp.startChallenge(challenge);
        }
    }

    // ---- Actualizaciones MQTT ----

    @Override
    public void onPotentiometerUpdated(float voltage) {
        // Regla 1: soa/escape/pot para actualizar el gauge/grafico del potenciometro en D2
        if (challenge == 2 && potGaugeView != null) {
            potGaugeView.setVoltage(voltage);
        }
    }

    @Override
    public void onVidasUpdated(int vidas) {
        // Regla 1: soa/escape/vidas para pintar los corazones/vidas restantes
        updateVidasUi(vidas);
        if (vidas <= 0) {
            finish();
            GameStatusActivity.show(this, GameStatusActivity.MODE_BLOQUEADO);
        }
    }

    @Override
    public void onEstadoUpdated(String estado) {
        // Regla 1: soa/escape/estado para activar la pantalla de bloqueo o victoria
        if ("BLOQUEADO".equalsIgnoreCase(estado)) {
            finish();
            GameStatusActivity.show(this, GameStatusActivity.MODE_BLOQUEADO);
        } else if ("VICTORIA".equalsIgnoreCase(estado)) {
            finish();
            GameStatusActivity.show(this, GameStatusActivity.MODE_VICTORIA);
        }
    }

    private void updateVidasUi(int vidas) {
        if (heart1 == null || heart2 == null || heart3 == null) return;
        heart1.setImageResource(vidas >= 1 ? R.drawable.ic_heart_full : R.drawable.ic_heart_empty);
        heart2.setImageResource(vidas >= 2 ? R.drawable.ic_heart_full : R.drawable.ic_heart_empty);
        heart3.setImageResource(vidas >= 3 ? R.drawable.ic_heart_full : R.drawable.ic_heart_empty);
    }

    // ---- Respuestas del ESP32 (desafios 1 y 2) ----

    @Override
    public void onResult(boolean success) {
        showResult(success, success ? R.string.win_message : R.string.lose_message);
    }

    @Override
    public void onError() {
        showResult(false, R.string.error_message);
    }

    // ---- Codigo secreto del desafio 3 ----

    /** Deja el minijuego en su estado inicial (caja bloqueada, sin entrada de codigo). */
    private void resetGameUi() {
        winCode = null;
        codeReady = false;
        shakeDetector.stop();
        codeReveal.setText(R.string.code_locked);
        codeEntry.setVisibility(View.GONE);
        codeError.setVisibility(View.GONE);
        codeInput.setText("");
        btnProduce.setEnabled(true);
        btnConsume.setEnabled(true);
    }

    /** Se gano el minijuego: genera y muestra el codigo, y habilita la entrada. */
    private void revealCode() {
        winCode = generateCode();
        codeReady = false;
        codeReveal.setText(winCode);
        requestLabel.setText(R.string.game_enter_code);
        btnProduce.setEnabled(false);
        btnConsume.setEnabled(false);
        codeError.setVisibility(View.GONE);
        codeInput.setText("");
        codeEntry.setVisibility(View.VISIBLE);
        codeInput.requestFocus();
    }

    /**
     * Cada cambio del campo: al completar los 4 caracteres pide la sacudida.
     * Si el dispositivo no tiene acelerometro, valida directamente (respaldo).
     */
    private void onCodeChanged(String entered) {
        if (winCode == null) return;
        codeError.setVisibility(View.GONE);

        if (entered.trim().length() < CODE_LENGTH) {
            codeReady = false;
            shakeDetector.stop();
            requestLabel.setText(R.string.game_enter_code);
            return;
        }

        codeReady = true;
        if (shakeDetector.isAvailable()) {
            requestLabel.setText(R.string.shake_to_confirm);
            shakeDetector.start();
        } else {
            // Sin acelerometro: no hay forma de sacudir, validamos de una.
            verifyCode();
        }
    }

    /** Disparado por el acelerometro cuando se detecta una sacudida. */
    private void onShake() {
        if (!codeReady || winCode == null) return;
        verifyCode();
    }

    /** Compara el codigo ingresado con el generado. */
    private void verifyCode() {
        String entered = codeInput.getText().toString().trim();
        if (entered.equalsIgnoreCase(winCode)) {
            codeReady = false;
            shakeDetector.stop();

            // Regla 2: Publicar en soa/escape/d3 con "OK" cuando el jugador gane el minijuego de la Activity 2
            mqttManager.publishD3Ok();

            showResult(true, getString(R.string.win_memory));
        } else {
            codeError.setVisibility(View.VISIBLE);
        }
    }

    private String generateCode() {
        StringBuilder sb = new StringBuilder(CODE_LENGTH);
        for (int i = 0; i < CODE_LENGTH; i++) {
            sb.append(CODE_CHARS.charAt(codeRandom.nextInt(CODE_CHARS.length())));
        }
        return sb.toString();
    }

    // ---- UI ----

    private String failMessage(int code) {
        switch (code) {
            case MemoryGame.FAIL_OVERFLOW:
                return getString(R.string.fail_overflow);
            case MemoryGame.FAIL_UNDERFLOW:
                return getString(R.string.fail_underflow);
            case MemoryGame.FAIL_TIMEOUT:
                return getString(R.string.fail_timeout);
            default:
                return getString(R.string.fail_wrong);
        }
    }

    private void showResult(boolean won, @StringRes int message) {
        showResult(won, getString(message));
    }

    private void showResult(boolean won, CharSequence message) {
        resultTitle.setText(won ? R.string.win_title : R.string.lose_title);
        resultTitle.setTextColor(ContextCompat.getColor(
                this, won ? R.color.result_win : R.color.result_lose));
        resultMessage.setText(message);

        // Si gano, no debe mostrar el boton de Reintentar, solo Volver al menu principal
        View btnRetry = findViewById(R.id.btnRetry);
        if (btnRetry != null) {
            btnRetry.setVisibility(won ? View.GONE : View.VISIBLE);
        }

        showPanel(panelResult);
    }

    /** Boton principal con el color indicado y esquinas redondeadas. */
    private void stylePrimaryButton(Button button, int color) {
        GradientDrawable fill = new GradientDrawable();
        fill.setColor(color);
        fill.setCornerRadius(16 * getResources().getDisplayMetrics().density);
        button.setBackground(new RippleDrawable(ColorStateList.valueOf(0x33000000), fill, null));
    }

    private void showPanel(View panel) {
        panelIntro.setVisibility(panel == panelIntro ? View.VISIBLE : View.GONE);
        panelWaiting.setVisibility(panel == panelWaiting ? View.VISIBLE : View.GONE);
        panelGame.setVisibility(panel == panelGame ? View.VISIBLE : View.GONE);
        panelResult.setVisibility(panel == panelResult ? View.VISIBLE : View.GONE);
    }

    @Override
    protected void onPause() {
        super.onPause();
        mqttManager.removeListener(this);
        shakeDetector.stop();
    }

    @Override
    protected void onResume() {
        super.onResume();
        mqttManager.addListener(this);
        updateVidasUi(mqttManager.getVidas());

        if (challenge == 2 && potGaugeView != null) {
            potGaugeView.setVoltage(mqttManager.getLastVoltage());
        }

        // Si quedo un codigo completo pendiente de confirmar, reactivamos el sensor.
        if (codeReady && shakeDetector.isAvailable()) {
            shakeDetector.start();
        }
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        if (esp != null) {
            esp.cleanup();
        }
        game.stop();
        shakeDetector.stop();
    }
}
