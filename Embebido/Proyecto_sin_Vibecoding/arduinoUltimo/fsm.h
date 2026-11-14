// ============================================================================
// fsm.h — MÁQUINA DE ESTADOS (único lugar de decisión y lógica del juego).
// Patrón de switch anidados (el de la cátedra): fsm() hace el switch por
// estado y, adentro de cada estado, el switch por evento. En cada case: la
// acción y el estado siguiente. La FSM no lee sensores ni genera eventos:
// los recibe por cola_eventos.
// ============================================================================
#ifndef FSM_H
#define FSM_H

#include "tipos.h"
#include "eventos.h"
#include "actuadores.h"
#include "comunicacion.h"

// ============================================================================
// VARIABLES DEL JUEGO (sólo las usa tarea_fsm)
// ============================================================================
estado_fsm_t estado_actual;
int vidas = MAX_VIDAS;
bool desafio_resuelto[TOTAL_DESAFIOS + 1];      // Índices 1..3
int total_resueltos = 0;

// Nombres para los logs, en el mismo orden que los enum (tipos.h)
const char *const NOMBRE_ESTADO[CANT_ESTADOS] = {
    "ESPERANDO_DESAFIO", "D1_ESPERANDO_APLAUSO", "D2_ESPERANDO_CAMBIO_POT",
    "D3_ESPERANDO_CODIGO", "DESAFIO_FINALIZADO", "PENALIZACION_BLOQUEADO",
    "CAJA_DESBLOQUEADA", "RESETEANDO"
};
const char *const NOMBRE_EVENTO[CANT_EVENTOS] = {
    "CONTINUE", "DESAFIO_1", "DESAFIO_2", "DESAFIO_3", "APLAUSO_DETECTADO",
    "D1_CANT_APLAUSOS_ALCANZADA", "VALOR_POT_CAMBIO", "D2_VALOR_POT_CORRECTO",
    "D3_CODIGO_OK", "D3_CODIGO_ERROR", "TIMEOUT_DESAFIO", "CON_VIDAS", "SIN_VIDAS",
    "DESAFIOS_COMPLETADOS", "TIMEOUT_FIN_PENALIZACION", "BTN_RESET", "FIN_RESET"
};

// ============================================================================
// FUNCIONES DE CONTROL DE JUEGO (acciones de las transiciones)
// ============================================================================
void registrar_acierto_desafio(int num) {
    if (!desafio_resuelto[num]) {
        desafio_resuelto[num] = true;
        total_resueltos++;
        act_led_desafio_completado(num, true);
    }
}

void registrar_fallo_vida() {
    if (vidas > 0) vidas--;
}

// Qué pasa cuando termine de mostrarse el resultado.
tipo_evento_t evento_tras_resultado() {
    if (total_resueltos >= TOTAL_DESAFIOS) return EVENTO_DESAFIOS_COMPLETADOS;
    if (vidas == 0) return EVENTO_SIN_VIDAS;
    return EVENTO_CON_VIDAS;
}

void finalizar_desafio(int num, bool exito, const char *msg) {
    detener_timeout();
    enviar_resultado_mqtt(num, exito);
    if (exito) {
        registrar_acierto_desafio(num);
        act_buzzer_acierto();
    } else {
        registrar_fallo_vida();
        enviar_vidas_mqtt(vidas);
        act_led_amarillo_apagar();
        act_buzzer_error();
    }
    act_lcd_resultado(exito, msg);
    programar_evento(evento_tras_resultado(), TIEMPO_MOSTRAR_RESULTADO);
    LOG_DEBUG(">>> Desafio %d: %s. Vidas: %d\n", num, exito ? "SUPERADO" : "FALLADO", vidas);
}

// Al entrar a un desafío: consigna en pantalla y arranca su tiempo límite.
void iniciar_desafio(unsigned long tiempo_limite) {
    act_led_amarillo_apagar();
    act_buzzer_beep();
    iniciar_timeout(tiempo_limite);
}

void iniciar_d1() {
    act_lcd_d1_intro();
    iniciar_desafio(TIEMPO_LIMITE_DESAFIO);
}

void iniciar_d2() {
    act_lcd_d2_intro();
    iniciar_desafio(TIEMPO_LIMITE_DESAFIO);
}

void iniciar_d3() {
    act_lcd_d3_intro();
    iniciar_desafio(TIEMPO_LIMITE_D3);
}

