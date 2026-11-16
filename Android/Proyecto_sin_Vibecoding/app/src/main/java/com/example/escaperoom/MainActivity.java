package com.example.escaperoom;

import android.content.Intent;
import android.graphics.Color;
import android.graphics.drawable.GradientDrawable;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.EditText;
import android.widget.ImageView;
import android.widget.TextView;
import android.widget.Toast;

import androidx.appcompat.app.AlertDialog;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.content.ContextCompat;

public class MainActivity extends AppCompatActivity implements MqttManager.MqttListener {

    private static final int[] CARD_IDS = {
            R.id.challenge1, R.id.challenge2, R.id.challenge3 };
    private static final int[] TITLES = {
            R.string.challenge_1_title, R.string.challenge_2_title, R.string.challenge_3_title };
    private static final int[] DESCS = {
            R.string.challenge_1_desc, R.string.challenge_2_desc, R.string.challenge_3_desc };
    private static final int[] COLORS = {
            R.color.challenge_1, R.color.challenge_2, R.color.challenge_3 };

    private MqttManager mqttManager;
    private MqttConnectionDialog mqttDialog;

    private View mqttStatusDot;
    private View resetOverlay;
    private TextView mqttStatusText;
    private ImageView heart1, heart2, heart3;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        mqttManager = MqttManager.getInstance(this);
        mqttDialog = new MqttConnectionDialog(this, this::showBrokerSettingsDialog);

        mqttStatusDot = findViewById(R.id.mqttStatusDot);
        resetOverlay = findViewById(R.id.resetOverlay);
        mqttStatusText = findViewById(R.id.mqttStatusText);
        heart1 = findViewById(R.id.heart1);
        heart2 = findViewById(R.id.heart2);
        heart3 = findViewById(R.id.heart3);

        findViewById(R.id.btnBrokerSettings).setOnClickListener(v -> showBrokerSettingsDialog());

        for (int i = 0; i < CARD_IDS.length; i++) {
            final int challenge = i + 1;
            View card = findViewById(CARD_IDS[i]);

            ((TextView) card.findViewById(R.id.challengeTitle)).setText(TITLES[i]);
            ((TextView) card.findViewById(R.id.challengeDesc)).setText(DESCS[i]);
            ((ImageView) card.findViewById(R.id.challengeIcon))
                    .setColorFilter(ContextCompat.getColor(this, COLORS[i]));

            // Al pulsar los botones del menu:
            // 1. Publicar en soa/escape/seleccion con "1", "2" o "3"
            // 2. Abrir la Activity del desafio correspondiente
            card.setOnClickListener(v -> openChallenge(challenge));
        }

