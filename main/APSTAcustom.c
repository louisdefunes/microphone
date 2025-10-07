#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h" //for delay,mutexs,semphrs rtos operations
#include "esp_system.h" //esp_init funtions esp_err_t 
#include "esp_wifi.h" //esp_wifi_init functions and wifi operations
#include "esp_log.h" //for showing logs
#include "esp_event.h" //for wifi event
#include "lwip/err.h" //light weight ip packets error handling
#include "lwip/sys.h" //system applications for light weight ip apps
#include "lwip/ip4_addr.h" // for inet_ntoa_r
#include "lwip/inet.h"     // for inet_ntoa_r
#include "nvs_flash.h"

#include "Config.h"
#include <driver/gpio.h>
#include "HTTP.h"

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1
#define HTTP_SERVER_START BIT2

static EventGroupHandle_t s_wifi_event_group;
int retry_num = 0;
esp_netif_t *ran_ap = NULL;
esp_netif_t *sta;
TaskHandle_t http_task = NULL;


void led_blink(int led, int level)
{
    gpio_reset_pin(led);
    gpio_set_direction(led, GPIO_MODE_OUTPUT);
    gpio_set_level(led, level);
}

/*Event Group służy do czekania na określone 'events' i definiowania reakcji na nie z danej grupy; tutaj: 's_wifi_event_group'. 
  xEventGroupSetBits służy do tego, żeby samodzielnie zdefiniować zdarzenie i obsłużyć je, tak jak np ustawiono bit HTTP_SERVER_START
  który, jeżeli ustawiony, rozpoczyna funkcje run_http */
static void wifi_event_handler(void *ev_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT) {
        switch (event_id)
        {
        case WIFI_EVENT_STA_START:
            printf("%s: connecting...\n", ssid);
            esp_wifi_connect();
            // assert(ret == ESP_OK);
            break;
        case WIFI_EVENT_STA_DISCONNECTED:
            led_blink(LEDGPIO, 0);
            printf("WIFI SSID: %s: Disconnected!\n", ssid);
            if (retry_num<RETRYS) {esp_wifi_connect(); retry_num++; printf("Trying %d time to reconnect\n", retry_num);}
            if (retry_num == RETRYS) xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
            break;
        case WIFI_EVENT_STA_CONNECTED:
            connected = 1;
            break;
        case WIFI_EVENT_AP_STACONNECTED:
            printf("Station connected to AP\n");
            xEventGroupSetBits(s_wifi_event_group, HTTP_SERVER_START);
            break;
        default:
            printf("Some other event: %ld\n", event_id);
            break;
        }
    }
    if (event_base == IP_EVENT) {
        switch (event_id) {
            case IP_EVENT_STA_GOT_IP:
                xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
                printf("Succesfully connected to SSID and got IP\n");
                led_blink(LEDGPIO, 1);
                break;
            default:
                printf("Some other event: %ld\n", event_id);
                break;
        }
    }
}


void dhcp_set_captiveportal_url(void) {
    // get the IP of the access point to redirect to
    esp_netif_ip_info_t ip_info;
    esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_AP_DEF"), &ip_info);

    char ip_addr[16];
    inet_ntoa_r(ip_info.ip.addr, ip_addr, 16);
    printf("Set up softAP with IP: %s\n", ip_addr);

    // turn the IP into a URI
    char* captiveportal_uri = malloc(32 * sizeof(char));
    assert(captiveportal_uri && "Failed to allocate captiveportal_uri");
    strcpy(captiveportal_uri, "http://");
    strcat(captiveportal_uri, ip_addr);
    strcat(captiveportal_uri, PORT_STRING);

    // get a handle to configure DHCP with
    esp_netif_t* netif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");

    // set the DHCP option 114
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_netif_dhcps_stop(netif));
    ESP_ERROR_CHECK(esp_netif_dhcps_option(netif, ESP_NETIF_OP_SET, ESP_NETIF_CAPTIVEPORTAL_URI, captiveportal_uri, strlen(captiveportal_uri)));
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_netif_dhcps_start(netif));
}

esp_netif_t *wifi_init_ap() {
    esp_netif_t *wifi_ap = esp_netif_create_default_wifi_ap();

    wifi_config_t ap_conf = { 0 };
    strncpy((char *)ap_conf.ap.ssid, AP_SSID, sizeof(ap_conf.ap.ssid));
    strncpy((char *)ap_conf.ap.password, AP_PASSWD, sizeof(ap_conf.ap.password));
    ap_conf.ap.max_connection = MAX_CONN;
    ap_conf.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;
    
    esp_wifi_set_config(WIFI_IF_AP, &ap_conf);
    printf("WiFi AP configuration set SSID:%s password:%s\n", ap_conf.ap.ssid, ap_conf.ap.password);
    
    return wifi_ap;
}

esp_netif_t *wifi_init_sta(char *ssid, char *password) {
    esp_netif_t *wifi_sta = esp_netif_create_default_wifi_sta();

    wifi_config_t sta_conf = { 0 };
    strncpy((char *)sta_conf.sta.ssid, ssid, sizeof(sta_conf.sta.ssid) - 1);
    strncpy((char *)sta_conf.sta.password, password, sizeof(sta_conf.sta.password) - 1);
    sta_conf.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    esp_wifi_set_config(WIFI_IF_STA, &sta_conf);
    printf("WiFi STA configuration set SSID:%s password:%s\n", sta_conf.sta.ssid, sta_conf.sta.password);

    return wifi_sta;
}

