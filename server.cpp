#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <winsock2.h>
#include <windows.h>

#pragma comment(lib, "ws2_32.lib")

#define PORT        8888
#define BUF_SIZE    1024
#define MAX_CLIENTS 100
#define NAME_LEN    32
#define MAX_ROOMS   10
#define ROOM_NAME_LEN 32

// ── Estados ──────────────────────────────────
#define STATE_NAME     0
#define STATE_CHOOSING 1
#define STATE_CHATTING 2
#define STATE_ROOM     3   // Usuario dentro de una sala

// ── Sala de chat ─────────────────────────────
typedef struct {
    char name[ROOM_NAME_LEN];   // e.g. "#general"
    bool active;
} room_t;

// ── Cliente ───────────────────────────────────
typedef struct {
    SOCKET s;
    char   name[NAME_LEN];
    int    state;
    int    peer_idx;    // Para chat privado
    int    room_idx;    // Para sala (-1 si no está en sala)
} client_t;

room_t   rooms[MAX_ROOMS];
client_t clients[MAX_CLIENTS + 1];

// ── Inicializar salas predeterminadas ─────────
void inicializar_salas() {
    for (int i = 0; i < MAX_ROOMS; i++) {
        rooms[i].active = false;
        memset(rooms[i].name, 0, ROOM_NAME_LEN);
    }
    // Salas por defecto
    strncpy(rooms[0].name, "#general", ROOM_NAME_LEN - 1);
    rooms[0].active = true;
    strncpy(rooms[1].name, "#equipo1", ROOM_NAME_LEN - 1);
    rooms[1].active = true;
    strncpy(rooms[2].name, "#random", ROOM_NAME_LEN - 1);
    rooms[2].active = true;
}

// ── Enviar mensaje a un cliente ───────────────
void enviar(int idx, const char* msg) {
    send(clients[idx].s, msg, (int)strlen(msg), 0);
}

// ── Enviar a todos en una sala ─────────────────
void enviar_sala(int room_idx, const char* msg, int except_idx) {
    for (int i = 1; i <= MAX_CLIENTS; i++) {
        if (i == except_idx) continue;
        if (clients[i].s != INVALID_SOCKET &&
            clients[i].state == STATE_ROOM &&
            clients[i].room_idx == room_idx) {
            enviar(i, msg);
        }
    }
}

// ── Contar usuarios en sala ────────────────────
int contar_sala(int room_idx) {
    int count = 0;
    for (int i = 1; i <= MAX_CLIENTS; i++) {
        if (clients[i].s != INVALID_SOCKET &&
            clients[i].state == STATE_ROOM &&
            clients[i].room_idx == room_idx) {
            count++;
        }
    }
    return count;
}

