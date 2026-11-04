/*
 * WYRÁ AUDIO HUB - ESP32 (clássico)
 * Modos: Servidor de Áudio TCP | Bluetooth (A2DP Sink) | Auxiliar (Pass-through)
 * Hardware: ESP32 (WROOM/WROVER) + OLED SSD1306 0.96" + PCM1808 (ADC) + PCM5102A (DAC)
 *           + chave de 3 posições (GPIO 34/35, com pull-ups externos de 10 kΩ)
 *
 * Antes de compilar: em Ferramentas > Partition Scheme, escolha um esquema
 * com mais espaço de app (ex.: "Huge APP" ou "Minimal SPIFFS"). Wi-Fi +
 * WebServer + Bluetooth Clássico juntos costumam estourar o esquema padrão.
 *
 * Author: Sergio Levy (Wyrá, Universidade Federal do Ceará - UFC)
 * License: proprietária - uso pessoal, educacional e não comercial. Veja o arquivo LICENSE.
 * Date: 16/07/2026
 */

#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "time.h"
#include "BluetoothA2DPSink.h"

// ============================================================
// DEFINIÇÃO DE PINOS E CONSTANTES
// ============================================================
#define OLED_SDA      21
#define OLED_SCL      22

#define I2S_TX_BCK    26
#define I2S_TX_WS     25
#define I2S_TX_DOUT   18

#define I2S_RX_BCK    14
#define I2S_RX_WS     12
#define I2S_RX_DIN    13

// Pinos da Chave Seletora de 3 Posições.
// GPIO 34 e 35 só funcionam como entrada e NÃO têm pull-up interno:
// cada um precisa de um resistor de 10 kΩ para 3,3 V.
#define SWITCH_PIN_A  34
#define SWITCH_PIN_B  35
#define SWITCH_DEBOUNCE_MS 300  // a chave precisa ficar parada este tempo para trocar de modo

#define SAMPLE_RATE   44100
#define BUFFER_SAMPLES 128
#define AUDIO_PORT    7000
#define MAX_CLIENTS   4

// Rede e relógio
#define SETUP_AP_SSID           "AudioServer_Setup"
#define BT_DEVICE_NAME          "Audio_Server_ESP32"
#define WIFI_CONNECT_TIMEOUT_MS 10000   // espera pelo Wi-Fi ao entrar no modo servidor
#define WIFI_RETRY_INTERVAL_MS  30000   // no modo de configuração, tenta de novo a rede salva
#define PORTAL_IDLE_MS          120000  // sem tentar enquanto alguém usa a página de configuração
#define NTP_SERVER              "pool.ntp.org"
#define DEFAULT_TZ_MINUTES      (-180)  // UTC-03:00 até o usuário escolher outro fuso

// ============================================================
// LOGO DA WYRÁ (OLED)
// ============================================================
#define LOGO_WIDTH  128
#define LOGO_HEIGHT 64

// Só o pássaro (linhas 0-49). O nome "Wyrá" é escrito em texto por showBootLogo().
const unsigned char wyra_logo [] PROGMEM = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0xe0, 0x00, 0x00, 0x01, 0x00, 0x00, 0xc0, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x78, 0x00, 0x00, 0x01, 0x00, 0x00, 0x80, 0x00, 0x00, 0x1e, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x27, 0x00, 0x00, 0x01, 0x80, 0x01, 0x80, 0x00, 0x00, 0x66, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x11, 0xc0, 0x00, 0x01, 0x80, 0x01, 0x80, 0x00, 0x01, 0x84, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x18, 0x70, 0x00, 0x00, 0xc0, 0x03, 0x80, 0x00, 0x0e, 0x08, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x08, 0x0e, 0x00, 0x00, 0xc0, 0x03, 0x00, 0x00, 0x70, 0x10, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x04, 0x03, 0xc0, 0x00, 0xa0, 0x07, 0x00, 0x03, 0xc0, 0x10, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x02, 0x01, 0xf0, 0x00, 0xa0, 0x05, 0x00, 0x0d, 0x80, 0x20, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x03, 0x00, 0xcc, 0x00, 0x50, 0x0f, 0x00, 0x33, 0x00, 0xc0, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0xe0, 0x33, 0x80, 0x50, 0x0a, 0x01, 0xcc, 0x03, 0x80, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x01, 0xb8, 0x0c, 0x60, 0x48, 0x1a, 0x06, 0x30, 0x0d, 0xc0, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x01, 0xfe, 0x06, 0x18, 0x48, 0x12, 0x18, 0x60, 0x7f, 0x80, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x8f, 0x81, 0x84, 0x25, 0x96, 0x21, 0x81, 0xf9, 0x80, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x43, 0xe0, 0x62, 0x27, 0xe4, 0x46, 0x07, 0xc1, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x60, 0x78, 0x31, 0x23, 0xe4, 0x84, 0x1e, 0x02, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x20, 0x0f, 0x10, 0xa7, 0xe5, 0x88, 0x70, 0x04, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x18, 0x01, 0x88, 0xd6, 0xfb, 0x19, 0x80, 0x1c, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x07, 0x00, 0x08, 0x7c, 0xbe, 0x10, 0x00, 0xf0, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x0f, 0xf0, 0x04, 0x3d, 0x9c, 0x20, 0x07, 0xf0, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x04, 0xfe, 0x02, 0x1b, 0x5c, 0x20, 0x3f, 0xa0, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x02, 0x0f, 0xc2, 0x32, 0x4c, 0x43, 0xf0, 0x60, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x71, 0x34, 0x2e, 0x86, 0x00, 0xc0, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x01, 0xe4, 0x17, 0x80, 0x01, 0x80, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x78, 0x00, 0xf8, 0x1f, 0x00, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x0f, 0x80, 0x68, 0x17, 0x01, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x78, 0x74, 0x3e, 0x3e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x34, 0x2c, 0x60, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x3a, 0x5c, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xda, 0x5b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x6d, 0xbe, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x77, 0xfa, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x6f, 0xf7, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xad, 0xb5, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0xdc, 0x9a, 0xfe, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0xd8, 0x1d, 0xfe, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1f, 0xb8, 0x1d, 0xf8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0f, 0x30, 0x0a, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0xf0, 0x07, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0xa0, 0x05, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x70, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3c, 0x1c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0e, 0x78, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0xe0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

