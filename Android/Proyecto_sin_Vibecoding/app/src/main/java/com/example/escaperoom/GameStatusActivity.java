package com.example.escaperoom;

import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.appcompat.app.AppCompatActivity;
import androidx.core.content.ContextCompat;

/**
 * Pantalla que se activa al recibir estado "BLOQUEADO" (penalizacion)
 * o "VICTORIA" desde el topico soa/escape/estado.
 */
public class GameStatusActivity extends AppCompatActivity implements MqttManager.MqttListener {

    public static final String EXTRA_MODE = "mode";
    public static final String MODE_BLOQUEADO = "BLOQUEADO";
    public static final String MODE_VICTORIA = "VICTORIA";

    private String currentMode = MODE_BLOQUEADO;
    private MqttManager mqttManager;

    private ImageView statusIcon;
    private TextView statusTitle;
    private TextView statusSubtitle;
    private TextView statusHint;
    private Button btnStatusAction;

    public static void show(Context context, String mode) {
        Intent intent = new Intent(context, GameStatusActivity.class);
        intent.putExtra(EXTRA_MODE, mode);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_SINGLE_TOP);
        context.startActivity(intent);
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_game_status);

        currentMode = getIntent().getStringExtra(EXTRA_MODE);
        if (currentMode == null)
            currentMode = MODE_BLOQUEADO;

        statusIcon = findViewById(R.id.statusIcon);
        statusTitle = findViewById(R.id.statusTitle);
        statusSubtitle = findViewById(R.id.statusSubtitle);
        statusHint = findViewById(R.id.statusHint);
        btnStatusAction = findViewById(R.id.btnStatusAction);

        mqttManager = MqttManager.getInstance(this);
        mqttManager.addListener(this);

        updateUi();

        btnStatusAction.setOnClickListener(v -> {
            Intent intent = new Intent(this, MainActivity.class);
            intent.addFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP | Intent.FLAG_ACTIVITY_SINGLE_TOP);
            startActivity(intent);
            finish();
        });
    }

    @Override
    protected void onResume() {
        super.onResume();
        mqttManager.addListener(this);

        // Si la pantalla recupera el foco y el ESP32 ya restauro vidas o salio del
        // bloqueo,
        // nos desbloqueamos inmediatamente y volvemos al menu principal
        if (MODE_BLOQUEADO.equalsIgnoreCase(currentMode)) {
            if (mqttManager.getVidas() > 0 || !"BLOQUEADO".equalsIgnoreCase(mqttManager.getCurrentEstado())) {
                volverAlMenu();
            }
        }
    }

    @Override
    protected void onPause() {
        super.onPause();
        // Mantener suscrito para recibir eventos incluso si la ventana pierde el foco
    }

    private void volverAlMenu() {
        Intent intent = new Intent(this, MainActivity.class);
        intent.addFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP | Intent.FLAG_ACTIVITY_SINGLE_TOP);
        startActivity(intent);
        finish();
    }

    @Override
    protected void onNewIntent(Intent intent) {
        super.onNewIntent(intent);
        setIntent(intent);
        String mode = intent.getStringExtra(EXTRA_MODE);
        if (mode != null) {
            currentMode = mode;
            updateUi();
        }
    }

    private void updateUi() {
        if (MODE_VICTORIA.equalsIgnoreCase(currentMode)) {
            statusIcon.setImageResource(R.drawable.ic_trophy);
            statusIcon.setColorFilter(ContextCompat.getColor(this, R.color.piece_amber));
            statusTitle.setText("¡VICTORIA!");
            statusTitle.setTextColor(ContextCompat.getColor(this, R.color.result_win));
            statusSubtitle.setText("¡Escape Room completado!\nResolviste todos los desafíos y la caja se abrió.");
            statusHint.setText(
                    "Completaste:\n✓ D1: Patrón acústico\n✓ D2: Calibración analógica\n✓ D3: Gestión de memoria");
            btnStatusAction.setVisibility(android.view.View.VISIBLE);
            btnStatusAction.setText("Volver al menú");
        } else {
            statusIcon.setImageResource(R.drawable.ic_lock_closed);
            statusIcon.setColorFilter(ContextCompat.getColor(this, R.color.result_lose));
            statusTitle.setText("SISTEMA BLOQUEADO");
            statusTitle.setTextColor(ContextCompat.getColor(this, R.color.result_lose));
            statusSubtitle.setText("¡Se te acabaron las vidas!");
            statusHint.setText(
                    "Esperá 60 segundos a que se desbloquee el sistema.");
            // Si ya tiene vidas o ya no está bloqueado, habilitar botón de retorno
            if (mqttManager.getVidas() > 0 || !"BLOQUEADO".equalsIgnoreCase(mqttManager.getCurrentEstado())) {
                btnStatusAction.setVisibility(android.view.View.VISIBLE);
                btnStatusAction.setText("Volver al menú");
            } else {
                btnStatusAction.setVisibility(android.view.View.GONE);
            }
        }
    }

    @Override
    public void onBackPressed() {
        if (MODE_BLOQUEADO.equalsIgnoreCase(currentMode) && mqttManager.getVidas() <= 0
                && "BLOQUEADO".equalsIgnoreCase(mqttManager.getCurrentEstado())) {
            // Impedir salir al menu durante el bloqueo mientras no tenga vidas
            return;
        }
        super.onBackPressed();
    }

    @Override
    public void onVidasUpdated(int vidas) {
        // Si estabamos bloqueados y el ESP32 restablecio vidas (vidas > 0), finalizamos
        // la penalizacion y volvemos automaticamente al menu principal
        if (MODE_BLOQUEADO.equalsIgnoreCase(currentMode) && vidas > 0) {
            btnStatusAction.setVisibility(android.view.View.VISIBLE);
            btnStatusAction.setText("Volver al menú");
            volverAlMenu();
        }
    }

    @Override
    public void onEstadoUpdated(String estado) {
        if (MODE_BLOQUEADO.equalsIgnoreCase(currentMode) && !"BLOQUEADO".equalsIgnoreCase(estado)) {
            btnStatusAction.setVisibility(android.view.View.VISIBLE);
            btnStatusAction.setText("Volver al menú");
            volverAlMenu();
            return;
        }

        if (!currentMode.equalsIgnoreCase(estado)) {
            currentMode = estado;
            updateUi();
        }
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        mqttManager.removeListener(this);
    }
}
