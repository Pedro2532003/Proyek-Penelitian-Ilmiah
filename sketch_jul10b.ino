#define BLYNK_TEMPLATE_ID "TMPL6TqnGteLW"
#define BLYNK_TEMPLATE_NAME "Monitoring Kualitas Udara"

#define BLYNK_PRINT Serial

#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include <DHT.h>
#include <LiquidCrystal_I2C.h>

// Kredensial WiFi Anda
char ssid[] = "AndroidAP";       // Ganti dengan nama WiFi kamu
char pass[] = "M4!nG0@L0F\\if3";  // Ganti dengan password WiFi kamu

// Auth Token Blynk Anda
char auth[] = "2IBCknr9MjLEFvx0rV4tfWL_Q8uXgYxL"; // Ganti dengan Auth Token Anda dari email Blynk

// --- Konfigurasi LCD I2C ---
// Pastikan alamat I2C (0x27) sudah benar sesuai hasil I2C scanner Anda.
// Sesuaikan ukuran LCD (16, 2) jika Anda menggunakan 20x4, dll.
LiquidCrystal_I2C lcd(0x27, 16, 2); 

// --- Konfigurasi Sensor DHT22 ---
#define DHTPIN 4     // Pin data DHT22 terhubung ke GPIO4 ESP32
#define DHTTYPE DHT22   // Tipe sensor DHT: DHT22 (AM2302)
DHT dht(DHTPIN, DHTTYPE);

// --- Konfigurasi Pin Sensor MQ (Analog) ---
// Pastikan koneksi fisik sesuai dengan pin ini pada ESP32 Anda.
#define MQ7_PIN 34  // Output analog MQ-7 (Karbon Monoksida) terhubung ke GPIO34 ESP32
#define MQ135_PIN 35 // Output analog MQ-135 (Polutan Udara) terhubung ke GPIO35 ESP32

// --- Tegangan Referensi ADC ESP32 ---
const float VREF = 3.3;     
const float ADC_MAX = 4095.0; 

// --- Konfigurasi Parameter Sensor MQ untuk Perhitungan PPM ---
const float RL_MQ7 = 10.0; 
const float MQ7_CO_A = 9.87; 
const float MQ7_CO_B = -0.925; 
float Ro_MQ7 = 0.0; 

const float RL_MQ135 = 20.0; 
const float MQ135_AIR_A = 116.6020682; 
const float MQ135_AIR_B = -2.769034857; 
float Ro_MQ135 = 0.0; 

// --- Ambang Batas PPM untuk Menentukan Status Kualitas Udara ---
const float CO_PPM_THRESHOLD_BAD = 25.0;    
const float AIR_QUALITY_PPM_THRESHOLD_BAD = 1000.0; 

// --- Variabel untuk menyimpan nilai sensor dan status ---
float temperature;
float humidity;
float co_ppm;          
float air_quality_ppm; 
String airQualityStatus = "Unknown"; 

// --- Objek Timer Blynk ---
BlynkTimer timer;

// --- KONFIGURASI BARU UNTUK BUZZER ---
#define BUZZER_PIN 13 // Pin GPIO untuk buzzer aktif (contoh: GPIO13)

// Variabel untuk mengontrol status fisik buzzer (ON/OFF)
bool buzzerActive = false; 
// Variabel untuk melacak keinginan manual pengguna (TRUE jika pengguna ingin ON, FALSE jika pengguna ingin OFF)
bool buzzerManualOverrideState = false; 


// --- Fungsi Pembantu untuk Perhitungan Sensor MQ ---
float calculateRs(int adcValue, float R_L) {
  float Vout = adcValue * (VREF / ADC_MAX); 
  float Rs = (VREF / Vout - 1) * R_L; 
  return Rs;
}

float calculatePPM(float rs_ro_ratio, float a_const, float b_const) {
  if (rs_ro_ratio <= 0) return 0; 
  return a_const * pow(rs_ro_ratio, b_const);
}

