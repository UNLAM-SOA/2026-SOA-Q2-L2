# EscapeRoom

App Android que acompaña a un escape room físico armado con un ESP32. El jugador
tiene tres desafíos: dos se resuelven interactuando con el hardware (el ESP32
confirma cuándo están completos) y el tercero es un minijuego que corre dentro del
teléfono.

Está hecha con Views clásicas (XML + `AppCompatActivity`), sin Jetpack Compose.

## Desafíos

**Desafío 1 y 2 — con el ESP32.**
Al tocar *Comenzar*, la app le avisa al ESP32 que arranque el reto y se queda
esperando la respuesta, que indica si el jugador lo resolvió. Toda esa
comunicación pasa por la interfaz `EspClient`:

- App → ESP32: `START:1` / `START:2`
- ESP32 → App: `RESULT:OK` / `RESULT:FAIL`

Por ahora la app trae `FakeEspClient`, una implementación de prueba que simula al
ESP32 (responde después de unos segundos) para poder desarrollar sin tener el
hardware conectado. Para usar el ESP32 real hay que escribir otra implementación
de `EspClient` sobre Wi-Fi o Bluetooth y enchufarla en `ChallengeActivity`.

**Desafío 3 — gestión de memoria (local).**
Un minijuego que simula el manejo de un *heap*. En cada paso el sistema pide
PRODUCIR (reservar un bloque) o CONSUMIR (liberar uno), y el jugador responde con
el botón correcto. Hay que completar 15 operaciones antes de que se acaben los 30
segundos, sin desbordar la capacidad (8 bloques) ni consumir con la memoria vacía.

Al ganar se revela un código de 4 caracteres. El jugador lo escribe y, para
confirmarlo, **sacude el teléfono**: el acelerómetro detecta la sacudida y recién
ahí se valida el código. Si no coincide, puede corregir y volver a sacudir.

## Requisitos

- Android Studio reciente (el proyecto usa AGP 9 / Gradle 9.1).
- JDK 17.
- Un emulador o dispositivo con Android 7.0 (API 24) o superior.

El acelerómetro es opcional: está declarado como `required="false"`, así que en un
equipo sin ese sensor el código del desafío 3 se valida al terminar de escribirlo.

## Cómo ejecutarlo

1. Abrir la carpeta del proyecto en Android Studio.
2. Dejar que sincronice Gradle.
3. Elegir un emulador o conectar un teléfono.
4. Run (Shift+F10).

## Estructura

    app/src/main/java/com/example/escaperoom/
      MainActivity.java       menú con los tres desafíos
      ChallengeActivity.java  flujo de cada desafío (intro -> espera/juego -> resultado)
      EspClient.java          interfaz de comunicación con el ESP32
      FakeEspClient.java      simulación del ESP32 para probar sin hardware
      MemoryGame.java         lógica del minijuego de memoria (desafío 3)
      ShakeDetector.java      detección de sacudidas con el acelerómetro

Los parámetros del minijuego (capacidad, cantidad de operaciones, tiempo) están
como constantes al principio de `MemoryGame`, y la sensibilidad de la sacudida en
`ShakeDetector` (`SHAKE_THRESHOLD_G`).

## Pendiente

- Implementar el cliente real del ESP32 (Wi-Fi o BLE) reemplazando `FakeEspClient`.
- Definir el contenido concreto de los desafíos 1 y 2.

## Autores

- (completar)