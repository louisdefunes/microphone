#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <driver/i2s.h>
#include <driver/i2s_std.h>
#include <freertos/queue.h>
#include <esp_netif.h>

#include "Config.h"
#include "task_structs.h"

int ret;
#define handle_error(msg) \
        do {perror(msg); ret = 10;} while(0)

int initialize_udp(struct sockaddr_in *client_address,
                   struct sockaddr_in *server_address,
                   char *ip)
    {
    esp_netif_ip_info_t ip_info;
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (netif == NULL) {
        printf("Nie udało się uzyskać uchwytu do interfejsu WIFI_STA_DEF\n");
        return 0;
    }

    //to jest po prostu memcpy(netif do ip_info)
    if (esp_netif_get_ip_info(netif, &ip_info) != ESP_OK) {
        printf("Nie udało się pobrać informacji o IP\n");
        return 0;
    }

    // wypelnianie struktur adresów klienta(esp32) i serwera
    client_address->sin_family=AF_INET;
    client_address->sin_port=htons(UDP_PORT);
    client_address->sin_addr.s_addr=ip_info.ip.addr;

    server_address->sin_family=AF_INET;
    server_address->sin_port=htons(UDP_PORT);
    server_address->sin_addr.s_addr=inet_addr(SERVER_IP_ADDR);

    // socket i przypisanie adresu
    int sock_serv = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_serv == -1) handle_error("server socket:");

    if (bind(sock_serv, (struct sockaddr *) client_address, sizeof(client_address))==-1 ) handle_error("bind: ");

    return sock_serv;
}

void send_through_udp(int sock_serv,
                      int16_t *buffer, 
                      int size,
                      struct sockaddr_in server_address)
{
    ssize_t bytes_sent = sendto(sock_serv, (char *)buffer, size, 0, (struct sockaddr *) &server_address, sizeof(server_address));
    if (bytes_sent==-1) handle_error("sendto: ");
    // printf("Wyslano %d bajtow\n", bytes_sent);
}

void udp_transfer(void *pvParameters)
{
    udp_send_config_t *udp_conf = (udp_send_config_t *)pvParameters;
    struct sockaddr_in server_address, client_address;
    memset(&server_address, 0, sizeof(server_address));
    memset(&client_address, 0, sizeof(client_address));

    int socket = initialize_udp(&client_address, &server_address, SERVER_IP_ADDR);

    while (1) 
    {
        memset(udp_out_buff, 0, sizeof(udp_out_buff));
        esp_err_t i2s_ret = i2s_read(I2S_NUM_0, udp_out_buff, udp_packet_size*sizeof(int16_t), udp_conf->bytes_read, pdMS_TO_TICKS(100));

        if (i2s_ret != ESP_OK) handle_error("i2s_read: ");
        // printf("\nI2S read %d bytes\n", *(udp_conf->bytes_read));
        // for (int i = 0; i < *(udp_conf->bytes_read) / sizeof(int16_t); i++) {
        //     printf("%d ", udp_out_buff[i]);
        // }
        send_through_udp(socket, udp_out_buff, *(udp_conf->bytes_read), server_address);
        // send_through_udp(socket, udp_out_buff, udp_packet_size, server_address);
    }
    close(socket);
}
 