// --- Fungsi Kalibrasi Awal untuk Menentukan Ro ---
void calibrateSensors() {
  Serial.println("\nMemulai kalibrasi sensor MQ... Pastikan sensor berada di udara bersih dan stabil.");
  Serial.println("Ini akan memakan waktu sekitar 10 detik...");
  lcd.clear();
  lcd.print("Kalibrasi MQ...");
  lcd.setCursor(0,1);
  lcd.print("Udara Bersih!");
  delay(10000); 

  float RsSum_MQ7 = 0;
  float RsSum_MQ135 = 0;
  int numReadings = 50; 

  for (int i = 0; i < numReadings; i++) {
    RsSum_MQ7 += calculateRs(analogRead(MQ7_PIN), RL_MQ7);
    RsSum_MQ135 += calculateRs(analogRead(MQ135_PIN), RL_MQ135);
    delay(100); 
  }
  
  Ro_MQ7 = RsSum_MQ7 / numReadings;
  Ro_MQ135 = RsSum_MQ135 / numReadings;

  Serial.print("Kalibrasi selesai.");
  Serial.print(" Ro MQ-7 (CO) = "); Serial.print(Ro_MQ7, 2); Serial.println(" kOhm");
  Serial.print(" Ro MQ-135 (Air) = "); Serial.print(Ro_MQ135, 2); Serial.println(" kOhm");
  
  lcd.clear();
  lcd.print("Kalibrasi OK!");
  lcd.setCursor(0,1);
  lcd.print("Ro7:"); lcd.print(Ro_MQ7,0); lcd.print(" Ro135:"); lcd.print(Ro_MQ135,0);
  delay(2000); 
}

// --- FUNGSI BARU UNTUK KONTROL BUZZER MANUAL DARI BLYNK ---
// Fungsi ini akan dipanggil otomatis oleh Blynk setiap kali widget Switch di V5 berubah status
BLYNK_WRITE(V5) { // V5 adalah Virtual Pin untuk kontrol manual buzzer
  int pinValue = param.asInt(); // Mengambil nilai dari widget (0 untuk OFF, 1 untuk ON)

  if (pinValue == 1) { // Jika pengguna menyalakan Switch di Blynk
    buzzerManualOverrideState = true; // Set keinginan manual ke ON
    Serial.println("Buzzer MANUAL ON dari Blynk.");
  } else { // Jika pengguna mematikan Switch di Blynk
    buzzerManualOverrideState = false; // Set keinginan manual ke OFF
    Serial.println("Buzzer MANUAL OFF dari Blynk.");
  }
  // Status fisik buzzer akan diperbarui di readAndProcessSensorData()
  // berdasarkan kombinasi keinginan manual dan kondisi otomatis.
}


