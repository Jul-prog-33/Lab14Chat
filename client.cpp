#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <winsock2.h>
#include <windows.h>
#include <conio.h>

#pragma comment(lib, "ws2_32.lib")

#define PORT        8888
#define BUF_SIZE    1024
#define SERVER_IP   "127.0.0.1"  // Cambia si es necesario
#define HISTORY_MAX 1000
#define HISTORY_LINE 1400

char history[HISTORY_MAX][HISTORY_LINE];
int history_count = 0;

void agregar_historial(const char* origen, const char* texto) {
    if (history_count >= HISTORY_MAX) {
        return;
    }

    snprintf(history[history_count], HISTORY_LINE, "%s: %s", origen, texto);
    history_count++;
}

void exportar_historial() {
    FILE* f = fopen("historial_chat.txt", "w");
    if (f == NULL) {
        printf("\n[Cliente] No se pudo exportar el historial\n");
        return;
    }

    fprintf(f, "Historial local de chat\n");
    fprintf(f, "=======================\n\n");

    for (int i = 0; i < history_count; i++) {
        fprintf(f, "%s", history[i]);
        if (strlen(history[i]) == 0 || history[i][strlen(history[i]) - 1] != '\n') {
            fprintf(f, "\n");
        }
    }

    fclose(f);
    printf("\n[Cliente] Historial exportado en historial_chat.txt\n");
}

// ── MAIN ─────────────────────────────────────
int main() {
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        printf("Error WSAStartup\n");
        return 1;
    }

    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        printf("Error creando socket\n");
        WSACleanup();
        return 1;
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);

    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        printf("Error conectando al servidor\n");
        closesocket(sock);
        WSACleanup();
        return 1;
    }

    printf("Conectado al servidor\n");
    printf("Comandos: /list, /exit, /export, /quit\n");

    char buffer[BUF_SIZE];
    char input_buffer[BUF_SIZE];
    int input_len = 0;

    while (1) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(sock, &readfds);

        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 100000;  // 100ms timeout

        int actividad = select((int)sock + 1, &readfds, NULL, NULL, &tv);

        if (actividad > 0 && FD_ISSET(sock, &readfds)) {
            int n = recv(sock, buffer, BUF_SIZE - 1, 0);
            if (n <= 0) {
                printf("[Cliente] Desconectado del servidor\n");
                break;
            }
            buffer[n] = '\0';
            agregar_historial("Servidor/Chat", buffer);
            printf("%s", buffer);
        }

        // Verificar entrada del usuario
        if (_kbhit()) {
            char ch = _getch();
            if (ch == '\r' || ch == '\n') {
                input_buffer[input_len] = '\0';

                if (input_len == 0) {
                    printf("\n");
                    continue;
                }

                if (strcmp(input_buffer, "/export") == 0) {
                    exportar_historial();
                    input_len = 0;
                    continue;
                }

                if (strcmp(input_buffer, "/quit") == 0) {
                    send(sock, "/exit", 5, 0);
                    agregar_historial("Yo", "/quit\n");
                    break;
                }
                if (send(sock, input_buffer, input_len, 0) == SOCKET_ERROR) {
                    printf("Error enviando mensaje\n");
                    break;
                }
                agregar_historial("Yo", input_buffer);
                input_len = 0;
                printf("\n");  // Nueva línea después de enviar
            } else if (ch == '\b' && input_len > 0) {
                input_len--;
                printf("\b \b");  // Borrar carácter
            } else if (input_len < BUF_SIZE - 1) {
                input_buffer[input_len++] = ch;
                putchar(ch);
            }
        }
    }

    closesocket(sock);
    WSACleanup();
    return 0;
}
