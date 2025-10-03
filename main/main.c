#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <nvs_flash.h> //non volatile storage
#include <driver/i2s.h>
#include <driver/i2s_std.h>
#include <driver/uart.h>
#include "driver/gpio.h"
#include "esp_log.h"

#include "UART.h"
#include "Config.h"
#include "WIFI.h"
#include "UDP.h"
#include "task_structs.h"

// vTaskDelay(pdMS_TO_TICKS(500));
int ret_hand;
#define handle_error(msg) \
        do {perror(msg); ret_hand = 10;} while(0)


static size_t bytes_read;
SemaphoreHandle_t udp_sem;
  
//i2s config
void i2s_init()
{
    static const i2s_config_t i2s_config = {
        .mode = I2S_MODE_RX | I2S_MODE_MASTER, 
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = 16,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = 0,
        .dma_desc_num = 64, //wczesniej 8 POMOGLA ZMIANA NA 64 NAGLE ZACZELO DZIALAC Z TYM WERSJA Z DWOMA TASKAMI
        .dma_frame_num = 16,
    };

    
    static const i2s_pin_config_t pin_config = {
        .bck_io_num = 14,
        .ws_io_num = 33,
        .data_out_num = I2S_PIN_NO_CHANGE, 
        .data_in_num = 25
    };
    i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
    
    i2s_set_pin(I2S_PORT, &pin_config);
}

void slider_task(void *pvParameters)
{
    int received_size;
    int bytes_read_i2s = 0;
    
    // czekaj na rozmiar pakietu
    while (1)
    {
        uart_write_bytes(UART_NUM_0, "Waiting for input: ", strlen("Waiting for input: "));
        do
        {
            bytes_read_i2s = uart_read_bytes(UART_NUM_0, &received_size, sizeof(int), portMAX_DELAY);
        } while (bytes_read_i2s == 0);
        
        //zmien wartosc udp_packet_size
        udp_packet_size = received_size;
        
        printf("Packet size set: %d\n", udp_packet_size);
    }
}

//When pressed for more than 5s, reboot and nvs_flash_erase
void button_task() {
    gpio_config_t reset_button = {
        .pin_bit_mask = 1ULL << BUTTON,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = 1,
        .pull_down_en = 0,
        .intr_type = GPIO_INTR_DISABLE
    };

    gpio_config(&reset_button);
    int pressed_time = 0;

    while (1) {
        if (gpio_get_level(BUTTON) == 0) {
            pressed_time += 1;
            if (pressed_time >= 5) {
                printf("Kasuje NVS...\n");
                nvs_flash_erase();
                esp_restart();
            }
        } else {
            pressed_time = 0;
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void run_uart_task(SemaphoreHandle_t semaphore)
{
    TaskHandle_t uart_task = NULL;

    slider_task_args_t *slider = malloc(sizeof(slider_task_args_t));

    slider->sem = semaphore;

    xTaskCreate(slider_task, "slider", 4096, slider, tskIDLE_PRIORITY, &uart_task);
}

void run_udp_task(SemaphoreHandle_t semaphore)
{

    TaskHandle_t udp_task = NULL;
    
    udp_send_config_t *udp_config = malloc(sizeof(udp_send_config_t));

    udp_config->udp_size_semaphore = semaphore;
    // udp_config->changed_size = udp_packet_size;
    // udp_config->out_buff = udp_out_buff;
    udp_config->bytes_read = &bytes_read;

    xTaskCreate(udp_transfer, "send_udp", 8192, udp_config, tskIDLE_PRIORITY, &udp_task);

}

void run_button_task() {
    xTaskCreate(button_task, "button", 2048, NULL, 5, NULL);
}

void app_main(void)
{   
    // esp_log_level_set("wifi", ESP_LOG_DEBUG);
    //Inicjalizacja nieulotnej pamięci, w której są konfiguracje
    nvs_flash_init();
    ESP_ERROR_CHECK(nvs_open("storage", NVS_READWRITE, &handle_nvs));
    
    //Task do przycisku resetujacego NVS
    run_button_task();

    printf("HelloWorld "__DATE__", "__TIME__"\n");  
    
    //Połącz z WIFI
    run_APSTA();
    while (!connected) vTaskDelay(pdMS_TO_TICKS(1000));

    //Inicjalizacja I2S 
    i2s_init();
    //Inicjalizacja UART
    uart_init();
    
    //Task do zmiany rozmiaru pakietu UDP
    run_uart_task(udp_sem);    
    //Task do wysyłania pakietów UDP
    run_udp_task(udp_sem);
}