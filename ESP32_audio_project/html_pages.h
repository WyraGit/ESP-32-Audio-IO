#ifndef HTML_PAGES_H
#define HTML_PAGES_H

#define VERSAO_LIB 1.0

#include "logo_png.h"

// ============================================================
// DECLARAÇÃO DE VARIÁVEIS EXTERNAS (Vêm do ficheiro principal)
// ============================================================
extern bool isSetupMode;
extern OperationMode currentMode;
extern int connected_apps;
extern int vu_in;
extern int vu_out;
extern String ssid;
extern int tz_minutes;
extern unsigned long lastPortalActivity;
extern WebServer webServer;
extern Preferences preferences;


// ============================================================
// PÁGINAS WEB (HTML)
// ============================================================

const char* html_setup = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Wyrá - Setup</title>
  <link rel="icon" type="image/png" href="/logo.png">
  <style>
    :root {
      --bg-color: #000000;
      --card-bg: #121212;
      --primary: #c38b38;
      --primary-hover: #a3722b;
      --text-main: #ffffff;
      --text-muted: #888888;
      --input-bg: #1e1e1e;
      --border-color: #333333;
    }
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body {
      font-family: 'Segoe UI', Roboto, Helvetica, Arial, sans-serif;
      background-color: var(--bg-color);
      color: var(--text-main);
      display: flex; justify-content: center; align-items: center;
      min-height: 100vh; padding: 20px;
    }
    .card {
      background: var(--card-bg); width: 100%; max-width: 400px;
      padding: 40px 30px; border-radius: 12px;
      box-shadow: 0 10px 30px rgba(0, 0, 0, 0.8);
      border: 1px solid var(--border-color); text-align: center;
    }
    .logo-container { margin-bottom: 25px; background: rgba(0, 0, 0, 0)}
    .logo-container img { max-width: 220px; width: 100%; height: auto; }
    .brand { font-family: ui-monospace, Consolas, Menlo, monospace; font-size: 26px; letter-spacing: 2px; margin-top: 8px; }
    .header { margin-bottom: 30px; }
    .header p { color: var(--text-muted); font-size: 15px; letter-spacing: 1px; text-transform: uppercase;}
    .notice {
      background: rgba(195, 139, 56, 0.12); border: 1px solid rgba(195, 139, 56, 0.4);
      color: var(--text-main); font-size: 14px; line-height: 1.5; text-align: left;
      padding: 12px 14px; border-radius: 8px; margin-bottom: 20px;
    }
    .input-group { margin-bottom: 20px; text-align: left; }
    label {
      display: block; font-size: 12px; font-weight: 600; color: var(--text-muted);
      margin-bottom: 8px; text-transform: uppercase; letter-spacing: 0.5px;
    }
    select, input {
      width: 100%; padding: 14px; background: var(--input-bg); color: var(--text-main);
      border: 1px solid var(--border-color); border-radius: 8px; font-size: 16px;
      transition: all 0.3s ease; appearance: none; -webkit-appearance: none;
    }
    select {
      background-image: url("data:image/svg+xml;charset=US-ASCII,%3Csvg%20xmlns%3D%22http%3A%2F%2Fwww.w3.org%2F2000%2Fsvg%22%20width%3D%22292.4%22%20height%3D%22292.4%22%3E%3Cpath%20fill%3D%22%23888888%22%20d%3D%22M287%2069.4a17.6%2017.6%200%200%200-13-5.4H18.4c-5%200-9.3%201.8-12.9%205.4A17.6%2017.6%200%200%200%200%2082.2c0%205%201.8%209.3%205.4%2012.9l128%20127.9c3.6%203.6%207.8%205.4%2012.8%205.4s9.2-1.8%2012.8-5.4L287%2095c3.5-3.5%205.4-7.8%205.4-12.8%200-5-1.9-9.2-5.5-12.8z%22%2F%3E%3C%2Fsvg%3E");
      background-repeat: no-repeat; background-position: right 14px top 50%; background-size: 12px auto;
    }
    select:focus, input:focus { outline: none; border-color: var(--primary); box-shadow: 0 0 0 2px rgba(195, 139, 56, 0.3); }
    button {
      width: 100%; padding: 14px; margin-top: 10px;
      background: linear-gradient(135deg, var(--primary), var(--primary-hover));
      color: white; font-size: 16px; font-weight: bold; border: none; border-radius: 8px; cursor: pointer;
      transition: transform 0.2s, box-shadow 0.2s;
    }
    button:hover { transform: translateY(-2px); box-shadow: 0 6px 15px rgba(195, 139, 56, 0.4); }
  </style>
