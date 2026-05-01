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
            printf("%s", buffer);
        }

        // Verificar entrada del usuario
        if (_kbhit()) {
            char ch = _getch();
            if (ch == '\r' || ch == '\n') {
                input_buffer[input_len] = '\0';
                if (send(sock, input_buffer, input_len, 0) == SOCKET_ERROR) {
                    printf("Error enviando mensaje\n");
                    break;
                }
                if (strcmp(input_buffer, "/exit") == 0) {
                    break;
                }
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