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
