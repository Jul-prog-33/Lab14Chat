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
#define SERVER_IP   "127.0.0.1"
#define HISTORY_MAX 1000
#define HISTORY_LINE 1400

char history[HISTORY_MAX][HISTORY_LINE];
int  history_count = 0;

// ── Mostrar el prompt "Tú:" ───────────────────
// Llámalo cada vez que el usuario pueda escribir
void mostrar_prompt(const char* input_buffer, int input_len) {
    printf("\rTu: %.*s", input_len, input_buffer);
    fflush(stdout);
}

// ── Limpiar línea actual antes de imprimir msg ─
void limpiar_linea(int input_len) {
    // Borra "Tú: " + lo que el usuario lleve escrito
    int total = 4 + input_len; // "Tú: " son 4 caracteres
    printf("\r");
    for (int i = 0; i < total; i++) printf(" ");
    printf("\r");
    fflush(stdout);
}

// ── Historial ─────────────────────────────────
void agregar_historial(const char* origen, const char* texto) {
    if (history_count >= HISTORY_MAX) return;
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
        if (strlen(history[i]) == 0 ||
            history[i][strlen(history[i]) - 1] != '\n') {
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
    printf("Comandos disponibles:\n");
    printf("  /list          - Ver usuarios disponibles\n");
    printf("  /rooms         - Ver salas disponibles\n");
    printf("  /join #sala    - Entrar a una sala\n");
    printf("  /leave         - Salir de la sala actual\n");
    printf("  /who           - Ver usuarios en la sala\n");
    printf("  /export        - Exportar historial\n");
    printf("  /exit          - Salir del chat actual\n");
    printf("  /quit          - Desconectarse\n");
    printf("------------------------------------------\n\n");

    char buffer[BUF_SIZE];
    char input_buffer[BUF_SIZE];
    int  input_len = 0;
    bool prompt_visible = false;

    // Mostrar prompt inicial
    mostrar_prompt(input_buffer, input_len);
    prompt_visible = true;

    while (1) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(sock, &readfds);

        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 100000; // 100ms

        int actividad = select((int)sock + 1, &readfds, NULL, NULL, &tv);

        // ── Mensaje entrante del servidor ──────
        if (actividad > 0 && FD_ISSET(sock, &readfds)) {
            int n = recv(sock, buffer, BUF_SIZE - 1, 0);
            if (n <= 0) {
                limpiar_linea(input_len);
                printf("[Cliente] Desconectado del servidor\n");
                break;
            }
            buffer[n] = '\0';
            agregar_historial("Servidor/Chat", buffer);

            // Borrar el prompt, imprimir mensaje, redibujar prompt
            limpiar_linea(input_len);
            printf("%s", buffer);

            // Si el mensaje no termina en \n, agregar uno
            if (buffer[n - 1] != '\n') printf("\n");

            mostrar_prompt(input_buffer, input_len);
            prompt_visible = true;
        }

        // ── Entrada del usuario ────────────────
        if (_kbhit()) {
            char ch = _getch();

            if (ch == '\r' || ch == '\n') {
                // Limpiar el prompt de la línea actual
                limpiar_linea(input_len);

                input_buffer[input_len] = '\0';

                if (input_len == 0) {
                    mostrar_prompt(input_buffer, 0);
                    continue;
                }

                // Eco del mensaje enviado por el usuario
                printf("Tu: %s\n", input_buffer);

                // Comando local: exportar
                if (strcmp(input_buffer, "/export") == 0) {
                    exportar_historial();
                    agregar_historial("Yo", "/export\n");
                    input_len = 0;
                    mostrar_prompt(input_buffer, 0);
                    continue;
                }

                // Comando local: quit
                if (strcmp(input_buffer, "/quit") == 0) {
                    send(sock, "/exit", 5, 0);
                    agregar_historial("Yo", "/quit\n");
                    break;
                }

                // Enviar al servidor
                if (send(sock, input_buffer, input_len, 0) == SOCKET_ERROR) {
                    printf("[Error] No se pudo enviar el mensaje\n");
                    break;
                }

                agregar_historial("Yo", input_buffer);
                input_len = 0;

                mostrar_prompt(input_buffer, 0);

            } else if (ch == '\b' && input_len > 0) {
                // Retroceso
                input_len--;
                // Borrar el carácter visualmente
                printf("\b \b");
                fflush(stdout);

            } else if (input_len < BUF_SIZE - 1 && ch >= 32 && ch < 127) {
                // Carácter normal
                input_buffer[input_len++] = ch;
                putchar(ch);
                fflush(stdout);
            }
        }
    }

    closesocket(sock);
    WSACleanup();
    return 0;
}