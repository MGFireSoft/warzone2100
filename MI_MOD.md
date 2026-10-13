# Mi mod de Warzone 2100

Fork personal de [Warzone2100/warzone2100](https://github.com/Warzone2100/warzone2100).

Los cambios propios se desarrollan en la rama `mi-mod`.

## Cambios hechos

- **Colisiones / desbloqueo de unidades** (`src/move.cpp`): cuando una unidad en movimiento choca con aliadas quietas,
  ahora todas las que estorban reciben el pedido de correrse (no solo la primera). Si una unidad no tiene lado libre,
  le pide a las vecinas de los costados y de adelante que hagan lugar, en cadena (hasta 5 de profundidad).
- **Escenario de prueba** `packed` en el banco de movimiento: `warzone2100 --movementbench=packed --movementarrangement=N`.
  Un bloque de 5x5 tanques pesados quietos con uno ordenado desde el medio. Sin los cambios: 0 de 9 variantes llegan;
  con los cambios: 6 de 9. En el juego, con un bloque de 7x7 el tanque del medio sale en ~45 s (antes no salía).

- **Formaciones por grupo** (`src/formationlayout.*`, `src/formationorder.*`): formas (cuadrado, circulo, linea de 2/3, columna de 1/2/3),
  orden por alcance o resistencia, separacion cerrada/normal/abierta y opcion de moverse a la velocidad del mas lento.
  Se manejan con Ctrl+F1 (forma), Ctrl+F2 (orden), Ctrl+F3 (separacion), Ctrl+F5 (velocidad), Ctrl+F4 (apagar) sobre un grupo Ctrl+numero.
- **Las unidades que se corren vuelven a su lugar** una vez que pasa la que las empujo (`moveReturnHome` en `src/move.cpp`).
- **Evitacion de obstaculos**: el tanque mantiene el lado elegido para esquivar en vez de cambiarlo en cada tick (`collision_avoidance_behavior.cpp`).
- **Modos de objetivo por grupo** (`src/targetweights.*`, `src/targetmode.*`): por defecto, por tipo e inteligente.
  Ctrl+F6 siguiente modo, Ctrl+F7 vuelve al por defecto, sobre un grupo Ctrl+numero.
- **Modo de objetivo inteligente por tipo de arma** (`src/targetweights.cpp`, `src/targetsmart.cpp`): cada arma tiene su orden de prioridad
  (artilleria contra artilleria, anti-tanque guarda sus tiros para tanques, bunker buster a estructuras, etc.). Prueba: `--movementbench=assault`.
- **Medicion de si algo pasa por encima del modo de objetivo** (`tests/movebench/target_agreement.py`): en modo inteligente el 72 % de los disparos
  va al mismo objetivo que elige el modo, el 22 % a un muro que bloquea la linea de tiro (regla del juego, conservada a proposito) y el resto casi nunca peor.
- **Modos de objetivo "mas cercano" y "concentracion"** (Ctrl+F6 los recorre junto con los otros): el primero se queda con lo primero que tiene a tiro
  hasta destruirlo; el segundo pone a todo el grupo sobre un mismo objetivo, el mas cercano al centro del grupo. Prueba: `--movementbench=precision`
  y `tests/movebench/precision_runs.py`, que guarda cada disparo e impacto en `F:\wz-precision`.
