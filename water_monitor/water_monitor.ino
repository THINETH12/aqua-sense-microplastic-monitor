#include <WiFi.h>
#include <WebServer.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <SPI.h>
#include <SD.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

// WIFI
const char* ssid = "Rasindu";
const char* password = "167890911";

// FIREBASE 
const char* firebaseHost = "https://water-monitoring-system-b4e14-default-rtdb.asia-southeast1.firebasedatabase.app/";
const char* firebaseApiKey = "AIzaSyATluU21UHzJlTeARSHnsvg0KPvCUz4rj8";

// SENSOR PINS 
#define ONE_WIRE_BUS   4
#define TURBIDITY_PIN  1
#define EC_PIN         2
#define PH_PIN         3

// SD CARD PINS 
#define SD_CS    10
#define SD_MOSI  11
#define SD_SCK   12
#define SD_MISO  13

// MRI NORMALIZATION BOUNDS
#define TURBIDITY_MAX  3000.0
#define EC_MAX         3000.0
#define PH_IDEAL       7.0
#define PH_MAX_DEV     7.0   // max deviation from ideal (0–14 scale)
#define TEMP_IDEAL     25.0
#define TEMP_MAX_DEV   35.0  // max deviation considered

// OBJECTS 
WebServer server(80);
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
SPIClass sdSPI(FSPI);

// PH CALIBRATION 
float phSlope = -5.5556;
float phOffset = 20.7778;

// GLOBAL VALUES 
float temperatureC = 0.0;
float phValue = 0.0;
float ecValue = 0.0;
float turbidityNTU = 0.0;
float wqiScore = 0.0;
float mriScore = 0.0;
String waterStatus = "Unknown";
String usability = "Checking";
String microplasticRisk = "Unknown";

// MAX SAVED SAMPLES
#define MAX_SAVED 20
struct SavedSample {
  String name;
  float temperature;
  float ph;
  float ec;
  float turbidity;
  float wqi;
  float mri;
  String status;
  String usability;
  String mpRisk;
  unsigned long timestamp;
};
SavedSample savedSamples[MAX_SAVED];
int savedCount = 0;

// ─── HELPERS ───────────────────────────────────────────────────
int readAverageADC(int pin, int samples = 20) {
  long total = 0;
  for (int i = 0; i < samples; i++) {
    total += analogRead(pin);
    delay(10);
  }
  return total / samples;
}

float readPHVoltage(int samples = 20) {
  int raw = readAverageADC(PH_PIN, samples);
  return raw * (3.3 / 4095.0);
}

float readPHValue(int samples = 20) {
  float voltage = readPHVoltage(samples);
  return (phSlope * voltage) + phOffset;
}

float readECuS() {
  int raw = readAverageADC(EC_PIN, 20);
  float voltage = raw * (3.3 / 4095.0);
  float ec = voltage * 1000.0;
  if (ec < 0) ec = 0;
  return ec;
}

float readTurbidityNTU() {
  int raw = readAverageADC(TURBIDITY_PIN, 20);
  float voltage = raw * (3.3 / 4095.0);
  float ntu = 3000.0 - (voltage * 1000.0);
  if (ntu < 0) ntu = 0;
  return ntu;
}

// ─── MRI CALCULATION ───────────────────────────────────────────
// MRI = (Turbidity_norm × 0.4) + (EC_norm × 0.3) + (pH_dev × 0.2) + (Temp_dev × 0.1)
float calculateMRI(float turb, float ec, float ph, float temp) {
  float turbNorm = constrain(turb / TURBIDITY_MAX, 0.0, 1.0);
  float ecNorm   = constrain(ec   / EC_MAX,        0.0, 1.0);
  float phDev    = constrain(abs(ph   - PH_IDEAL)   / PH_MAX_DEV,   0.0, 1.0);
  float tempDev  = constrain(abs(temp - TEMP_IDEAL) / TEMP_MAX_DEV, 0.0, 1.0);

  return (turbNorm * 0.4f) + (ecNorm * 0.3f) + (phDev * 0.2f) + (tempDev * 0.1f);
}

String getMicroplasticRisk(float mri) {
  if (mri < 0.25) return "Very Low";
  if (mri < 0.50) return "Low";
  if (mri < 0.70) return "Moderate";
  if (mri < 0.85) return "High";
  return "Very High";
}

// ─── WQI ───────────────────────────────────────────────────────
float calculateWQI(float temp, float ph, float ec, float turbidity) {
  float score = 100.0;
  if (ph < 6.5 || ph > 8.5)  score -= 20;
  if (temp < 15 || temp > 35) score -= 10;
  if (ec > 1500)              score -= 15;
  if (turbidity > 5)          score -= 15;
  if (score < 0) score = 0;
  return score;
}

// ─── STATUS UPDATE ─────────────────────────────────────────────
void updateWaterStatus() {
  if (phValue >= 6.8 && phValue <= 7.5 && turbidityNTU <= 1 && ecValue <= 500) {
    waterStatus = "Excellent";
    usability   = "Good for Drinking";
  } else if (phValue >= 6.5 && phValue <= 8.5 && turbidityNTU <= 5 && ecValue <= 1000) {
    waterStatus = "Good";
    usability   = "Good for Drinking";
  } else if (phValue >= 6.0 && phValue <= 9.0 && turbidityNTU <= 10 && ecValue <= 1500) {
    waterStatus = "Poor";
    usability   = "Not Good for Drinking";
  } else {
    waterStatus = "Very Poor";
    usability   = "Not Good for Drinking";
  }
}

// ─── SENSOR READ ───────────────────────────────────────────────
void readSensors() {
  sensors.requestTemperatures();
  float temp = sensors.getTempCByIndex(0);
  temperatureC = (temp == DEVICE_DISCONNECTED_C || temp < -100) ? 0.0 : temp;

  phValue       = readPHValue(20);
  ecValue       = readECuS();
  turbidityNTU  = readTurbidityNTU();
  wqiScore      = calculateWQI(temperatureC, phValue, ecValue, turbidityNTU);
  mriScore      = calculateMRI(turbidityNTU, ecValue, phValue, temperatureC);
  microplasticRisk = getMicroplasticRisk(mriScore);

  updateWaterStatus();
}

