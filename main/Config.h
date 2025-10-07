#include "nvs_flash.h"

// Konfiguracja UART
#define I2S_PORT 0
#define SAMPLE_RATE 44100 // 44100
#define RATIO (SAMPLE_RATE / BUFFER_SIZE)
#define BAUD_RATE 115200 //912600 //115200 
#define BUFFER_SIZE 8820 // for UART

// Konfiguracja UDP
#define UDP_PORT 2500
#define PACKET_SIZE 1024 // for UDP
#define MAX_SIZE 32000 // 65536
#define SERVER_IP_ADDR "192.168.8.101" //"192.168.8.105" //"192.168.0.124"
#define SERVER_HOSTNAME "serveriuseforesp.ddns.net"

// Konfiguracja WiFi STA
#define WIFI_SSID "wifi" 
#define WIFI_PASSWD "Fdab5gmgGdmBrLe2"
#define RETRYS 10
#define LEDGPIO 13

//Konfiguracja WiFi AP
#define AP_SSID "ESP32-AP"
#define AP_PASSWD "12345678"
#define MAX_CONN 1

extern int udp_packet_size;
// extern int16_t *udp_out_buff;
extern int16_t udp_out_buff[MAX_SIZE];

// Konfiguracja HTTP
#define PORT 80
#define PORT_STRING ":80/"
#define B_SIZE 1024

//Inne
#define BUTTON GPIO_NUM_18

extern char ssid[32];
extern char passphrase[64];
extern int8_t provisioned;
extern int connected;
extern nvs_handle_t handle_nvs;