// ============================================================
// MÁQUINA DE ESTADOS E OBJETOS GLOBAIS
// ============================================================
enum OperationMode {
  MODE_SERVER,    // Lado A: Servidor TCP
  MODE_BLUETOOTH, // Centro: Bluetooth
  MODE_AUXILIAR,  // Lado B: Auxiliar (Pass-through)
  MODE_NONE       // Nenhum modo iniciado ainda (boot)
};

OperationMode currentMode = MODE_NONE;

Adafruit_SSD1306 display(128, 64, &Wire, -1);
WebServer webServer(80);
WiFiServer audioServer(AUDIO_PORT);
WiFiClient audioClients[MAX_CLIENTS];
Preferences preferences;
BluetoothA2DPSink a2dp_sink;

String ssid = "";
String password = "";
int tz_minutes = DEFAULT_TZ_MINUTES;

bool isSetupMode = false;         // rede de configuração (AudioServer_Setup) no ar
bool webServerRunning = false;
bool audioServerRunning = false;
bool i2sRunning = false;
unsigned long lastWiFiRetry = 0;
unsigned long lastPortalActivity = 0;  // último acesso à página de configuração

int16_t buffer_in[BUFFER_SAMPLES * 2];
int16_t buffer_out[BUFFER_SAMPLES * 2];
int vu_in = 0, vu_out = 0, connected_apps = 0;

// ============================================================
// INCLUDES LOCAIS (Devem vir após as declarações globais)
// ============================================================
#include "html_pages.h"
#include "AudioI2SForESP32.h"

// ============================================================
// CONFIGURAÇÕES SALVAS (NVS)
// ============================================================
void loadSettings() {
  preferences.begin("audio_cfg", true);
  ssid = preferences.getString("ssid", "");
  password = preferences.getString("pass", "");
  tz_minutes = preferences.getInt("tz_min", DEFAULT_TZ_MINUTES);
  preferences.end();
}

bool hasSavedWiFi() {
  return ssid.length() > 0;
}

// ============================================================
// TELAS DO OLED
// ============================================================
// Corta o texto para caber numa linha do OLED (21 caracteres)
String fitLine(const String &s) {
  return s.length() > 21 ? s.substring(0, 21) : s;
}

// Mostra até 5 linhas de texto, uma a cada 12 px
void showScreen(const String lines[], int count) {
  display.clearDisplay();
  display.setTextSize(1);
  for (int i = 0; i < count && i < 5; i++) {
    display.setCursor(0, i * 12);
    display.print(fitLine(lines[i]));
  }
  display.display();
}

void showBootLogo() {
  display.clearDisplay();
  display.drawBitmap(0, 0, wyra_logo, LOGO_WIDTH, LOGO_HEIGHT, WHITE);
  display.setTextSize(1);
  display.setCursor(52, 54);  // 4 caracteres de 6 px, centralizados em 128 px
  display.print("Wyr");
  display.write(0xA0);        // 'á' na tabela CP437 da fonte padrão
  display.display();
}