        updateAllUi();
    }

    @Override
    protected void onResume() {
        super.onResume();
        mqttManager.addListener(this);
        updateAllUi();

        if (mqttManager.getVidas() <= 0 || "BLOQUEADO".equalsIgnoreCase(mqttManager.getCurrentEstado())) {
            GameStatusActivity.show(this, GameStatusActivity.MODE_BLOQUEADO);
        }
    }

    @Override
    protected void onPause() {
        super.onPause();
        mqttManager.removeListener(this);
        mqttDialog.dismiss();
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        if (mqttDialog != null) {
            mqttDialog.dismiss();
        }
    }

    private void openChallenge(int challenge) {
        if ("RESETEANDO".equalsIgnoreCase(mqttManager.getCurrentEstado())) {
            return;
        }
        if (Boolean.TRUE.equals(mqttManager.getDesafioStatus(challenge))) {
            new AlertDialog.Builder(this)
                    .setTitle(R.string.already_completed_title)
                    .setMessage(R.string.already_completed_message)
                    .setPositiveButton(R.string.accept, null)
                    .show();
            return;
        }
        if (mqttManager.getVidas() <= 0 || "BLOQUEADO".equalsIgnoreCase(mqttManager.getCurrentEstado())) {
            Toast.makeText(this, "Sistema bloqueado: penalización en curso.", Toast.LENGTH_SHORT).show();
            GameStatusActivity.show(this, GameStatusActivity.MODE_BLOQUEADO);
            return;
        }

        // La publicacion en soa/escape/seleccion se realiza cuando el usuario
        // pulsa "Comenzar" dentro de ChallengeActivity, para darle tiempo a leer la
        // consigna.
        Intent intent = new Intent(this, ChallengeActivity.class);
        intent.putExtra(ChallengeActivity.EXTRA_CHALLENGE, challenge);
        startActivity(intent);
    }

    private void updateAllUi() {
        updateConnectionUi(mqttManager.isConnected());
        updateMqttDialog();
        updateVidasUi(mqttManager.getVidas());
        for (int i = 1; i <= 3; i++) {
            updateDesafioUi(i, mqttManager.getDesafioStatus(i));
        }
        updateResetOverlay("RESETEANDO".equalsIgnoreCase(mqttManager.getCurrentEstado()));
    }

    private void updateMqttDialog() {
        if (mqttDialog != null) {
            mqttDialog.update(mqttManager.isConnected(), mqttManager.hasEverConnected());
        }
    }

    private void updateResetOverlay(boolean resetting) {
        if (resetOverlay != null) {
            resetOverlay.setVisibility(resetting ? View.VISIBLE : View.GONE);
        }
    }

    private void updateConnectionUi(boolean connected) {
        if (mqttStatusDot != null) {
            GradientDrawable dot = new GradientDrawable();
            dot.setShape(GradientDrawable.OVAL);
            dot.setColor(connected ? Color.parseColor("#4FD1A5") : Color.parseColor("#FF7A7A"));
            mqttStatusDot.setBackground(dot);
        }
        if (mqttStatusText != null) {
            mqttStatusText.setText(connected ? "MQTT conectado" : "MQTT desconectado");
            mqttStatusText.setTextColor(ContextCompat.getColor(this,
                    connected ? R.color.result_win : R.color.result_lose));
        }
    }

    /**
     * Pinta los corazones / vidas restantes (soa/escape/vidas).
     */
    private void updateVidasUi(int vidas) {
        if (heart1 == null || heart2 == null || heart3 == null)
            return;

        heart1.setImageResource(vidas >= 1 ? R.drawable.ic_heart_full : R.drawable.ic_heart_empty);
        heart2.setImageResource(vidas >= 2 ? R.drawable.ic_heart_full : R.drawable.ic_heart_empty);
        heart3.setImageResource(vidas >= 3 ? R.drawable.ic_heart_full : R.drawable.ic_heart_empty);
    }

    /**
     * Marca si cada desafio esta pendiente o resuelto (soa/escape/desafio).
     */
    private void updateDesafioUi(int challenge, Boolean success) {
        if (challenge < 1 || challenge > CARD_IDS.length)
            return;
        View card = findViewById(CARD_IDS[challenge - 1]);
        if (card == null)
            return;

        ImageView lockIcon = card.findViewById(R.id.challengeLockIcon);
        TextView statusBadge = card.findViewById(R.id.challengeStatusBadge);

        if (Boolean.TRUE.equals(success)) {
            // Resuelto exitosamente
            lockIcon.setImageResource(R.drawable.ic_status_ok);
            lockIcon.setColorFilter(ContextCompat.getColor(this, R.color.result_win));
            statusBadge.setText("RESUELTO");
            statusBadge.setTextColor(ContextCompat.getColor(this, R.color.result_win));
        } else {
            // Pendiente (incluye fallo o abandono previos)
            lockIcon.setImageResource(R.drawable.ic_lock_closed);
            lockIcon.setColorFilter(ContextCompat.getColor(this, R.color.text_muted));
            statusBadge.setText("PENDIENTE");
            statusBadge.setTextColor(ContextCompat.getColor(this, R.color.text_muted));
        }
    }

    // --- Callbacks de MqttListener ---

    @Override
    public void onConnectionChanged(boolean connected) {
        updateConnectionUi(connected);
        updateMqttDialog();
    }

    @Override
    public void onVidasUpdated(int vidas) {
        updateVidasUi(vidas);
        if (vidas <= 0) {
            GameStatusActivity.show(this, GameStatusActivity.MODE_BLOQUEADO);
        }
    }

    @Override
    public void onDesafioUpdated(int challenge, Boolean success) {
        updateDesafioUi(challenge, success);
    }

    @Override
    public void onEstadoUpdated(String estado) {
        if ("BLOQUEADO".equalsIgnoreCase(estado)) {
            updateResetOverlay(false);
            GameStatusActivity.show(this, GameStatusActivity.MODE_BLOQUEADO);
        } else if ("VICTORIA".equalsIgnoreCase(estado)) {
            updateResetOverlay(false);
            GameStatusActivity.show(this, GameStatusActivity.MODE_VICTORIA);
        } else if ("RESETEANDO".equalsIgnoreCase(estado)) {
            updateResetOverlay(true);
        } else if ("RESET".equalsIgnoreCase(estado) || "NORMAL".equalsIgnoreCase(estado)) {
            updateAllUi();
        }
    }

    // --- Dialogo de Configuracion del Broker MQTT ---

    private void showBrokerSettingsDialog() {
        AlertDialog.Builder builder = new AlertDialog.Builder(this);
        builder.setTitle("Configurar Broker MQTT");

        View dialogView = LayoutInflater.from(this).inflate(android.R.layout.select_dialog_item, null);
        android.widget.LinearLayout layout = new android.widget.LinearLayout(this);
        layout.setOrientation(android.widget.LinearLayout.VERTICAL);
        layout.setPadding(48, 24, 48, 16);

        TextView labelHost = new TextView(this);
        labelHost.setText("Host / IP del Broker:");
        labelHost.setTextColor(Color.WHITE);
        layout.addView(labelHost);

        EditText inputHost = new EditText(this);
        inputHost.setText(mqttManager.getBrokerHost());
        inputHost.setTextColor(Color.WHITE);
        layout.addView(inputHost);

        TextView labelPort = new TextView(this);
        labelPort.setText("Puerto (por defecto 1883):");
        labelPort.setTextColor(Color.WHITE);
        labelPort.setPadding(0, 20, 0, 0);
        layout.addView(labelPort);

        EditText inputPort = new EditText(this);
        inputPort.setText(String.valueOf(mqttManager.getBrokerPort()));
        inputPort.setInputType(android.text.InputType.TYPE_CLASS_NUMBER);
        inputPort.setTextColor(Color.WHITE);
        layout.addView(inputPort);

        builder.setView(layout);

        builder.setPositiveButton("Guardar y Conectar", (dialog, which) -> {
            String host = inputHost.getText().toString().trim();
            String portStr = inputPort.getText().toString().trim();
            int port = 1883;
            try {
                if (!portStr.isEmpty())
                    port = Integer.parseInt(portStr);
            } catch (NumberFormatException ignored) {
            }

            mqttManager.updateBrokerConfig(host, port);
            Toast.makeText(this, "Conectando a " + host + ":" + port, Toast.LENGTH_SHORT).show();
        });

        builder.setNegativeButton("Cancelar", null);
        AlertDialog dialog = builder.create();
        dialog.setOnDismissListener(d -> updateMqttDialog());
        dialog.show();
    }
}