// ── Buscar sala por nombre ─────────────────────
int buscar_sala(const char* name) {
    for (int i = 0; i < MAX_ROOMS; i++) {
        if (rooms[i].active && strcmp(rooms[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

// ── Crear sala nueva ───────────────────────────
int crear_sala(const char* name) {
    for (int i = 0; i < MAX_ROOMS; i++) {
        if (!rooms[i].active) {
            strncpy(rooms[i].name, name, ROOM_NAME_LEN - 1);
            rooms[i].name[ROOM_NAME_LEN - 1] = '\0';
            rooms[i].active = true;
            printf("[Servidor] Sala creada: %s\n", name);
            return i;
        }
    }
    return -1; // Sin espacio
}

// ── Lista de salas disponibles ─────────────────
void enviar_lista_salas(int dest_idx) {
    char buf[BUF_SIZE];
    int len = 0;

    len += snprintf(buf + len, sizeof(buf) - len,
                    "\n=== Salas disponibles ===\n");

    for (int i = 0; i < MAX_ROOMS; i++) {
        if (rooms[i].active) {
            int cnt = contar_sala(i);
            len += snprintf(buf + len, sizeof(buf) - len,
                            "  %s  (%d usuarios)\n", rooms[i].name, cnt);
        }
    }

    len += snprintf(buf + len, sizeof(buf) - len,
                    "=========================\n"
                    "Usa /join #sala para entrar o /list para usuarios:\n");

    send(clients[dest_idx].s, buf, len, 0);
}

// ── Lista de usuarios disponibles ─────────────
void enviar_lista(int dest_idx) {
    char buf[BUF_SIZE];
    int len = 0;

    len += snprintf(buf + len, sizeof(buf) - len,
                    "\n=== Usuarios disponibles ===\n");

    bool hay = false;
    for (int j = 1; j <= MAX_CLIENTS; j++) {
        if (j == dest_idx) continue;
        if (clients[j].s != INVALID_SOCKET &&
            clients[j].state == STATE_CHOOSING) {
            len += snprintf(buf + len, sizeof(buf) - len,
                            "  - %s\n", clients[j].name);
            hay = true;
        }
    }

    if (!hay)
        len += snprintf(buf + len, sizeof(buf) - len, "  (ninguno)\n");

    len += snprintf(buf + len, sizeof(buf) - len,
                    "============================\n"
                    "Escribe nombre, /join #sala o /rooms:\n");

    send(clients[dest_idx].s, buf, len, 0);
}

// ── Notificar lista a todos en STATE_CHOOSING ──
void notificar_lista() {
    for (int i = 1; i <= MAX_CLIENTS; i++) {
        if (clients[i].s != INVALID_SOCKET &&
            clients[i].state == STATE_CHOOSING) {
            enviar_lista(i);
        }
    }
}

// ── Desconectar cliente ────────────────────────
void desconectar(int idx) {
    printf("[Servidor] '%s' desconectado\n", clients[idx].name);

    // Si estaba en chat privado
    int peer = clients[idx].peer_idx;
    if (peer > 0 && clients[peer].s != INVALID_SOCKET &&
        clients[peer].state == STATE_CHATTING) {
        enviar(peer, "[Sistema] El otro usuario se desconecto\n");
        clients[peer].state = STATE_CHOOSING;
        clients[peer].peer_idx = -1;
        enviar_lista(peer);
    }

    // Si estaba en una sala
    if (clients[idx].state == STATE_ROOM) {
        int ri = clients[idx].room_idx;
        if (ri >= 0) {
            char msg[BUF_SIZE];
            snprintf(msg, sizeof(msg), "[Sistema] %s salio de la sala\n", clients[idx].name);
            enviar_sala(ri, msg, idx);
        }
    }

    closesocket(clients[idx].s);
    clients[idx].s = INVALID_SOCKET;
    clients[idx].state = -1;
    clients[idx].peer_idx = -1;
    clients[idx].room_idx = -1;
    memset(clients[idx].name, 0, NAME_LEN);

    notificar_lista();
}

// ── MAIN ─────────────────────────────────────
int main() {
    WSADATA wsa;

    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        printf("Error WSAStartup\n");
        return 1;
    }

    SOCKET listen_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_sock == INVALID_SOCKET) {
        printf("Error creando socket servidor\n");
        WSACleanup();
        return 1;
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);

    if (bind(listen_sock, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        printf("Error bind. Puerto ocupado o sin permisos\n");
        closesocket(listen_sock);
        WSACleanup();
        return 1;
    }

    if (listen(listen_sock, 5) == SOCKET_ERROR) {
        printf("Error listen\n");
        closesocket(listen_sock);
        WSACleanup();
        return 1;
    }

    inicializar_salas();
    printf("Servidor en puerto %d...\n", PORT);

    for (int i = 1; i <= MAX_CLIENTS; i++) {
        clients[i].s = INVALID_SOCKET;
        clients[i].state = -1;
        clients[i].peer_idx = -1;
        clients[i].room_idx = -1;
    }

    char buffer[BUF_SIZE];

    while (1) {
        fd_set readfds;
        FD_ZERO(&readfds);

        FD_SET(listen_sock, &readfds);
        SOCKET max_sd = listen_sock;

        for (int i = 1; i <= MAX_CLIENTS; i++) {
            SOCKET sd = clients[i].s;
            if (sd != INVALID_SOCKET) {
                FD_SET(sd, &readfds);
                if (sd > max_sd) max_sd = sd;
            }
        }

        int actividad = select((int)max_sd + 1, &readfds, NULL, NULL, NULL);

        if (actividad < 0) {
            printf("Error select\n");
            break;
        }

        // ── Nueva conexión ──────────────────────
        if (FD_ISSET(listen_sock, &readfds)) {
            SOCKET conn = accept(listen_sock, NULL, NULL);
            if (conn == INVALID_SOCKET) {
                printf("Error accept\n");
                continue;
            }

            int slot = -1;
            for (int i = 1; i <= MAX_CLIENTS; i++) {
                if (clients[i].s == INVALID_SOCKET) {
                    slot = i;
                    break;
                }
            }

            if (slot == -1) {
                send(conn, "Servidor lleno\n", 15, 0);
                closesocket(conn);
            } else {
                clients[slot].s = conn;
                clients[slot].state = STATE_NAME;
                clients[slot].peer_idx = -1;
                clients[slot].room_idx = -1;

                send(conn, "Ingresa nombre:\n", 16, 0);
                printf("Nuevo cliente slot %d\n", slot);
            }
        }

        // ── Mensajes de clientes ────────────────
        for (int i = 1; i <= MAX_CLIENTS; i++) {
            SOCKET sd = clients[i].s;

            if (sd == INVALID_SOCKET) continue;
            if (!FD_ISSET(sd, &readfds)) continue;

            int n = recv(sd, buffer, BUF_SIZE - 1, 0);

            if (n <= 0) {
                desconectar(i);
                continue;
            }

            buffer[n] = '\0';

            while (n > 0 && (buffer[n-1] == '\n' || buffer[n-1] == '\r'))
                buffer[--n] = '\0';

            switch (clients[i].state) {

                // ── Registro de nombre ─────────
                case STATE_NAME: {
                    if (strlen(buffer) == 0) {
                        enviar(i, "Nombre invalido\n");
                        break;
                    }

                    bool usado = false;
                    for (int k = 1; k <= MAX_CLIENTS; k++) {
                        if (k != i && clients[k].s != INVALID_SOCKET &&
                            strcmp(clients[k].name, buffer) == 0) {
                            usado = true;
                            break;
                        }
                    }

                    if (usado) {
                        enviar(i, "Nombre en uso\n");
                        break;
                    }

                    strncpy(clients[i].name, buffer, NAME_LEN - 1);
                    clients[i].name[NAME_LEN - 1] = '\0';
                    clients[i].state = STATE_CHOOSING;

                    printf("[Servidor] Slot %d registrado como '%s'\n", i, clients[i].name);

                    enviar(i, "Bienvenido\n");
                    enviar_lista(i);
                    enviar_lista_salas(i);
                    notificar_lista();
                    break;
                }

                // ── Eligiendo destino ──────────
                case STATE_CHOOSING: {
                    // Listar usuarios
                    if (strcmp(buffer, "/list") == 0) {
                        enviar_lista(i);
                        break;
                    }

                    // Listar salas
                    if (strcmp(buffer, "/rooms") == 0) {
                        enviar_lista_salas(i);
                        break;
                    }

                    // Unirse a sala: /join #nombre
                    if (strncmp(buffer, "/join ", 6) == 0) {
                        const char* room_name = buffer + 6;

                        if (room_name[0] != '#') {
                            enviar(i, "[Error] El nombre de sala debe comenzar con #\n");
                            break;
                        }

                        int ri = buscar_sala(room_name);

                        if (ri == -1) {
                            // Crear sala nueva
                            ri = crear_sala(room_name);
                            if (ri == -1) {
                                enviar(i, "[Error] No se pueden crear mas salas\n");
                                break;
                            }
                            char msg[BUF_SIZE];
                            snprintf(msg, sizeof(msg), "[Sistema] Sala %s creada\n", room_name);
                            enviar(i, msg);
                        }

                        clients[i].state = STATE_ROOM;
                        clients[i].room_idx = ri;

                        char msg[BUF_SIZE];
                        snprintf(msg, sizeof(msg),
                                 "[Sistema] Entraste a %s (%d usuarios)\n"
                                 "Usa /leave para salir, /who para ver usuarios\n",
                                 rooms[ri].name, contar_sala(ri));
                        enviar(i, msg);

                        snprintf(msg, sizeof(msg),
                                 "[Sistema] %s entro a la sala\n", clients[i].name);
                        enviar_sala(ri, msg, i);

                        notificar_lista();
                        break;
                    }

                    // Chat privado: buscar por nombre
                    int encontrado = -1;
                    for (int j = 1; j <= MAX_CLIENTS; j++) {
                        if (j != i &&
                            clients[j].s != INVALID_SOCKET &&
                            clients[j].state == STATE_CHOOSING &&
                            strcmp(clients[j].name, buffer) == 0) {
                            encontrado = j;
                            break;
                        }
                    }

                    if (encontrado == -1) {
                        enviar(i, "No disponible\n");
                        break;
                    }

                    clients[i].peer_idx = encontrado;
                    clients[encontrado].peer_idx = i;

                    clients[i].state = STATE_CHATTING;
                    clients[encontrado].state = STATE_CHATTING;

                    char inicio[BUF_SIZE];
                    snprintf(inicio, sizeof(inicio),
                             "[Sistema] Chat privado iniciado con %s\n", clients[encontrado].name);
                    enviar(i, inicio);
                    snprintf(inicio, sizeof(inicio),
                             "[Sistema] Chat privado iniciado con %s\n", clients[i].name);
                    enviar(encontrado, inicio);

                    notificar_lista();
                    break;
                }

                // ── Chat privado ───────────────
                case STATE_CHATTING: {
                    int peer = clients[i].peer_idx;

                    if (strcmp(buffer, "/exit") == 0) {
                        enviar(i, "Saliste del chat\n");

                        if (peer > 0 && clients[peer].s != INVALID_SOCKET) {
                            enviar(peer, "El otro salio\n");
                            clients[peer].state = STATE_CHOOSING;
                            clients[peer].peer_idx = -1;
                            enviar_lista(peer);
                        }

                        clients[i].state = STATE_CHOOSING;
                        clients[i].peer_idx = -1;
                        enviar_lista(i);

                        notificar_lista();
                        break;
                    }

                    if (peer > 0 && clients[peer].s != INVALID_SOCKET) {
                        char msg[BUF_SIZE];
                        snprintf(msg, sizeof(msg), "%s: %s\n", clients[i].name, buffer);
                        send(clients[peer].s, msg, strlen(msg), 0);
                    }
                    break;
                }

                // ── Sala de chat ───────────────
                case STATE_ROOM: {
                    int ri = clients[i].room_idx;

                    // Ver usuarios en la sala
                    if (strcmp(buffer, "/who") == 0) {
                        char buf[BUF_SIZE];
                        int len = 0;
                        len += snprintf(buf + len, sizeof(buf) - len,
                                        "\n=== Usuarios en %s ===\n", rooms[ri].name);
                        for (int j = 1; j <= MAX_CLIENTS; j++) {
                            if (clients[j].s != INVALID_SOCKET &&
                                clients[j].state == STATE_ROOM &&
                                clients[j].room_idx == ri) {
                                len += snprintf(buf + len, sizeof(buf) - len,
                                                "  - %s%s\n", clients[j].name,
                                                (j == i) ? " (tu)" : "");
                            }
                        }
                        len += snprintf(buf + len, sizeof(buf) - len, "====================\n");
                        send(clients[i].s, buf, len, 0);
                        break;
                    }

                    // Salir de la sala
                    if (strcmp(buffer, "/leave") == 0) {
                        char msg[BUF_SIZE];
                        snprintf(msg, sizeof(msg),
                                 "[Sistema] %s salio de la sala\n", clients[i].name);
                        enviar_sala(ri, msg, i);

                        clients[i].state = STATE_CHOOSING;
                        clients[i].room_idx = -1;

                        enviar(i, "Saliste de la sala\n");
                        enviar_lista(i);
                        enviar_lista_salas(i);
                        notificar_lista();
                        break;
                    }

                    // Mensaje normal → broadcast a la sala
                    char msg[BUF_SIZE];
                    snprintf(msg, sizeof(msg), "%s: %s\n", clients[i].name, buffer);
                    enviar_sala(ri, msg, i);
                    break;
                }
            }
        }
    }

    closesocket(listen_sock);
    WSACleanup();
    return 0;
}