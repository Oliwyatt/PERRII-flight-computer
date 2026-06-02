#include <stdint.h>


#ifndef TELEMETRY_H
#define TELEMETRY_H

typedef enum {
    PACKET_GPS,
    PACKET_BARO,
    PACKET_IMU,
    PACKET_GAS
} packet_type_t;

typedef struct __attribute__((packed)) {
    float latitude;
    float longitude;
    float altitude;
    float speed;
    int   satellites;
} gps_data_t;

typedef struct __attribute__((packed)) {
    float altitude;
    float pressure;
    float temperature;
} baro_data_t;

typedef struct __attribute__((packed)) {
    float accel_x;
    float accel_y;
    float accel_z;
    float gyro_x;
    float gyro_y;
    float gyro_z;
} imu_data_t;

typedef struct __attribute__((packed)) {
    char gas_type;
    float concentration;
} gas_data_t;

typedef struct __attribute__((packed)) {
    packet_type_t type;   // tells the radio task what's inside
    uint32_t      timestamp;
    union {
        gps_data_t  gps;
        baro_data_t baro;
        imu_data_t  imu;
        gas_data_t  gas;
    } data;
} telemetry_packet_t;

#endif // TELEMETRY_H