// ─── SD CARD ───────────────────────────────────────────────────
void initSDCard() {
  sdSPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  if (!SD.begin(SD_CS, sdSPI)) { Serial.println("SD card initialization failed"); return; }
  Serial.println("SD card initialized");
  if (!SD.exists("/water_log.csv")) {
    File file = SD.open("/water_log.csv", FILE_WRITE);
    if (file) {
      file.println("time_ms,temperature_c,ph,ec_us_cm,turbidity_ntu,wqi,mri,mp_risk,status,usability");
      file.close();
    }
  }
  // Load saved samples from SD
  if (SD.exists("/saved_samples.csv")) {
    File f = SD.open("/saved_samples.csv");
    if (f) {
      f.readStringUntil('\n'); // skip header
      while (f.available() && savedCount < MAX_SAVED) {
        String line = f.readStringUntil('\n');
        line.trim();
        if (line.length() == 0) continue;
        // Parse CSV: name,temp,ph,ec,turbidity,wqi,mri,status,usability,mpRisk,timestamp
        int idx = 0;
        String fields[11];
        for (int i = 0; i < line.length() && idx < 11; i++) {
          if (line[i] == ',') { idx++; }
          else { fields[idx] += line[i]; }
        }
        savedSamples[savedCount].name        = fields[0];
        savedSamples[savedCount].temperature = fields[1].toFloat();
        savedSamples[savedCount].ph          = fields[2].toFloat();
        savedSamples[savedCount].ec          = fields[3].toFloat();
        savedSamples[savedCount].turbidity   = fields[4].toFloat();
        savedSamples[savedCount].wqi         = fields[5].toFloat();
        savedSamples[savedCount].mri         = fields[6].toFloat();
        savedSamples[savedCount].status      = fields[7];
        savedSamples[savedCount].usability   = fields[8];
        savedSamples[savedCount].mpRisk      = fields[9];
        savedSamples[savedCount].timestamp   = fields[10].toInt();
        savedCount++;
      }
      f.close();
    }
  }
}

void logToSD() {
  File file = SD.open("/water_log.csv", FILE_APPEND);
  if (file) {
    file.print(millis()); file.print(",");
    file.print(temperatureC, 2); file.print(",");
    file.print(phValue, 2);      file.print(",");
    file.print(ecValue, 2);      file.print(",");
    file.print(turbidityNTU, 2); file.print(",");
    file.print(wqiScore, 1);     file.print(",");
    file.print(mriScore, 4);     file.print(",");
    file.print(microplasticRisk);file.print(",");
    file.print(waterStatus);     file.print(",");
    file.println(usability);
    file.close();
    Serial.println("Logged to SD");
  }
}

void saveSamplesToSD() {
  SD.remove("/saved_samples.csv");
  File file = SD.open("/saved_samples.csv", FILE_WRITE);
  if (file) {
    file.println("name,temperature,ph,ec,turbidity,wqi,mri,status,usability,mpRisk,timestamp");
    for (int i = 0; i < savedCount; i++) {
      file.print(savedSamples[i].name);        file.print(",");
      file.print(savedSamples[i].temperature, 2); file.print(",");
      file.print(savedSamples[i].ph, 2);       file.print(",");
      file.print(savedSamples[i].ec, 2);       file.print(",");
      file.print(savedSamples[i].turbidity, 2);file.print(",");
      file.print(savedSamples[i].wqi, 1);      file.print(",");
      file.print(savedSamples[i].mri, 4);      file.print(",");
      file.print(savedSamples[i].status);      file.print(",");
      file.print(savedSamples[i].usability);   file.print(",");
      file.print(savedSamples[i].mpRisk);      file.print(",");
      file.println(savedSamples[i].timestamp);
    }
    file.close();
  }
}

// ─── FIREBASE ──────────────────────────────────────────────────
bool firebasePut(const String& path, const String& jsonPayload) {
  if (WiFi.status() != WL_CONNECTED) return false;
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient https;
  String url = String(firebaseHost) + path + ".json";
  https.begin(client, url);
  https.addHeader("Content-Type", "application/json");
  int httpCode = https.PUT(jsonPayload);
  https.end();
  return (httpCode == 200);
}

void uploadToFirebase() {
  String json = "{";
  json += "\"temperature\":" + String(temperatureC, 2) + ",";
  json += "\"ph\":"          + String(phValue, 2)      + ",";
  json += "\"ec\":"          + String(ecValue, 2)      + ",";
  json += "\"turbidity\":"   + String(turbidityNTU, 2) + ",";
  json += "\"wqi\":"         + String(wqiScore, 1)     + ",";
  json += "\"mri\":"         + String(mriScore, 4)     + ",";
  json += "\"mp_risk\":\""   + microplasticRisk        + "\",";
  json += "\"status\":\""    + waterStatus             + "\",";
  json += "\"usability\":\""  + usability              + "\",";
  json += "\"time_ms\":"     + String(millis());
  json += "}";
  firebasePut("/live", json);
  firebasePut("/logs/" + String(millis()), json);
}

