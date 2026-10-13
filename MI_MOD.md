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
- **Modos de objetivo, solo para pruebas** (`src/targetweights.*`, `src/targetmode.*`, `src/targetsmart.cpp`): por defecto, por tipo,
  inteligente, mas cercano, concentracion, concentracion por amenaza y orden de eliminacion (peligro / tiempo para matarlo). Ya no se eligen en el juego (se quito el selector Ctrl+F6/F7):
  se activan con la variable de entorno `WZ_TARGETMODE` (0 a 6) para los escenarios de prueba. En el juego, todas las unidades usan el criterio normal.
- **Modo de objetivo inteligente por tipo de arma** (`src/targetweights.cpp`, `src/targetsmart.cpp`): cada arma tiene su orden de prioridad
  (artilleria contra artilleria, anti-tanque guarda sus tiros para tanques, bunker buster a estructuras, etc.). Prueba: `--movementbench=assault`.
- **Medicion de si algo pasa por encima del modo de objetivo** (`tests/movebench/target_agreement.py`): en modo inteligente el 72 % de los disparos
  va al mismo objetivo que elige el modo, el 22 % a un muro que bloquea la linea de tiro (regla del juego, conservada a proposito) y el resto casi nunca peor.
- **Modos de objetivo "mas cercano" y "concentracion"**: el primero se queda con lo primero que tiene a tiro
  hasta destruirlo; el segundo pone a todo el grupo sobre un mismo objetivo, el mas cercano al centro del grupo. Prueba: `--movementbench=precision`
  y `tests/movebench/precision_runs.py`, que guarda cada disparo e impacto en `F:\wz-precision`.
- **Conclusion de las pruebas de objetivos**: ningun modo supera al criterio normal del juego en el escenario `precision`; la concentracion
  por amenaza lo empata (118,5 s contra 116,5 s, misma vida restante) y mata los bunkers antes, pero tarda mas con la torre Tank Killer.
- **Orden de eliminacion** (10 combates por modo en `precision`): empata con el criterio normal (+4,9 s y +142 de dano recibido, ambos dentro del
  margen de error); concentrar a todo el grupo en un solo blanco pierde la especializacion de cada arma, que el criterio normal si aprovecha.
