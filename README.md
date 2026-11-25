# BH Fitness i.Booster treadmill web bridge

The Spanish version is available below: **[Leer en español](#espanol)**.

An experimental project for controlling a BH Fitness treadmill using an **ESP32-C3**, Wi-Fi and Bluetooth Low Energy (BLE). In satellite mode the ESP32 serves a web page and acts as a client of the treadmill's Pafers/i.Concept module. The device used during development advertises itself as `PAFERS_C7BE99`. An experimental integrated UART/FTMS mode can be selected at compile time.

> **Safety:** begin testing with the belt empty, the surrounding area clear and the safety key within reach. Connecting sends initialization commands, including STOP. The web STOP button and BLE disconnection **do not replace the physical emergency stop**. Compatibility and fractional speeds must be checked on each machine.

> **Network security:** the web interface and API have no authentication or HTTPS. Any device that can reach the ESP32 can attempt to control the treadmill. Use it only on an isolated, trusted local network; do not expose its port to the Internet.

> **OTA security:** firmware upload is intentionally unauthenticated and unencrypted. Anyone who can reach the ESP32 can install arbitrary firmware; a malicious web page opened on a device with local network access may also submit an upload. The `.bin` can contain your Wi-Fi credentials; do not publish it or transfer it over an untrusted network.

> **FTMS security:** in integrated mode the BLE service is also unauthenticated. A nearby BLE client can request control and send commands. Do not enable this mode in an uncontrolled environment.

## Features

- HTTP server available without waiting for a BLE connection.
- Five-second BLE scan, selection and manual connection from the web interface.
- Start, stop, target speed from 0 to 16 km/h and target incline from 1 to 15.
- Speed adjustments of ±0.1 and ±0.5 km/h, plus preset buttons.
- Speed and incline telemetry, with distance and time **estimated by the ESP32**.
- HEX console showing transmitted and received packets, with manual transmission of up to 20 bytes.
- USB serial diagnostic output at 115200 baud.
- Firmware updates from the web in both modes, without a password.

## Operating modes

Select one mode before compiling in [`src/config.h`](src/config.h). There is still a single PlatformIO environment:

```cpp
#define MODO_ACTUAL MODO_OPERACION_SATELITE
// Or: #define MODO_ACTUAL MODO_OPERACION_INTEGRADO
```

**Satellite** is the default and is the mode used by the BLE scan/connect instructions below. It retains the Pafers BLE client and existing web controls.

In satellite mode, the ESP32 disconnects from the selected BLE device after 10 minutes without a START following connection or a STOP command/event. A START from the web or treadmill console cancels the timer until the next STOP. Recognized telemetry indicating movement also postpones disconnection. Recent valid telemetry must continue arriving; otherwise the timer restarts. A 1.0 km/h reading is ambiguous on this machine and is not treated as definite movement: the timer relies on the observed START/STOP events in that case, which may not detect every physical operation. Sending an arbitrary raw HEX frame cancels the timer until a recognized STOP is sent or received. The disconnect is logged in the HEX console, and reconnection is manual. This is not a safety stop and does not guarantee that the treadmill enters standby.

**Integrated** is experimental and has **not been validated against the physical UART**. It uses `Serial1` at 9600 baud, 8N1, with RX GPIO20 and TX GPIO21, configured in `src/config.h`. Before wiring, independently verify the console pinout, logic voltage, common ground, electrical protection, available board pins and actual protocol. Never assume that the BLE frames or 3.3 V levels are safe on an unidentified connector.

In integrated mode, the web interface replaces the BLE scan panel with UART/FTMS status. An initialized UART does **not** prove that the physical link is connected; recent valid telemetry is shown separately. No motion initialization sequence is sent at startup. The UART sends periodic keep-alives, and sends control frames when requested by the web or an FTMS client.

The ESP32 advertises `BH-iBooster-FTMS` with treadmill data and a control point. FTMS control requires a client to request control and subscribe to indications. Speed, incline, start and stop are supported; pause and reset are not implemented. A successful response means the UART write was accepted locally, **not** that the treadmill confirmed it. Interoperability with Kinomap, Zwift and QZ has not yet been verified.

For this treadmill, the user-confirmed mapping is **one Pafers incline level = 1% grade**; the FTMS ramp angle is calculated from that grade. Verify the mapping on the physical machine before using incline control. FTMS speed requests must be in 0.1 km/h steps, and incline requests in whole levels 1–15; unsupported increments are rejected rather than rounded. Distance is estimated locally, not read from the console. FTMS telemetry is sent only while UART telemetry is recent.

The remainder of the connection and control instructions describes **satellite mode**, except where noted. To use integrated mode, change the selector, build and upload again only after confirming the hardware is safe.

## Screenshots

The screenshots show the current interface with **simulated data**: the MAC address, telemetry and packets are examples, not proof of machine operation. The interface labels are in Spanish.

### BLE connection

![BLE connection panel with example data](docs/capturas/conexion.png)

### Telemetry

![Telemetry panel with example data](docs/capturas/telemetria.png)

### Controls

![Speed and incline controls](docs/capturas/controles.png)

### HEX console

![HEX console with example packets](docs/capturas/consola.png)

## Hardware and requirements

- ESP32-C3 SuperMini compatible with the PlatformIO `esp32-c3-devkitm-1` environment.
- USB **data** cable and a computer with an available serial port.
- A treadmill compatible with the Pafers GATT service listed below.
- A **2.4 GHz** Wi-Fi access point and a computer or phone able to reach the same network.
- [Visual Studio Code](https://code.visualstudio.com/) with the [PlatformIO IDE](https://platformio.org/platformio-ide) extension, or PlatformIO Core.

The project uses Arduino, NimBLE-Arduino 2.5.2, ArduinoJson 7.4.1 and the `WebServer` included in the framework. Versions and board options are in [`platformio.ini`](platformio.ini).

## 1. Configure Wi-Fi

If you have just cloned the project, copy the template to a local file. From PowerShell in the project directory:

```powershell
Copy-Item .\src\wifi_config.example.h .\src\wifi_config.h
```

If `src\wifi_config.h` already exists, **do not overwrite it**: edit it directly. Replace the placeholders with your network settings:

```cpp
#pragma once

const char* WIFI_SSID = "YOUR_WIFI_NAME";
const char* WIFI_PASS = "YOUR_WIFI_PASSWORD";
```

The local `src/wifi_config.h` file is excluded by [`.gitignore`](.gitignore); the template is shared. Do not put real credentials in the template, screenshots or Git messages. The exclusion protects **untracked** files only: if credentials were previously added to Git, removing them from the current file does not remove them from history. Compiled binaries can also contain them and should not be shared.

## 2. Configure the IP address and name

Near the beginning of [`src/main.cpp`](src/main.cpp), these values are defined:

```cpp
const IPAddress LOCAL_IP(192, 168, 218, 61);
const IPAddress GATEWAY(192, 168, 218, 1);
const IPAddress SUBNET(255, 255, 255, 0);
const char* TARGET_NAME = "PAFERS_C7BE99";
```

| Parameter | Purpose |
| --- | --- |
| `LOCAL_IP` | Static ESP32 address. It must be unused and belong to the router's subnet. |
| `GATEWAY` | Router address; also used as DNS in `WiFi.config`. |
| `SUBNET` | Network mask. The example corresponds to `/24`. |
| `TARGET_NAME` | BLE name highlighted during scanning and used when connecting with an empty JSON object. You can also select another device from the list. |

Adapt the three network values to your installation and avoid DHCP conflicts, for example by reserving an appropriate address on the router. Rebuild and upload the firmware after changing them.

With the values above, the web page is at `http://192.168.218.61/`. The ESP32 also attempts to advertise `http://cinta.local/` using mDNS; resolution depends on the system and network. The name `cinta` appears in `WiFi.setHostname` and `MDNS.begin` in `src/main.cpp`: update both if you change it. **The informational connection message also contains a hard-coded IP and name**; update it so the log does not show an old address. The initial selector text in [`src/web_ui.h`](src/web_ui.h) also mentions the original BLE name.

## 3. Build and upload over USB

### From Visual Studio Code

1. Open the directory containing `platformio.ini` as a PlatformIO project.
2. Connect the ESP32-C3 with a USB data cable and check its port in Device Manager.
3. Use **PlatformIO: Build** to compile and **PlatformIO: Upload** to flash.
4. If you need to specify a port, you can add `upload_port = COM3` to the environment in `platformio.ini`, adjusting it for your computer.
5. Open **PlatformIO: Serial Monitor** at 115200 baud to check the Wi-Fi connection.

### From PowerShell on Windows

From the project directory, if PlatformIO was installed by the VS Code extension:

```powershell
$pio = "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe"
& $pio run
& $pio device list
& $pio run --target upload --upload-port COM3
& $pio device monitor --port COM3 --baud 115200
```

Replace `COM3` with the actual port. Close the monitor before uploading if it holds the port open. Use `Ctrl+C` to exit the monitor. If `pio` is on your `PATH`, you can also run `pio run` and `pio run --target upload --upload-port COM3`.

The configuration retains `ARDUINO_USB_MODE=1` and `ARDUINO_USB_CDC_ON_BOOT=1`. The [`partitions.csv`](partitions.csv) table provides two 1.875 MiB OTA application slots; USB upload also writes the partition table. Do not assume the previous layout or its data will be preserved when changing it.

**Uploading restarts the ESP32 and disconnects BLE.** Do not upload while the treadmill is in use. After restarting, BLE must be connected manually again.

### Subsequent updates over Wi-Fi (OTA)

The currently running firmware must already include OTA; install it once over USB first. Keep a physical USB recovery option accessible and test the complete OTA process before enclosing the board.

1. Stop the treadmill physically, remove all users and clear the area. OTA does not send STOP, and web/FTMS control and keep-alives are paused during the upload. The physical emergency stop remains essential.
2. Select the intended mode and network settings in the source, then compile with PlatformIO. In satellite mode, disconnect the BLE device in the web interface and wait until it shows disconnected. In integrated mode, ensure the treadmill is physically stopped; UART initialization or telemetry alone does not prove this.
3. Open the **Actualización OTA** panel, choose `.pio/build/esp32-c3-devkitm-1/firmware.bin`, confirm the warning and wait for the transfer, validation and restart. Upload only an application image built for this ESP32-C3 and compatible partition layout, not a bootloader, partition table or merged image.
4. Reopen the web page after reboot. If the uploaded build changed Wi-Fi, IP or mode, use its new settings. Satellite BLE reconnection is manual. If the result is uncertain after a network interruption, check whether the device returned before retrying.

The upload streams into the inactive app partition and rejects invalid or oversized images. The current partition remains untouched during transfer, but automatic rollback for a firmware that boots incorrectly is not guaranteed. OTA does not change the partition table, and it cannot recover a device whose new firmware cannot connect to the configured Wi-Fi; USB recovery may be necessary. Do not remove power during the update. The `.bin` embeds build-time Wi-Fi settings and may contain secrets even though `wifi_config.h` is ignored by Git.

## 4. Use the web interface

1. Turn on the treadmill and leave its BLE module available; another application may already be connected to it.
2. Power the ESP32 and wait for it to associate with the configured Wi-Fi.
3. On a computer or phone on the same network, open `http://192.168.218.61/`, or your configured IP address. If mDNS works, you can also use `http://cinta.local/`.
4. Press **Escanear BLE** (Scan BLE). The scan lasts five seconds and retains up to 24 devices; name, MAC address and RSSI are shown. If the limit is reached, scan again closer to the treadmill.
5. Select the treadmill from the list and press **Conectar** (Connect). The ESP32 makes up to three BLE connection attempts, then discovers the service and characteristics, subscribes to Notify and sends initialization.
6. Wait for **Conectado** (Connected). Controls are enabled after initialization is transmitted, **not** after a specific acknowledgement from the console.
7. With the area clear, test START, STOP and adjustments gradually. Check the results on the treadmill's physical console. To finish, use STOP and then **Desconectar** (Disconnect).

The ESP32 does not connect automatically at startup or reconnect when the link is lost. A command accepted for transmission does not guarantee that the treadmill executed it.

### Controls and limits

- With recent telemetry, STOP is disabled at 0 km/h and START and START (ECO) are disabled above 0 km/h. After a satellite BLE connection, the ambiguous 1 km/h reading instead enables START and disables STOP until a START is requested or unambiguous movement is reported. STOP remains available during the start sequence. Once a STOP request is accepted, START and START (ECO) are enabled regardless of speed, and STOP remains available while speed is positive. Acceptance does not confirm that the belt has stopped: check it physically before reconnecting or starting again. Without recent telemetry, speed does not disable either button.
- Speed buttons change the target in tenths; presets range from 2 to 16 km/h. The software limit is 0–16 km/h. Check the limits of your model.
- Incline is adjusted by ±1 or by selecting an integer from 1 to 15. On connection, the reference sequence initially sends incline **0**; this does not change the selectable 1–15 range.
- When adjusting speed or incline, the firmware sends the speed target followed by the incline target. The value displayed as the sent target is not an acknowledgement from the console.
- Sending fractional speeds is **experimental**: their representation was inferred and must be checked against the console and notifications.
- STOP discards pending commands in the queue and takes the BLE worker's next turn; it cannot interrupt a GATT transaction already in progress.

### Telemetry and console

Speed and incline come from decoded notifications. Motion state is inferred: some BH consoles report **1 km/h even when stopped**, so this value is displayed as ambiguous. If no recent telemetry arrives for three seconds, it is marked as unknown.

Distance and time are calculated locally from recent telemetry, reset on each BLE connection and are not counters read from the console. They do not accumulate when speed is 0 or the ambiguous 1 km/h value.

In the console, `TX -> BLE` indicates a written packet and `RX <- BLE` a received notification. **Enviar HEX** (Send HEX) transmits 1 to 20 literal bytes without adding a checksum. An unknown frame can move or reconfigure the machine: use it only if you know its effects.

The web log retains the 64 most recent entries on the ESP32. USB output is nonblocking; if lines cannot be transmitted, the web page displays a count of omitted USB messages. Old entries can also be lost if the browser does not read them in time.

## How the code works

```text
Browser --HTTP/JSON--> WebServer / command queue
                              |
                          BLE worker
                              |
                   NimBLE client --GATT--> treadmill
                              |
                    notifications / telemetry
```

| File | Responsibility |
| --- | --- |
| [`src/main.cpp`](src/main.cpp) | Wi-Fi, mDNS, HTTP, shared state, command queue, BLE worker, GATT and logs. |
| [`src/pafers_protocol.h`](src/pafers_protocol.h) | UUIDs, command encoding, initialization, validation and frame decoding. It does not depend on the web server. |
| [`src/config.h`](src/config.h), [`src/treadmill_transport.h`](src/treadmill_transport.h), [`src/treadmill_transport.cpp`](src/treadmill_transport.cpp) | Compile-time mode selection and shared BLE/UART transport. |
| [`src/ftms_protocol.h`](src/ftms_protocol.h), [`src/ftms_server.cpp`](src/ftms_server.cpp) | FTMS frame decoding/encoding and experimental BLE peripheral. |
| [`src/web_ui.h`](src/web_ui.h) | HTML, CSS and JavaScript served from program memory. |
| [`src/wifi_config.example.h`](src/wifi_config.example.h) | Credential-free template for creating local configuration. |
| [`platformio.ini`](platformio.ini), [`partitions.csv`](partitions.csv) | Board, dependencies, USB and flash memory layout. |
| [`test/test_protocol.cpp`](test/test_protocol.cpp) | Independent protocol tests for a C++ compiler. |

At startup, `setup()` starts the server and BLE worker without attempting to connect to the treadmill. NimBLE initialization is deferred until Wi-Fi association. The browser periodically polls `/api/status` and `/api/logs`; control requests are validated and put into a queue processed by `bleWorker`, without scanning inside the HTTP handler.

On connection, these UUIDs are searched:

| Element | UUID |
| --- | --- |
| Service | `72d70001-501f-46f7-95f9-23846ee1aba3` |
| Write | `72d70002-501f-46f7-95f9-23846ee1aba3` |
| Notify | `72d70003-501f-46f7-95f9-23846ee1aba3` |

After subscribing, nine initialization frames for the base variant are sent: they include STOP, a reference profile, speed 1 km/h and incline 0, but **not** START. While connected, the worker sends a periodic keep-alive. If the link is lost, the error is reported and the user must reconnect.

Frames use the form `55 <opcode> <length> <data>`; this implementation does not add a checksum. The decoder reconstructs fragmented or concatenated frames and recognizes 13-byte telemetry and some button events. This is an independent implementation based on [qdomyos-zwift: paferstreadmill](https://github.com/cagnulein/qdomyos-zwift/tree/master/src/devices/paferstreadmill), not an official BH specification. The integrated UART transport uses the same frame encoder, but its physical compatibility remains unverified.

## HTTP API

POST routes accept JSON when parameters are required. A **202** response means the operation was queued, not that the treadmill executed it. Check `error`, `state` and the log to follow the result. An incompatible operation can return 409; a full queue or uninitialized BLE can return 503.

In integrated mode `/api/status` reports `mode: "integrado"` and `ftmsClients`; BLE scan, connect and disconnect requests return 409. The raw route and control routes use UART instead. In satellite mode `mode` is `"satelite"`.

`POST /api/ota` accepts one multipart file field named `firmware`. It requires satellite BLE to be disconnected, pauses the command worker in both modes and responds with JSON; success is followed by a reboot. There is no authentication.

| Method and route | Purpose / body |
| --- | --- |
| `GET /api/status` | BLE state, devices, UUIDs, targets, telemetry and errors. |
| `GET /api/logs?since=0` | Log entries after the `since` identifier; returns `latest` and `lost`. |
| `POST /api/ble/scan` | Starts the five-second scan. |
| `POST /api/ble/connect` | `{"address":"AA:BB:CC:DD:EE:FF","type":1}` or `{}` to find the target in the last scan. `type` is 0 for public and 1 for random; use the value returned by the scan. |
| `POST /api/ble/disconnect` | Requests disconnection. |
| `POST /api/control/start` | Requests START. |
| `POST /api/control/stop` | Requests STOP and discards pending commands. |
| `POST /api/control/speed` | `{"value":41}` for 4.1 km/h, or `{"delta":1}`, `{"delta":-1}`, `{"delta":5}` or `{"delta":-5}` in tenths. |
| `POST /api/control/incline` | `{"value":3}` for level 3, or `{"delta":1}` / `{"delta":-1}`. |
| `POST /api/ble/raw` | `{"hex":"55 0A 01 02"}`; literal bytes with no checksum added. This example is STOP. |

PowerShell query examples **without sending movement commands**:

```powershell
Invoke-RestMethod 'http://192.168.218.61/api/status'
Invoke-RestMethod 'http://192.168.218.61/api/logs?since=0'
```

When using POST from code, send `Content-Type: application/json` and do not combine `value` with `delta`. Test movement and HEX commands only under supervision and safe conditions.

## Troubleshooting

| Symptom | Checks |
| --- | --- |
| The web page does not load | Check SSID and password, 2.4 GHz Wi-Fi, IP/mask/gateway, possible IP conflicts and whether the client can reach the ESP32 network. Check the serial monitor. |
| `cinta.local` does not respond | Use the IP directly; check mDNS support and ensure Wi-Fi clients are not isolated from each other. |
| The treadmill does not appear | Turn it on, move it closer, release other BLE connections and scan again. Check whether the 24-device limit was reached. |
| Cannot connect or UUIDs are missing | Check the selected device, the address type from the scan and compatibility with the UUIDs above. Check the error and HEX console. |
| START is greyed out | Wait for **Conectado** after initialization finishes and check that the page can communicate with the ESP32. If errors occur, inspect state and logs; this is not proof that the treadmill is stopped. |
| The console shows 1 km/h while stopped | This is a known ambiguous reading on some consoles; it is not proof of movement or stopping. |
| The port is missing or upload fails | Check the data cable, COM port, closed monitor and power supply. Depending on the board, manual entry into programming mode may be necessary. |
| USB messages omitted | Connect a serial monitor or consult the web console; USB logging does not block the BLE worker when the computer is not reading. |

## Development tests

Building the firmware checks integration but does not verify the physical treadmill's behavior. [`test/README`](test/README) describes independent C++ tests for protocol encoding, limits and decoding. To run them with a compatible desktop compiler:

```powershell
g++ -std=c++11 test\test_protocol.cpp -o .pio\test_protocol.exe
.\.pio\test_protocol.exe
```

The executable returns `0` if the tests pass. The first PlatformIO build creates the `.pio` directory. Hardware tests must be performed separately, with the belt empty and physical safety measures ready.

---

<a id="espanol"></a>

# Puente web para cinta BH Fitness i.Booster

Proyecto experimental para controlar una cinta BH Fitness mediante un **ESP32-C3**, Wi-Fi y Bluetooth Low Energy (BLE). En modo satélite, el ESP32 sirve una página web y actúa como cliente del módulo Pafers/i.Concept de la cinta. El dispositivo utilizado durante el desarrollo anuncia el nombre `PAFERS_C7BE99`. También se puede seleccionar al compilar un modo integrado UART/FTMS experimental.

> **Seguridad:** prueba inicialmente con la banda vacía, la zona despejada y la llave de seguridad al alcance. La conexión envía comandos de inicialización, incluido STOP. El botón STOP de la web y la desconexión BLE **no sustituyen la parada de emergencia física**. La compatibilidad y las velocidades decimales deben comprobarse en cada máquina.

> **Seguridad de red:** la web y la API no tienen autenticación ni HTTPS. Cualquier dispositivo que pueda acceder al ESP32 puede intentar controlar la cinta. Úsalo solo en una red local aislada y de confianza; no abras el puerto a Internet.

> **Seguridad OTA:** la carga de firmware no tiene contraseña ni cifrado por decisión de diseño. Cualquiera que acceda al ESP32 puede instalar firmware arbitrario; una página web maliciosa abierta en un dispositivo con acceso a la red local también podría iniciar una carga. El `.bin` puede contener las credenciales Wi-Fi; no lo publiques ni lo transmitas por una red no confiable.

> **Seguridad FTMS:** en modo integrado el servicio BLE tampoco tiene autenticación. Un cliente BLE cercano puede solicitar el control y enviar órdenes. No actives este modo en un entorno no controlado.

## Funciones

- Servidor HTTP disponible sin esperar a una conexión BLE.
- Escaneo BLE de cinco segundos, selección y conexión manual desde la web.
- Inicio, parada, velocidad objetivo de 0 a 16 km/h e inclinación objetivo de 1 a 15.
- Ajustes de velocidad de ±0,1 y ±0,5 km/h, y accesos directos.
- Telemetría de velocidad e inclinación, con distancia y tiempo **estimados por el ESP32**.
- Consola HEX con paquetes transmitidos y recibidos y envío manual de hasta 20 bytes.
- Salida de diagnóstico por USB serie a 115200 baudios.
- Actualización de firmware desde la web en ambos modos, sin contraseña.

## Modos de funcionamiento

Selecciona el modo antes de compilar en [`src/config.h`](src/config.h). El proyecto mantiene un único entorno PlatformIO:

```cpp
#define MODO_ACTUAL MODO_OPERACION_SATELITE
// O bien: #define MODO_ACTUAL MODO_OPERACION_INTEGRADO
```

El modo **satélite** es el predeterminado y corresponde a las instrucciones de escaneo y conexión BLE descritas más abajo. Conserva el cliente Pafers BLE y los controles web.

En modo satélite, el ESP32 desconecta el dispositivo BLE seleccionado tras 10 minutos sin START después de la conexión o de un comando/evento STOP. Un START desde la web o la consola de la cinta cancela el temporizador hasta el siguiente STOP. La telemetría que indica movimiento inequívoco también aplaza la desconexión. Debe seguir llegando telemetría válida reciente; si falta, el plazo se reinicia. Una lectura de 1,0 km/h es ambigua en esta máquina y no se considera movimiento inequívoco: en ese caso el temporizador depende de los eventos START/STOP observados, que podrían no detectar todas las operaciones físicas. El envío de una trama HEX arbitraria cancela el temporizador hasta enviar o recibir un STOP reconocido. La desconexión se registra en la consola HEX y la reconexión es manual. No es una parada de seguridad ni garantiza que la cinta entre en reposo.

El modo **integrado** es experimental y **no se ha validado con la UART física**. Utiliza `Serial1` a 9600 baudios, 8N1, con RX GPIO20 y TX GPIO21, configurables en `src/config.h`. Antes de soldar, verifica por separado el pinout de la consola, los niveles lógicos, la masa común, la protección eléctrica, los pines disponibles de la placa y el protocolo real. No supongas que las tramas BLE o los niveles de 3,3 V son seguros en un conector no identificado.

En modo integrado, la web sustituye el panel de escaneo BLE por el estado UART/FTMS. Una UART inicializada **no** confirma que exista conexión física; la llegada de telemetría válida reciente se indica por separado. Al arrancar no se envía la secuencia de inicialización de movimiento. La UART envía keep-alives periódicos y envía comandos de control cuando los solicitan la web o un cliente FTMS.

El ESP32 anuncia `BH-iBooster-FTMS` con datos de cinta y un punto de control. El cliente FTMS debe suscribirse a indicaciones y solicitar el control. Se admiten velocidad, inclinación, inicio y parada; pausa y reset no están implementados. Una respuesta correcta significa que la escritura UART se aceptó localmente, **no** que la cinta la haya confirmado. Todavía no se ha verificado la interoperabilidad con Kinomap, Zwift ni QZ.

Para esta cinta, el usuario ha confirmado la equivalencia **un nivel Pafers = 1 % de pendiente**; el ángulo FTMS se calcula a partir de esa pendiente. Verifica la equivalencia en la máquina física antes de usar la inclinación. Las velocidades FTMS deben llegar en pasos de 0,1 km/h y las inclinaciones en niveles enteros de 1–15; los incrementos no admitidos se rechazan, no se redondean. La distancia es una estimación local, no un contador leído de la consola. Solo se envía telemetría FTMS mientras haya datos UART recientes.

El resto de las instrucciones de conexión y control describe el **modo satélite**, salvo indicación contraria. Para usar el integrado, cambia el selector y vuelve a compilar y cargar solo después de comprobar la seguridad del hardware.

## Capturas

Las imágenes muestran la interfaz actual con **datos simulados**: la dirección MAC, la telemetría y los paquetes son ejemplos, no una prueba de funcionamiento de la máquina.

### Conexión BLE

![Panel de conexión BLE con datos de ejemplo](docs/capturas/conexion.png)

### Telemetría

![Panel de telemetría con datos de ejemplo](docs/capturas/telemetria.png)

### Controles

![Controles de velocidad e inclinación](docs/capturas/controles.png)

### Consola HEX

![Consola HEX con paquetes de ejemplo](docs/capturas/consola.png)

## Material y requisitos

- ESP32-C3 SuperMini compatible con el entorno PlatformIO `esp32-c3-devkitm-1`.
- Cable USB **de datos** y ordenador con puerto serie disponible.
- Cinta compatible con el servicio GATT Pafers indicado más abajo.
- Punto de acceso Wi-Fi de **2,4 GHz** y un ordenador o móvil que pueda acceder a la misma red.
- [Visual Studio Code](https://code.visualstudio.com/) con la extensión [PlatformIO IDE](https://platformio.org/platformio-ide), o PlatformIO Core instalado.

El proyecto usa Arduino, NimBLE-Arduino 2.5.2, ArduinoJson 7.4.1 y el servidor `WebServer` incluido en el framework. Las versiones y las opciones de la placa están en [`platformio.ini`](platformio.ini).

## 1. Configurar Wi-Fi

Si acabas de clonar el proyecto, copia la plantilla a un archivo local. Desde PowerShell, situado en la carpeta del proyecto:

```powershell
Copy-Item .\src\wifi_config.example.h .\src\wifi_config.h
```

Si `src\wifi_config.h` ya existe, **no lo sobrescribas**: edítalo directamente. Sustituye los marcadores por los valores de tu red:

```cpp
#pragma once

const char* WIFI_SSID = "NOMBRE_DE_TU_WIFI";
const char* WIFI_PASS = "PASSWORD_DE_TU_WIFI";
```

El archivo `src/wifi_config.h` es local y está excluido en [`.gitignore`](.gitignore); la plantilla sí se comparte. No pongas datos reales en la plantilla, en capturas o en mensajes de Git. La exclusión solo protege archivos **no rastreados**: si alguna vez se añadieron credenciales a Git, eliminarlas del archivo actual no las borra del historial. Los binarios compilados también pueden contenerlas y no deben compartirse.

## 2. Configurar IP y nombre

En [`src/main.cpp`](src/main.cpp), cerca del comienzo, están estos valores:

```cpp
const IPAddress LOCAL_IP(192, 168, 218, 61);
const IPAddress GATEWAY(192, 168, 218, 1);
const IPAddress SUBNET(255, 255, 255, 0);
const char* TARGET_NAME = "PAFERS_C7BE99";
```

| Parámetro | Función |
| --- | --- |
| `LOCAL_IP` | Dirección fija del ESP32. Debe estar libre y pertenecer a la subred del router. |
| `GATEWAY` | Dirección del router; también se utiliza como DNS en `WiFi.config`. |
| `SUBNET` | Máscara de la red. El ejemplo corresponde a `/24`. |
| `TARGET_NAME` | Nombre BLE que se destaca al escanear y que se utiliza al conectar con un objeto JSON vacío. También puedes seleccionar otro dispositivo desde la lista. |

Adapta los tres valores de red a tu instalación y evita conflictos con DHCP, por ejemplo reservando una dirección apropiada en el router. Después de cambiarlos hay que volver a compilar y cargar el firmware.

La web está en `http://192.168.218.61/` con los valores anteriores. También intenta anunciarse como `http://cinta.local/` mediante mDNS; su resolución depende del sistema y la red. El nombre `cinta` aparece en `WiFi.setHostname` y `MDNS.begin` en `src/main.cpp`: si lo cambias, actualiza ambos. **El mensaje informativo de conexión también contiene la IP y el nombre escritos literalmente**; actualízalo para que el registro no muestre una dirección antigua. El texto inicial del selector en [`src/web_ui.h`](src/web_ui.h) también menciona el nombre BLE original.

## 3. Compilar y cargar por USB

### Desde Visual Studio Code

1. Abre la carpeta que contiene `platformio.ini` como proyecto PlatformIO.
2. Conecta el ESP32-C3 mediante USB de datos y comprueba su puerto en el Administrador de dispositivos.
3. Usa **PlatformIO: Build** para compilar y **PlatformIO: Upload** para grabar.
4. Si necesitas fijar un puerto, puedes añadir por ejemplo `upload_port = COM3` al entorno en `platformio.ini`, adaptándolo a tu equipo.
5. Abre **PlatformIO: Serial Monitor** a 115200 baudios para comprobar la conexión Wi-Fi.

### Desde PowerShell en Windows

En la carpeta del proyecto, si PlatformIO está instalado por la extensión de VS Code:

```powershell
$pio = "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe"
& $pio run
& $pio device list
& $pio run --target upload --upload-port COM3
& $pio device monitor --port COM3 --baud 115200
```

Sustituye `COM3` por el puerto real. Cierra el monitor antes de cargar si mantiene el puerto ocupado. Para salir del monitor usa `Ctrl+C`. Si tienes `pio` en el `PATH`, también puedes ejecutar `pio run` y `pio run --target upload --upload-port COM3`.

La configuración conserva `ARDUINO_USB_MODE=1` y `ARDUINO_USB_CDC_ON_BOOT=1`. La tabla [`partitions.csv`](partitions.csv) proporciona dos particiones de aplicación OTA de 1,875 MiB; la carga USB escribe también la tabla de particiones. No presupongas que la distribución anterior o sus datos se conservarán al cambiarla.

**La carga reinicia el ESP32 y corta el enlace BLE.** No la realices mientras se utiliza la cinta. Tras reiniciar, hay que conectar BLE manualmente de nuevo.

### Actualizaciones posteriores por Wi-Fi (OTA)

El firmware que esté funcionando debe incluir ya OTA; instálalo primero una vez por USB. Mantén accesible una conexión USB de recuperación y prueba el proceso OTA completo antes de cerrar el montaje.

1. Detén físicamente la cinta, retira a las personas y despeja la zona. OTA no envía STOP; durante la carga se pausan el control web/FTMS y los keep-alives. La parada de emergencia física sigue siendo imprescindible.
2. Selecciona en el código el modo y la configuración de red deseados y compila con PlatformIO. En modo satélite, desconecta el dispositivo BLE desde la web y espera a que indique desconectado. En modo integrado, comprueba físicamente que la cinta está parada; la UART inicializada o la telemetría por sí solas no lo garantizan.
3. Abre el panel **Actualización OTA**, selecciona `.pio/build/esp32-c3-devkitm-1/firmware.bin`, acepta el aviso y espera a la transferencia, validación y reinicio. Envía únicamente una imagen de aplicación compilada para este ESP32-C3 y una tabla de particiones compatible, no el bootloader, la tabla de particiones ni una imagen combinada.
4. Vuelve a abrir la web tras el reinicio. Si el firmware nuevo cambia la Wi-Fi, la IP o el modo, usa su nueva configuración. En satélite la conexión BLE vuelve a ser manual. Si una interrupción de red deja el resultado incierto, comprueba si el dispositivo ha vuelto antes de reintentar.

La carga escribe por partes en la partición de aplicación inactiva y rechaza imágenes inválidas o demasiado grandes. La partición actual no se sobrescribe durante la transferencia, pero no se garantiza la reversión automática si el nuevo firmware arranca mal. OTA no cambia la tabla de particiones ni puede recuperar un equipo cuyo firmware nuevo no se conecte a la Wi-Fi configurada; puede ser necesario recuperarlo por USB. No cortes la alimentación durante la actualización. El `.bin` incorpora la configuración Wi-Fi de compilación y puede contener secretos aunque `wifi_config.h` esté ignorado por Git.

## 4. Uso desde la web

1. Enciende la cinta y deja libre su módulo BLE; otra aplicación podría estar conectada a él.
2. Alimenta el ESP32 y espera a que se asocie a la Wi-Fi configurada.
3. Desde el ordenador o móvil en la misma red abre `http://192.168.218.61/`, o la IP que hayas configurado. Si mDNS funciona, también puedes usar `http://cinta.local/`.
4. Pulsa **Escanear BLE**. El escaneo dura cinco segundos y conserva hasta 24 dispositivos; se muestran nombre, MAC y RSSI. Si se alcanza el límite, repite el escaneo cerca de la cinta.
5. Selecciona la cinta en la lista y pulsa **Conectar**. El ESP32 intenta conectar por BLE hasta tres veces y, después, descubre el servicio y las características, se suscribe a Notify y transmite la inicialización.
6. Espera al estado **Conectado**. Los controles se habilitan después de transmitir la inicialización, **no** tras recibir una confirmación específica de la consola.
7. Con la zona libre, prueba START, STOP y los ajustes gradualmente. Verifica los resultados en la consola física de la cinta. Para terminar, utiliza STOP y después **Desconectar**.

El ESP32 no conecta automáticamente al arrancar ni reconecta si se pierde el enlace. Una orden aceptada para su envío no garantiza que la cinta la haya ejecutado.

### Controles y límites

- Con telemetría reciente, STOP se deshabilita a 0 km/h y START y START (ECO) por encima de 0 km/h. Tras conectar BLE en modo satélite, la lectura ambigua de 1 km/h habilita START y deshabilita STOP hasta que se solicite START o se detecte movimiento no ambiguo. STOP permanece disponible durante la secuencia de arranque. Cuando se acepta una petición STOP, START y START (ECO) se habilitan independientemente de la velocidad, y STOP sigue disponible mientras esta sea positiva. La aceptación no confirma que la banda se haya detenido: compruébalo físicamente antes de reconectar o volver a arrancar. Sin telemetría reciente, la velocidad no deshabilita ninguno de los botones.
- Los botones de velocidad cambian el objetivo en décimas; los accesos directos van de 2 a 16 km/h. El límite software es 0–16 km/h. Comprueba los límites de tu modelo.
- La inclinación se ajusta con ±1 o seleccionando un entero de 1 a 15. Al conectar, la secuencia de referencia envía inicialmente inclinación **0**; esto no modifica el rango seleccionable de 1–15.
- Al ajustar velocidad o inclinación, el firmware transmite el objetivo de velocidad y después el de inclinación. El valor mostrado como «objetivo enviado» no es una confirmación de la consola.
- El envío de velocidades con decimales es **experimental**: se ha inferido su representación y debe comprobarse frente a la consola y las notificaciones.
- STOP descarta los comandos pendientes en la cola y ocupa el siguiente turno del trabajador BLE; no puede interrumpir una transacción GATT ya iniciada.

### Telemetría y consola

La velocidad y la inclinación se obtienen de las notificaciones interpretadas. El estado de movimiento es inferido: algunas consolas BH notifican **1 km/h incluso estando paradas**, por eso se muestra como ambiguo. Si no llega telemetría reciente durante tres segundos, se marca como desconocida.

Distancia y tiempo se calculan localmente a partir de telemetría reciente, se reinician en cada conexión BLE y no son contadores leídos de la consola. No se acumulan cuando la velocidad es 0 o el valor ambiguo de 1 km/h.

En la consola, `TX -> BLE` indica un paquete escrito y `RX <- BLE` una notificación recibida. El campo **Enviar HEX** transmite literalmente de 1 a 20 bytes, sin añadir checksum. Una trama desconocida puede mover o reconfigurar la máquina: úsala solo si conoces sus efectos.

El registro web conserva las 64 entradas más recientes en el ESP32. La salida USB es no bloqueante; si no se pueden transmitir líneas, la web muestra un contador de mensajes USB omitidos. Los registros antiguos también pueden perderse si el navegador no los lee a tiempo.

## Cómo funciona el código

```text
Navegador --HTTP/JSON--> WebServer / cola de órdenes
                              |
                         tarea BLE
                              |
                   NimBLE cliente --GATT--> cinta
                              |
                    notificaciones / telemetría
```

| Archivo | Responsabilidad |
| --- | --- |
| [`src/main.cpp`](src/main.cpp) | Wi-Fi, mDNS, HTTP, estado compartido, cola de órdenes, tarea BLE, GATT y registros. |
| [`src/pafers_protocol.h`](src/pafers_protocol.h) | UUID, codificación de comandos, inicialización, validación y decodificación de tramas. No depende del servidor web. |
| [`src/config.h`](src/config.h), [`src/treadmill_transport.h`](src/treadmill_transport.h), [`src/treadmill_transport.cpp`](src/treadmill_transport.cpp) | Selección de modo en compilación y transporte compartido BLE/UART. |
| [`src/ftms_protocol.h`](src/ftms_protocol.h), [`src/ftms_server.cpp`](src/ftms_server.cpp) | Codificación y decodificación FTMS y periférico BLE experimental. |
| [`src/web_ui.h`](src/web_ui.h) | HTML, CSS y JavaScript servidos desde la memoria de programa. |
| [`src/wifi_config.example.h`](src/wifi_config.example.h) | Plantilla sin credenciales para crear la configuración local. |
| [`platformio.ini`](platformio.ini), [`partitions.csv`](partitions.csv) | Placa, dependencias, USB y distribución de la memoria flash. |
| [`test/test_protocol.cpp`](test/test_protocol.cpp) | Pruebas independientes del protocolo para un compilador C++. |

Al arrancar, `setup()` inicia el servidor y la tarea BLE sin intentar conectarse a la cinta. La inicialización de NimBLE se difiere hasta la asociación Wi-Fi. El navegador consulta periódicamente `/api/status` y `/api/logs`; las peticiones de control se validan y pasan a una cola procesada por `bleWorker`, sin realizar el escaneo dentro del manejador HTTP.

Al conectar se buscan estos UUID:

| Elemento | UUID |
| --- | --- |
| Servicio | `72d70001-501f-46f7-95f9-23846ee1aba3` |
| Escritura | `72d70002-501f-46f7-95f9-23846ee1aba3` |
| Notificaciones | `72d70003-501f-46f7-95f9-23846ee1aba3` |

Tras suscribirse, se envían nueve tramas de inicialización de la variante base: incluyen STOP, perfil de referencia, velocidad de 1 km/h e inclinación 0, pero **no** START. Mientras está conectado, el trabajador envía un keep-alive periódicamente. Si se pierde el enlace se informa del error y la reconexión queda a cargo del usuario.

La trama utilizada tiene la forma `55 <opcode> <longitud> <datos>`; esta implementación no añade checksum. El decodificador reconstruye tramas fragmentadas o concatenadas y reconoce la telemetría de 13 bytes y algunos eventos de botones. Es una implementación independiente basada en la referencia [qdomyos-zwift: paferstreadmill](https://github.com/cagnulein/qdomyos-zwift/tree/master/src/devices/paferstreadmill), no una especificación oficial de BH. El transporte UART integrado utiliza el mismo codificador, pero su compatibilidad física aún no está verificada.

## API HTTP

Las rutas POST reciben JSON cuando necesitan parámetros. Una respuesta **202** indica que la operación se ha encolado, no que la cinta la haya ejecutado. Consulta `error`, `state` y el registro para seguir el resultado. Una operación incompatible puede devolver 409; una cola llena o un BLE no inicializado, 503.

En modo integrado `/api/status` devuelve `mode: "integrado"` y `ftmsClients`; las peticiones de escaneo, conexión y desconexión BLE devuelven 409. Las rutas de control y envío HEX utilizan UART. En modo satélite `mode` vale `"satelite"`.

`POST /api/ota` recibe un único archivo multipart con el nombre de campo `firmware`. Exige que el BLE satélite esté desconectado, pausa la tarea de comandos en ambos modos y responde con JSON; si tiene éxito, reinicia. No requiere autenticación.

| Método y ruta | Función / cuerpo |
| --- | --- |
| `GET /api/status` | Estado BLE, dispositivos, UUID, objetivos, telemetría y errores. |
| `GET /api/logs?since=0` | Entradas del registro posteriores al identificador `since`; devuelve `latest` y `lost`. |
| `POST /api/ble/scan` | Inicia el escaneo de cinco segundos. |
| `POST /api/ble/connect` | `{"address":"AA:BB:CC:DD:EE:FF","type":1}` o `{}` para buscar el objetivo en el último escaneo. `type` es 0 para pública y 1 para aleatoria; usa el valor devuelto por el escaneo. |
| `POST /api/ble/disconnect` | Solicita la desconexión. |
| `POST /api/control/start` | Solicita START. |
| `POST /api/control/stop` | Solicita STOP y descarta órdenes pendientes. |
| `POST /api/control/speed` | `{"value":41}` para 4,1 km/h, o `{"delta":1}`, `{"delta":-1}`, `{"delta":5}` o `{"delta":-5}` en décimas. |
| `POST /api/control/incline` | `{"value":3}` para nivel 3, o `{"delta":1}` / `{"delta":-1}`. |
| `POST /api/ble/raw` | `{"hex":"55 0A 01 02"}`; bytes literales, sin checksum añadido. Este ejemplo es STOP. |

Ejemplos de consulta **sin enviar órdenes de movimiento**, desde PowerShell:

```powershell
Invoke-RestMethod 'http://192.168.218.61/api/status'
Invoke-RestMethod 'http://192.168.218.61/api/logs?since=0'
```

Al usar POST desde código, envía `Content-Type: application/json` y no combines `value` con `delta`. Las órdenes de movimiento y HEX deben probarse únicamente con supervisión y condiciones seguras.

## Resolución de problemas

| Síntoma | Comprobaciones |
| --- | --- |
| No carga la web | Comprueba SSID y contraseña, Wi-Fi de 2,4 GHz, IP/máscara/gateway, posible conflicto de IP y que el cliente pueda acceder a la red del ESP32. Revisa el monitor serie. |
| `cinta.local` no responde | Usa la IP directamente; verifica compatibilidad mDNS y que no exista aislamiento entre clientes Wi-Fi. |
| No aparece la cinta | Enciéndela, acércala, libera otras conexiones BLE y repite el escaneo. Revisa si se alcanzó el límite de 24 dispositivos. |
| No conecta o faltan UUID | Comprueba el dispositivo seleccionado, el tipo de dirección del escaneo y la compatibilidad con los UUID indicados. Consulta el error y la consola HEX. |
| START está gris | Espera a **Conectado** tras finalizar la inicialización y comprueba que la página comunica con el ESP32. Si hay errores, revisa el estado y los registros; no supone que la cinta esté parada. |
| La consola muestra 1 km/h estando parada | Es una lectura ambigua conocida en algunas consolas; no se usa como prueba de movimiento o parada. |
| No aparece el puerto o falla la carga | Comprueba cable de datos, puerto COM, monitor cerrado y alimentación. Según la placa, puede ser necesario entrar manualmente en modo de programación. |
| Mensajes USB omitidos | Conecta un monitor serie o consulta la consola web; el registro USB no bloquea la tarea BLE cuando el ordenador no lee. |

## Pruebas de desarrollo

La compilación del firmware comprueba la integración, pero no verifica el comportamiento físico de la cinta. [`test/README`](test/README) describe las pruebas C++ independientes para la codificación, límites y decodificación del protocolo. Para ejecutarlas con un compilador de escritorio compatible:

```powershell
g++ -std=c++11 test\test_protocol.cpp -o .pio\test_protocol.exe
.\.pio\test_protocol.exe
```

La ejecución devuelve `0` si las pruebas pasan. La primera compilación con PlatformIO crea la carpeta `.pio`. Las pruebas de hardware deben realizarse aparte, con la banda vacía y la seguridad física preparada.
