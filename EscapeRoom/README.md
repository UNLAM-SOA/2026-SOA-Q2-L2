# EscapeRoom (app Android, Java + XML)

## Instalación
1. En Android Studio: New Project > "Empty Views Activity", lenguaje Java, package `com.example.escaperoom`.
   (Si usan otro package, cambien la línea `package` de los .java.)
2. Copiar `java/` y `res/` dentro de `app/src/main/` y reemplazar `AndroidManifest.xml`.
3. Dependencia necesaria: `androidx.appcompat:appcompat` (viene en la plantilla).

## Flujo
- Desafíos 1 y 2: explicación -> Comenzar -> envía la señal al ESP32 -> "Esperando" -> ganó / falló.
- Desafío 3: explicación -> Comenzar -> minijuego en la app (TapGame) -> ganó / falló.

## ESP32
La comunicación está aislada en la interfaz `EspClient`. Hoy se usa `FakeEspClient`, que simula
la respuesta a los 3 s y alterna ganar/fallar. Para conectar el hardware real, crear una clase
que implemente `EspClient` (BLE o Wi-Fi) y cambiarla en `ChallengeActivity` (línea con el TODO).

Protocolo propuesto: App -> ESP32 `START:1` / `START:2`; ESP32 -> App `RESULT:OK` / `RESULT:FAIL`.

Created using Anthropic Claude. Mantener como versión interna hasta que una persona la revise y verifique.