</head>
<body>
  <div class="card">
    <div class="logo-container">
      <img src="/logo.png" alt="">
      <div class="brand">Wyrá</div>
    </div>
    <div class="header">
      <p>Network setup</p>
    </div>
    %STATUS%
    <form action="/save" method="POST">
      <div class="input-group">
        <label>Wi-Fi network</label>
        <select name="ssid" required>
          <option value="" disabled selected>Choose a network</option>
          %OPTIONS%
        </select>
      </div>
      <div class="input-group">
        <label>Wi-Fi password</label>
        <input type="password" name="pass" placeholder="Leave blank for an open network">
      </div>
      <div class="input-group">
        <label>Time zone</label>
        <select name="tz">
          %TZ_OPTIONS%
        </select>
      </div>
      <button type="submit">Save and connect</button>
    </form>
  </div>
</body>
</html>
)rawliteral";

const char* html_dashboard = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Wyrá - Dashboard</title>
  <link rel="icon" type="image/png" href="/logo.png">
  <meta http-equiv="refresh" content="3">
  <style>
    :root {
      --bg-color: #000000;
      --card-bg: #121212;
      --primary: #c38b38;
      --danger: #dc3545;
      --text-main: #ffffff;
      --text-muted: #888888;
      --box-bg: #1e1e1e;
    }
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body {
      font-family: 'Segoe UI', Roboto, Helvetica, Arial, sans-serif;
      background-color: var(--bg-color); color: var(--text-main);
      display: flex; justify-content: center; align-items: center;
      min-height: 100vh; padding: 20px;
    }
    .card {
      background: var(--card-bg); width: 100%; max-width: 450px;
      padding: 30px; border-radius: 12px; box-shadow: 0 10px 30px rgba(0, 0, 0, 0.8);
      text-align: center; border: 1px solid #333;
    }
    .logo-container { margin-bottom: 20px; background: rgba(0, 0, 0, 0)}
    .logo-container img { max-width: 220px; width: 100%; height: auto; display: inline-block; }
    .brand { font-family: ui-monospace, Consolas, Menlo, monospace; font-size: 26px; letter-spacing: 2px; margin-top: 8px; }
    .status-badge {
      display: inline-block; background: rgba(0, 0, 0, 0); color: var(--primary);
      padding: 6px 18px; border-radius: 20px; font-size: 13px; font-weight: bold;
      letter-spacing: 1px; margin-bottom: 25px; border: 1px solid rgba(195, 139, 56, 0.4);
    }
    .grid { display: grid; grid-template-columns: 1fr; gap: 12px; margin-bottom: 20px; }
    .box {
      background: var(--box-bg); padding: 15px; border-radius: 8px; text-align: left;
      border-left: 4px solid var(--primary);
    }
    .box-title { font-size: 11px; color: var(--text-muted); text-transform: uppercase; letter-spacing: 0.5px; margin-bottom: 5px; }
    .box-value { font-size: 16px; font-weight: bold; }

    /* VU METERS */
    .vu-container { background: var(--box-bg); padding: 15px; border-radius: 8px; margin-bottom: 12px; text-align: left; }
    .vu-header { display: flex; justify-content: space-between; font-size: 12px; color: var(--text-muted); text-transform: uppercase; font-weight: bold; margin-bottom: 8px; }
    .vu-bar-bg { width: 100%; height: 12px; background: #000; border-radius: 6px; overflow: hidden; }
    .vu-bar-fill { height: 100%; background: linear-gradient(90deg, #28a745 0%, #ffc107 75%, #dc3545 100%); transition: width 0.3s ease; }

    .btn-reset {
      display: block; width: 100%; padding: 14px; margin-top: 20px; background: transparent;
      color: var(--danger); font-size: 15px; font-weight: bold; text-decoration: none;
      border: 1px solid var(--danger); border-radius: 8px; transition: all 0.3s ease;
    }
    .btn-reset:hover { background: var(--danger); color: #fff; box-shadow: 0 4px 15px rgba(220, 53, 69, 0.4); }
  </style>
</head>
<body>
  <div class="card">
    <div class="logo-container">
      <img src="/logo.png" alt="">
      <div class="brand">Wyrá</div>
    </div>

    <div class="status-badge">%MODE% MODE</div>

    <div class="grid">
      <div class="box">
        <div class="box-title">Device IP</div>
        <div class="box-value">%IP%</div>
      </div>
      <div class="box">
        <div class="box-title">TCP clients (port 7000)</div>
        <div class="box-value">%CLIENTS% / 4</div>
      </div>
      <div class="box">
        <div class="box-title">Time (%TZ%)</div>
        <div class="box-value">%TIME%</div>
      </div>
    </div>

    <div class="vu-container">
      <div class="vu-header"><span>Line in</span><span>%VU_IN%%</span></div>
      <div class="vu-bar-bg"><div class="vu-bar-fill" style="width: %VU_IN%%;"></div></div>
    </div>

    <div class="vu-container">
      <div class="vu-header"><span>Line out</span><span>%VU_OUT%%</span></div>
      <div class="vu-bar-bg"><div class="vu-bar-fill" style="width: %VU_OUT%%;"></div></div>
    </div>

    <a href="/reset" class="btn-reset" onclick="return confirm('Erase the saved Wi-Fi and restart?');">Reset Wi-Fi</a>
  </div>
</body>
</html>
)rawliteral";

// Página curta para as respostas de "salvo" e "resetado"
const char* html_message = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Wyrá</title>
  <style>
    body { font-family: 'Segoe UI', Roboto, Helvetica, Arial, sans-serif; background: #000; color: #fff;
           display: flex; justify-content: center; align-items: center; min-height: 100vh; margin: 0; padding: 20px; text-align: center; }
    h2 { font-size: 20px; font-weight: 600; }
    p { color: #888; margin-top: 10px; }
  </style>
</head>
<body><div><h2>%TITLE%</h2><p>%TEXT%</p></div></body>
</html>
)rawliteral";


// ============================================================
// FUNÇÕES AUXILIARES
// ============================================================

// Escapa texto vindo de fora (nomes de rede) antes de pôr no HTML
String htmlEscape(const String &s) {
  String out;
  out.reserve(s.length() + 8);
  for (unsigned int i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '&') out += "&amp;";
    else if (c == '<') out += "&lt;";
    else if (c == '>') out += "&gt;";
    else if (c == '"') out += "&quot;";
    else if (c == '\'') out += "&#39;";
    else out += c;
  }
  return out;
}

// "UTC-03:00", "UTC+05:30"...
String formatUtcOffset(int minutes) {
  char label[12];
  int a = abs(minutes);
  snprintf(label, sizeof(label), "UTC%c%02d:%02d", minutes < 0 ? '-' : '+', a / 60, a % 60);
  return String(label);
}

// Fusos de UTC-12:00 a UTC+14:00, de 30 em 30 minutos
String timeZoneOptions(int selected) {
  String out;
  for (int m = -12 * 60; m <= 14 * 60; m += 30) {
    out += "<option value='" + String(m) + "'";
    if (m == selected) out += " selected";
    out += ">" + formatUtcOffset(m) + "</option>";
  }
  return out;
}

void sendMessagePage(const String &title, const String &text) {
  String html = html_message;
  html.replace("%TITLE%", title);
  html.replace("%TEXT%", text);
  webServer.send(200, "text/html", html);
}


// ============================================================
// FUNÇÕES DO SERVIDOR WEB
// ============================================================
void handleRoot() {
  if (isSetupMode) {
    lastPortalActivity = millis();
    String html = html_setup;

    // Aviso quando já existe uma rede salva que não respondeu
    String status = "";
    if (ssid.length() > 0) {
      status = "<div class='notice'>Couldn't join <b>" + htmlEscape(ssid) +
               "</b>. The device keeps retrying every 30 seconds. To use another network, choose it below.</div>";
    }
    html.replace("%STATUS%", status);

    String options = "";
    int n = WiFi.scanNetworks();
    if (n <= 0) {
      options = "<option value='' disabled>No networks found - reload the page</option>";
    } else {
      for (int i = 0; i < n; i++) {
        String name = WiFi.SSID(i);
        if (name.length() == 0) continue;  // redes ocultas
        String safe = htmlEscape(name);
        options += "<option value='" + safe + "'";
        if (name == ssid) options += " selected";
        options += ">" + safe + " (" + String(WiFi.RSSI(i)) + " dBm)</option>";
      }
    }
    WiFi.scanDelete();
    html.replace("%OPTIONS%", options);
    html.replace("%TZ_OPTIONS%", timeZoneOptions(tz_minutes));
    webServer.send(200, "text/html", html);
  } else {
    String html = html_dashboard;

    String modeStr;
    if (currentMode == MODE_SERVER) modeStr = "TCP SERVER";
    else if (currentMode == MODE_BLUETOOTH) modeStr = "BLUETOOTH";
    else modeStr = "AUX";
    html.replace("%MODE%", modeStr);

    html.replace("%IP%",   WiFi.localIP().toString());
    html.replace("%CLIENTS%", String(connected_apps));
    html.replace("%TZ%", formatUtcOffset(tz_minutes));

    struct tm timeinfo;
    if (getLocalTime(&timeinfo, 0)) {
      char timeStr[32];
      sprintf(timeStr, "%02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
      html.replace("%TIME%", String(timeStr));
    } else {
      html.replace("%TIME%", "Syncing...");
    }

    html.replace("%VU_IN%",  String(vu_in));
    html.replace("%VU_OUT%", String(vu_out));
    webServer.send(200, "text/html", html);
  }
}

void handleSave() {
  String newSsid = webServer.arg("ssid");
  if (newSsid.length() == 0) {
    sendMessagePage("Choose a network", "<a href='/' style='color:#c38b38'>Go back</a> and pick a Wi-Fi network.");
    return;
  }
  int newTz = webServer.hasArg("tz") ? webServer.arg("tz").toInt() : tz_minutes;

  preferences.begin("audio_cfg", false);
  preferences.putString("ssid", newSsid);
  preferences.putString("pass", webServer.arg("pass"));
  preferences.putInt("tz_min", newTz);
  preferences.end();
  sendMessagePage("Saved!", "The device is restarting and will join " + htmlEscape(newSsid) + ".");
  delay(2000);
  ESP.restart();
}

void handleReset() {
  preferences.begin("audio_cfg", false);
  preferences.clear();
  preferences.end();
  sendMessagePage("Wi-Fi settings erased", "The device is restarting.");
  delay(2000);
  ESP.restart();
}

// Logo servido da flash, com cache para o painel (que recarrega a cada 3 s)
void handleLogo() {
  webServer.sendHeader("Cache-Control", "max-age=86400");
  webServer.send_P(200, "image/png", (const char *)logo_png, logo_png_len);
}

#endif // HTML_PAGES_H