// --- Fungsi Utama untuk Membaca dan Memproses Data Sensor ---
void readAndProcessSensorData() {
  temperature = dht.readTemperature(); 
  humidity = dht.readHumidity();

  float rs_mq7_current = calculateRs(analogRead(MQ7_PIN), RL_MQ7);
  float rs_ro_mq7 = rs_mq7_current / Ro_MQ7;
  co_ppm = calculatePPM(rs_ro_mq7, MQ7_CO_A, MQ7_CO_B);

  float rs_mq135_current = calculateRs(analogRead(MQ135_PIN), RL_MQ135);
  float rs_ro_mq135 = rs_mq135_current / Ro_MQ135;
  air_quality_ppm = calculatePPM(rs_ro_mq135, MQ135_AIR_A, MQ135_AIR_B);

  // Tentukan apakah buzzer harus ON secara otomatis berdasarkan kondisi udara
  bool buzzerAutoShouldBeOn = false;
  if (co_ppm > CO_PPM_THRESHOLD_BAD || air_quality_ppm > AIR_QUALITY_PPM_THRESHOLD_BAD) {
      buzzerAutoShouldBeOn = true;
  }

  // Tentukan airQualityStatus berdasarkan kondisi otomatis
  if (isnan(temperature) || isnan(humidity)) {
      airQualityStatus = "DHT Error";
  } else {
      if (buzzerAutoShouldBeOn) { 
          airQualityStatus = "Buruk";
      } else {
          airQualityStatus = "Baik";
      }
  }

// Di readAndProcessSensorData()
// ...
if (buzzerAutoShouldBeOn || buzzerManualOverrideState) {
    if (!buzzerActive) {
        digitalWrite(BUZZER_PIN, HIGH);
        buzzerActive = true;
        if (buzzerAutoShouldBeOn && !buzzerManualOverrideState) {
            // Ganti Blynk.notify() dengan mengirim nilai ke pin virtual pemicu notifikasi
            Blynk.virtualWrite(V10, 1); // Kirim 1 untuk menandakan kondisi alarm
        }
    }
} else {
    if (buzzerActive) {
        digitalWrite(BUZZER_PIN, LOW);
        buzzerActive = false;
        // Jika Anda ingin mengirim notifikasi bahwa kondisi sudah baik, kirim 0
        // Blynk.virtualWrite(V10, 0); // Kirim 0 untuk menandakan kondisi normal
    }
}
// ...
// Di fungsi sendDataToBlynk(), Anda juga bisa secara periodik mengirim V10,0 jika kondisi baik
// Atau pastikan saja logika di atas mengirim 0 saat buzzer mati.
  
  Serial.print("Suhu: "); Serial.print(temperature, 1); Serial.print(" C | Kelembaban: "); Serial.print(humidity, 1); Serial.print(" % | ");
  Serial.print("CO: "); Serial.print(co_ppm, 2); Serial.print(" PPM | ");
  Serial.print("AirQ: "); Serial.print(air_quality_ppm, 2); Serial.print(" PPM | ");
  Serial.print("Status: "); Serial.println(airQualityStatus);
}

// --- Fungsi untuk Mengirim Data Sensor ke Blynk ---
void sendDataToBlynk() {
  Blynk.virtualWrite(V0, temperature);    
  Blynk.virtualWrite(V1, humidity);       
  Blynk.virtualWrite(V2, air_quality_ppm); 
  Blynk.virtualWrite(V3, co_ppm);       
  Blynk.virtualWrite(V4, airQualityStatus); 
  // Kirim juga status manual override ke V5 agar widget di Blynk tetap sinkron
  Blynk.virtualWrite(V5, buzzerManualOverrideState ? 1 : 0); 
}

// --- Fungsi untuk Memperbarui Tampilan LCD ---
void updateLCDDisplay() {
  lcd.clear(); 

  lcd.setCursor(0, 0);
  if (isnan(temperature) || isnan(humidity)) {
    lcd.print("DHT Error!"); 
  } else {
    lcd.print("T:");
    lcd.print(temperature, 1); 
    lcd.print((char)223);      
    lcd.print("C H:");
    lcd.print(humidity, 1);    
    lcd.print("%");
  }

  lcd.setCursor(0, 1);
  lcd.print("Status:");
  lcd.print(airQualityStatus);
}

// --- Fungsi Setup (Dijalankan Sekali Saat Board Boot) ---
void setup() {
  Serial.begin(115200); 

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW); 

  lcd.init();      
  lcd.backlight(); 

  lcd.print("Starting Up..."); 
  delay(1000); 

  dht.begin(); 

  pinMode(MQ7_PIN, INPUT);
  pinMode(MQ135_PIN, INPUT);

  calibrateSensors();

  Blynk.begin(auth, ssid, pass);

  timer.setInterval(5000L, readAndProcessSensorData);
  timer.setInterval(5000L, sendDataToBlynk);
  timer.setInterval(1000L, updateLCDDisplay);
}

// --- Fungsi Loop (Dijalankan Berulang Kali) ---
void loop() {
  Blynk.run(); 
  timer.run(); 
}