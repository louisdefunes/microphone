#include <fcntl.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <ctype.h>
#include "nvs_flash.h"

#include "Config.h"

#define handle_error(msg) \
        do {perror(msg); exit(1);} while(0)

#define ARRAY_SIZE(arr) (sizeof((arr)) / sizeof((arr)[0]))

void url_decode(char *dst, const char *src) {
        char a, b;
        while (*src != '\0') {
                if ((*src == '%') && ((a = src[1]) && (b = src[2])) && (isxdigit(a) && isxdigit(b))) {
                        if (a >= 'a') a -= 'a' - 'A';
                        if (a >= 'A') a -= 'A' - 10;
                        else a -= '0';

                        if (b >= 'a') b -= 'a' - 'A';
                        if (b >= 'A') b -= 'A' - 10;
                        else b -= '0';

                        *dst++ = 16*a + b;
                        src += 3;
                } else if (*src == '+') {
                        *dst++ = ' ';
                        src++;
                } else {
                        *dst++ = *src++;
                }
        }
        *dst++ = '\0';
} 


int parse_body(char *str, int content_len) {

        const char *ssid_start = strstr(str, "ssid=");
        const char *ssid_end = strchr(str, '&');
        if (ssid_start) {
                ssid_start += strlen("ssid=");
                if (!ssid_end) ssid_end = ssid_start + strlen(ssid_start);
                ssize_t ssid_len = ssid_end - ssid_start;
                if (ssid_len >= sizeof(ssid)) ssid_len = sizeof(ssid) - 1;
                strncpy(ssid, ssid_start, ssid_len);
                ssid[ssid_len] = '\0';


                content_len -= strlen("ssid=") + ssid_len + strlen("password=") + 1;
                // printf("\ncontent_len = %d\nstrlen(ssid=)=%d\nssid_len=%d\n",content_len, strlen("ssid="), ssid_len);
        }

        const char *pass_start = strstr(str, "password=");
        if (pass_start) {
                pass_start += strlen("password=");

                ssize_t pass_len = content_len;
                printf("\nContent nigga len is equal to %d\n", content_len);
                if (pass_len >= sizeof(passphrase)) pass_len = sizeof(passphrase) - 1;
                char temp[64];
                strncpy(temp, pass_start, pass_len);
                temp[pass_len] = '\0';
                url_decode(passphrase, temp);
        }

        printf("SSID = %s\n", ssid);
        printf("Password = %s\n", passphrase);

        return 1;
}

