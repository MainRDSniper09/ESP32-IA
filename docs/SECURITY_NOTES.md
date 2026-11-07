# Notas de seguridad

## 1. Credenciales

- Todas las claves viven en `firmware/AgenteESP32/src/config/agente_secrets.h`, que está en
  `.gitignore`. El repositorio solo contiene la plantilla `agente_secrets.example.h` con valores vacíos.
- La API **nunca** devuelve claves: `/api/status` solo informa `"configured": true/false`.
- La interfaz web no recibe ni almacena la clave del LLM; las solicitudes al LLM salen del ESP32.
- Si una clave se sube por error: **revóquela inmediatamente** en el panel del proveedor
  (borrar el commit no basta: el historial de git y los *forks* la conservan).
- Antes de cada commit puede verificar: `git diff --cached | grep -i "sk-\|api_key\|token"`.

> Limitación conocida: las claves quedan compiladas en el binario. Quien tenga acceso físico
> al ESP32 podría extraerlas de la flash. Para producción se recomienda *flash encryption* y
> *secure boot* del ESP32-S3, o un *proxy* propio que guarde la clave en un servidor.

## 2. Protección de la API del dispositivo

Si define `SECRET_DEVICE_API_TOKEN`, todas las rutas `/api/...` (excepto `/api/health`)
exigen `Authorization: Bearer <token>`. La comparación es de tiempo constante. En la
interfaz web el token se ingresa con el botón ⚙ y se guarda en el `localStorage` del navegador.

El servidor es HTTP (sin TLS) y está pensado para una **red local de confianza**. No exponga
el puerto 80 a internet.

## 3. TLS hacia el LLM

Por defecto el ESP32 cifra la conexión HTTPS pero **no verifica el certificado** del servidor
(`setInsecure()`), lo que permitiría un ataque de intermediario en una red comprometida. Para
verificarlo, copie el certificado raíz del proveedor en `SECRET_LLM_ROOT_CA`:

```bash
# Muestra la cadena; copie el ÚLTIMO certificado (la raíz) completo, con BEGIN/END
openssl s_client -showcerts -connect api.anthropic.com:443 </dev/null
```

```cpp
#define SECRET_LLM_ROOT_CA \
"-----BEGIN CERTIFICATE-----\n" \
"MIIF...\n" \
"-----END CERTIFICATE-----\n"
```

Los certificados raíz caducan cada varios años; si las llamadas empiezan a fallar con
`llm_network`, actualícelo.

## 4. El LLM como entrada no confiable

El diseño asume que el LLM puede equivocarse o ser manipulado (p. ej. con *prompt injection*
escrita por el usuario). Por eso:

- El LLM **no tiene acceso** a pines, red ni memoria: solo produce texto.
- El firmware acepta únicamente 7 acciones de un catálogo cerrado, con parámetros tipados
  y validados (claves desconocidas → rechazo).
- Las condiciones se evalúan con los sensores reales, no con lo que «diga» el modelo.
- Un plan con cualquier error se rechaza completo.
- Los textos libres (`message`, `summary`) se limitan en longitud y se muestran escapados en la interfaz web.

## 5. Reporte de vulnerabilidades

Ver [SECURITY.md](../SECURITY.md).
