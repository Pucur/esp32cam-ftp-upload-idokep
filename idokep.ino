/*
  ESP32-CAM FTP Uploader
  ---------------------
  Author: Pucur
  Created: 2026
  License: MIT

  Description:
  ESP32-CAM based image capture and FTP upload system
  with OTA update support and optimized camera handling.

  This project is free to use, modify, and distribute
  under the MIT License.
*/
#include "esp_camera.h"
#include <WiFi.h>
#include <ArduinoOTA.h>
#include "esp_system.h"
#include <time.h>
//#include <WebServer.h>

// ---- WiFi ----
const char* ssid = "WIFI_SSID";
const char* password = "WIFI_PASSWORD";
const char* otaPassword = "OTA_PASSWORD";
bool firstBoot = true;
unsigned long lastShot = 0;
const unsigned long interval = 30000;
volatile bool otaInProgress = false;
static unsigned long lastWiFiTry = 0;


// ---- FTP ----
const char* ftp_host = "ftp.viharvadasz.hu";
const int ftp_port = 21;
const char* ftp_user = "USERNAME";
const char* ftp_pass = "PASSWORD";

WiFiClient ftpCtrl;
WiFiClient ftpData;

// ---- WEB LOG ----
//WebServer server(80);
//String ftpLog = "";
//const size_t MAX_LOG = 6000;

// ---- Kamera pinout ----
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

// ================= LOG HELPER =================
/*
void addLog(const String &msg) {
  ftpLog += msg + "\n";
  if (ftpLog.length() > MAX_LOG) {
    ftpLog.remove(0, ftpLog.length() - MAX_LOG);
  }
}
*/
// ================= FTP HELPERS =================

String readFTP() {
  String r = "";
  unsigned long t = millis();

  while (millis() - t < 2000) {
    while (ftpCtrl.available()) {
      char c = ftpCtrl.read();
      r += c;
    }
  }

/*
  if (r.length()) addLog("FTP << " + r);
  return r;
  */
  return r;
}

void sendFTP(String cmd) {
  //addLog("FTP >> " + cmd);
  ftpCtrl.print(cmd + "\r\n");
}

bool ftpConnect() {
  //addLog("FTP CONNECT");
  if (!ftpCtrl.connect(ftp_host, ftp_port)) {
  //  addLog("FTP CONNECT FAILED");
    return false;
  }

  readFTP();

  sendFTP("USER " + String(ftp_user));
  readFTP();

  sendFTP("PASS " + String(ftp_pass));
  readFTP();

  return true;
}

bool ftpUpload(uint8_t *data, size_t len) {

  sendFTP("TYPE I");
  readFTP();

  sendFTP("CWD lol2");
  readFTP();

  sendFTP("PASV");
  String resp = readFTP();

int ip[4], p1, p2;

int start = resp.indexOf('(');
int end   = resp.indexOf(')');

if (start < 0 || end < 0 || end <= start) {
  //addLog("PASV parse error: no brackets");
  return false;
}

String pasvData = resp.substring(start + 1, end);

if (sscanf(pasvData.c_str(), "%d,%d,%d,%d,%d,%d",
           &ip[0], &ip[1], &ip[2], &ip[3], &p1, &p2) != 6) {
  //addLog("PASV parse error: bad numbers");
  return false;
}

  int dataPort = p1 * 256 + p2;
  IPAddress dataIP(ip[0], ip[1], ip[2], ip[3]);

  if (!ftpData.connect(dataIP, dataPort)) {
    //addLog("DATA CONNECT FAILED");
    return false;
  }

  // ---- Feltöltés indítása ----
  sendFTP("STOR kep.jpg");

  String storResp = readFTP();
  if (!storResp.startsWith("150")) {
    //addLog("STOR not accepted");
    ftpData.stop();
    return false;
  }

  // ---- Adatküldés ----
  size_t sent = 0;
  while (sent < len) {
    size_t chunk = ftpData.write(data + sent, len - sent);
    if (chunk == 0) break;
    sent += chunk;
  }

  ftpData.stop();
  readFTP();

  sendFTP("QUIT");
  readFTP();

  return true;
}

// ================= CAMERA =================

void warmUpCamera(int frames = 5) {
  for (int i = 0; i < frames; i++) {
    camera_fb_t * fb = esp_camera_fb_get();
    if (fb) esp_camera_fb_return(fb);
    delay(200);
  }
  firstBoot = false;
}

void initCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  if (psramFound()) {
    config.frame_size = FRAMESIZE_UXGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

  if (esp_camera_init(&config) != ESP_OK) {
    return;
  }

  sensor_t *s = esp_camera_sensor_get();

  // Rotate image
  s->set_vflip(s, 1);

  s->set_brightness(s, 0);
  s->set_contrast(s, 1);
  s->set_saturation(s, 1);
  s->set_sharpness(s, 1);

  if (firstBoot) {
    warmUpCamera();
    firstBoot = false;
  }
}

bool isNightTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return false;
  int hour = timeinfo.tm_hour;
  return (hour >= 22 || hour < 5);
}

// ================= SETUP =================

void setup() {
  delay(2000);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) delay(500);

  ArduinoOTA.setHostname("IDOKEP_CAM");
  ArduinoOTA.setPassword(otaPassword);

  ArduinoOTA
    .onStart([]() {
      otaInProgress = true;
      Serial.println("OTA START");
    })
    .onEnd([]() {
      otaInProgress = false;
      Serial.println("OTA END");
    })
    .onError([](ota_error_t error) {
      Serial.printf("OTA Error: %u\n", error);
      otaInProgress = false;
    });

  ArduinoOTA.begin();

  configTime(3600, 3600, "pool.ntp.org", "time.nist.gov");

  // ---- WEB LOG ENDPOINT ----
  /*
  server.on("/log", []() {
    server.send(200, "text/plain", ftpLog);
  });
  server.begin();
*/
  unsigned long otaWindowStart = millis();
  while (millis() - otaWindowStart < 30000) {
    ArduinoOTA.handle();
    delay(10);
  }

  if (!otaInProgress) {
    initCamera();
  }
}

// ================= LOOP =================

void loop() {
  ArduinoOTA.handle();
  //server.handleClient();

  if (otaInProgress) {
    delay(1);
    return;
  }

if (WiFi.status() != WL_CONNECTED) {
  if (millis() - lastWiFiTry > 5000) {
    lastWiFiTry = millis();
    WiFi.disconnect();
    WiFi.begin(ssid, password);
  }
}


  if (isNightTime()) {
    for (int i = 0; i < 60; i++) {
      ArduinoOTA.handle();
      delay(1000);
    }
    return;
  }

  if (millis() - lastShot < interval) return;
  lastShot = millis();

  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) return;

  if (!otaInProgress && ftpConnect()) {
    ftpUpload(fb->buf, fb->len);
    ftpCtrl.stop();
  }

  esp_camera_fb_return(fb);
}
