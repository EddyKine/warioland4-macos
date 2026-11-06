# Audio y reversión de latencia — 27 de octubre de 2026

## Estado entregado

Se conserva exactamente la app que el usuario confirmó fluida, con gráficos y
Machenike G5 Pro Bluetooth funcionando: `/Volumes/Extreme SSD/GBA/Wario Land 4.app`.
SHA-256 de su ejecutable: `671d524e42d3f543665640c95de79f222acb1aeda8d1090ba6a31ad40cc8f9a8`.
Este ejecutable NO se reemplazó durante la investigación de audio. Conserva
la recuperación del mando y el audio original con crepitaciones ocasionales.

La app separada de 100 ms sí fue probada por el usuario y rechazada por sensación
de retraso de audio y de acciones. Su propio log confirma `pre_roll_ms=100`,
2980 cuadros/50.023 s, un clock reset, cinco solicitudes con datos faltantes,
23244 bytes adicionales demandados y cero errores de API. Se preservó completa
en `build/macos-audio-fix/rejected-latency.app`, fuera de los accesos principales.
No se cerró ningún proceso ni se reemplazó una app en ejecución; inventario y
lista de procesos mostraron que ambas estaban cerradas antes de mover la prueba.

Ambas partidas se mantienen en sus rutas originales:
`GBA/saves/wl4.sav` y `GBA/saves/wl4-audio-test.sav`.
La partida habitual conserva el hash anterior a esta investigación:
`453047505cd8c4a158bd33188fd10b866a1b02da485697a7c198a66b881c13a7`.
También se preservó una copia de la partida de la prueba bajo build/.

## Diagnóstico comprobado y límites

La sesión anterior tuvo 9911 cuadros en 166.208 s (~59.63 fps), dos reinicios
acotados del reloj, ocho rellenos de silencio y ningún descarte por cola excesiva.
El código original inserta ceros al caer la cola por debajo de un umbral incluso
cuando aún contiene música. Un caso de 1000 cuadros en cola provoca 3369 cuadros
adicionales de silencio (~51.4 ms), con discontinuidades posibles en los bordes.
Esto es una vía de generación de chasquidos; no prueba que explique cada crepitación.

La prueba inicial de 100 ms eliminó rellenos preventivos y suavizó el ajuste de
frecuencia. En una reproducción GUI se registraron faltas de datos adicionales,
por lo que aumentar el búfer no resolvió toda la entrega de audio. SDL define
additional_amount como una estimación de bytes de entrada que faltan al atender
una solicitud; no es por sí solo una medición de cada underrun audible del hardware.
No se atribuye el defecto al mando, parlantes, Bluetooth o compatibilidad del Mac.

La precarga añadía nominalmente ~33 ms respecto al objetivo anterior de ~67 ms.
El código de lectura de botones y PortFramePacerDue no cambiaron en esa prueba.
Sus fps medios (~59.57) no prueban latencia de control ni descartan la percepción
reportada: no se midió botón-a-fotón con equipo externo. La regresión de respuesta
que reportó el usuario se respeta volviendo a su binario previo, no negándola por fps.

## Alternativa acotada, no activada

Se ensayó en código una política con el objetivo antiguo de ~67 ms, sin ceros
preventivos y con rampas cortas únicamente ante falta real de datos: 64 muestras
al bajar a silencio y 128 al volver a la señal. No cambia el generador de sonido
GBA. La prueba de señal reduce un salto simulado de 12000 a un máximo de 188
unidades PCM entre muestras y conserva la longitud de la demanda atendida.
Esto demuestra continuidad de esa señal de prueba; no demuestra calidad audible,
ni ausencia de huecos en todas las transiciones, ni latencia equivalente del juego.
No se desplegó ni se dejó activa, conforme a la prioridad de respuesta del usuario.

`WL4_DARWIN_AUDIO_EXPERIMENT` queda OFF por defecto y explícitamente OFF en build.sh.
El generador normal vuelve a la cola original. package.py rechaza empaquetar una
build experimental como la app normal, o una build normal como prueba de audio.
Los fuentes del ensayo se conservan reversibles, separados del flujo predeterminado.

## Verificaciones

- La app normal sigue byte a byte con el SHA indicado y firma ad-hoc estricta válida.
- Corutina/pila y pruebas I/O del paquete normal pasan.
- Pruebas de teclado/mando virtual y pacing sintético pasan.
- Recuperación de mando con eventos perdidos pasa; el harness excluye mandos físicos
  para que un Xbox real presente no sea seleccionado en lugar del mando virtual.
- La captura WAV de 300 cuadros es byte idéntica a la previa: SHA-256
  `6f893497533708e6d2d48bbcfcb865a3f0728391cf7eddff9110547c8a05d555`.
- Prueba de continuidad/rampas aislada pasa. Una compilación intermedia de esa prueba
  falló por faltar stdlib.h; la salida antigua de ese ejecutable no se usó como
  evidencia de la ampliación. Se corrigió el include y se recompiló antes del PASS
  que incluye el resultado de 188 unidades.
- Fuentes compartidas src/, include/, port/generated/ no tienen diff.

Evidencia: build/macos-audio-fix/. La prioridad final es conservar la respuesta
que el usuario aprobó. No hay arreglo de audio validado con escucha real; las
crepitaciones ocasionales quedan pendientes y se evita prolongar el ensayo.
