# Chat TCP en C — Servidor y Cliente

Sistema de chat en tiempo real implementado en C usando **Winsock2** (Windows). Soporta chat privado entre dos usuarios y salas de chat con múltiples participantes.

---

## Características

- Registro de usuarios con nombre único
- Chat privado entre dos usuarios
- Salas de chat con múltiples usuarios (`#general`, `#equipo1`, `#random`, y más)
- Creación dinámica de salas nuevas
- Historial local exportable a `.txt`
- Prompt `Tú:` en el cliente para identificar el turno de escritura
- Hasta 100 clientes simultáneos

---

## Requisitos

- Windows (usa Winsock2 y `conio.h`)
- Compilador C compatible con C99 o superior (MinGW / MSVC)
- Librería `ws2_32` enlazada al compilar

---

## Compilación

**Con GCC / MinGW:**

```bash
# Servidor
gcc servidor.c -o servidor.exe -lws2_32

# Cliente
gcc cliente.c -o cliente.exe -lws2_32
```

**Con MSVC:**

```bash
cl servidor.c ws2_32.lib
cl cliente.c ws2_32.lib
```

---

## Uso

### 1. Iniciar el servidor

```bash
./servidor.exe
```

El servidor escucha en el puerto `8888` por defecto.

### 2. Conectar clientes

```bash
./cliente.exe
```

Cada cliente se conecta a `127.0.0.1:8888`. Para conectarse desde otra máquina, cambia `SERVER_IP` en `cliente.c`.

### 3. Flujo de conexión

```
Conectado al servidor
  → Ingresar nombre de usuario
  → Ver lista de usuarios disponibles y salas
  → Elegir: chat privado con un usuario o entrar a una sala
```

---

## Comandos disponibles

| Comando | Dónde funciona | Descripción |
|---|---|---|
| `/list` | Menú principal | Lista usuarios disponibles para chat privado |
| `/rooms` | Menú principal | Lista salas disponibles con conteo de usuarios |
| `/join #sala` | Menú principal | Entrar a una sala (la crea si no existe) |
| `/leave` | Dentro de sala | Salir de la sala y volver al menú |
| `/who` | Dentro de sala | Ver usuarios en la sala actual |
| `/exit` | Chat privado | Salir del chat privado y volver al menú |
| `/export` | Cualquier momento | Exportar historial local a `historial_chat.txt` |
| `/quit` | Cualquier momento | Desconectarse del servidor |

---

## Salas por defecto

El servidor crea estas salas al iniciar:

- `#general`
- `#equipo1`
- `#random`

Se pueden crear salas adicionales con `/join #nombre` desde cualquier cliente. El máximo es 10 salas simultáneas.

---

## Visibilidad de usuarios

| Estado del usuario | Aparece en `/list` |
|---|---|
| Esperando (menú principal) |  Sí |
| En chat privado |  No |
| Dentro de una sala |  No |

---

## Configuración

Las constantes de configuración están en la parte superior de cada archivo:

**`servidor.c`**
```c
#define PORT        8888     // Puerto del servidor
#define MAX_CLIENTS 100      // Máximo de clientes simultáneos
#define MAX_ROOMS   10       // Máximo de salas simultáneas
```

**`cliente.c`**
```c
#define PORT        8888          // Puerto al que conectarse
#define SERVER_IP   "127.0.0.1"  // IP del servidor
#define HISTORY_MAX 1000          // Máximo de mensajes en historial
```

---

## Estructura del proyecto

```
.
├── servidor.cpp          # Lógica del servidor (acepta conexiones, gestiona estados)
├── cliente.cpp           # Lógica del cliente (UI, envío y recepción)
└── historial_chat.txt    # Generado por /export (uno por cliente)
```

---

## Limitaciones conocidas

- Solo funciona en **Windows** por el uso de Winsock2 y `conio.h`
- No hay cifrado de mensajes
- El historial es local a cada cliente (no centralizado en el servidor)
- Los nombres de sala deben comenzar con `#`