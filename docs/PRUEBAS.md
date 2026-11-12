# Pruebas del asistente

Lista de pruebas para comprobar que el asistente interpreta bien las órdenes y controla
los LED. Las columnas **«Esperado»** son los **porcentajes de similitud TF-IDF** que produce
el intérprete del código (secciones 2 a 6), calculados con estas mismas frases y el corpus
original de 44 frases. Si agregas frases al corpus, los porcentajes cambian un poco porque
cambia el IDF.

> Escribe cada frase en el **Monitor Serie** o en la página web (así el resultado no depende
> del reconocimiento de voz). Después repite algunas **hablando** con el micrófono y anota qué
> texto captó Chrome. Completa la columna **Resultado** (✅ / ❌ y lo observado).

**Antes de empezar:** escribe `prende todas las luces` para dejar todo encendido.

## 0. Entrenamiento

| # | Qué revisar | Esperado | Resultado |
|---|-------------|----------|-----------|
| 0.1 | Monitor Serie al arrancar | «MODELO TF-IDF ENTRENADO» · 44 documentos · 42 palabras | |
| 0.2 | IDF de `luz` | 19 documentos · IDF **0.840** (la más baja) | |
| 0.3 | IDF de `cocina` y de `apaga` | `cocina`: 2 documentos, **3.091** · `apaga`: 1 documento, **3.784** | |

## 1. Órdenes claras

| # | Orden | Esperado | Resultado |
|---|-------|----------|-----------|
| 1.1 | `Apaga la luz de la cocina` | Apagar **78 %**, Cocina **65 %** → se apaga GPIO 7 · verde · confianza 71 % | |
| 1.2 | `Enciende la oficina y la recepción` | Encender **64 %**, Oficina **53 %**, Recepción **53 %** → encienden GPIO 4 y 5 · 58 % | |
| 1.3 | `oye asistente, prende la sala de juntas por favor` | Encender **66 %**, Sala de juntas **73 %** → GPIO 6 · 69 % | |
| 1.4 | `enciende la luz del baño` | Encender **78 %**, Baño **65 %** → GPIO 15 · 71 % | |

## 2. Todas a la vez

| # | Orden | Esperado | Resultado |
|---|-------|----------|-----------|
| 2.1 | `prende todas las luces` | Encender **64 %**, Todas **76 %** → 6 LED encendidos · 70 % | |
| 2.2 | `apaga las luces` | Apagar **76 %**, Todas **63 %** → 6 LED apagados · 69 % | |
| 2.3 | `apaga todo` | Apagar **69 %**, Todas **50 %** → 6 LED apagados · 59 % | |
| 2.4 | `apaga las luces de la cocina` | Cocina **53 %** gana a Todas → solo se apaga la Cocina · 58 % | |

## 3. El efecto del IDF

| # | Orden | Esperado | Resultado |
|---|-------|----------|-----------|
| 3.1 | `enciende la luz` | Encender **100 %**, cada habitación solo **4–6 %** (comparten «luz», que pesa poco) → **rojo** · «no en qué lugar» · 50 % | |
| 3.2 | `apaga la sala` | Sala de juntas **38 %** («sala» está en 3 frases: pesa algo menos) → apaga Sala de juntas · 59 % | |
| 3.3 | `enciende la recepcion la oficina la cocina el baño y la bodega` | Cada habitación **39 %** (el peso se reparte entre 6 palabras) → enciende 5 LED · 43 % | |

## 4. Sinónimos

| # | Orden | Esperado | Resultado |
|---|-------|----------|-----------|
| 4.1 | `desactiva el almacén` | Apagar **69 %** («desactiva la luz»), Bodega **71 %** («almacén») → apaga Bodega · 70 % | |
| 4.2 | `prende el comedor` | Encender **69 %**, Cocina **71 %** («comedor») → enciende Cocina · 70 % | |
| 4.3 | `enciende el despacho` | Encender **69 %**, Oficina **71 %** («despacho») → enciende Oficina · 70 % | |

## 5. Órdenes incompletas o sin sentido (no debe cambiar ningún LED)

| # | Orden | Esperado | Resultado |
|---|-------|----------|-----------|
| 5.1 | `la cocina` | Cocina **100 %**, ninguna acción → **rojo** · «no si quieres encender o apagar» · 50 % | |
| 5.2 | `hola como estas` | Ninguna palabra está en el corpus → todo **0 %** · **rojo** · «No entendí la orden» · 0 % | |
| 5.3 | `apaga la cosina` | «cosina» no está en el corpus → Apagar **98 %**, ningún lugar → **rojo** · «no en qué lugar» | |
| 5.4 | `alumbra la cocina` | «alumbra» no está en el corpus → **rojo** · «no si quieres encender o apagar». Agrega `{"alumbra la luz", "encender"}` al corpus, sube el programa y repite: debe encender la Cocina | |

## 6. Voz real

| # | Qué hacer | Esperado | Resultado (texto que captó Chrome) |
|---|-----------|----------|------------------------------------|
| 6.1 | Pulsar 🎤 y decir «apaga la luz de la cocina» | Se apaga Cocina | |
| 6.2 | Pulsar 🎤 y decir «prende todas las luces» | Se encienden todas | |
| 6.3 | Pulsar 🎤 y hablar rápido o con ruido de fondo | Revisar en la tabla TF-IDF qué palabras captó y cuáles se ignoraron | |

## 7. Interfaces

| # | Qué hacer | Esperado | Resultado |
|---|-----------|----------|-----------|
| 7.1 | Tocar «Cocina» en el plano de la página | Cambia de estado (pasa por el mismo intérprete) | |
| 7.2 | `Invoke-RestMethod http://IP/api/estado` | JSON con las 6 luces y su estado | |
| 7.3 | `Invoke-RestMethod http://IP/api/modelo` | JSON con 44 documentos, el vocabulario y el IDF de cada palabra | |
| 7.4 | Sección «📚 Modelo entrenado» de la página | Tabla ordenada de menor a mayor IDF; `luz` primero | |
| 7.5 | Arrancar sin Wi-Fi configurado | Aparece la red «Asistente-ESP32»; la página abre en 192.168.4.1 | |

## Verificación del código

- El programa compila sin errores ni advertencias para **ESP32-S3** (plataforma espressif32
  6.13.0 = placa esp32 2.0.17, ArduinoJson 7): usa ~23 % de la RAM y ~23 % de la memoria de
  programa.
- Los porcentajes de la columna «Esperado» se calcularon ejecutando las secciones 2 a 6 del
  código con estas frases. En la placa deben salir iguales.
