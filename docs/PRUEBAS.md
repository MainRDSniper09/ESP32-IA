# Pruebas del asistente

Lista de pruebas para comprobar que el asistente interpreta bien las órdenes y controla
los LED. Las columnas **«Esperado»** son los porcentajes que produce el intérprete del
código (secciones 2 a 5), calculados con estas mismas frases.

> Escribe cada frase en el **Monitor Serie** o en la página web (así el resultado no depende
> del reconocimiento de voz). Después repite algunas **hablando** con el micrófono y anota qué
> texto captó Chrome. Completa la columna **Resultado** (✅ / ❌ y lo observado).

**Antes de empezar:** escribe `prende todas las luces` para dejar todo encendido.

## 1. Órdenes claras

| # | Orden | Esperado | Resultado |
|---|-------|----------|-----------|
| 1.1 | `Apaga la luz de la cocina` | Apagar 100 %, Cocina 100 % → se apaga GPIO 7 · verde · confianza 100 % | |
| 1.2 | `Enciende la oficina y la recepción` | Encender 100 %, Oficina 100 %, Recepción 100 % → encienden GPIO 4 y 5 | |
| 1.3 | `oye asistente, prende la sala de juntas por favor` | Encender 100 %, Sala de juntas 100 % → GPIO 6 | |
| 1.4 | `enciende la luz del baño` | Encender 100 %, Baño 100 % → GPIO 15 | |

## 2. Todas a la vez

| # | Orden | Esperado | Resultado |
|---|-------|----------|-----------|
| 2.1 | `prende todas las luces` | Encender 100 %, Todas 100 % → 6 LED encendidos | |
| 2.2 | `apaga las luces` | Apagar 100 %, sin lugar pero dijo «luces» → 6 LED apagados | |

## 3. Errores del reconocimiento de voz (palabras parecidas)

| # | Orden | Esperado | Resultado |
|---|-------|----------|-----------|
| 3.1 | `apaga la cosina` | Cocina **83 %** (cosina ≈ cocina) → apaga Cocina · confianza 91 % | |
| 3.2 | `apaga la vodega` | Bodega **83 %** (vodega ≈ bodega) → apaga Bodega · confianza 91 % | |
| 3.3 | `ensiende el despacho` | Encender **88 %** (ensiende ≈ enciende), Oficina 100 % → enciende Oficina · 94 % | |

## 4. Sinónimos

| # | Orden | Esperado | Resultado |
|---|-------|----------|-----------|
| 4.1 | `desactiva el almacén` | Apagar 100 % (desactiva), Bodega 100 % (almacén) → apaga Bodega | |
| 4.2 | `prende el comedor` | Encender 100 %, Cocina 100 % (comedor) → enciende Cocina | |

## 5. Órdenes incompletas o sin sentido (no debe cambiar ningún LED)

| # | Orden | Esperado | Resultado |
|---|-------|----------|-----------|
| 5.1 | `enciende la luz` | Encender 100 %, ningún lugar ≥ 70 % → **rojo** · «no en qué lugar» · confianza 50 % | |
| 5.2 | `la cocina` | Cocina 100 %, ninguna acción ≥ 70 % → **rojo** · «no si quieres encender o apagar» | |
| 5.3 | `hola como estas` | Nada ≥ 70 % → **rojo** · «No entendí la orden» · confianza 20 % | |

## 6. Voz real

| # | Qué hacer | Esperado | Resultado (texto que captó Chrome) |
|---|-----------|----------|------------------------------------|
| 6.1 | Pulsar 🎤 y decir «apaga la luz de la cocina» | Se apaga Cocina | |
| 6.2 | Pulsar 🎤 y decir «prende todas las luces» | Se encienden todas | |
| 6.3 | Pulsar 🎤 y hablar rápido o con ruido de fondo | Revisar el % de parecido aunque haya errores | |

## 7. Interfaces

| # | Qué hacer | Esperado | Resultado |
|---|-----------|----------|-----------|
| 7.1 | Tocar «Cocina» en el plano de la página | Cambia de estado (pasa por el mismo intérprete) | |
| 7.2 | `Invoke-RestMethod http://IP/api/estado` | JSON con las 6 luces y su estado | |
| 7.3 | Arrancar sin Wi-Fi configurado | Aparece la red «Asistente-ESP32»; la página abre en 192.168.4.1 | |

## Verificación del código

- El programa compila sin errores ni advertencias para **ESP32-S3** (placa esp32 2.0.17,
  ArduinoJson 7): usa ~14 % de la RAM y ~23 % de la memoria de programa.
- Los porcentajes de la columna «Esperado» se calcularon ejecutando las secciones 2 a 5 del
  código con estas frases. En la placa deben salir iguales.
