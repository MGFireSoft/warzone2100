# Artillería inteligente

Modo de selección de objetivos para las unidades del jugador local. Se activa y desactiva en la partida con
**Ctrl+F8** (la tecla se puede cambiar en Opciones → Teclas). Viene apagado.

## Qué hace

- **Todas las unidades** eligen como lo hace el juego normalmente (daño de su arma contra cada tipo de blanco,
  distancia, daño ya recibido por el blanco, visibilidad, línea de tiro, control de overkill) y además suman cuánto
  daño evitan por cada segundo que le disparan a un blanco:

  ```
  peligro del blanco × daño por segundo de mi arma contra él / vida que le queda
  ```

  El peligro es el daño por segundo que ese blanco le hace al bando propio, contando armadura, modificadores de
  propulsión y cuerpo, cadencia y probabilidad de acertar. Sumado sobre todas las unidades da "peligro / tiempo para
  matarlo", que es el orden que minimiza el daño total recibido (regla de Smith), pero cada unidad aporta donde su arma
  rinde más, así que los bunker busters siguen yendo a los búnkeres y las ametralladoras a los cyborgs.
- **La artillería propia** (morteros, obuses, artillería de cohetes) **y los tanques sensor** que le marcan blancos
  van primero contra los **sensores enemigos** (torres y tanques sensor de todo tipo), después contra la **artillería
  enemiga** (unidades y emplazamientos a la vista y en alcance), y después siguen con el criterio anterior. Sin
  sensores, la artillería enemiga no ve a quién disparar.

## Limitaciones

- **Solo en partidas sin red** (escaramuza contra la IA, campaña). La selección de objetivos es parte de la simulación,
  que tiene que ser idéntica en todas las máquinas; una opción que solo conoce un jugador la desincronizaría, así que
  en partidas en red el atajo no hace nada. Para ofrecerlo en multijugador habría que sincronizar la opción (como las
  órdenes secundarias) y pasar el cálculo, que hoy usa números con decimales, a aritmética entera.
- **No se guarda** con la partida ni entre sesiones: al abrir el juego vuelve a estar apagado.
- Las repeticiones (replays) de una partida jugada con el modo encendido no se reproducen igual, porque la repetición
  no sabe que estaba encendido.
- Probado en escenarios de laboratorio, no en partidas completas.

## Cómo se llegó a este modo

Todas las pruebas usan el banco de pruebas de movimiento del juego (`--movementbench`), que es determinista: el mismo
escenario con la misma semilla da siempre el mismo combate, así que dos modos se comparan en exactamente las mismas
condiciones. El bando defensor usa siempre el criterio normal del juego; el atacante, el modo que se prueba. Las
diferencias se dan "emparejadas" (combate contra el mismo combate con el criterio normal) con su margen de error.

| Ronda | Escenario | Modos probados | Resultado |
|---|---|---|---|
| 1 | `targeting`: 24 atacantes contra 12 unidades y 2 torres | 7 modos (por tipo, más cercano, más débil, estructuras primero, unidades primero, inteligente v1) | Ninguno distinto del normal; el enemigo era demasiado débil para que importara a quién se dispara |
| 2 | `targeting`: cyborgs adelante y tanques con cañón atrás | los mismos | Todos entre 49 y 52 s, dentro del ruido |
| 3 | `duel`: dos bandos iguales y variados | los mismos | "Por tipo" +6 puntos de vida restante (±2,5), el resto ruido |
| 4 | `duel` reequilibrado (30 contra 22 + 4 torres) | normal, por tipo, inteligente radical | Todos empatados: el +6 de la ronda 3 no se repitió |
| 5 | `assault`: 30 tanques cruzan una base | inteligente v1 (tabla de prioridades por tipo de arma) | Las armas eligen lo que deben (anti-tanque a tanques: 64 % → 84 %) pero el resultado empata |
| 6 | `assault2`: base más dura | inteligente v1 con "no disparar a través de muros" | Peor (−3 tanques). Apagando regla por regla se vio que la de los muros era la culpable: una unidad que no dispara al muro se queda quieta bajo fuego |
| 7 | `assault2`, disparo por disparo | inteligente v1 | Las unidades disparan al blanco que elige el modo el 92 % de las veces (sin contar muros): el juego no pasa por encima del modo |
| 8 | `precision`: 12 Python contra una base chica | más cercano, concentración, concentración por amenaza, orden de eliminación | Ninguno mejora al normal; concentrar a todos en un blanco pierde la especialización de cada arma |
| 9 | `precision`, 10 combates | **inteligente v2** (peso normal + peligro × mi daño / vida) | 15 % menos de daño recibido (−394 ± 271), sin los combates desastrosos del normal; prometedor pero no concluyente |
| 10 | `precision2`: artillería y sensores en ambos bandos, 10 combates | v2, v2 con prioridad a sensores, v2 con prioridad a artillería | **v2 sensores: 37 % menos de daño** (−1.658 ± 569), sensores enemigos caen a los 30 s en vez de 51 s y los Hellstorm enemigos hacen 67 % menos daño. v2 sola y v2 artillería empatan |
| 11 | `precision2` | **inteligente v3** (peso normal + sensores, sin el término de amenaza) | Peor que el normal (+976 ± 371): los bunker busters persiguen sensores, los búnkeres viven el doble y la tropa queda quieta bajo los Hellstorm |
| 12 | `precision2`, 5 combates | **inteligente v4** (v2 para todos, prioridad a sensores y artillería solo para la artillería propia) | **33 % menos de daño** (−1.442 ± 453), igual que v2 sensores (2.914 contra 2.734) con un comportamiento más simple |

Lo que enseñaron las pruebas:

1. El criterio normal del juego ya elige muy bien en el combate directo, porque valora el daño de cada arma contra
   cada tipo de blanco. Reordenar prioridades a mano no le gana.
2. Concentrar todo el fuego en un solo blanco empeora: cada arma rinde muy distinto contra cada blanco.
3. Lo que el juego no contempla es que un sensor no hace daño pero habilita a la artillería. Cegar la artillería
   enemiga vale más que destruirla.
4. Hace falta el término de amenaza para que esa prioridad no le quite atención al búnker que está disparando ahora; y
   basta con que la tenga la artillería propia.

## Cómo repetir las pruebas

Desde la carpeta del ejecutable compilado:

```
set WZ_TARGETMODE=12
set WZ_FIRELOG=1
warzone2100.exe --movementbench=precision2 --movementarrangement=0
```

`WZ_TARGETMODE` elige el modo del atacante (0 normal, 12 artillería inteligente; los demás números son los modos
descartados, ver `src/targetweights.h`) y `WZ_FIRELOG` agrega al registro cada disparo e impacto.
`tests/movebench/precision_runs.py precision2 "default,smart v4" 10` corre varias veces cada modo y escribe los CSV y
un resumen en `F:\wz-precision`.

## Dónde está el código

- `src/targetweights.h` y `.cpp`: los modos y las tablas puras, sin depender del juego (probadas por
  `tests/formation/targetweights_test.cpp`).
- `src/targetsmart.cpp`: lo que depende del juego: peligro de cada blanco, sensores, artillería, registros.
- `src/targetmode.cpp`: qué modo usa cada unidad y el atajo Ctrl+F8.
- `src/ai.cpp`: `targetAttackWeightIfGreaterThan()` suma el aporte del modo al peso normal de cada blanco.
- `data/mp/tests/movebench_precision2.js`: el escenario donde se midió.