esp_netif_t *run_ap() {
    /* rozpoczynamy działanie w trybie AP, czekamy na połączenie klienta, co rozpoczyna rozgłaszanie formularza http */
    esp_wifi_set_mode(WIFI_MODE_AP);
    esp_netif_t *ap = wifi_init_ap();
    esp_wifi_start();
    
    //ustawienie flagi 114 podczas rozdawania adresu, nie daje zamierzonego efektu
    dhcp_set_captiveportal_url();

    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group, 
                                            HTTP_SERVER_START,
                                            pdFALSE,
                                            pdFALSE,
                                            portMAX_DELAY);

    if (bits & HTTP_SERVER_START) {
        printf("HTTP server starting...\n");
        xTaskCreate(run_http, "http server", 1024, NULL, tskIDLE_PRIORITY, &http_task);
    }

    return ap;
}

int run_APSTA(){
    //Konfiguracja handlera, loop_event_handlera, konfiguracji połączenia, podjęcie próby podłączenia
    s_wifi_event_group = xEventGroupCreate();
    esp_netif_init();//inicjalizacja stosu TCP/IP poprzez LwIP, zmienia zmienną s_netif_initialized na true/network interface initialization
    esp_event_loop_create_default();

    //obsługa zdarzeń z kategorii WIFI_EVENT przez nasz handler
    esp_event_handler_register(WIFI_EVENT,
                                ESP_EVENT_ANY_ID,
                                wifi_event_handler,
                                NULL);

    //obsługa zdarzeń z kategorii IP_EVENT...
    esp_event_handler_register(IP_EVENT,
                                IP_EVENT_STA_GOT_IP, 
                                wifi_event_handler, 
                                NULL);

    wifi_init_config_t wifi_init = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&wifi_init);

    //Start w trybie AP jezeli nie ma provisioned w nvs
    esp_err_t ret = nvs_get_i8(handle_nvs, "provisioned", &provisioned);
    assert(ret == ESP_OK || ret == ESP_ERR_NVS_NOT_FOUND);
    if (!provisioned) {
        ran_ap = run_ap();
    }

    //dopóki task http nie ustawił zmiennej provisioned, nie wykonuj dalej
    while (!provisioned) vTaskDelay(pdMS_TO_TICKS(1000));

    printf("koniec AP\n");
    
    // Jeżeli tryb AP byl wczesniej włączony (ran_ap != NULL), to wyłącz i przełącz na STA 
    if (ran_ap != NULL) {
        esp_wifi_stop();
        vTaskDelay(pdMS_TO_TICKS(500));

        esp_wifi_set_mode(WIFI_MODE_NULL);
        vTaskDelay(pdMS_TO_TICKS(100));

        esp_netif_destroy(ran_ap);
        vTaskDelay(pdMS_TO_TICKS(100));

        // printf("SSID: %s\n", ssid);
        // printf("PASSWORD: %s\n", passphrase);

        // for (int i = 0; i < sizeof(ssid); i++) {
        //     printf("%c", ssid[i]);
        // }
        // printf("\n");
        // for (int i = 0; i < sizeof(passphrase); i++) {
        //     printf("%c", passphrase[i]);
        // }
        // printf("\n");
        
        // Ustawienie WIFI_MODE_STA musi byc przed uzyskaniem handlera esp_netif_t *sta, bo inaczej sie wykrzaczy
        esp_wifi_set_mode(WIFI_MODE_STA);
        vTaskDelay(pdMS_TO_TICKS(100));
        esp_netif_t *sta = wifi_init_sta(ssid, passphrase);

    } else {
        //Jeżeli AP nie był aktywowany, zacznij od STA
        esp_wifi_set_mode(WIFI_MODE_STA);

        size_t required_size = 0;

        //Pobierz zmienne wartości SSID i PASSPHRASE z pamięci NVS
        nvs_get_str(handle_nvs, "ssid", NULL, &required_size);
        char *nvs_ssid = malloc(required_size);        
        nvs_get_str(handle_nvs, "ssid", nvs_ssid, &required_size);

        // for (int i = 0; i < required_size; i++) {
        //     printf("%c", nvs_ssid[i]);
        // }
        // printf("\n");

        snprintf(ssid, required_size, "%s", nvs_ssid);

        required_size = 0;

        nvs_get_str(handle_nvs, "password", NULL, &required_size);
        char *nvs_pass = malloc(required_size);
        nvs_get_str(handle_nvs, "password", nvs_pass, &required_size);
        
        // printf("SSID: %s\n", nvs_ssid);
        // printf("PASSWORD: %s\n", nvs_pass);

        // for (int i = 0; i < required_size; i++) {
        //     printf("%c", nvs_pass[i]);
        // }
        // printf("\n");

        esp_netif_t *sta = wifi_init_sta(nvs_ssid, nvs_pass);

        free(nvs_ssid);
        free(nvs_pass);
    }
    
    //Przełączenie w tryb STA    
    esp_wifi_start();

    //Wyświetlenie adresu IP
     EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group, 
                                            WIFI_CONNECTED_BIT,
                                            pdFALSE,
                                            pdFALSE,
                                            portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT) {
        esp_netif_ip_info_t ip_info;
        esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
        if (netif == NULL) {
                printf("Nie udało się uzyskać uchwytu do interfejsu WIFI_STA_DEF\n");
                return 2;
            }
        if (esp_netif_get_ip_info(netif, &ip_info) != ESP_OK) {
                printf("Nie udało się pobrać informacji o IP\n");
                return 2;
            }
        printf("Got IP: " IPSTR "\n", IP2STR(&ip_info.ip));
    }
    
    return 1;
}
