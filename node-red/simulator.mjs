// Simulator lokal 4 node + gateway (pengganti Wokwi bila tidak ada browser/Wokwi).
// Menerbitkan JSON yang sama persis dengan firmware/gateway dan wokwi/sketch.ino.
//
//   npm run sim                         # interval 10 detik
//   INTERVAL_DETIK=2 npm run sim        # lebih cepat
//   PANAS=B npm run sim                 # Ruang B dibuat panas untuk menguji peringatan

import mqtt from 'mqtt';

const HOST = process.env.MQTT_URL || 'mqtt://broker.hivemq.com:1883';
const PREFIX = process.env.TOPIC_PREFIX || 'itts-wsn-kelompok2';
const INTERVAL = (Number(process.env.INTERVAL_DETIK) || 10) * 1000;
const PANAS = (process.env.PANAS || '').toUpperCase();
const HILANG = Number(process.env.PELUANG_HILANG ?? 0.03); // peluang paket hilang (uji PDR)

const nodes = [
  { id: 1, ruang: 'A', suhu: 26.0, hum: 55, vbat: 4.15 },
  { id: 2, ruang: 'B', suhu: 28.5, hum: 62, vbat: 4.12 },
  { id: 3, ruang: 'C', suhu: 24.0, hum: 48, vbat: 4.08 },
  { id: 4, ruang: 'D', suhu: 27.0, hum: 58, vbat: 4.01 },
].map((n) => ({ ...n, seq: 0, mac: `24:0A:C4:00:00:0${n.id}` }));

const statusTopic = `${PREFIX}/gateway/status`;
const client = mqtt.connect(HOST, {
  clientId: 'wsn-sim-' + Math.random().toString(16).slice(2, 8),
  will: { topic: statusTopic, payload: 'offline', qos: 1, retain: true },
});

const acak = (a) => (Math.random() * 2 - 1) * a;
const bulat = (x) => Math.round(x * 100) / 100;

function kirimSemua() {
  for (const n of nodes) {
    // jalan acak pelan di sekitar nilai awal; ruang PANAS naik sampai ~34 °C / 78 %
    const targetSuhu = PANAS === n.ruang ? 34 : n.suhu;
    const targetHum = PANAS === n.ruang ? 78 : n.hum;
    n.t = (n.t ?? n.suhu) + (targetSuhu - (n.t ?? n.suhu)) * 0.2 + acak(0.3);
    n.h = (n.h ?? n.hum) + (targetHum - (n.h ?? n.hum)) * 0.2 + acak(1);
    n.vbat = n.vbat > 3.3 ? n.vbat - 0.001 : 4.2;
    n.seq++;
    if (Math.random() < HILANG) {
      console.log(`x  node ${n.id} seq ${n.seq} hilang (simulasi)`);
      continue;
    }
    const json = JSON.stringify({
      node: n.id, seq: n.seq, suhu: bulat(n.t), kelembapan: bulat(n.h), vbat: bulat(n.vbat),
      rssi: -45 - Math.floor(Math.random() * 30), status: 0, mac: n.mac,
    });
    client.publish(`${PREFIX}/node/${n.id}`, json);
    console.log(`-> ${PREFIX}/node/${n.id} ${json}`);
  }
}

client.on('connect', () => {
  console.log(`Simulator tersambung ke ${HOST}, topik ${PREFIX}/node/<id>, tiap ${INTERVAL / 1000} s`);
  client.publish(statusTopic, 'online', { qos: 1, retain: true });
  kirimSemua();
  setInterval(kirimSemua, INTERVAL);
});
client.on('error', (e) => console.error('MQTT error:', e.message));

process.on('SIGINT', () => {
  client.publish(statusTopic, 'offline', { qos: 1, retain: true }, () => client.end(false, () => process.exit(0)));
});
