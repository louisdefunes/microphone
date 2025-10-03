    #include <freertos/FreeRTOS.h>
    #include <freertos/semphr.h>

    typedef struct {
        
        SemaphoreHandle_t udp_size_semaphore;
        // int changed_size;
        // int16_t *out_buff;
        size_t *bytes_read;

    } udp_send_config_t;

    typedef struct {
        // int base_packet_size;
        SemaphoreHandle_t sem;
    } slider_task_args_t;