# Investigación de entrada Bluetooth — 27 de octubre de 2026

El usuario reporta Machenike G5 Pro por Bluetooth, reconocido como Xbox.
Al entrar a la caverna de la primera etapa, la imagen y música continuaron pero
Wario no se movía. No sabemos si el teclado respondía. El registro inicial solo
contiene apertura de SDL y cambios de pantalla completa; no establece la causa.
macOS muestra un Xbox Wireless Controller Bluetooth mediante hidutil; el harness
SDL ejecutado desde el shell no enumeró mandos físicos. No se afirma prueba del
Machenike real por el agente.

## Fallo reproducido y corrección

La implementación anterior solo recuperaba conexión mediante eventos SDL. Si
se perdían los eventos y quedaba un SDL_Gamepad abierto pero desconectado,
OpenFirst devolvía inmediatamente por tener una referencia no nula. Los botones
quedaban permanentemente en cero aunque existiera un nuevo dispositivo conectado.

`port/macos/controls.c` comprueba SDL_GamepadConnected al leer entrada, libera
referencias desconectadas y vuelve a enumerar mandos. Se mantiene el filtro de
foco para no controlar la partida al trabajar en otras ventanas; el hint SDL de
actualizaciones en segundo plano mantiene fresca la conexión y el estado del
mando, pero no entrega controles al juego mientras la ventana carezca de foco.
No se cambió el código compartido GBA ni src/controls.c.

La corrección cubre una vía que produce el síntoma. Sin datos del fallo original,
no se puede concluir que sea la única causa ni que la incidencia física ya esté
resuelta.

## Verificación

- `wl4-controller-recovery-test`: referencia desconectada, eventos eliminados,
  nueva conexión y botón B. Falla antes; pasa después. Sin ROM ni partida.
- `wl4-input-test`: ajustes/teclado/mando virtual PASS con configuración nueva.
- Ruta de habitación usando un mando virtual SDL real, no inyección directa en
  KEYINPUT: la misma secuencia del proyecto conduce al primer cambio 0 -> 11.
  Se desconecta en step 8070, reconecta en 8071 y se suprimen ambos eventos.
- Antes: 229 discrepancias; botones deseados RIGHT pero juego recibe cero.
- Después: 8800 cuadros, exit 0, cero discrepancias; room 11 en step 8094,
  submodo activo 2 en 8104, botones llegan a gButtonsHeld, Wario cambia de
  posición y responde al salto y direcciones posteriores. No cubre todas las salas.
- Paquete actualizado: firma local estricta PASS, corutinas e I/O PASS.

Evidencia preservada en `build/macos-input-fix/`: registros before/after,
secuencia room11-movement.keys y captura after-fix-virtual.png. Copia del paquete
anterior en previous-app.app. No se modificó ni probó sobre la partida real.
Hash de la partida del usuario antes/después:
`2476ee2e3f5e7fcb2f1d20ead2cdb18a6868d27cf791029da1e5ac0ae9bf50ee`.

## Diagnóstico de la siguiente prueba

App actual: `/Volumes/Extreme SSD/GBA/Wario Land 4.app`.
No se dejó ninguna instancia de prueba gráfica abierta. El usuario puede abrirla
con doble clic, cargar su partida y repetir la entrada a la caverna.

`wl4-last-run.log` registra nombre/id/conexión, eventos de conexión y foco,
botones físicos/teclado agregados (raw), botones enviados (sent), botones que
observa el juego (gba), modo/submodo, sala, pose/reacción, posición, velocidad,
temporizador de pausa, desactivación y modo demo. Registra cada transición y
cada 120 cuadros. Los registros anteriores se archivan automáticamente en
`GBA/logs/`; las partidas siguen en `GBA/saves/`.

Si recurre, probar flechas del teclado y hacer clic en la ventana, anotando si
alguna acción recupera el movimiento; después cerrar normalmente si es posible.
Raw=0 con mando desconectado apunta a dispositivo; raw distinto de cero pero
sent=0/focus=0 apunta a foco; sent y gba activos con Wario quieto apunta a estado
personaje/colisión/transición. Esta separación requiere revisar los valores
concretos, no inferirla solo del movimiento del fondo.

## Segundo fallo: crepitaciones de audio

El usuario aclara que siempre hubo crepitaciones pequeñas, aunque la música en
general se oye bien. Compatibilidad es una hipótesis del usuario, no una causa
establecida. No se cambió el mezclador ni se afirma corregido. Se añadieron
contadores periódicos queued_bytes, refills, drops y clock_resets y un resumen
de timing al salir. Permitirán comprobar underruns y pausas antes de ajustar
búfer o frecuencia. Los registros del arranque anterior no contenían esos datos.