// El primer aplauso abre la ventana de 5 s para completar el ritmo.
void contar_aplauso(int numero_aplauso) {
    act_buzzer_beep();
    act_led_amarillo_progreso(numero_aplauso, D1_APLAUSOS_OBJETIVO);
    if (numero_aplauso == D1_PRIMER_APLAUSO) {
        iniciar_timeout(TIEMPO_VENTANA_D1);
    }
}

void volver_al_menu() {
    act_led_amarillo_apagar();
    act_lcd_menu_principal(vidas);
    act_buzzer_beep();
    enviar_vidas_mqtt(vidas);
}

void iniciar_penalizacion() {
    programar_evento(EVENTO_TIMEOUT_FIN_PENALIZACION, TIEMPO_PENALIZACION);
    act_lcd_penalizacion(TIEMPO_PENALIZACION / MS_POR_SEGUNDO);
    enviar_vidas_mqtt(0);
    enviar_a_mqtt(TOPICO_TX_ESTADO, "BLOQUEADO");
}

void restaurar_vidas() {
    vidas = MAX_VIDAS;
    enviar_a_mqtt(TOPICO_TX_ESTADO, "NORMAL");
    volver_al_menu();
}

void abrir_caja() {
    act_led_amarillo_apagar();
    act_lcd_victoria();
    act_servo_abrir();
    act_buzzer_acierto();
    enviar_a_mqtt(TOPICO_TX_ESTADO, "VICTORIA");
}

void iniciar_reseteo() {
    act_lcd_reseteando();
    act_buzzer_beep();
    programar_evento(EVENTO_FIN_RESET, TIEMPO_RESETEANDO);
}

void reiniciar_juego() {
    vidas = MAX_VIDAS;
    total_resueltos = 0;
    for (int i = 1; i <= TOTAL_DESAFIOS; i++) {
        desafio_resuelto[i] = false;
        act_led_desafio_completado(i, false);
    }
    act_servo_cerrar();
    detener_timers();
    volver_al_menu();
}

