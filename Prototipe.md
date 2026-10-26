# Prototipe Sistem Monitoring Kualitas Udara Berbasis ESP32

Sistem monitoring kualitas udara real-time menggunakan **ESP32**, sensor **MQ-7**
(CO), **MQ-135** (polutan/CO₂), dan **DHT22** (suhu & kelembaban). Data
ditampilkan di LCD dan dikirim ke **Blynk IoT**, dengan **buzzer** sebagai
peringatan saat polutan melebihi ambang batas.

> Proyek Penulisan Ilmiah, Sistem Komputer, Universitas Gunadarma (2025).

## Fitur
- Pengukuran CO dan polutan udara dalam satuan PPM
- Pemantauan suhu dan kelembaban
- Status kualitas udara otomatis: **Baik** / **Buruk**
- Tampilan lokal di LCD I2C 16x2
- Dashboard jarak jauh via Blynk (web & mobile)
- Buzzer otomatis saat melewati ambang batas, plus kontrol manual dari Blynk

## Komponen
| Komponen | Jumlah |
|---|---|
| ESP32 DevKit V1 | 1 |
| Sensor MQ-7 | 1 |
| Sensor MQ-135 | 1 |
| Sensor DHT22 | 1 |
| LCD 16x2 I2C | 1 |
| Buzzer | 1 |
| Kabel jumper, breadboard, box | – |

Total biaya sekitar **Rp224.000**.

## Pin
| Perangkat | Pin ESP32 |
|---|---|
| MQ-7 (analog) | GPIO34 |
| MQ-135 (analog) | GPIO35 |
| DHT22 | GPIO4 |
| Buzzer | GPIO13 |
| LCD I2C (SDA / SCL) | GPIO21 / GPIO22 |

## Cara Kerja
1. Saat menyala, sensor MQ dikalibrasi di udara bersih (±10 detik) untuk
   mendapatkan nilai Ro.
2. Setiap 5 detik ESP32 membaca sensor dan menghitung PPM dari rasio Rs/Ro.
3. Jika CO > 25 PPM atau polutan > 1000 PPM, status menjadi **Buruk** dan
   buzzer menyala.
4. Data dikirim ke Blynk dan ditampilkan di LCD.

### Virtual Pin Blynk
| Pin | Fungsi |
|---|---|
| V0 | Suhu |
| V1 | Kelembaban |
| V2 | Polutan udara (PPM) |
| V3 | Karbon monoksida (PPM) |
| V4 | Status kualitas udara |
| V5 | Kontrol manual buzzer |

## Instalasi
1. Install **Arduino IDE** dan board package **ESP32**.
2. Install library: `Blynk`, `DHT sensor library`, `LiquidCrystal_I2C`.
3. Salin `secrets.example.h` menjadi `secrets.h`, lalu isi WiFi dan Blynk token.
4. Buka `air_quality_monitor.ino`, pilih board **ESP32 Dev Module**, lalu upload.
5. Buat template di Blynk sesuai tabel virtual pin di atas.

## Dokumentasi
- 📄 [Laporan lengkap (PDF)](docs/laporan-PI.pdf)
- 🔌 [Gambar rangkaian](docs/rangkaian.png)
- 📱 [Tampilan Blynk](docs/blynk-web.png)

## Hasil Pengujian
- DHT22: suhu stabil sekitar 31–32 °C, kelembaban sekitar 82 %
- MQ-7: pembacaan CO sekitar 9 PPM (udara bersih)
- Buzzer aktif saat salah satu sensor melewati ambang batas

## Keterbatasan & Pengembangan
- Sensor MQ perlu kalibrasi dan pemanasan awal, hasil PPM bersifat estimasi
- Kalibrasi hanya 10 detik; idealnya sensor MQ dipanaskan lebih lama
- Ke depan: sensor partikulat (PM2.5/PM10), notifikasi push, penyimpanan data

## Penulis
Pedro Martin Sianipar, [@Pedro2532003](https://github.com/Pedro2532003)