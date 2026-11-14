package com.example.escaperoom;

import android.animation.ValueAnimator;
import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.RectF;
import android.util.AttributeSet;
import android.view.View;
import android.view.animation.DecelerateInterpolator;

import androidx.annotation.Nullable;

/**
 * Vista de Gauge / Instrumento para el potenciometro del Desafio 2 (D2).
 * Muestra el voltaje en tiempo real (0.0V - 3.3V) recibido desde
 * soa/escape/pot,
 * con la zona objetivo destacada (2.00V +- 0.15V).
 */
public class PotentiometerGaugeView extends View {

    private static final float MIN_V = 0.0f;
    private static final float MAX_V = 3.3f;
    public static final float TARGET_V = 2.0f;
    public static final float TOLERANCE_V = 0.15f;

    private static final float START_ANGLE = 150f;
    private static final float SWEEP_ANGLE = 240f;

    private float currentVoltage = 0.0f;
    private float animatedVoltage = 0.0f;
    private ValueAnimator animator;

    private final Paint trackPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint targetZonePaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint progressPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint needlePaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint textValuePaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint textLabelPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint textTargetPaint = new Paint(Paint.ANTI_ALIAS_FLAG);

    private final RectF arcRect = new RectF();

    public PotentiometerGaugeView(Context context) {
        super(context);
        init();
    }

    public PotentiometerGaugeView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
        init();
    }

    public PotentiometerGaugeView(Context context, @Nullable AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
        init();
    }

    private void init() {
        trackPaint.setStyle(Paint.Style.STROKE);
        trackPaint.setStrokeCap(Paint.Cap.ROUND);
        trackPaint.setColor(Color.parseColor("#1A2747"));

        targetZonePaint.setStyle(Paint.Style.STROKE);
        targetZonePaint.setStrokeCap(Paint.Cap.BUTT);
        targetZonePaint.setColor(Color.parseColor("#334FD1A5"));

        progressPaint.setStyle(Paint.Style.STROKE);
        progressPaint.setStrokeCap(Paint.Cap.ROUND);
        progressPaint.setColor(Color.parseColor("#F5B841"));

        needlePaint.setStyle(Paint.Style.STROKE);
        needlePaint.setStrokeCap(Paint.Cap.ROUND);
        needlePaint.setColor(Color.WHITE);

        textValuePaint.setColor(Color.parseColor("#EEF1FA"));
        textValuePaint.setTextAlign(Paint.Align.CENTER);
        textValuePaint.setFakeBoldText(true);

        textLabelPaint.setColor(Color.parseColor("#8E9AB8"));
        textLabelPaint.setTextAlign(Paint.Align.CENTER);

        textTargetPaint.setColor(Color.parseColor("#4FD1A5"));
        textTargetPaint.setTextAlign(Paint.Align.CENTER);
        textTargetPaint.setFakeBoldText(true);
    }

    public void setVoltage(float voltage) {
        if (voltage < MIN_V)
            voltage = MIN_V;
        if (voltage > MAX_V)
            voltage = MAX_V;

        this.currentVoltage = voltage;

        if (animator != null && animator.isRunning()) {
            animator.cancel();
        }

        animator = ValueAnimator.ofFloat(animatedVoltage, currentVoltage);
        animator.setDuration(180);
        animator.setInterpolator(new DecelerateInterpolator());
        animator.addUpdateListener(animation -> {
            animatedVoltage = (float) animation.getAnimatedValue();
            invalidate();
        });
        animator.start();
    }

    public float getVoltage() {
        return currentVoltage;
    }

    public boolean isInTargetZone() {
        return Math.abs(currentVoltage - TARGET_V) <= TOLERANCE_V;
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        int width = MeasureSpec.getSize(widthMeasureSpec);
        int height = MeasureSpec.getSize(heightMeasureSpec);
        int size = Math.min(width, height > 0 ? height : width);
        if (size <= 0)
            size = 300;
        setMeasuredDimension(size, size);
    }

    @Override
    protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);

        int width = getWidth();
        int height = getHeight();
        if (width == 0 || height == 0)
            return;

        float strokeWidth = width * 0.08f;
        trackPaint.setStrokeWidth(strokeWidth);
        targetZonePaint.setStrokeWidth(strokeWidth * 1.15f);
        progressPaint.setStrokeWidth(strokeWidth);
        needlePaint.setStrokeWidth(strokeWidth * 0.35f);

        float padding = strokeWidth + 12f;
        arcRect.set(padding, padding, width - padding, height - padding);

        // 1. Pista de fondo
        canvas.drawArc(arcRect, START_ANGLE, SWEEP_ANGLE, false, trackPaint);

        // 2. Zona objetivo (TARGET_V +- TOLERANCE_V)
        float targetMinFrac = (TARGET_V - TOLERANCE_V - MIN_V) / (MAX_V - MIN_V);
        float targetMaxFrac = (TARGET_V + TOLERANCE_V - MIN_V) / (MAX_V - MIN_V);
        float targetStartAngle = START_ANGLE + targetMinFrac * SWEEP_ANGLE;
        float targetSweepAngle = (targetMaxFrac - targetMinFrac) * SWEEP_ANGLE;
        canvas.drawArc(arcRect, targetStartAngle, targetSweepAngle, false, targetZonePaint);

        // 3. Pista de progreso actual
        float progressFrac = (animatedVoltage - MIN_V) / (MAX_V - MIN_V);
        float progressSweep = progressFrac * SWEEP_ANGLE;

        boolean inTarget = isInTargetZone();
        if (inTarget) {
            progressPaint.setColor(Color.parseColor("#4FD1A5")); // Verde exito
        } else if (animatedVoltage > TARGET_V + TOLERANCE_V) {
            progressPaint.setColor(Color.parseColor("#FF7A7A")); // Rojo alto
        } else {
            progressPaint.setColor(Color.parseColor("#F5B841")); // Ambar
        }

        if (progressSweep > 0.5f) {
            canvas.drawArc(arcRect, START_ANGLE, progressSweep, false, progressPaint);
        }

        // 4. Indicador del centro (Voltaje en grande)
        float cx = width / 2f;
        float cy = height / 2f;

        textValuePaint.setTextSize(width * 0.15f);
        String valStr = String.format(java.util.Locale.US, "%.2f V", animatedVoltage);
        canvas.drawText(valStr, cx, cy + (textValuePaint.getTextSize() * 0.25f), textValuePaint);

        // Subtexto indicador
        textLabelPaint.setTextSize(width * 0.055f);
        String statusText;
        if (inTarget) {
            textTargetPaint.setTextSize(width * 0.055f);
            canvas.drawText("¡Perfecto!", cx, cy + width * 0.16f, textTargetPaint);
        } else if (animatedVoltage < TARGET_V - TOLERANCE_V) {
            statusText = "Girá el potenciometro (+)";
            canvas.drawText(statusText, cx, cy + width * 0.16f, textLabelPaint);
        } else {
            statusText = "Girá el potenciometro (-)";
            canvas.drawText(statusText, cx, cy + width * 0.16f, textLabelPaint);
        }

        // Etiquetas min y max en los extremos
        textLabelPaint.setTextSize(width * 0.045f);
        canvas.drawText("0.0V", padding + strokeWidth, height - padding * 0.5f, textLabelPaint);
        canvas.drawText("3.3V", width - padding - strokeWidth, height - padding * 0.5f, textLabelPaint);
    }
}
