// Simulasi Wokwi: 4 node sensor + gateway dalam SATU ESP32.
//
// Di perangkat nyata ada 5 ESP32 (4 node + 1 gateway) yang berbicara lewat ESP-NOW.
// Wokwi menjalankan satu board per proyek, jadi simulasi ini menggabungkan keduanya:
//   - 4 DHT22 = sensor di Ruang A..D (klik sensornya untuk mengubah suhu/kelembapan)
//   - bagian "NODE" membuat Paket yang sama persis dengan firmware/node
//   - bagian "GATEWAY" menerbitkan JSON yang sama persis dengan firmware/gateway
// Jadi Node-RED tidak bisa membedakan data simulasi dengan data perangkat nyata.

#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>

// ================== KONFIGURASI ==================
#define WIFI_SSID     "Wokwi-GUEST"
#define WIFI_PASS     ""
#define MQTT_HOST     "broker.hivemq.com"
#define MQTT_PORT     1883
#define TOPIC_PREFIX  "itts-wsn-kelompok2"   // harus sama dengan Node-RED
#define INTERVAL_MS   10000                  // perangkat nyata: 5 menit; dipercepat untuk demo
// =================================================

// ---- Sama dengan firmware/node/paket.h ----
#define PAKET_VERSI 1
enum StatusSensor : uint8_t { SENSOR_OK = 0, SENSOR_GAGAL = 1 };
typedef struct __attribute__((packed)) {
  uint8_t versi;
  uint8_t node_id;
  uint32_t seq;
  float suhu;
  float kelembapan;
  uint16_t vbat_mv;
  uint8_t status;
} Paket;
// -------------------------------------------

struct NodeVirtual {
  uint8_t id;
  const char *ruang;
  DHT dht;
  uint32_t seq;
  uint16_t vbat_mv;
  uint8_t mac[6];
};

NodeVirtual nodes[] = {
  {1, "Ruang A", DHT(15, DHT22), 0, 4150, {0x24, 0x0A, 0xC4, 0x00, 0x00, 0x01}},
  {2, "Ruang B", DHT(4, DHT22), 0, 4120, {0x24, 0x0A, 0xC4, 0x00, 0x00, 0x02}},
  {3, "Ruang C", DHT(16, DHT22), 0, 4080, {0x24, 0x0A, 0xC4, 0x00, 0x00, 0x03}},
  {4, "Ruang D", DHT(17, DHT22), 0, 4010, {0x24, 0x0A, 0xC4, 0x00, 0x00, 0x04}},
};
const int JUMLAH_NODE = sizeof(nodes) / sizeof(nodes[0]);

WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);
char topikStatus[64];

// ================= NODE =================
Paket ukur(NodeVirtual &n) {
  float suhu = n.dht.readTemperature();
  float hum = n.dht.readHumidity();

  // Baterai turun pelan (simulasi), kembali penuh di 3,3 V.
  n.vbat_mv = n.vbat_mv > 3300 ? n.vbat_mv - 1 : 4200;

  Paket p = {};
  p.versi = PAKET_VERSI;
  p.node_id = n.id;
  p.seq = ++n.seq;
  p.status = (isnan(suhu) || isnan(hum)) ? SENSOR_GAGAL : SENSOR_OK;
  p.suhu = p.status == SENSOR_OK ? suhu : 0;
  p.kelembapan = p.status == SENSOR_OK ? hum : 0;
  p.vbat_mv = n.vbat_mv;
  return p;
}

// ================ GATEWAY ================
void terbitkan(const Paket &p, const uint8_t *mac, int8_t rssi) {
  char topik[64];
  snprintf(topik, sizeof(topik), "%s/node/%u", TOPIC_PREFIX, p.node_id);

  char suhu[12], hum[12];
  if (p.status == SENSOR_OK) {
    snprintf(suhu, sizeof(suhu), "%.2f", p.suhu);
    snprintf(hum, sizeof(hum), "%.2f", p.kelembapan);
  } else {
    strcpy(suhu, "null");
    strcpy(hum, "null");
  }

  char json[200];
  snprintf(json, sizeof(json),
           "{\"node\":%u,\"seq\":%lu,\"suhu\":%s,\"kelembapan\":%s,\"vbat\":%.2f,"
           "\"rssi\":%d,\"status\":%u,\"mac\":\"%02X:%02X:%02X:%02X:%02X:%02X\"}",
           p.node_id, (unsigned long)p.seq, suhu, hum, p.vbat_mv / 1000.0f, rssi, p.status,
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

  bool ok = mqtt.publish(topik, json);
  Serial.printf("%s %s %s\n", ok ? "->" : "GAGAL", topik, json);
}

void sambungMQTT() {
  while (!mqtt.connected()) {
    char clientId[32];
    snprintf(clientId, sizeof(clientId), "wsn-wokwi-%04lx", (unsigned long)random(0xFFFF));
    Serial.print("Menghubungkan MQTT... ");
    if (mqtt.connect(clientId, nullptr, nullptr, topikStatus, 1, true, "offline")) {
      mqtt.publish(topikStatus, "online", true);
      Serial.println("ok");
    } else {
      Serial.printf("gagal (rc=%d), coba lagi\n", mqtt.state());
      delay(2000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  randomSeed(esp_random());
  for (auto &n : nodes) n.dht.begin();
  snprintf(topikStatus, sizeof(topikStatus), "%s/gateway/status", TOPIC_PREFIX);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Menghubungkan Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
    Serial.print(".");
  }
  Serial.println(" ok");

  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  sambungMQTT();
}

void loop() {
  static uint32_t terakhir = 0;
  if (!mqtt.connected()) sambungMQTT();
  mqtt.loop();

  if (terakhir != 0 && millis() - terakhir < INTERVAL_MS) return;
  terakhir = millis();

  for (auto &n : nodes) {
    Paket p = ukur(n);                       // node: baca sensor & kemas paket
    int8_t rssi = -45 - (int8_t)random(0, 30);  // RSSI tiruan (-45..-74 dBm)
    terbitkan(p, n.mac, rssi);               // gateway: teruskan ke MQTT
    mqtt.loop();
  }
}
