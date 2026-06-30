#ifndef CONFIG_H
#define CONFIG_H

// =================== Wi-Fi ===================
#define WIFI_SSID      "TESTE"
#define WIFI_PASSWORD  "MICROCONTROLADOR"

// =================== NTP / Fuso ===================
#define NTP_SERVER     "pool.ntp.org"
#define GMT_OFFSET_SEC (-10800)   // UTC-3 (Brasília) em segundos
#define DAYLIGHT_SEC   0          // 0 = sem horário de verão

// =================== Endereços I2C ===================
#define LCD_ADDR  0x27   // ou 0x3F, conforme o módulo
#define MCP_ADDR  0x20   // A0=A1=A2 ligados ao GND

// =================== RFID (MFRC522 - SPI) ===================
#define RFID_SS_PIN   5
#define RFID_RST_PIN  4
// SPI padrão do ESP32: SCK=18, MISO=19, MOSI=23

// =================== Trincos ===================
#define LOCK_PULSE_MS 800   // tempo (ms) que o solenoide fica energizado
#define N_SERVOS 3
#define S3 2
#define S2 15
#define S1 16
#define S0 17
#define E 0

// =================== Capacidade do armário ===================
#define N_SLOTS 16          // 8 'P' (1-8), 4 'M' (9-12), 4 'G' (13-16)
#define PEQUENOS 8
#define MEDIOS 12

#endif