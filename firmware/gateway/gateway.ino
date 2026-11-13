// Gateway / sink WSN — ESP32 menerima paket ESP-NOW dari node sensor
// lalu meneruskannya ke MQTT broker lewat Wi-Fi.
//
// Topik yang diterbitkan:
//   <TOPIC_PREFIX>/node/<id>        JSON data tiap paket
//   <TOPIC_PREFIX>/gateway/status   "online" / "offline" (retained, LWT)
//
// Library: "PubSubClient" (Nick O'Leary).
// Board  : ESP32 Dev Module (Arduino-ESP32 core 2.x atau 3.x).

#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <PubSubClient.h>
#include "paket.h"

#if __has_include("config.h")
#include "config.h"
#else
#warning "config.h belum ada: memakai config.example.h. Salin dan isi config.h."
#include "config.example.h"
#endif

struct PaketMasuk {
  Paket p;
  int8_t rssi;
  uint8_t mac[6];
};

static QueueHandle_t antrean;
WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);
char topikStatus[64];

// Callback ESP-NOW berjalan di task Wi-Fi: cukup salin ke antrean, proses di loop().
#if ESP_ARDUINO_VERSION_MAJOR >= 3
void onRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  const uint8_t *mac = info->src_addr;
  int8_t rssi = info->rx_ctrl ? info->rx_ctrl->rssi : 0;
#else
void onRecv(const uint8_t *mac, const uint8_t *data, int len) {
  int8_t rssi = 0;
#endif
  if (len != sizeof(Paket)) return;
  PaketMasuk m;
  memcpy(&m.p, data, sizeof(Paket));
  if (m.p.versi != PAKET_VERSI) return;
  m.rssi = rssi;
  memcpy(m.mac, mac, 6);
  xQueueSend(antrean, &m, 0);
}

static void sambungWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Menghubungkan Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.printf("\nWi-Fi tersambung, IP %s, kanal %d\n", WiFi.localIP().toString().c_str(),
                WiFi.channel());
}

static void sambungMQTT() {
  static uint32_t terakhirCoba = 0;
  if (mqtt.connected() || millis() - terakhirCoba < 5000) return;
  terakhirCoba = millis();

  char clientId[32];
  snprintf(clientId, sizeof(clientId), "wsn-gw-%06lx", (unsigned long)(ESP.getEfuseMac() & 0xFFFFFF));
  const char *user = strlen(MQTT_USER) ? MQTT_USER : nullptr;
  const char *pass = strlen(MQTT_PASS) ? MQTT_PASS : nullptr;
  if (mqtt.connect(clientId, user, pass, topikStatus, 1, true, "offline")) {
    mqtt.publish(topikStatus, "online", true);
    Serial.println("MQTT tersambung");
  } else {
    Serial.printf("MQTT gagal (rc=%d), coba lagi 5 detik\n", mqtt.state());
  }
}

static void terbitkan(const PaketMasuk &m) {
  char topik[64];
  snprintf(topik, sizeof(topik), "%s/node/%u", TOPIC_PREFIX, m.p.node_id);

  char suhu[12], hum[12];
  if (m.p.status == SENSOR_OK) {
    snprintf(suhu, sizeof(suhu), "%.2f", m.p.suhu);
    snprintf(hum, sizeof(hum), "%.2f", m.p.kelembapan);
  } else {
    strcpy(suhu, "null");
    strcpy(hum, "null");
  }

  char json[200];
  snprintf(json, sizeof(json),
           "{\"node\":%u,\"seq\":%lu,\"suhu\":%s,\"kelembapan\":%s,\"vbat\":%.2f,"
           "\"rssi\":%d,\"status\":%u,\"mac\":\"%02X:%02X:%02X:%02X:%02X:%02X\"}",
           m.p.node_id, (unsigned long)m.p.seq, suhu, hum, m.p.vbat_mv / 1000.0f, m.rssi,
           m.p.status, m.mac[0], m.mac[1], m.mac[2], m.mac[3], m.mac[4], m.mac[5]);

  bool ok = mqtt.publish(topik, json);
  Serial.printf("%s %s %s\n", ok ? "->" : "GAGAL", topik, json);
}

void setup() {
  Serial.begin(115200);
  antrean = xQueueCreate(32, sizeof(PaketMasuk));
  snprintf(topikStatus, sizeof(topikStatus), "%s/gateway/status", TOPIC_PREFIX);

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);  // radio harus selalu aktif agar paket ESP-NOW tidak terlewat
  sambungWiFi();
  Serial.printf("MAC gateway: %s  <- salin ke GATEWAY_MAC di node.ino\n", WiFi.macAddress().c_str());

  if (esp_now_init() != ESP_OK) {
    Serial.println("esp_now_init gagal, restart...");
    delay(2000);
    ESP.restart();
  }
  esp_now_register_recv_cb(onRecv);

  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setKeepAlive(30);
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    // Kanal bisa berubah setelah tersambung ulang; node akan mencarinya sendiri.
    sambungWiFi();
  }
  sambungMQTT();
  mqtt.loop();

  PaketMasuk m;
  while (mqtt.connected() && xQueueReceive(antrean, &m, 0) == pdTRUE) terbitkan(m);
}
