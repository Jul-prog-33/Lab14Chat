#include <stdio.h>
//#include <ws2_32.h>
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

#define STATE_NAME     0
#define STATE_CHOOSING 1
#define STATE_CHATTING 2

struct client_t {
    SOCKET s;
    char   name[NAME_LEN];
    int    state;
    int    peer_idx;
};

client_t clients[MAX_CLIENTS + 1];

//  Enviar mensaje 
void enviar(int idx, const char* msg) {
    send(clients[idx].s, msg, (int)strlen(msg), 0);
}

//  Lista de usuarios 
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
        len += snprintf(buf + len, sizeof(buf) - len,
                        "  (ninguno)\n");

    len += snprintf(buf + len, sizeof(buf) - len,
                    "============================\n"
                    "Escribe nombre o /list:\n");

    send(clients[dest_idx].s, buf, len, 0);
}

//  Notificar a todos 
void notificar_lista() {
    for (int i = 1; i <= MAX_CLIENTS; i++) {
        if (clients[i].s != INVALID_SOCKET &&
            clients[i].state == STATE_CHOOSING) {
            enviar_lista(i);
        }
    }
}

//  Desconectar 
void desconectar(int idx) {
    printf("[Servidor] '%s' desconectado\n", clients[idx].name);

    int peer = clients[idx].peer_idx;

    if (peer > 0 && clients[peer].s != INVALID_SOCKET &&
        clients[peer].state == STATE_CHATTING) {
        enviar(peer, "[Sistema] El otro usuario se desconecto\n");
        clients[peer].state = STATE_CHOOSING;
        clients[peer].peer_idx = -1;
        enviar_lista(peer);
    }

    closesocket(clients[idx].s);
    clients[idx].s = INVALID_SOCKET;
    clients[idx].state = -1;
    clients[idx].peer_idx = -1;
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

    printf("Servidor en puerto %d...\n", PORT);

    for (int i = 1; i <= MAX_CLIENTS; i++) {
        clients[i].s = INVALID_SOCKET;
        clients[i].state = -1;
        clients[i].peer_idx = -1;
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

        // ── Nueva conexión ──
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

                send(conn, "Ingresa nombre:\n", 16, 0);
                printf("Nuevo cliente slot %d\n", slot);
            }
        }

        // ── Clientes ──
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

                    printf("[Servidor] Cliente slot %d registrado como '%s'\n",
                           i, clients[i].name);

                    enviar(i, "Bienvenido\n");
                    enviar_lista(i);
                    notificar_lista();
                    break;
                }

                case STATE_CHOOSING: {
                    if (strcmp(buffer, "/list") == 0) {
                        enviar_lista(i);
                        break;
                    }

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

                    enviar(i, "Chat iniciado\n");
                    enviar(encontrado, "Chat iniciado\n");

                    notificar_lista();
                    break;
                }

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
                        snprintf(msg, sizeof(msg), "%s: %s\n",
                                 clients[i].name, buffer);
                        send(clients[peer].s, msg, strlen(msg), 0);
                    }
                    break;
                }
            }
        }
    }

    closesocket(listen_sock);
    WSACleanup();
    return 0;
}
