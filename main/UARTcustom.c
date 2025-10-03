#include <driver/uart.h>
#include <stdio.h>
#include <string.h>
#include <driver/i2s.h>
#include <driver/i2s_std.h>
#include <freertos/queue.h>

#include "Config.h"
// int sec_count, rec_mode;
// static size_t bytes_read;
// int16_t out_buf[BUFFER_SIZE];

char word[16];
const char *stop_msg = "STOP\n";
const char *start_cmd = "START\n";
  
void uart_init()
{
    //uart config
    uart_config_t uart_conf = {
        .baud_rate = BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
     };

    //inicjalizacja uart
    uart_param_config(UART_NUM_0, &uart_conf);
    uart_set_pin(UART_NUM_0, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(UART_NUM_0, 256, BUFFER_SIZE, 0, NULL, 0);
}

void start_uart(int sec_count, size_t bytes_read, int rec_mode, int16_t *out_buf, char *stop_msg, char *start_cmd, char *word)
{
    uart_init();    
    //czekaj na sygnal START
    do
    {
        uart_read_bytes(UART_NUM_0, word, sizeof(word), pdMS_TO_TICKS(500));
    } while (strncmp(word, start_cmd, strlen(start_cmd)) != 0);
    
    uart_write_bytes(UART_NUM_0, "Odebrano START\n", strlen("Odebrano START\n"));

    //odczytaj dlugosc nagrania
    do
    {
        bytes_read = uart_read_bytes(UART_NUM_0, &sec_count, sizeof(int), pdMS_TO_TICKS(500));
    } while (bytes_read != 4);

    uart_write_bytes(UART_NUM_0, "Odebrano czas:", strlen("Odebrano czas:\n"));
    uart_write_bytes(UART_NUM_0, (char *)&sec_count, sizeof(int));

    if (sec_count == 0xffffffff){
        rec_mode = 1;
    }
    int repeats = sec_count * RATIO;
    while (1)
    {
        if (repeats > 0)
        {
            // Wysłanie przez UDP
            // udp_transfer((char *)buffer);
            
            //zeruj to
            bytes_read = 0;
            memset(&out_buf, 0, sizeof(out_buf));
            
            esp_err_t ret = i2s_read(I2S_NUM_0, out_buf, sizeof(out_buf), &bytes_read, pdMS_TO_TICKS(500)); //sizeof(raw_buf) jezeli bufory alokowane na stosie, przez malloc sa na stercie, wiec sizeof(raw_buf) zwroci rozmiar wskaznika
            
            if (ret == ESP_OK && bytes_read > 0)
            {
                uart_write_bytes(UART_NUM_0, (const char *)out_buf, bytes_read);
                // ESP_ERROR_CHECK(uart_wait_tx_done(UART_NUM_0, portMAX_DELAY));
            }
            if (rec_mode == 1) {
                continue;
            }
            repeats -= 1;
        }
        else
        {
            //wyslij sygnal STOP po ostatnim powtorzeniu
            uart_write_bytes(UART_NUM_0, stop_msg, strlen(stop_msg));
            break;
        }
    }
}