// ─── WEB DASHBOARD HTML ────────────────────────────────────────
String htmlPage() {
  return R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>IoT Water Quality Monitoring</title>
  <link href="https://fonts.googleapis.com/css2?family=Space+Grotesk:wght@400;500;600;700&family=JetBrains+Mono:wght@400;600&display=swap" rel="stylesheet">
  <style>
    :root {
      --bg: #0a0f1e;
      --surface: #111827;
      --surface2: #1a2235;
      --border: #1e2d45;
      --accent: #00d4ff;
      --accent2: #7c3aed;
      --green: #10b981;
      --yellow: #f59e0b;
      --red: #ef4444;
      --text: #e2e8f0;
      --muted: #64748b;
      --font: 'Space Grotesk', sans-serif;
      --mono: 'JetBrains Mono', monospace;
    }
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body {
      background: var(--bg);
      color: var(--text);
      font-family: var(--font);
      min-height: 100vh;
      padding: 24px;
    }
    /* Animated background grid */
    body::before {
      content: '';
      position: fixed;
      inset: 0;
      background-image:
        linear-gradient(rgba(0,212,255,0.03) 1px, transparent 1px),
        linear-gradient(90deg, rgba(0,212,255,0.03) 1px, transparent 1px);
      background-size: 40px 40px;
      pointer-events: none;
      z-index: 0;
    }
    .wrap { max-width: 1200px; margin: auto; position: relative; z-index: 1; }

    /* Header */
    header {
      display: flex;
      align-items: center;
      justify-content: space-between;
      margin-bottom: 32px;
      flex-wrap: wrap;
      gap: 16px;
    }
    .logo-area h1 {
      font-size: 28px;
      font-weight: 700;
      letter-spacing: -0.5px;
    }
    .logo-area h1 span { color: var(--accent); }
    .logo-area p { color: var(--muted); font-size: 13px; font-family: var(--mono); margin-top: 4px; }
    .live-badge {
      display: flex; align-items: center; gap: 8px;
      background: rgba(16,185,129,0.1);
      border: 1px solid rgba(16,185,129,0.3);
      border-radius: 999px;
      padding: 6px 16px;
      font-size: 13px;
      color: var(--green);
      font-family: var(--mono);
    }
    .live-dot {
      width: 8px; height: 8px;
      border-radius: 50%;
      background: var(--green);
      animation: pulse 1.5s infinite;
    }
    @keyframes pulse { 0%,100%{opacity:1} 50%{opacity:0.3} }

    /* Status card */
    .status-card {
      background: var(--surface);
      border: 1px solid var(--border);
      border-radius: 20px;
      padding: 32px;
      margin-bottom: 24px;
      display: flex;
      align-items: center;
      justify-content: space-between;
      flex-wrap: wrap;
      gap: 24px;
      position: relative;
      overflow: hidden;
    }
    .status-card::before {
      content: '';
      position: absolute;
      top: -50px; right: -50px;
      width: 200px; height: 200px;
      border-radius: 50%;
      background: radial-gradient(circle, rgba(0,212,255,0.08) 0%, transparent 70%);
    }
    .status-left .label { font-size: 12px; color: var(--muted); text-transform: uppercase; letter-spacing: 1.5px; margin-bottom: 8px; }
    .status-left .value { font-size: 52px; font-weight: 700; line-height: 1; letter-spacing: -2px; }
    .status-right { text-align: right; }
    .status-right .label { font-size: 12px; color: var(--muted); text-transform: uppercase; letter-spacing: 1.5px; margin-bottom: 8px; }
    .usability-chip {
      display: inline-flex; align-items: center; gap: 8px;
      border: 1.5px solid var(--green);
      border-radius: 999px;
      padding: 10px 22px;
      font-size: 15px;
      font-weight: 600;
      color: var(--green);
    }

    /* Metric cards */
    .cards {
      display: grid;
      grid-template-columns: repeat(3, 1fr);
      gap: 16px;
      margin-bottom: 24px;
    }
    .card {
      background: var(--surface);
      border: 1px solid var(--border);
      border-radius: 16px;
      padding: 24px;
      position: relative;
      overflow: hidden;
      transition: border-color 0.2s, transform 0.2s;
    }
    .card:hover { border-color: var(--accent); transform: translateY(-2px); }
    .card::after {
      content: '';
      position: absolute;
      bottom: 0; left: 0; right: 0;
      height: 2px;
      background: linear-gradient(90deg, var(--accent), var(--accent2));
      transform: scaleX(0);
      transition: transform 0.3s;
    }
    .card:hover::after { transform: scaleX(1); }
    .card-icon { font-size: 22px; margin-bottom: 12px; }
    .card-title { font-size: 11px; color: var(--muted); text-transform: uppercase; letter-spacing: 1.5px; margin-bottom: 6px; }
    .card-value { font-size: 32px; font-weight: 700; font-family: var(--mono); }
    .card-unit { font-size: 14px; color: var(--muted); font-weight: 400; }

    /* MRI Section */
    .mri-card {
      background: linear-gradient(135deg, #0f1a2e 0%, #1a0a2e 100%);
      border: 1px solid rgba(124,58,237,0.4);
      border-radius: 20px;
      padding: 28px;
      margin-bottom: 24px;
    }
    .mri-header {
      display: flex; justify-content: space-between; align-items: flex-start;
      flex-wrap: wrap; gap: 16px; margin-bottom: 24px;
    }
    .mri-title { font-size: 14px; color: var(--muted); text-transform: uppercase; letter-spacing: 1.5px; margin-bottom: 6px; }
    .mri-score { font-size: 48px; font-weight: 700; font-family: var(--mono); color: var(--accent2); }
    .mp-risk-chip {
      display: inline-flex; align-items: center; gap: 8px;
      border: 1.5px solid var(--accent2);
      border-radius: 999px;
      padding: 8px 18px;
      font-size: 14px;
      font-weight: 600;
      color: var(--accent2);
    }
    .mri-formula {
      background: rgba(0,0,0,0.3);
      border-radius: 10px;
      padding: 12px 16px;
      font-family: var(--mono);
      font-size: 12px;
      color: var(--muted);
      margin-bottom: 20px;
    }
    .mri-formula span { color: var(--accent2); }
    /* Gauge bar */
    .gauge-wrap { position: relative; }
    .gauge-label { display: flex; justify-content: space-between; font-size: 11px; color: var(--muted); margin-bottom: 8px; }
    .gauge-track {
      height: 10px;
      border-radius: 999px;
      background: var(--border);
      overflow: hidden;
    }
    .gauge-fill {
      height: 100%;
      border-radius: 999px;
      background: linear-gradient(90deg, #10b981, #f59e0b, #ef4444);
      transition: width 0.8s cubic-bezier(0.4,0,0.2,1);
    }
    .gauge-ticks { display: flex; justify-content: space-between; margin-top: 6px; }
    .gauge-tick { font-size: 10px; color: var(--muted); font-family: var(--mono); }

    /* Breakdown bars */
    .breakdown { display: grid; grid-template-columns: repeat(2, 1fr); gap: 12px; margin-top: 20px; }
    .bk-item { }
    .bk-label { display: flex; justify-content: space-between; font-size: 12px; margin-bottom: 5px; }
    .bk-label span:first-child { color: var(--muted); }
    .bk-label span:last-child { color: var(--text); font-family: var(--mono); }
    .bk-bar { height: 6px; border-radius: 999px; background: var(--border); overflow: hidden; }
    .bk-fill { height: 100%; border-radius: 999px; background: var(--accent2); transition: width 0.8s; }

    /* Action buttons */
    .actions {
      display: flex; gap: 14px; flex-wrap: wrap;
    }
    .btn {
      display: inline-flex; align-items: center; gap: 8px;
      padding: 14px 24px;
      border-radius: 12px;
      font-size: 14px;
      font-weight: 600;
      font-family: var(--font);
      text-decoration: none;
      cursor: pointer;
      border: none;
      transition: all 0.2s;
    }
    .btn-primary {
      background: var(--accent);
      color: #000;
    }
    .btn-primary:hover { background: #00b8d9; transform: translateY(-1px); }
    .btn-secondary {
      background: var(--surface2);
      color: var(--text);
      border: 1px solid var(--border);
    }
    .btn-secondary:hover { border-color: var(--accent); color: var(--accent); transform: translateY(-1px); }
    .btn-purple {
      background: var(--surface2);
      color: var(--accent2);
      border: 1px solid rgba(124,58,237,0.4);
    }
    .btn-purple:hover { background: rgba(124,58,237,0.15); transform: translateY(-1px); }

    /* Save modal */
    .modal-overlay {
      display: none;
      position: fixed; inset: 0;
      background: rgba(0,0,0,0.7);
      backdrop-filter: blur(6px);
      z-index: 100;
      align-items: center;
      justify-content: center;
    }
    .modal-overlay.active { display: flex; }
    .modal {
      background: var(--surface);
      border: 1px solid var(--border);
      border-radius: 20px;
      padding: 32px;
      width: 100%;
      max-width: 440px;
      animation: slideUp 0.3s ease;
    }
    @keyframes slideUp { from{opacity:0;transform:translateY(20px)} to{opacity:1;transform:translateY(0)} }
    .modal h2 { font-size: 20px; margin-bottom: 6px; }
    .modal p { color: var(--muted); font-size: 14px; margin-bottom: 24px; }
    .input-wrap { margin-bottom: 20px; }
    .input-wrap label { display: block; font-size: 12px; color: var(--muted); text-transform: uppercase; letter-spacing: 1px; margin-bottom: 8px; }
    .input-wrap input {
      width: 100%;
      background: var(--surface2);
      border: 1px solid var(--border);
      border-radius: 10px;
      padding: 12px 16px;
      color: var(--text);
      font-family: var(--font);
      font-size: 16px;
      outline: none;
      transition: border-color 0.2s;
    }
    .input-wrap input:focus { border-color: var(--accent); }
    .modal-actions { display: flex; gap: 12px; }
    .modal-actions .btn { flex: 1; justify-content: center; }

    /* Toast */
    .toast {
      position: fixed; bottom: 24px; right: 24px;
      background: var(--surface);
      border: 1px solid var(--green);
      border-radius: 12px;
      padding: 14px 20px;
      font-size: 14px;
      color: var(--green);
      display: none;
      animation: slideUp 0.3s ease;
      z-index: 200;
    }
    .toast.show { display: block; }

    @media (max-width: 900px) {
      .cards { grid-template-columns: repeat(2, 1fr); }
      .breakdown { grid-template-columns: 1fr; }
    }
    @media (max-width: 600px) {
      body { padding: 16px; }
      .cards { grid-template-columns: 1fr; }
      .status-left .value { font-size: 38px; }
      .mri-score { font-size: 36px; }
      .actions { flex-direction: column; }
      .btn { width: 100%; justify-content: center; }
    }
  </style>
</head>
<body>
<div class="wrap">

  <header>
    <div class="logo-area">
      <h1>💧 Water <span>Quality</span> Monitor</h1>
      <p>ESP32 · Real-time IoT Sensing</p>
    </div>
    <div class="live-badge">
      <div class="live-dot"></div>
      LIVE
    </div>
  </header>

  <!-- Status Card -->
  <div class="status-card">
    <div class="status-left">
      <div class="label">Water Quality Status</div>
      <div class="status-value" id="status">—</div>
    </div>
    <div class="status-right">
      <div class="label">Usability</div>
      <div class="usability-chip" id="usability">Checking...</div>
    </div>
  </div>

  <!-- Metric Cards -->
  <div class="cards">
    <div class="card">
      <div class="card-icon">🌡️</div>
      <div class="card-title">Temperature</div>
      <div class="card-value"><span id="temperature">--</span><span class="card-unit"> °C</span></div>
    </div>
    <div class="card">
      <div class="card-icon">⚗️</div>
      <div class="card-title">pH Level</div>
      <div class="card-value" id="ph">--</div>
    </div>
    <div class="card">
      <div class="card-icon">⚡</div>
      <div class="card-title">EC</div>
      <div class="card-value"><span id="ec">--</span><span class="card-unit"> µS/cm</span></div>
    </div>
    <div class="card">
      <div class="card-icon">🌊</div>
      <div class="card-title">Turbidity</div>
      <div class="card-value"><span id="turbidity">--</span><span class="card-unit"> NTU</span></div>
    </div>
    <div class="card">
      <div class="card-icon">📊</div>
      <div class="card-title">WQI Score</div>
      <div class="card-value" id="wqi">--</div>
    </div>
  </div>

  <!-- MRI Card -->
  <div class="mri-card">
    <div class="mri-header">
      <div>
        <div class="mri-title">🔬 Microplastic Risk Index (MRI)</div>
        <div class="mri-score" id="mriScore">0.000</div>
      </div>
      <div>
        <div class="mri-title">Risk Level</div>
        <div class="mp-risk-chip" id="mpRisk">—</div>
      </div>
    </div>
    <div class="mri-formula">
      MRI = (<span>Turbidity_norm</span> × 0.4) + (<span>EC_norm</span> × 0.3) + (<span>pH_deviation</span> × 0.2) + (<span>Temp_deviation</span> × 0.1)
    </div>
    <div class="gauge-wrap">
      <div class="gauge-label"><span>0.0 — Very Low</span><span>1.0 — Very High</span></div>
      <div class="gauge-track"><div class="gauge-fill" id="mriBar" style="width:0%"></div></div>
      <div class="gauge-ticks">
        <span class="gauge-tick">0.0</span>
        <span class="gauge-tick">0.25</span>
        <span class="gauge-tick">0.50</span>
        <span class="gauge-tick">0.75</span>
        <span class="gauge-tick">1.0</span>
      </div>
    </div>
    <div class="breakdown">
      <div class="bk-item">
        <div class="bk-label"><span>Turbidity (×0.4)</span><span id="bk_turb">0.000</span></div>
        <div class="bk-bar"><div class="bk-fill" id="bkBar_turb" style="width:0%"></div></div>
      </div>
      <div class="bk-item">
        <div class="bk-label"><span>EC Conductivity (×0.3)</span><span id="bk_ec">0.000</span></div>
        <div class="bk-bar"><div class="bk-fill" id="bkBar_ec" style="width:0%"></div></div>
      </div>
      <div class="bk-item">
        <div class="bk-label"><span>pH Deviation (×0.2)</span><span id="bk_ph">0.000</span></div>
        <div class="bk-bar"><div class="bk-fill" id="bkBar_ph" style="width:0%"></div></div>
      </div>
      <div class="bk-item">
        <div class="bk-label"><span>Temp Deviation (×0.1)</span><span id="bk_temp">0.000</span></div>
        <div class="bk-bar"><div class="bk-fill" id="bkBar_temp" style="width:0%"></div></div>
      </div>
    </div>
  </div>

  <!-- Actions -->
  <div class="actions">
    <a class="btn btn-primary" href="/download">⬇ Download CSV</a>
    <button class="btn btn-secondary" onclick="openSaveModal()">💾 Save This Result</button>
    <a class="btn btn-purple" href="/advanced">⚙ Advanced Settings</a>
  </div>

</div>

<!-- Save Modal -->
<div class="modal-overlay" id="saveModal">
  <div class="modal">
    <h2>💾 Save Result</h2>
    <p>Give this water sample a name so you can compare it later.</p>
    <div class="input-wrap">
      <label>Sample Name</label>
      <input type="text" id="sampleName" placeholder="e.g. River Water, Salt Water…" maxlength="30">
    </div>
    <div class="modal-actions">
      <button class="btn btn-secondary" onclick="closeSaveModal()">Cancel</button>
      <button class="btn btn-primary" onclick="confirmSave()">Save Sample</button>
    </div>
  </div>
</div>

<div class="toast" id="toast">✅ Sample saved successfully!</div>

<script>
  let currentData = {};

  async function loadData() {
    try {
      const r = await fetch('/data');
      const d = await r.json();
      currentData = d;

      document.getElementById('status').innerText    = d.status;
      document.getElementById('usability').innerText = d.usability;
      document.getElementById('temperature').innerText = d.temperature;
      document.getElementById('ph').innerText          = d.ph;
      document.getElementById('ec').innerText          = d.ec;
      document.getElementById('turbidity').innerText   = d.turbidity;
      document.getElementById('wqi').innerText         = d.wqi;

      const mri = parseFloat(d.mri);
      document.getElementById('mriScore').innerText   = mri.toFixed(3);
      document.getElementById('mpRisk').innerText     = d.mp_risk;
      document.getElementById('mriBar').style.width   = (mri * 100).toFixed(1) + '%';

      const bkT = (parseFloat(d.turbidity) / 3000 * 0.4);
      const bkE = (parseFloat(d.ec)        / 3000 * 0.3);
      const bkP = (Math.abs(parseFloat(d.ph) - 7.0) / 7.0 * 0.2);
      const bkTm= (Math.abs(parseFloat(d.temperature) - 25.0) / 35.0 * 0.1);
      document.getElementById('bk_turb').innerText  = bkT.toFixed(3);
      document.getElementById('bk_ec').innerText    = bkE.toFixed(3);
      document.getElementById('bk_ph').innerText    = bkP.toFixed(3);
      document.getElementById('bk_temp').innerText  = bkTm.toFixed(3);
      document.getElementById('bkBar_turb').style.width  = Math.min(bkT/0.4*100,100)+'%';
      document.getElementById('bkBar_ec').style.width    = Math.min(bkE/0.3*100,100)+'%';
      document.getElementById('bkBar_ph').style.width    = Math.min(bkP/0.2*100,100)+'%';
      document.getElementById('bkBar_temp').style.width  = Math.min(bkTm/0.1*100,100)+'%';
    } catch(e) { console.error(e); }
  }

  function openSaveModal() {
    document.getElementById('saveModal').classList.add('active');
    document.getElementById('sampleName').focus();
  }
  function closeSaveModal() {
    document.getElementById('saveModal').classList.remove('active');
    document.getElementById('sampleName').value = '';
  }

  async function confirmSave() {
    const name = document.getElementById('sampleName').value.trim();
    if (!name) { document.getElementById('sampleName').focus(); return; }
    try {
      const r = await fetch('/save?name=' + encodeURIComponent(name), { method: 'POST' });
      if (r.ok) {
        closeSaveModal();
        const t = document.getElementById('toast');
        t.classList.add('show');
        setTimeout(() => t.classList.remove('show'), 3000);
      }
    } catch(e) { alert('Save failed: ' + e); }
  }

  loadData();
  setInterval(loadData, 3000);
</script>
</body>
</html>
)rawliteral";
}

// ─── ADVANCED PAGE HTML ────────────────────────────────────────
String htmlAdvanced() {
  // Build JSON array of saved samples
  String samplesJson = "[";
  for (int i = 0; i < savedCount; i++) {
    if (i > 0) samplesJson += ",";
    samplesJson += "{";
    samplesJson += "\"name\":\""        + savedSamples[i].name        + "\",";
    samplesJson += "\"temperature\":"   + String(savedSamples[i].temperature, 2) + ",";
    samplesJson += "\"ph\":"            + String(savedSamples[i].ph, 2)          + ",";
    samplesJson += "\"ec\":"            + String(savedSamples[i].ec, 2)          + ",";
    samplesJson += "\"turbidity\":"     + String(savedSamples[i].turbidity, 2)   + ",";
    samplesJson += "\"wqi\":"           + String(savedSamples[i].wqi, 1)         + ",";
    samplesJson += "\"mri\":"           + String(savedSamples[i].mri, 4)         + ",";
    samplesJson += "\"status\":\""      + savedSamples[i].status                 + "\",";
    samplesJson += "\"usability\":\""   + savedSamples[i].usability              + "\",";
    samplesJson += "\"mp_risk\":\""     + savedSamples[i].mpRisk                 + "\",";
    samplesJson += "\"timestamp\":"     + String(savedSamples[i].timestamp);
    samplesJson += "}";
  }
  samplesJson += "]";

  String page = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Advanced Settings — Water Monitor</title>
  <link href="https://fonts.googleapis.com/css2?family=Space+Grotesk:wght@400;500;600;700&family=JetBrains+Mono:wght@400;600&display=swap" rel="stylesheet">
  <script src="https://cdn.jsdelivr.net/npm/chart.js@4.4.0/dist/chart.umd.min.js"></script>
  <style>
    :root {
      --bg: #0a0f1e; --surface: #111827; --surface2: #1a2235;
      --border: #1e2d45; --accent: #00d4ff; --accent2: #7c3aed;
      --green: #10b981; --yellow: #f59e0b; --red: #ef4444;
      --text: #e2e8f0; --muted: #64748b;
      --font: 'Space Grotesk', sans-serif;
      --mono: 'JetBrains Mono', monospace;
    }
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body { background: var(--bg); color: var(--text); font-family: var(--font); min-height: 100vh; padding: 24px; }
    body::before {
      content: '';
      position: fixed; inset: 0;
      background-image: linear-gradient(rgba(0,212,255,0.03) 1px, transparent 1px),
        linear-gradient(90deg, rgba(0,212,255,0.03) 1px, transparent 1px);
      background-size: 40px 40px;
      pointer-events: none; z-index: 0;
    }
    .wrap { max-width: 1200px; margin: auto; position: relative; z-index: 1; }
    header { display: flex; align-items: center; gap: 16px; margin-bottom: 32px; flex-wrap: wrap; }
    .back-btn {
      display: inline-flex; align-items: center; gap: 8px;
      background: var(--surface2); border: 1px solid var(--border);
      border-radius: 10px; padding: 10px 16px;
      color: var(--text); text-decoration: none; font-size: 14px; font-weight: 600;
      transition: all 0.2s;
    }
    .back-btn:hover { border-color: var(--accent); color: var(--accent); }
    header h1 { font-size: 26px; font-weight: 700; }
    header h1 span { color: var(--accent2); }
    .section-title {
      font-size: 13px; color: var(--muted); text-transform: uppercase;
      letter-spacing: 1.5px; margin-bottom: 16px;
    }

    /* Stats summary row */
    .stats-row { display: grid; grid-template-columns: repeat(4,1fr); gap: 16px; margin-bottom: 28px; }
    .stat-card {
      background: var(--surface); border: 1px solid var(--border);
      border-radius: 14px; padding: 20px 18px;
    }
    .stat-card .label { font-size: 11px; color: var(--muted); text-transform: uppercase; letter-spacing: 1px; margin-bottom: 6px; }
    .stat-card .val { font-size: 28px; font-weight: 700; font-family: var(--mono); color: var(--accent); }

    /* Chart area */
    .chart-card {
      background: var(--surface); border: 1px solid var(--border);
      border-radius: 20px; padding: 28px; margin-bottom: 28px;
    }
    .chart-card canvas { width: 100% !important; }

    /* Table */
    .table-card {
      background: var(--surface); border: 1px solid var(--border);
      border-radius: 20px; overflow: hidden; margin-bottom: 28px;
    }
    .table-header {
      display: flex; justify-content: space-between; align-items: center;
      padding: 20px 24px; border-bottom: 1px solid var(--border);
    }
    table { width: 100%; border-collapse: collapse; }
    thead tr { background: var(--surface2); }
    th {
      text-align: left; padding: 12px 16px;
      font-size: 11px; color: var(--muted);
      text-transform: uppercase; letter-spacing: 1px;
      font-weight: 600; white-space: nowrap;
    }
    td { padding: 14px 16px; font-size: 14px; border-bottom: 1px solid var(--border); }
    tr:last-child td { border-bottom: none; }
    tr:hover td { background: var(--surface2); }
    .risk-badge {
      display: inline-block; border-radius: 999px;
      padding: 3px 10px; font-size: 11px; font-weight: 600;
    }
    .risk-very-low  { background: rgba(16,185,129,0.15);  color: #10b981; }
    .risk-low       { background: rgba(52,211,153,0.15);  color: #34d399; }
    .risk-moderate  { background: rgba(245,158,11,0.15);  color: #f59e0b; }
    .risk-high      { background: rgba(239,68,68,0.15);   color: #ef4444; }
    .risk-very-high { background: rgba(220,38,38,0.15);   color: #dc2626; }
    .name-cell { font-weight: 600; }
    .mono { font-family: var(--mono); font-size: 13px; }

    .empty-state {
      text-align: center; padding: 64px 24px;
      color: var(--muted); font-size: 15px;
    }
    .empty-state .icon { font-size: 48px; margin-bottom: 12px; }

    .btn {
      display: inline-flex; align-items: center; gap: 8px;
      padding: 10px 18px; border-radius: 10px;
      font-size: 13px; font-weight: 600; font-family: var(--font);
      text-decoration: none; cursor: pointer; border: none; transition: all 0.2s;
    }
    .btn-danger {
      background: rgba(239,68,68,0.1); color: var(--red);
      border: 1px solid rgba(239,68,68,0.3);
    }
    .btn-danger:hover { background: rgba(239,68,68,0.2); }

    @media(max-width:900px){.stats-row{grid-template-columns:repeat(2,1fr)}}
    @media(max-width:600px){
      body{padding:14px}
      .stats-row{grid-template-columns:1fr}
      table{font-size:12px}
      td,th{padding:10px 10px}
    }
  </style>
</head>
<body>
<div class="wrap">
  <header>
    <a class="back-btn" href="/">← Dashboard</a>
    <h1>Advanced <span>Settings</span> &amp; Saved Results</h1>
  </header>

  <div id="main"></div>
</div>

<script>
const SAMPLES = )rawliteral";

  page += samplesJson;

  page += R"rawliteral(;

function riskClass(r) {
  const m = {
    'Very Low':'risk-very-low','Low':'risk-low',
    'Moderate':'risk-moderate','High':'risk-high','Very High':'risk-very-high'
  };
  return m[r] || '';
}

function formatTime(ms) {
  const d = new Date(ms);
  if (ms < 1000000000000) {
    // millis since boot — just show as elapsed
    const s = Math.floor(ms/1000);
    const m = Math.floor(s/60);
    const h = Math.floor(m/60);
    return h+'h '+(m%60)+'m';
  }
  return d.toLocaleString();
}

function render() {
  const el = document.getElementById('main');
  if (SAMPLES.length === 0) {
    el.innerHTML = `
      <div class="empty-state">
        <div class="icon">🧪</div>
        <p>No saved samples yet.</p>
        <p style="margin-top:8px;font-size:13px">Go to the dashboard and click <strong>Save This Result</strong> after a reading.</p>
      </div>`;
    return;
  }

  const avgMRI = (SAMPLES.reduce((a,s)=>a+s.mri,0)/SAMPLES.length).toFixed(3);
  const maxMRI = Math.max(...SAMPLES.map(s=>s.mri)).toFixed(3);
  const minMRI = Math.min(...SAMPLES.map(s=>s.mri)).toFixed(3);

  el.innerHTML = `
    <div class="stats-row">
      <div class="stat-card"><div class="label">Total Samples</div><div class="val">${SAMPLES.length}</div></div>
      <div class="stat-card"><div class="label">Average MRI</div><div class="val">${avgMRI}</div></div>
      <div class="stat-card"><div class="label">Highest MRI</div><div class="val">${maxMRI}</div></div>
      <div class="stat-card"><div class="label">Lowest MRI</div><div class="val">${minMRI}</div></div>
    </div>

    <div class="chart-card">
      <div class="section-title">MRI Score Comparison — Bar Chart</div>
      <canvas id="mriChart" height="80"></canvas>
    </div>

    <div class="table-card">
      <div class="table-header">
        <div class="section-title" style="margin-bottom:0">All Saved Results</div>
        <button class="btn btn-danger" onclick="clearAll()">🗑 Clear All</button>
      </div>
      <div style="overflow-x:auto">
        <table>
          <thead>
            <tr>
              <th>Name</th><th>Temp °C</th><th>pH</th><th>EC µS/cm</th>
              <th>Turbidity NTU</th><th>WQI</th><th>MRI</th>
              <th>MP Risk</th><th>Status</th>
            </tr>
          </thead>
          <tbody>
            ${SAMPLES.map(s=>`
            <tr>
              <td class="name-cell">🧪 ${s.name}</td>
              <td class="mono">${s.temperature}</td>
              <td class="mono">${s.ph}</td>
              <td class="mono">${s.ec}</td>
              <td class="mono">${s.turbidity}</td>
              <td class="mono">${s.wqi}</td>
              <td class="mono" style="color:var(--accent2);font-weight:600">${parseFloat(s.mri).toFixed(3)}</td>
              <td><span class="risk-badge ${riskClass(s.mp_risk)}">${s.mp_risk}</span></td>
              <td>${s.status}</td>
            </tr>`).join('')}
          </tbody>
        </table>
      </div>
    </div>
  `;

  // Draw bar chart
  const ctx = document.getElementById('mriChart').getContext('2d');
  const colors = SAMPLES.map(s => {
    if (s.mri < 0.25) return 'rgba(16,185,129,0.8)';
    if (s.mri < 0.50) return 'rgba(52,211,153,0.8)';
    if (s.mri < 0.70) return 'rgba(245,158,11,0.8)';
    if (s.mri < 0.85) return 'rgba(239,68,68,0.8)';
    return 'rgba(220,38,38,0.9)';
  });

  new Chart(ctx, {
    type: 'bar',
    data: {
      labels: SAMPLES.map(s => s.name),
      datasets: [{
        label: 'MRI Score',
        data: SAMPLES.map(s => parseFloat(s.mri).toFixed(4)),
        backgroundColor: colors,
        borderColor: colors.map(c=>c.replace('0.8','1')),
        borderWidth: 1.5,
        borderRadius: 8,
      }]
    },
    options: {
      responsive: true,
      plugins: {
        legend: { display: false },
        tooltip: {
          backgroundColor: '#1a2235',
          borderColor: '#1e2d45',
          borderWidth: 1,
          titleColor: '#e2e8f0',
          bodyColor: '#94a3b8',
          callbacks: {
            label: ctx => ' MRI: ' + ctx.parsed.y
          }
        }
      },
      scales: {
        x: {
          ticks: { color: '#94a3b8', font: { family: 'Space Grotesk' } },
          grid: { color: 'rgba(255,255,255,0.04)' }
        },
        y: {
          min: 0, max: 1,
          ticks: { color: '#94a3b8', font: { family: 'JetBrains Mono', size: 11 } },
          grid: { color: 'rgba(255,255,255,0.06)' }
        }
      }
    }
  });
}

async function clearAll() {
  if (!confirm('Delete all saved samples?')) return;
  await fetch('/clear_samples', { method: 'POST' });
  SAMPLES.length = 0;
  render();
}

render();
</script>
</body>
</html>
)rawliteral";

  return page;
}

// ─── WEB HANDLERS ──────────────────────────────────────────────
void handleRoot() {
  server.send(200, "text/html", htmlPage());
}

void handleAdvanced() {
  server.send(200, "text/html", htmlAdvanced());
}

void handleData() {
  readSensors();

  // Breakdown components for JS
  float turbNorm = constrain(turbidityNTU / 3000.0, 0.0, 1.0);
  float ecNorm   = constrain(ecValue      / 3000.0, 0.0, 1.0);
  float phDev    = constrain(abs(phValue     - 7.0) / 7.0,  0.0, 1.0);
  float tempDev  = constrain(abs(temperatureC- 25.0)/ 35.0, 0.0, 1.0);

  String json = "{";
  json += "\"temperature\":\"" + String(temperatureC, 2) + "\",";
  json += "\"ph\":\""          + String(phValue, 2)      + "\",";
  json += "\"ec\":\""          + String(ecValue, 2)      + "\",";
  json += "\"turbidity\":\""   + String(turbidityNTU, 2) + "\",";
  json += "\"wqi\":\""         + String(wqiScore, 1)     + "\",";
  json += "\"mri\":\""         + String(mriScore, 4)     + "\",";
  json += "\"mp_risk\":\""     + microplasticRisk         + "\",";
  json += "\"status\":\""      + waterStatus              + "\",";
  json += "\"usability\":\""   + usability                + "\",";
  json += "\"turb_norm\":\""   + String(turbNorm, 4)     + "\",";
  json += "\"ec_norm\":\""     + String(ecNorm,   4)     + "\",";
  json += "\"ph_dev\":\""      + String(phDev,    4)     + "\",";
  json += "\"temp_dev\":\""    + String(tempDev,  4)     + "\"";
  json += "}";

  server.send(200, "application/json", json);
}

void handleSave() {
  if (!server.hasArg("name") || server.arg("name").length() == 0) {
    server.send(400, "text/plain", "Missing name");
    return;
  }

  if (savedCount >= MAX_SAVED) {
    // Shift array left (drop oldest)
    for (int i = 0; i < MAX_SAVED - 1; i++) savedSamples[i] = savedSamples[i+1];
    savedCount = MAX_SAVED - 1;
  }

  savedSamples[savedCount].name        = server.arg("name");
  savedSamples[savedCount].temperature = temperatureC;
  savedSamples[savedCount].ph          = phValue;
  savedSamples[savedCount].ec          = ecValue;
  savedSamples[savedCount].turbidity   = turbidityNTU;
  savedSamples[savedCount].wqi         = wqiScore;
  savedSamples[savedCount].mri         = mriScore;
  savedSamples[savedCount].status      = waterStatus;
  savedSamples[savedCount].usability   = usability;
  savedSamples[savedCount].mpRisk      = microplasticRisk;
  savedSamples[savedCount].timestamp   = millis();
  savedCount++;

  saveSamplesToSD();
  server.send(200, "text/plain", "OK");
}

void handleClearSamples() {
  savedCount = 0;
  SD.remove("/saved_samples.csv");
  server.send(200, "text/plain", "OK");
}

void handleDownload() {
  File file = SD.open("/water_log.csv");
  if (!file) { server.send(404, "text/plain", "CSV file not found"); return; }
  server.sendHeader("Content-Type", "text/csv");
  server.sendHeader("Content-Disposition", "attachment; filename=water_log.csv");
  server.streamFile(file, "text/csv");
  file.close();
}

// ─── WIFI ──────────────────────────────────────────────────────
void connectWiFi() {
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println();
  Serial.println("WiFi connected");
  Serial.print("ESP32 IP: ");
  Serial.println(WiFi.localIP());
}

// ─── SETUP ─────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(TURBIDITY_PIN, INPUT);
  pinMode(EC_PIN, INPUT);
  pinMode(PH_PIN, INPUT);

  sensors.begin();
  initSDCard();
  connectWiFi();

  server.on("/",              handleRoot);
  server.on("/advanced",      handleAdvanced);
  server.on("/data",          handleData);
  server.on("/save",   HTTP_POST, handleSave);
  server.on("/clear_samples", HTTP_POST, handleClearSamples);
  server.on("/download",      handleDownload);

  server.begin();
  Serial.println("Web server started");
}

// ─── LOOP ──────────────────────────────────────────────────────
unsigned long lastLogTime = 0;

void loop() {
  server.handleClient();

  if (millis() - lastLogTime > 5000) {
    readSensors();
    logToSD();
    uploadToFirebase();
    lastLogTime = millis();
  }
}
