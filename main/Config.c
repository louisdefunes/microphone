#include <stdint.h>
#include "Config.h"
#include <stddef.h>
#include "nvs_flash.h"

//UDP
int udp_packet_size = PACKET_SIZE;
// int16_t *udp_out_buff = NULL;
int16_t udp_out_buff[MAX_SIZE];

//HTTP credentials
int8_t provisioned = 0;
int connected = 0;
char ssid[32];
char passphrase[64];
nvs_handle_t handle_nvs;