void showSetupScreen() {
  if (hasSavedWiFi()) {
    String lines[] = { "CAN'T JOIN WI-FI", "Retrying: " + ssid, "Or set up a network:", SETUP_AP_SSID, "http://192.168.4.1" };
    showScreen(lines, 5);
  } else {
    String lines[] = { "SETUP MODE", "Join the Wi-Fi", SETUP_AP_SSID, "and open", "http://192.168.4.1" };
    showScreen(lines, 5);
  }
}

// ============================================================
// LÓGICA DA CHAVE SELETORA
// ============================================================
OperationMode readSwitchMode() {
  bool stA = digitalRead(SWITCH_PIN_A);
  bool stB = digitalRead(SWITCH_PIN_B);

  if (!stA && stB) return MODE_SERVER;      // Chave no Lado A
  if (stA && !stB) return MODE_AUXILIAR;    // Chave no Lado B
  return MODE_BLUETOOTH;                    // Chave no Centro (Ambos HIGH)
}

// Só aceita a posição nova depois de SWITCH_DEBOUNCE_MS parada. Assim, ao
// deslizar de A para B, o aparelho não liga o Bluetooth no meio do caminho.
OperationMode readSwitchModeDebounced() {
  static OperationMode lastReading = MODE_NONE;
  static unsigned long lastChange = 0;

  OperationMode reading = readSwitchMode();
  if (reading != lastReading) {
    lastReading = reading;
    lastChange = millis();
  }
  if (millis() - lastChange < SWITCH_DEBOUNCE_MS) return currentMode;
  return reading;
}

// ============================================================
// WI-FI, SERVIDOR WEB E I2S
// ============================================================
// Conecta à rede salva, esperando no máximo timeoutMs
bool connectWiFi(unsigned long timeoutMs) {
  if (WiFi.status() == WL_CONNECTED) return true;
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(ssid.c_str(), password.c_str());
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < timeoutMs) delay(250);
  return WiFi.status() == WL_CONNECTED;
}

void startNetworkTime() {
  configTime((long)tz_minutes * 60, 0, NTP_SERVER);
}

void startWebServer() {
  if (webServerRunning) return;
  webServer.begin();
  webServerRunning = true;
}

void stopWebServer() {
  if (!webServerRunning) return;
  webServer.stop();
  webServerRunning = false;
}

void startI2S() {
  if (i2sRunning) return;
  initI2S_TX();
  initI2S_RX();
  i2sRunning = true;
}

void stopI2S() {
  if (!i2sRunning) return;
  i2s_driver_uninstall(I2S_NUM_0);
  i2s_driver_uninstall(I2S_NUM_1);
  i2sRunning = false;
}

// Abre a rede AudioServer_Setup com a página de configuração.
// Se já houver uma rede salva, ela NÃO é apagada: retrySavedWiFi() continua
// tentando (ex.: depois de uma queda de luz, o roteador costuma ligar depois do ESP32).
void startSetupPortal() {
  isSetupMode = true;
  WiFi.setAutoReconnect(false);   // as novas tentativas ficam por conta de retrySavedWiFi()
  WiFi.mode(WIFI_AP_STA);
  if (hasSavedWiFi()) WiFi.disconnect();
  WiFi.softAP(SETUP_AP_SSID);
  startWebServer();
  lastWiFiRetry = millis();
  lastPortalActivity = millis() - PORTAL_IDLE_MS;  // ninguém usou a página ainda
  showSetupScreen();
}

void retrySavedWiFi() {
  if (!hasSavedWiFi()) return;

  if (WiFi.status() == WL_CONNECTED) {
    // A rede salva voltou: reinicia para entrar no modo normal
    String lines[] = { "Wi-Fi is back!", "Restarting..." };
    showScreen(lines, 2);
    delay(1000);
    ESP.restart();
  }

  // Não procura a rede enquanto alguém estiver usando a página de configuração:
  // a busca troca o canal do rádio e pode derrubar o celular. Um celular que
  // só ficou conectado à rede, sem abrir a página, não impede as tentativas.
  if (millis() - lastPortalActivity < PORTAL_IDLE_MS) return;
  if (millis() - lastWiFiRetry < WIFI_RETRY_INTERVAL_MS) return;
  lastWiFiRetry = millis();
  WiFi.begin(ssid.c_str(), password.c_str());
}