void *handle_client(void *arg) {

        const char *page;
        char html_buffer[B_SIZE];

        int content_length = 0;

        printf("thread created, waiting for tcp info to receive\n");
        int client_fd = *((int *)arg);
        char *buffer = malloc(BUFFER_SIZE * sizeof(char));

        //read from tcp socket
        ssize_t bytes_received = recv(client_fd, buffer, BUFFER_SIZE*sizeof(char), 0);
        printf("Succesfully received, proceeding to print buffer\n");

        if (bytes_received > 0) {
                for (int i=0; i < bytes_received; i++) printf("%c", buffer[i]);
        }

        //parse http request type
        if (strncmp(buffer, "GET ", 4) == 0) printf("to jest GET\n");
        else if (strncmp(buffer, "POST", 4) == 0) {
                printf("\nTo jest POST\n");

                //get content-length
                const char *cont_start = strstr(buffer, "Content-Length: ");
                cont_start += strlen("Content-Length: ");
                content_length = atoi(cont_start);
                printf("\nContent-Length: %d\n", content_length);

                provisioned = parse_body(buffer, content_length);

                //write to flash
                ESP_ERROR_CHECK(nvs_set_i8(handle_nvs, "provisioned", provisioned));
                ESP_ERROR_CHECK(nvs_set_str(handle_nvs, "ssid", ssid));
                ESP_ERROR_CHECK(nvs_set_str(handle_nvs, "password", passphrase));
                nvs_commit(handle_nvs);
        }

        //Forge HTTP header
        char *header = malloc(BUFFER_SIZE);
        snprintf(header, BUFFER_SIZE,
                        "HTTP/1.0 200 OK\r\n"
                        "Content-Type: text/html\r\n"
                        "\r\n");
        printf("forged header\n");

        //Allocate resource for the full response: header + content
        int response_count = 0;
        char *response = malloc(BUFFER_SIZE*2);
        memcpy(response, header, strlen(header));
        response_count += strlen(header);
        printf("allocated response\n");

        //copy file to the response
        if (provisioned) {
                page = 
                "<!doctype html>\n"
                "<html lang=\"en\">\n"
                "<head>\n"
                "  <meta charset=\"utf-8\" />\n"
                "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\" />\n"
                "  <title>Wi‑Fi Provisioning</title>\n"
                "  <style>\n"
                "    body {font-family: sans-serif;background: #0f172a;color: #e5e7eb;display: grid;place-items: center;height: 100vh;margin: 0;}\n"
                "    .card {background: #111827;padding: 20px;border-radius: 12px;width: 100%;max-width: 400px;text-align: center;}\n"
                "    h1 {font-size: 1.4rem;margin-bottom: 10px;}\n"
                "    p {font-size: 1rem;color: #94a3b8;margin-bottom: 10px;}\n"
                "    .creds {font-family: monospace;background: #0b1220;padding: 8px;border-radius: 6px;display: inline-block;color: #e5e7eb;}\n"
                "  </style>\n"
                "</head>\n"
                "<body>\n"
                "  <main class=\"card\">\n"
                "    <h1>Credentials already provided!\r\nPlease monitor the device's display to confirm a succesful connection</h1>\n"
                "    <p>Please restart the device to provision for it again.</p>\n"
                "    <p>SSID: <span class=\"creds\">%s</span></p>\n"
                "    <p>Password: <span class=\"creds\">%s</span></p>\n"
                "    <form method=\"POST\" action=\"/\" autocomplete=\"on\">\n"
                "      <label for=\"IP address\">Adres IP urzadzenia odbiorczego</label>\n"
                "      <input id=\"IP address\" name=\"IP\" class=\"field\" placeholder=\"np. 192.168.1.11\" required />\n"
                "      <div class=\"actions\">\n"
                "        <button type=\"submit\" class=\"btn\">Zapisz i połącz</button>\n"
                "        <button type=\"reset\" class=\"btn secondary\">Wyczyść</button>\n"
                "      </div>\n"
                "    </form>\n"
                "  </main>\n"
                "</body>\n"
                "</html>\n";
                snprintf(html_buffer, sizeof(html_buffer), page, ssid, passphrase);
                memcpy(response + response_count, html_buffer, strlen(html_buffer));
                response_count += strlen(html_buffer);
        } else if (!provisioned) {
                page = 
                "<!doctype html>\n"
                "<html lang=\"pl\">\n"
                "<head>\n"
                "  <meta charset=\"utf-8\" />\n"
                "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\" />\n"
                "  <title>Provisioning Wi‑Fi – ESP32</title>\n"
                "  <style>\n"
                "    body { font-family: sans-serif; background: #0f172a; color: #e5e7eb; display: grid; place-items: center; height: 100vh; margin: 0; }\n"
                "    .card { background: #111827; padding: 20px; border-radius: 12px; width: 100%; max-width: 400px; }\n"
                "    h1 { font-size: 1.4rem; margin-bottom: 10px; }\n"
                "    p.sub { font-size: .95rem; color: #94a3b8; margin-bottom: 20px; }\n"
                "    label { font-size: .9rem; color: #cbd5e1; display: block; margin-bottom: 4px; }\n"
                "    .field { width: 100%; padding: 10px; border-radius: 8px; border: 1px solid rgba(148,163,184,.25); background: #0b1220; color: #e5e7eb; margin-bottom: 14px; }\n"
                "    .actions { display: flex; gap: 10px; justify-content: space-between; }\n"
                "    .btn { border: none; border-radius: 8px; padding: 10px 14px; background: #22d3ee; color: #00212a; font-weight: 600; cursor: pointer; }\n"
                "    .btn.secondary { background: transparent; border: 1px solid rgba(148,163,184,.35); color: #e5e7eb; }\n"
                "    .hint { font-size: .85rem; color: #94a3b8; }\n"
                "  </style>\n"
                "</head>\n"
                "<body>\n"
                "  <main class=\"card\">\n"
                "    <h1>Provisioning Wi‑Fi dla ESP32</h1>\n"
                "    <p class=\"sub\">Podaj dane sieci, z którą ma połączyć się Twoje urządzenie.</p>\n"
                "    <form method=\"POST\" action=\"/\" autocomplete=\"on\">\n"
                "      <label for=\"ssid\">Nazwa sieci (SSID)</label>\n"
                "      <input id=\"ssid\" name=\"ssid\" class=\"field\" placeholder=\"np. Dom_2.4G\" required />\n"
                "      <label for=\"password\">Hasło Wi‑Fi</label>\n"
                "      <input id=\"password\" name=\"password\" class=\"field\" type=\"password\" placeholder=\"haslo podaj cieciu\" required />\n"
                "      <div class=\"hint\">WPA/WPA2 zwykle wymaga min. 8 znaków.</div>\n"
                "      <div class=\"actions\">\n"
                "        <button type=\"submit\" class=\"btn\">Zapisz i połącz</button>\n"
                "        <button type=\"reset\" class=\"btn secondary\">Wyczyść</button>\n"
                "      </div>\n"
                "    </form>\n"
                "  </main>\n"
                "</body>\n"
                "</html>\n";
                memcpy(response + response_count, page, strlen(page));
                response_count += strlen(page);
        }
        printf("file read\n");


        //send response to the client
        send(client_fd, response, response_count, 0);
        printf("response sent\n");

        free(header);
        free(response);
        free(buffer);
        close(client_fd);

        return NULL;
}

void run_http(void *pvParameters) {
        int server_fd;
        struct sockaddr_in serv_addr;

        //create socket file descriptor for server
        if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) { handle_error("socket lezy: "); }
        printf("socket\n");

        //config server socket
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons (PORT);
        serv_addr.sin_addr.s_addr = INADDR_ANY;

        //bind socket to port
        if ((bind(server_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr))) < 0) { handle_error("bind lezy: "); }
        printf("bind\n");

        //listen for connections
        if (listen(server_fd, 10) < 0) handle_error("listen: ");
        printf("listen\n");

        //while loop for handling connections
        while (1) {

                //client info
                struct sockaddr_in client_addr;
                socklen_t client_addr_len = sizeof(client_addr);
                int *client_fd = malloc(sizeof(int));
                printf("client info allocated\n");

                /* accept client connection, this ends the phase of establishing a tcp connection
                 * with now begginning a phase of handling a http request and later forging a response */
                if ((*client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_addr_len)) < 0 ) { perror("accept: "); continue; }
                printf("\nclient connection accepted\n");

                //create thread for handling request and detach it to automatically return resources to the OS after completion
                pthread_t tid;
                pthread_create(&tid, NULL, handle_client, (void *)client_fd);
                pthread_detach(tid);
        }
        close(server_fd);
}
