# Hardware y conexiones

El montaje es deliberadamente simple (el enunciado pide que la complejidad esté en el
software): **LED** simulan luces y aspersores, **interruptores** simulan sensores de
puertas y ventanas. No se conecta nada a la red eléctrica.

## 1. Lista de materiales

| Cant. | Componente | Uso |
|:----:|------------|-----|
| 1 | ESP32-S3-DevKitC-1 (N8, N16, N8R2, N16R8...) | Microcontrolador |
| 4 | LED amarillo/blanco 5 mm | Luces: entrada, oficina, parqueadero, bodega |
| 2 | LED azul 5 mm | Aspersores frontal y trasero |
| 1 | LED rojo 5 mm | Alerta (parpadea con alertas sin reconocer) |
| 1 | LED verde 5 mm | Estado de red |
| 8 | Resistencia 220 Ω – 330 Ω | Limitar corriente de cada LED |
| 5 | Interruptor deslizante (SPDT) o pulsador con enclavamiento | 3 puertas + 2 ventanas |
| 1 | Zumbador **activo** 3,3 V (opcional) | Pitido en alertas |
| 1 | Protoboard + cables dupont | Montaje |
| 1 | Cable USB-C con datos | Alimentación y programación |

> Opcional para mayor realismo: sensores magnéticos de puerta (reed switch, tipo MC-38).
> Se conectan exactamente igual que los interruptores.

## 2. Tabla de pines

Definida en [`firmware/AgenteESP32/src/config/Pins.h`](../firmware/AgenteESP32/src/config/Pins.h).

| GPIO | Dispositivo (id) | Tipo | Conexión |
|:----:|------------------|------|----------|
| **4** | `luz_entrada` | Salida | GPIO → 220 Ω → LED (ánodo, pata larga) → cátodo → GND |
| **5** | `luz_oficina` | Salida | ídem |
| **6** | `luz_parqueadero` | Salida | ídem |
| **7** | `luz_bodega` | Salida | ídem |
| **15** | `aspersor_frontal` | Salida | ídem (LED azul) |
| **16** | `aspersor_trasero` | Salida | ídem (LED azul) |
| **8** | `puerta_principal` | Entrada | GPIO → interruptor → GND (pull-up interno) |
| **9** | `puerta_bodega` | Entrada | ídem |
| **10** | `puerta_oficina` | Entrada | ídem |
| **11** | `ventana_oficina` | Entrada | ídem |
| **12** | `ventana_recepcion` | Entrada | ídem |
| **17** | LED de alerta | Salida | GPIO → 220 Ω → LED rojo → GND |
| **18** | Zumbador (opcional) | Salida | GPIO → (+) zumbador activo; (−) → GND |
| **21** | LED de estado | Salida | GPIO → 220 Ω → LED verde → GND |

### ¿Por qué estos pines?

En el ESP32-S3 se evitaron a propósito:

- **GPIO 0, 3, 45, 46** — pines de arranque (*strapping*): un interruptor ahí puede impedir que arranque.
- **GPIO 19, 20** — USB nativo (D−/D+).
- **GPIO 26–32** — conectados a la memoria flash.
- **GPIO 33–37** — ocupados por la PSRAM octal en módulos N8R8/N16R8.
- **GPIO 43, 44** — UART0 (consola serie).
- **GPIO 48 / 38** — LED RGB integrado según la revisión de la placa.

## 3. Lógica de las entradas (importante)

Cada entrada usa la **resistencia pull-up interna**; el interruptor conecta el pin a **GND**:

| Interruptor | Nivel en el GPIO | El sistema interpreta |
|-------------|:----------------:|-----------------------|
| Cerrado (contacto a GND) | LOW | **CERRADA** |
| Abierto (sin contacto) | HIGH | **ABIERTA** |

Así funciona un sensor magnético real: con la puerta cerrada el imán cierra el contacto.
Además es una **falla segura**: si un cable se suelta, el sistema ve la puerta *abierta*
y lo reporta, en lugar de suponer que está cerrada.

El firmware aplica **antirrebote por software** (muestreo cada 10 ms, cambio aceptado tras
50 ms estables), así que no se necesitan condensadores.

## 4. Diagrama de conexiones

```
                         ESP32-S3-DevKitC-1
                       ┌────────────────────┐
  LUZ ENTRADA   ◄──────┤ GPIO4         3V3  │
  LUZ OFICINA   ◄──────┤ GPIO5         GND  ├──────────┬──── riel GND de la protoboard
  LUZ PARQUEAD. ◄──────┤ GPIO6              │          │
  LUZ BODEGA    ◄──────┤ GPIO7              │          │
  ASPERSOR FRONT◄──────┤ GPIO15             │          │
  ASPERSOR TRAS.◄──────┤ GPIO16             │          │
  LED ALERTA    ◄──────┤ GPIO17             │          │
  ZUMBADOR (+)  ◄──────┤ GPIO18             │          │
  LED ESTADO    ◄──────┤ GPIO21             │          │
                       │                    │          │
  PUERTA PRINC. ───────┤ GPIO8              │          │
  PUERTA BODEGA ───────┤ GPIO9              │          │
  PUERTA OFIC.  ───────┤ GPIO10             │          │
  VENTANA OFIC. ───────┤ GPIO11             │          │
  VENTANA RECEP.───────┤ GPIO12             │          │
                       └────────────────────┘          │
                                                        │
  Cada salida:   GPIOx ──[ 220 Ω ]──►|── GND           │
                                 ánodo  cátodo          │
                                                        │
  Cada entrada:  GPIOy ──────o/ o──────────────────────┘
                         interruptor
                 (cerrado = CERRADA, abierto = ABIERTA)
```

Simulación lista para usar: [`firmware/diagram.json`](../firmware/diagram.json) (Wokwi),
con los 8 LED, 5 interruptores deslizantes y el zumbador ya cableados. Ver [SETUP.md](SETUP.md#5-simulación-en-wokwi-sin-hardware).

## 5. Consumo

Cada LED con 220 Ω a 3,3 V consume ~5–7 mA. Con las 8 salidas encendidas: < 60 mA,
dentro de lo que entrega el puerto USB y del límite por pin del ESP32-S3 (40 mA máx.,
20 mA recomendado).

## 6. Pasar a cargas reales (fuera del alcance)

Para controlar luces o electroválvulas reales **no** se conecta nada directamente al GPIO:
se usa un módulo de relés con optoacoplador o MOSFET, con fuente propia, y la instalación
de 110/220 V debe hacerla personal calificado. El firmware no cambia: un GPIO en ALTO
activa la salida.
