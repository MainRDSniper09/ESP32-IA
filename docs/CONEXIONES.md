# Conexiones (hardware)

El montaje es sencillo: **LED** para luces y aspersores, **interruptores** para puertas y
ventanas. No se conecta nada a 110/220 V.

## Materiales

| Cantidad | Componente | Para qué |
|:--:|------------|----------|
| 1 | ESP32-S3 DevKitC-1 (o similar) | El cerebro |
| 4 | LED amarillos o blancos | Luces: entrada, oficina, parqueadero, bodega |
| 2 | LED azules | Aspersores frontal y trasero |
| 1 | LED rojo | Alerta |
| 7 | Resistencias de 220 Ω o 330 Ω | Una por cada LED |
| 5 | Interruptores deslizantes (o pulsadores) | 3 puertas y 2 ventanas |
| 1 | Protoboard y cables | Para armar |
| 1 | Cable USB **de datos** | Programar y alimentar |

## Tabla de pines

| Pin (GPIO) | Dispositivo | Tipo |
|:--:|-------------|------|
| 4 | Luz de la entrada (`luz_entrada`) | Salida (LED) |
| 5 | Luz de la oficina (`luz_oficina`) | Salida (LED) |
| 6 | Luz del parqueadero (`luz_parqueadero`) | Salida (LED) |
| 7 | Luz de la bodega (`luz_bodega`) | Salida (LED) |
| 15 | Aspersor frontal (`aspersor_frontal`) | Salida (LED azul) |
| 16 | Aspersor trasero (`aspersor_trasero`) | Salida (LED azul) |
| 8 | Puerta principal (`puerta_principal`) | Entrada (interruptor) |
| 9 | Puerta de la bodega (`puerta_bodega`) | Entrada (interruptor) |
| 10 | Puerta de la oficina (`puerta_oficina`) | Entrada (interruptor) |
| 11 | Ventana de la oficina (`ventana_oficina`) | Entrada (interruptor) |
| 12 | Ventana de la recepción (`ventana_recepcion`) | Entrada (interruptor) |
| 17 | LED de alerta | Salida (LED rojo) |

Los pines se cambian en `AgenteESP32/config.h`. Se evitaron los pines que el ESP32-S3 usa
al arrancar (0, 3, 45, 46), los del USB (19, 20) y los de la memoria (26 a 37).

## Cómo conectar un LED (salidas)

```
 GPIO (ej. 4) ──── resistencia 220 Ω ────►|──── GND
                                     pata larga  pata corta
                                      (ánodo)    (cátodo)
```

Pin en ALTO (3,3 V) = LED encendido.

## Cómo conectar un interruptor (entradas)

```
 GPIO (ej. 8) ──── interruptor ──── GND
```

No hace falta resistencia: el ESP32 activa una resistencia interna (*pull-up*).

| Interruptor | El sistema muestra |
|-------------|--------------------|
| **Cerrado** (une el pin con GND) | Puerta **CERRADA** |
| **Abierto** | Puerta **ABIERTA** |

Así funcionan los sensores magnéticos de puerta reales, y además es seguro: si un cable
se suelta, el sistema muestra la puerta como *abierta* en lugar de ocultar el problema.

Con un interruptor deslizante de 3 patas: la pata del **centro** al GPIO y una de las
**extremas** a GND.

## Diagrama general

```
                    ESP32-S3
                 ┌────────────┐
  Luz entrada ◄──┤ 4          │
  Luz oficina ◄──┤ 5          │
  Luz parqueo ◄──┤ 6          │
  Luz bodega  ◄──┤ 7          │
  Aspersor F. ◄──┤ 15         │
  Aspersor T. ◄──┤ 16         │
  LED alerta  ◄──┤ 17         │
                 │            │
  Puerta princ ──┤ 8          │
  Puerta bodeg ──┤ 9          │
  Puerta ofic. ──┤ 10         │
  Ventana ofic ──┤ 11         │
  Ventana rec. ──┤ 12     GND ├──── a todos los cátodos de los LED
                 └────────────┘     y a la otra pata de los interruptores
```
