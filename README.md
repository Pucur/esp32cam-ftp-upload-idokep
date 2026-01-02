# 📷 ESP32-CAM FTP Uploader

A lightweight, stable **ESP32-CAM image capture & FTP upload system** designed for **24/7 operation** with OTA support and optimized camera handling.

---

## 🚀 Features

* 📸 High-resolution image capture (UXGA / SVGA)
* 🌐 FTP upload (passive mode)
* 🔁 OTA firmware update support
* 🧠 Optimized camera exposure handling (auto warm-up + lock)
* ⚡ Stable WiFi performance
* 🌙 Optional night-time idle mode
* 🔒 Memory-safe camera buffer handling

---

## 🧠 Design Goals

* Avoid ESP32 freezes and WiFi lag caused by continuous camera auto-exposure recalculation
* Keep OTA responsive during normal operation
* Keep FTP transfers reliable
* Support long-term unattended usage

---

## 🧰 Hardware

* **ESP32-CAM (AI Thinker)**
* **OV2640** camera module
* Stable **5V power supply** (recommended: 1–2A, short cable)
* WiFi connection
* FTP server with **PASV** enabled

---

## 🔌 Pin Configuration (AI Thinker)

```cpp
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
```

---

## ⚙️ Configuration

### WiFi

```cpp
const char* ssid = "YOUR_WIFI";
const char* password = "YOUR_PASSWORD";
```

### FTP

```cpp
const char* ftp_host = "ftp.example.com";
const int   ftp_port = 21;
const char* ftp_user = "username";
const char* ftp_pass = "password";
```

---

## 📸 Camera Settings

Default high-quality profile:

```cpp
config.frame_size   = FRAMESIZE_UXGA;
config.jpeg_quality = 10;
config.fb_count     = 2;
```

> ⚠️ Note: UXGA is heavy. This project keeps it, but camera exposure is handled carefully to avoid system lag.

---

## 🧠 Camera Stabilization (Warm-up → Lock)

Continuous auto exposure / gain can cause CPU load and WiFi latency. The solution is:

1. let the camera auto-adjust for a few frames
2. lock the settings for stable long-running performance

### Warm-up (auto)

```cpp
s->set_gain_ctrl(s, 1);
s->set_exposure_ctrl(s, 1);
```

### Lock (manual)

```cpp
s->set_exposure_ctrl(s, 0);
s->set_gain_ctrl(s, 0);
```

---

## 🌙 Night Mode

No capture between **22:00–05:00**:

```cpp
bool isNightTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return false;
  int hour = timeinfo.tm_hour;
  return (hour >= 22 || hour < 5);
}
```

## ✅ Tested

* ESP32-CAM (AI Thinker)
* OVA3660
* FTP server with PASV support

---

## 🧩 Optional Improvements

* Timestamped filenames
* FTP retry logic
* Day/night camera profiles
* Watchdog protection