// ============================================================================
// fsm(): DOBLE SWITCH. El de afuera pregunta en qué estado está la máquina;
// el de adentro, qué evento llegó. En cada case: la acción y el estado
// siguiente. Los eventos que un estado no espera caen en default y se ignoran.
// ============================================================================
void fsm(const evento_t &evento) {
    switch (estado_actual) {
        case ESTADO_ESPERANDO_DESAFIO:
            switch (evento.tipo) {
                case EVENTO_DESAFIO_1:
                    if (!desafio_resuelto[1]) {         // Un desafío superado no se repite
                        iniciar_d1();
                        estado_actual = ESTADO_D1_ESPERANDO_APLAUSO;
                    }
                    break;
                case EVENTO_DESAFIO_2:
                    if (!desafio_resuelto[2]) {
                        iniciar_d2();
                        estado_actual = ESTADO_D2_ESPERANDO_CAMBIO_POT;
                    }
                    break;
                case EVENTO_DESAFIO_3:
                    if (!desafio_resuelto[3]) {
                        iniciar_d3();
                        estado_actual = ESTADO_D3_ESPERANDO_CODIGO;
                    }
                    break;
                case EVENTO_BTN_RESET:
                    iniciar_reseteo();
                    estado_actual = ESTADO_RESETEANDO;
                    break;
                default:
                    break;
            }
            break;

        case ESTADO_D1_ESPERANDO_APLAUSO:
            switch (evento.tipo) {
                case EVENTO_APLAUSO_DETECTADO:
                    contar_aplauso(evento.valor_aux);
                    estado_actual = ESTADO_D1_ESPERANDO_APLAUSO;
                    break;
                case EVENTO_D1_CANT_APLAUSOS_ALCANZADA:
                    act_led_amarillo_progreso(D1_APLAUSOS_OBJETIVO, D1_APLAUSOS_OBJETIVO);
                    finalizar_desafio(1, true, "RITMO LOGRADO");
                    estado_actual = ESTADO_DESAFIO_FINALIZADO;
                    break;
                case EVENTO_TIMEOUT_DESAFIO:
                    finalizar_desafio(1, false, "TIEMPO AGOTADO");
                    estado_actual = ESTADO_DESAFIO_FINALIZADO;
                    break;
                default:
                    break;
            }
            break;

        case ESTADO_D2_ESPERANDO_CAMBIO_POT:
            switch (evento.tipo) {
                case EVENTO_VALOR_POT_CAMBIO:
                    enviar_tension_mqtt(evento.valor_aux);
                    estado_actual = ESTADO_D2_ESPERANDO_CAMBIO_POT;
                    break;
                case EVENTO_D2_VALOR_POT_CORRECTO:
                    finalizar_desafio(2, true, "VALOR EXACTO");
                    estado_actual = ESTADO_DESAFIO_FINALIZADO;
                    break;
                case EVENTO_TIMEOUT_DESAFIO:
                    finalizar_desafio(2, false, "TIEMPO AGOTADO");
                    estado_actual = ESTADO_DESAFIO_FINALIZADO;
                    break;
                default:
                    break;
            }
            break;

        case ESTADO_D3_ESPERANDO_CODIGO:
            switch (evento.tipo) {
                case EVENTO_D3_CODIGO_OK:
                    finalizar_desafio(3, true, "JUEGO SUPERADO");
                    estado_actual = ESTADO_DESAFIO_FINALIZADO;
                    break;
                case EVENTO_D3_CODIGO_ERROR:
                    finalizar_desafio(3, false, "CODIGO ERRONEO");
                    estado_actual = ESTADO_DESAFIO_FINALIZADO;
                    break;
                case EVENTO_TIMEOUT_DESAFIO:
                    finalizar_desafio(3, false, "TIEMPO AGOTADO");
                    estado_actual = ESTADO_DESAFIO_FINALIZADO;
                    break;
                default:
                    break;
            }
            break;

        case ESTADO_DESAFIO_FINALIZADO:
            switch (evento.tipo) {
                case EVENTO_CON_VIDAS:
                    volver_al_menu();
                    estado_actual = ESTADO_ESPERANDO_DESAFIO;
                    break;
                case EVENTO_SIN_VIDAS:
                    iniciar_penalizacion();
                    estado_actual = ESTADO_PENALIZACION_BLOQUEADO;
                    break;
                case EVENTO_DESAFIOS_COMPLETADOS:
                    abrir_caja();
                    estado_actual = ESTADO_CAJA_DESBLOQUEADA;
                    break;
                default:
                    break;
            }
            break;

        case ESTADO_PENALIZACION_BLOQUEADO:             // Ignora todo lo demás (entradas inhabilitadas)
            switch (evento.tipo) {
                case EVENTO_TIMEOUT_FIN_PENALIZACION:
                    restaurar_vidas();
                    estado_actual = ESTADO_ESPERANDO_DESAFIO;
                    break;
                default:
                    break;
            }
            break;

        case ESTADO_CAJA_DESBLOQUEADA:
            switch (evento.tipo) {
                case EVENTO_BTN_RESET:
                    iniciar_reseteo();
                    estado_actual = ESTADO_RESETEANDO;
                    break;
                default:
                    break;
            }
            break;

        case ESTADO_RESETEANDO:
            switch (evento.tipo) {
                case EVENTO_FIN_RESET:
                    reiniciar_juego();
                    estado_actual = ESTADO_ESPERANDO_DESAFIO;
                    break;
                default:
                    break;
            }
            break;

        default:
            break;
    }
}

// ============================================================================
// tarea_fsm: se bloquea (portMAX_DELAY) hasta que llega un evento, así no
// gasta CPU mientras no pasa nada. Reemplaza al loop() con delay(10) del
// original. Antes y después de fsm() escribe los logs de depuración.
// ============================================================================
void tarea_fsm(void *parameter) {
    evento_t evento;
    while (1) {
        if (xQueueReceive(cola_eventos, &evento, portMAX_DELAY) != pdTRUE ||
            estado_actual >= CANT_ESTADOS || evento.tipo >= CANT_EVENTOS) {
            continue;                                   // Fuera de rango: se descarta
        }
        estado_fsm_t anterior = estado_actual;
        if (evento.tipo != EVENTO_VALOR_POT_CAMBIO) {   // Llega muy seguido: no se loguea
            LOG_DEBUG("[FSM] Estado: %s | Evento: %s\n", NOMBRE_ESTADO[estado_actual], NOMBRE_EVENTO[evento.tipo]);
        }
        fsm(evento);
        if (estado_actual != anterior) {
            LOG_DEBUG("[FSM] Transicion: %s -> %s\n", NOMBRE_ESTADO[anterior], NOMBRE_ESTADO[estado_actual]);
        }
    }
}

#endif
