// Format paket ESP-NOW dari node sensor ke gateway.
// File ini HARUS sama persis di firmware/node/paket.h dan firmware/gateway/paket.h.
#pragma once
#include <stdint.h>

#define PAKET_VERSI 1

enum StatusSensor : uint8_t {
  SENSOR_OK = 0,
  SENSOR_GAGAL = 1,  // DHT22 tidak terbaca (kabel lepas / sensor rusak)
};

typedef struct __attribute__((packed)) {
  uint8_t  versi;        // PAKET_VERSI, untuk menolak paket dari firmware lain
  uint8_t  node_id;      // 1..4 = Ruang A..D
  uint32_t seq;          // nomor urut, naik tiap pengukuran (dipakai menghitung PDR)
  float    suhu;         // °C
  float    kelembapan;   // %RH
  uint16_t vbat_mv;      // tegangan baterai dalam milivolt
  uint8_t  status;       // StatusSensor
} Paket;