// ============================================================
// MODOS DE OPERAÇÃO
// ============================================================
void bt_data_callback(const uint8_t *data, uint32_t len) {
  int16_t *pcm_data = (int16_t *)data;
  int max_val = 0;
  for (uint32_t i = 0; i < len / 2; i++) {
    if (abs(pcm_data[i]) > max_val) max_val = abs(pcm_data[i]);
  }
  vu_out = map(max_val, 0, 32767, 0, 100);
}

// Servidor TCP: é o único modo que precisa do Wi-Fi para funcionar
void startServerMode() {
  if (!hasSavedWiFi()) {
    startSetupPortal();
    return;
  }

  String lines[] = { "Connecting Wi-Fi...", ssid };
  showScreen(lines, 2);
  if (!connectWiFi(WIFI_CONNECT_TIMEOUT_MS)) {
    startSetupPortal();   // mantém a rede salva e continua tentando
    return;
  }

  startNetworkTime();
  startI2S();
  audioServer.begin();
  audioServerRunning = true;
  startWebServer();
}

// Bluetooth: funciona sem Wi-Fi (o rádio Wi-Fi fica desligado)
void startBluetoothMode() {
  stopWebServer();
  WiFi.mode(WIFI_OFF);

  // 1. Configura o leitor de VU Meter e inicia o Bluetooth
  // (A biblioteca instalará o driver I2S internamente)
  a2dp_sink.set_stream_reader(bt_data_callback);
  a2dp_sink.start(BT_DEVICE_NAME);

  // 2. Redireciona os pinos nativamente direto no chip ESP32
  i2s_pin_config_t pin_conf = {};
  pin_conf.mck_io_num = I2S_PIN_NO_CHANGE;
  pin_conf.bck_io_num = I2S_TX_BCK;
  pin_conf.ws_io_num = I2S_TX_WS;
  pin_conf.data_out_num = I2S_TX_DOUT;
  pin_conf.data_in_num = I2S_PIN_NO_CHANGE;
  i2s_set_pin(I2S_NUM_0, &pin_conf);
}

// Auxiliar: o áudio não depende da rede. Se houver Wi-Fi salvo, conecta
// em segundo plano só para o painel web.
void startAuxMode() {
  startI2S();
  if (hasSavedWiFi()) {
    if (WiFi.status() != WL_CONNECTED) {
      WiFi.mode(WIFI_STA);
      WiFi.setAutoReconnect(true);
      WiFi.begin(ssid.c_str(), password.c_str());  // não bloqueia
    }
    startNetworkTime();
    startWebServer();
  } else {
    WiFi.mode(WIFI_OFF);
  }
}

void stopCurrentMode() {
  if (currentMode == MODE_BLUETOOTH) {
    a2dp_sink.end();
    delay(200);
    i2s_driver_uninstall(I2S_NUM_0);
  }

  if (audioServerRunning) {
    for (int i = 0; i < MAX_CLIENTS; i++) if (audioClients[i]) audioClients[i].stop();
    audioServer.end();
    audioServerRunning = false;
  }

  stopI2S();

  if (isSetupMode) {
    stopWebServer();
    WiFi.softAPdisconnect(true);
    isSetupMode = false;
  }
}

void changeMode(OperationMode newMode) {
  if (currentMode == newMode) return;

  if (currentMode != MODE_NONE) {
    String lines[] = { "Switching mode..." };
    showScreen(lines, 1);
  }

  stopCurrentMode();
  currentMode = newMode;
  vu_in = 0; vu_out = 0; connected_apps = 0;

  if (currentMode == MODE_SERVER) startServerMode();
  else if (currentMode == MODE_BLUETOOTH) startBluetoothMode();
  else startAuxMode();
}

// ============================================================
// ATUALIZAÇÃO DO DISPLAY OLED
// ============================================================
void updateOLED() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  if (currentMode == MODE_SERVER) display.print("MODE: TCP SERVER");
  else if (currentMode == MODE_BLUETOOTH) display.print("MODE: BLUETOOTH");
  else display.print("MODE: AUX");

  display.setCursor(0, 10);
  if (currentMode == MODE_BLUETOOTH) {
    if (a2dp_sink.get_audio_state() == ESP_A2D_AUDIO_STATE_STARTED) display.print("Playing");
    else display.print("Waiting for pairing");
  } else if (WiFi.status() == WL_CONNECTED) {
    String ipStr = WiFi.localIP().toString();
    display.print("IP: "); display.print(ipStr.length() > 13 ? ipStr.substring(0, 13) : ipStr);
    if (currentMode == MODE_SERVER) { display.setCursor(110, 10); display.print(connected_apps); }
  } else if (hasSavedWiFi()) {
    display.print("Wi-Fi connecting...");
  } else {
    display.print("Wi-Fi not set up");
  }

  display.setCursor(0, 25); display.print("IN: ");
  display.drawRect(28, 25, 98, 8, WHITE);
  display.fillRect(28, 25, map(vu_in, 0, 100, 0, 98), 8, WHITE);

  display.setCursor(0, 40); display.print("OUT:");
  display.drawRect(28, 40, 98, 8, WHITE);
  display.fillRect(28, 40, map(vu_out, 0, 100, 0, 98), 8, WHITE);

  display.display();
}

