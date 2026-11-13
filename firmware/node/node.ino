// Node sensor WSN — ESP32 + DHT22, kirim via ESP-NOW, lalu deep sleep.
//
// Siklus: bangun -> nyalakan DHT22 -> baca suhu & kelembapan -> baca baterai
//         -> kirim paket ke gateway (unicast, dengan ACK) -> deep sleep 5 menit.
//
// Library: "DHT sensor library" (Adafruit) + "Adafruit Unified Sensor".
// Board  : ESP32 Dev Module (Arduino-ESP32 core 2.x atau 3.x).

#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <DHT.h>
#include "paket.h"

// ================== KONFIGURASI (ubah per node) ==================
#define NODE_ID         1    // 1 = Ruang A, 2 = Ruang B, 3 = Ruang C, 4 = Ruang D
#define INTERVAL_MENIT  5    // jeda antar pengukuran

#define PIN_DHT_DATA    4    // DATA DHT22 (pakai pull-up 10k ke 3V3 jika modul tanpa resistor)
#define PIN_DHT_DAYA    25   // VCC DHT22 diambil dari GPIO agar sensor ikut mati saat tidur
#define PIN_VBAT        34   // pembagi tegangan 100k/100k dari baterai 18650 (ADC1)
#define FAKTOR_VBAT     2.0f // kebalikan rasio pembagi tegangan

// MAC address gateway: lihat Serial Monitor gateway saat boot ("MAC gateway: ...").
static uint8_t GATEWAY_MAC[6] = {0x24, 0x0A, 0xC4, 0x00, 0x00, 0x00};
// =================================================================

// Disimpan di memori RTC: tetap ada selama deep sleep, hilang jika baterai dicabut.
RTC_DATA_ATTR uint32_t seq = 0;
RTC_DATA_ATTR uint8_t kanalTersimpan = 1;  // kanal Wi-Fi tempat gateway terakhir ditemukan

DHT dht(PIN_DHT_DATA, DHT22);
volatile int8_t hasilKirim = -1;  // -1 menunggu, 1 sukses (ACK), 0 gagal

#if defined(ESP_IDF_VERSION_MAJOR) && \
    (ESP_IDF_VERSION_MAJOR > 5 || (ESP_IDF_VERSION_MAJOR == 5 && ESP_IDF_VERSION_MINOR >= 5))
void onSent(const wifi_tx_info_t *info, esp_now_send_status_t status) {
#else
void onSent(const uint8_t *mac, esp_now_send_status_t status) {
#endif
  hasilKirim = (status == ESP_NOW_SEND_SUCCESS) ? 1 : 0;
}

static void setKanal(uint8_t ch) {
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous(false);
}

static bool kirimDiKanal(const Paket &p, uint8_t ch) {
  setKanal(ch);
  hasilKirim = -1;
  if (esp_now_send(GATEWAY_MAC, (const uint8_t *)&p, sizeof(p)) != ESP_OK) return false;
  uint32_t t0 = millis();
  while (hasilKirim < 0 && millis() - t0 < 100) delay(1);
  return hasilKirim == 1;
}

static void tidur() {
  Serial.printf("Tidur %d menit...\n", INTERVAL_MENIT);
  Serial.flush();
  esp_sleep_enable_timer_wakeup((uint64_t)INTERVAL_MENIT * 60ULL * 1000000ULL);
  esp_deep_sleep_start();
}

void setup() {
  Serial.begin(115200);

  // 1) Ukur
  pinMode(PIN_DHT_DAYA, OUTPUT);
  digitalWrite(PIN_DHT_DAYA, HIGH);
  dht.begin();
  delay(2000);  // DHT22 butuh waktu stabil setelah diberi daya

  float suhu = dht.readTemperature();
  float hum = dht.readHumidity();
  if (isnan(suhu) || isnan(hum)) {
    delay(2100);
    suhu = dht.readTemperature(false, true);  // (Celsius, paksa baca ulang)
    hum = dht.readHumidity(true);
  }
  digitalWrite(PIN_DHT_DAYA, LOW);

  Paket p = {};
  p.versi = PAKET_VERSI;
  p.node_id = NODE_ID;
  p.seq = ++seq;
  p.status = (isnan(suhu) || isnan(hum)) ? SENSOR_GAGAL : SENSOR_OK;
  p.suhu = p.status == SENSOR_OK ? suhu : 0;
  p.kelembapan = p.status == SENSOR_OK ? hum : 0;
  p.vbat_mv = (uint16_t)(analogReadMilliVolts(PIN_VBAT) * FAKTOR_VBAT);

  Serial.printf("Node %d seq %lu: %.1f C, %.1f %%, %u mV, status %u\n", NODE_ID,
                (unsigned long)p.seq, p.suhu, p.kelembapan, p.vbat_mv, p.status);

  // 2) Kirim
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  if (esp_now_init() != ESP_OK) {
    Serial.println("esp_now_init gagal");
    tidur();
  }
  esp_now_register_send_cb(onSent);

  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, GATEWAY_MAC, 6);
  peer.channel = 0;  // 0 = ikut kanal aktif saat ini
  peer.encrypt = false;
  esp_now_add_peer(&peer);

  // Coba 3x di kanal terakhir; jika gagal, cari gateway di kanal 1..13
  // (kanal gateway mengikuti kanal router Wi-Fi-nya).
  bool terkirim = false;
  for (int i = 0; i < 3 && !terkirim; i++) terkirim = kirimDiKanal(p, kanalTersimpan);
  for (uint8_t ch = 1; ch <= 13 && !terkirim; ch++) {
    if (ch == kanalTersimpan) continue;
    if (kirimDiKanal(p, ch)) {
      kanalTersimpan = ch;
      terkirim = true;
    }
  }
  Serial.printf(terkirim ? "Terkirim di kanal %u\n" : "Gagal kirim (gateway tidak menjawab), kanal %u\n",
                kanalTersimpan);

  // 3) Tidur
  tidur();
}

void loop() {}
