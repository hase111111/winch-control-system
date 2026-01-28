#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"
#include "hardware/adc.h"

#include "pico/cyw43_arch.h"
#include "lwip/pbuf.h"
#include "lwip/udp.h"
#include "lwip/ip_addr.h"

// =========================
// ADC 設定
// =========================
constexpr float VREF = 3.3f;
constexpr float CONV = VREF / 65535.0f;

// =========================
// Wi-Fi 設定
// =========================
const char* SSID = "aterm-5d78db-g";
const char* PASSWORD = "5be63f163242f";

constexpr uint16_t DEST_PORT = 5005;
const char* DEST_IP = "192.168.0.230";

// =========================
// メイン
// =========================
int main() {
    stdio_init_all();
    sleep_ms(2000);

    // ---- ADC 初期化 ----
    adc_init();
    adc_gpio_init(26); // ADC0
    adc_gpio_init(28); // ADC2

    // ---- Wi-Fi 初期化 ----
    if (cyw43_arch_init()) {
        printf("Wi-Fi init failed\n");
        return -1;
    }

    cyw43_arch_enable_sta_mode();

    printf("Connecting to Wi-Fi...\n");
    if (cyw43_arch_wifi_connect_timeout_ms(
            SSID, PASSWORD,
            CYW43_AUTH_WPA2_AES_PSK,
            30000)) {
        printf("Wi-Fi connection failed\n");
        return -1;
    }

    printf("Wi-Fi connected\n");

    // ---- UDP 初期化 ----
    struct udp_pcb* pcb = udp_new();
    if (!pcb) {
        printf("UDP PCB creation failed\n");
        return -1;
    }

    ip_addr_t dest_addr;
    ipaddr_aton(DEST_IP, &dest_addr);

    absolute_time_t next_time = get_absolute_time();

    while (true) {
        // ADC0 (GPIO26)
        adc_select_input(0);
        uint16_t raw0 = adc_read();

        // ADC2 (GPIO28)
        adc_select_input(2);
        uint16_t raw2 = adc_read();

        float v0 = raw0 * CONV;
        float v2 = raw2 * CONV;

        char msg[32];
        int len = snprintf(msg, sizeof(msg), "%.3f,%.3f\n", v0, v2);

        struct pbuf* p = pbuf_alloc(PBUF_TRANSPORT, len, PBUF_RAM);
        if (p) {
            memcpy(p->payload, msg, len);
            udp_sendto(pcb, p, &dest_addr, DEST_PORT);
            pbuf_free(p);
        }

        printf("Sent: %s", msg);

        // 100 Hz
        next_time = delayed_by_ms(next_time, 10);
        sleep_until(next_time);
    }
}
