// Salin file ini menjadi config.h lalu isi. config.h tidak ikut di-commit (.gitignore).
#pragma once

#define WIFI_SSID     "NAMA_WIFI"
#define WIFI_PASS     "PASSWORD_WIFI"

#define MQTT_HOST     "broker.hivemq.com"   // broker publik; ganti dengan broker sendiri bila ada
#define MQTT_PORT     1883
#define MQTT_USER     ""                    // kosongkan jika broker tanpa login
#define MQTT_PASS     ""

// Awalan topik. Harus sama dengan Node-RED dan simulasi Wokwi.
// Ganti dengan nama unik jika memakai broker publik agar tidak tercampur data orang lain.
#define TOPIC_PREFIX  "itts-wsn-kelompok2"
