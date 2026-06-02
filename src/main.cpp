#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/uart.h"
#include "telemetry.h"
#include "esp_log.h"
#include "minmea.h"
#include "lora.h"

static const char* TAG = "MAIN";

#define GPS_UART_NUM    UART_NUM_1
#define GPS_TX_PIN      17
#define GPS_RX_PIN      18
#define GPS_BAUD_RATE   9600
#define GPS_BUF_SIZE    1024

QueueHandle_t telemetry_queue;
QueueHandle_t logging_queue;


void gps_task(void *pvParameters) {
    uart_config_t uart_config = {};
    uart_config.baud_rate = GPS_BAUD_RATE;
    uart_config.data_bits = UART_DATA_8_BITS;
    uart_config.parity    = UART_PARITY_DISABLE;
    uart_config.stop_bits = UART_STOP_BITS_1;
    uart_config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;

    uart_driver_install(GPS_UART_NUM, GPS_BUF_SIZE * 2, 0, 0, NULL, 0);
    uart_param_config(GPS_UART_NUM, &uart_config);
    uart_set_pin(GPS_UART_NUM, GPS_TX_PIN, GPS_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    ESP_LOGI(TAG, "GPS UART initialised");

    uint8_t data[GPS_BUF_SIZE];
    char line[128];
    int line_pos = 0;
    telemetry_packet_t packet;

    while (1) {
        int len = uart_read_bytes(GPS_UART_NUM, data, GPS_BUF_SIZE - 1, pdMS_TO_TICKS(100));

        for (int i = 0; i < len; i++) {
            char c = (char)data[i];

            if (c == '\n') {
                line[line_pos] = '\0';
                line_pos = 0;

                switch (minmea_sentence_id(line, false)) {
                    case MINMEA_SENTENCE_RMC: {
                        struct minmea_sentence_rmc frame;
                        if (minmea_parse_rmc(&frame, line)) {
                            ESP_LOGI("GPS", "Position: lat=%.6f lon=%.6f speed=%.2f knots",
                                minmea_tocoord(&frame.latitude),
                                minmea_tocoord(&frame.longitude),
                                minmea_tofloat(&frame.speed));
                            memset(&packet, 0, sizeof(telemetry_packet_t));
                            packet.type = PACKET_GPS;
                            packet.timestamp = esp_log_timestamp();
                            packet.data.gps.latitude  = minmea_tocoord(&frame.latitude);
                            packet.data.gps.longitude = minmea_tocoord(&frame.longitude);
                            packet.data.gps.speed     = minmea_tofloat(&frame.speed);
                            xQueueSend(telemetry_queue, &packet, pdMS_TO_TICKS(100));
                        }
                        break;
                    }
                    default:
                        break;
                }
            } else if (c != '\r') {
                if (line_pos < 127) {
                    line[line_pos++] = c;
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

/*
void logging_task(void *pvParameters) {
    telemetry_packet_t packet;

    while (1) {
        if (xQueueReceive(logging_queue, &packet, portMAX_DELAY)) {
            eeprom_write(&packet);

            xQueueSend(radio_queue, &packet, pdMS_TO_TICKS(100));
        }
    }
}
*/

void radio_task(void *pvParameters) {
    telemetry_packet_t packet;
    lora_init();
    lora_enable_crc();

    while (1) {
        // blocks here until something arrives in the queue
        if (xQueueReceive(telemetry_queue, &packet, portMAX_DELAY)) {
            switch (packet.type) {
                case PACKET_GPS:
                    ESP_LOGI("RADIO", "Sending GPS: lat=%.6f lon=%.6f",
                        packet.data.gps.latitude,
                        packet.data.gps.longitude);
                    break;
                case PACKET_BARO:
                    ESP_LOGI("RADIO", "Sending Baro: alt=%.2fm",
                        packet.data.baro.altitude);
                    break;
                case PACKET_IMU:
                    ESP_LOGI("RADIO", "Sending IMU: accel=(%.2f, %.2f, %.2f) gyro=(%.2f, %.2f, %.2f)",
                        packet.data.imu.accel_x,
                        packet.data.imu.accel_y,
                        packet.data.imu.accel_z,
                        packet.data.imu.gyro_x,
                        packet.data.imu.gyro_y,
                        packet.data.imu.gyro_z);
                    break;
                case PACKET_GAS:
                    ESP_LOGI("RADIO", "Sending Gas data: %c, %.2f",
                        packet.data.gas.gas_type,
                        packet.data.gas.concentration);
                    break;
                default:
                    break;
            }
            uint8_t buffer[sizeof(telemetry_packet_t)];
            memcpy(buffer, &packet, sizeof(telemetry_packet_t));
            lora_send_packet(buffer, sizeof(telemetry_packet_t));
            memset(&packet, 0, sizeof(telemetry_packet_t));
        }
    }
}

void barometer_task(void *pvParameters) {
    
}

extern "C" void app_main(void) {
    logging_queue = xQueueCreate(20, sizeof(telemetry_packet_t));
    telemetry_queue = xQueueCreate(20, sizeof(telemetry_packet_t));

    xTaskCreate(gps_task, "gps_task", 4096, NULL, 5, NULL);
    //xTaskCreate(logging_task, "logging_task", 4096, NULL, 5, NULL);
    xTaskCreate(radio_task, "radio_task", 4096, NULL, 4, NULL);
}