// ============================================================
// SETUP
// ============================================================
void setup() {
  Serial.begin(115200);

  // Pinos da chave (os pull-ups são externos: GPIO 34/35 não têm pull-up interno)
  pinMode(SWITCH_PIN_A, INPUT);
  pinMode(SWITCH_PIN_B, INPUT);

  Wire.begin(OLED_SDA, OLED_SCL);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) for (;;);
  display.cp437(true);
  display.setTextSize(1);
  display.setTextColor(WHITE);

  // ----------------------------------------------------
  // ANIMAÇÃO DE INICIALIZAÇÃO - MOSTRA A LOGO
  // ----------------------------------------------------
  showBootLogo();
  delay(3000); // Mantém a logo na tela por 3 segundos

  loadSettings();

  webServer.on("/", handleRoot);
  webServer.on("/save", HTTP_POST, handleSave);
  webServer.on("/reset", handleReset);
  webServer.on("/logo.png", handleLogo);

  // Inicia no modo em que a chave estiver
  changeMode(readSwitchMode());
}

// ============================================================
// LOOP PRINCIPAL
// ============================================================
void loop() {
  OperationMode selectedMode = readSwitchModeDebounced();
  if (selectedMode != currentMode) changeMode(selectedMode);

  if (webServerRunning) webServer.handleClient();

  if (isSetupMode) {
    // Sem áudio enquanto a rede não está configurada
    retrySavedWiFi();
    delay(2);
    return;
  }

  size_t bytes_read, bytes_written;

  if (currentMode == MODE_SERVER) {
    if (audioServer.hasClient()) {
      for (int i = 0; i < MAX_CLIENTS; i++) {
        if (!audioClients[i] || !audioClients[i].connected()) {
          if (audioClients[i]) audioClients[i].stop();
          audioClients[i] = audioServer.accept(); break;
        }
      }
    }

    connected_apps = 0;
    for (int i = 0; i < MAX_CLIENTS; i++) if (audioClients[i] && audioClients[i].connected()) connected_apps++;

    // === LÊ DA MESA E CALCULA VU_IN (PICO) ===
    i2s_read(I2S_NUM_1, buffer_in, sizeof(buffer_in), &bytes_read, portMAX_DELAY);
    int peak_in = 0;
    for (int i = 0; i < bytes_read / 2; i++) {
      if (abs(buffer_in[i]) > peak_in) peak_in = abs(buffer_in[i]);
    }
    vu_in = map(peak_in, 0, 32767, 0, 100);

    // Envia para o PC/App
    for (int i = 0; i < MAX_CLIENTS; i++) {
      if (audioClients[i] && audioClients[i].connected()) audioClients[i].write((uint8_t*)buffer_in, bytes_read);
    }

    // === RECEBE DO PC/APP E CALCULA VU_OUT (PICO) ===
    bool receivedAudio = false;
    for (int i = 0; i < MAX_CLIENTS; i++) {
      if (audioClients[i] && audioClients[i].connected() && audioClients[i].available() >= sizeof(buffer_out)) {
        int bytes = audioClients[i].read((uint8_t*)buffer_out, sizeof(buffer_out));
        i2s_write(I2S_NUM_0, buffer_out, bytes, &bytes_written, 0);

        int peak_out = 0;
        for (int j = 0; j < bytes / 2; j++) {
          if (abs(buffer_out[j]) > peak_out) peak_out = abs(buffer_out[j]);
        }
        vu_out = map(peak_out, 0, 32767, 0, 100);

        receivedAudio = true; break;
      }
    }
    if (!receivedAudio) vu_out = 0;
  } else if (currentMode == MODE_AUXILIAR) {
    i2s_read(I2S_NUM_1, buffer_in, sizeof(buffer_in), &bytes_read, portMAX_DELAY);
    vu_in = map(abs(buffer_in[0]), 0, 32767, 0, 100);
    i2s_write(I2S_NUM_0, buffer_in, bytes_read, &bytes_written, 0);
    vu_out = vu_in;
  } else {
    vu_in = 0;
  }

  static unsigned long lastOLED = 0;
  if (millis() - lastOLED > 1000) {
    updateOLED(); lastOLED = millis();
  }
}
