package com.example.escaperoom;

import android.app.Activity;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.Button;
import android.widget.TextView;

import androidx.annotation.Nullable;
import androidx.appcompat.app.AlertDialog;

/**
 * Popup modal reutilizable que muestra el estado de conexion/reconexion MQTT
 * sobre la pantalla actual sin regresar al menu principal.
 */
public class MqttConnectionDialog {

    private final Activity activity;
    private final Runnable onConfigureBroker;
    private AlertDialog dialog;
    private TextView messageView;

    public MqttConnectionDialog(Activity activity, @Nullable Runnable onConfigureBroker) {
        this.activity = activity;
        this.onConfigureBroker = onConfigureBroker;
    }

    public void update(boolean connected, boolean reconnecting) {
        if (connected) {
            dismiss();
        } else {
            show(reconnecting);
        }
    }

    public void show(boolean reconnecting) {
        if (activity.isFinishing() || activity.isDestroyed()) {
            return;
        }
        int msgRes = reconnecting ? R.string.mqtt_reconnecting : R.string.mqtt_connecting;
        if (dialog != null && dialog.isShowing()) {
            if (messageView != null) {
                messageView.setText(msgRes);
            }
            return;
        }

        View content = LayoutInflater.from(activity).inflate(R.layout.dialog_mqtt_loading, null);
        messageView = content.findViewById(R.id.mqttLoadingText);
        messageView.setText(msgRes);

        Button btnBroker = content.findViewById(R.id.btnMqttConfigureBroker);
        if (onConfigureBroker != null) {
            btnBroker.setVisibility(View.VISIBLE);
            btnBroker.setOnClickListener(v -> {
                dismiss();
                onConfigureBroker.run();
            });
        } else {
            btnBroker.setVisibility(View.GONE);
        }

        dialog = new AlertDialog.Builder(activity)
                .setView(content)
                .setCancelable(false)
                .create();
        if (dialog.getWindow() != null) {
            dialog.getWindow().setBackgroundDrawable(new ColorDrawable(Color.TRANSPARENT));
        }
        dialog.show();
    }

    public void dismiss() {
        if (dialog != null) {
            if (dialog.isShowing() && !activity.isDestroyed()) {
                dialog.dismiss();
            }
            dialog = null;
            messageView = null;
        }
    }
}

