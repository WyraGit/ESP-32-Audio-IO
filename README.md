# 🎧 Hub de Áudio Multiprotocolo (Wyrá)

<a href="https://buymeacoffee.com/hapyjatech" target="_blank"><img src="https://cdn.buymeacoffee.com/buttons/v2/default-yellow.png" alt="Buy Me A Coffee" style="height: 60px !important;width: 217px !important;" ></a>

Este projeto transforma um **ESP32** num servidor de áudio bidirecional de alta fidelidade (44.1kHz/16-bit). Ele elimina as limitações dos sistemas analógicos tradicionais, permitindo rotear áudio entre mesas de som, computadores e dispositivos mobile de forma digital e profissional.

O sistema opera com uma **Máquina de Estados**, permitindo alternar fisicamente entre três modos de operação distintos.

---

## 🏗️ Modos de Operação
O dispositivo é controlado através de uma chave seletora deslizante de 3 posições:

1. **Modo Servidor TCP:** O ESP32 atua como o hospedeiro (Host) na rede Wi-Fi. Ele abre a porta TCP `7000` para receber conexões de aplicações clientes. Ideal para streaming distribuído em tempo real.
2. **Modo Bluetooth (A2DP):** O ESP32 assume o papel de uma caixa de som Bluetooth de alta fidelidade. O rádio Wi-Fi é desligado para garantir performance máxima do áudio A2DP.
3. **Modo Auxiliar (Pass-through):** Roteamento analógico-digital-analógico de baixa latência (ADC → DAC), ideal para monitorização local direta sem necessidade de rede.

---

## 🛠️ Hardware Necessário

* **Processador:** ESP32 (WROOM/WROVER) com display OLED integrado.
* **ADC (Entrada):** Módulo PCM1808 (24-bit) – Converte o áudio da mesa para digital.
* **DAC (Saída):** Módulo PCM5102A (I2S) – Converte o digital para som analógico.
* **Seletor:** Chave deslizante de 3 posições (SP3T).
* **Alimentação:** Fonte 5V/2A estabilizada.

### Pinout (Esquema de Ligação)

| Função | Pino (Componente) | Pino (ESP32) | Notas |
| :--- | :--- | :--- | :--- |
| **I2C OLED** | SDA / SCL | **21 / 22** | Display de monitorização |
| **ADC I2S** | BCK / LRCK / DIN | **14 / 12 / 13** | Entrada da Mesa |
| **DAC I2S** | BCK / LCK / DIN | **26 / 25 / 18** | Saída para Retorno |
| **Chave A** | - | **GPIO 4** | Modo Servidor (Aterrado) |
| **Chave B** | - | **GPIO 5** | Modo Auxiliar (Aterrado) |
| **Chave Comum**| - | **GND** | Pino central da chave |

---

## ⚙️ Configuração e Instalação

### 1. Preparação do Arduino IDE
Antes de compilar, você **deve** configurar a memória para acomodar o projeto:
* Vá em **Ferramentas > Esquema de Partição**.
* Selecione **"Huge APP (3MB No OTA/1MB SPIFFS)"**. Isso é obrigatório, pois o uso combinado de Wi-Fi, Servidor Web e Bluetooth excede o limite padrão de memória.

### 2. Bibliotecas Necessárias
Instale via *Gerenciador de Bibliotecas* (`Ctrl+Shift+I`):
* `Adafruit GFX Library`
* `Adafruit SSD1306`
* `ESP32-A2DP` (por Phil Schatzmann) - *Essencial para o modo Bluetooth*.

### 3. Configuração Inicial (Portal Cativo)
Ao ligar o dispositivo pela primeira vez:
1. O ESP32 criará uma rede Wi-Fi chamada `AudioServer_Setup`.
2. Conecte-se a ela e acesse `192.168.4.1` no seu navegador.
3. Insira as credenciais da sua rede Wi-Fi. O dispositivo irá reiniciar e conectar-se automaticamente.

---

## 🌐 Integração com Aplicações (Clientes)

Se você estiver a desenvolver o seu próprio App (PC, Linux, Android ou iOS) para interagir com este servidor:

* **Protocolo:** TCP Raw Sockets.
* **Porta:** `7000`.
* **Formato de Áudio:** PCM Cru (Raw), 44100Hz, 16-bit, Estéreo.
* **Fluxo:**
    * **Envio:** Para enviar áudio do microfone do PC/Mobile para a mesa de som, escreva bytes diretamente no socket aberto.
    * **Recebimento:** O ESP32 enviará bytes constantemente para o cliente na mesma conexão TCP.

> **Dica de Desenvolvimento:** Ao programar o cliente, implemente um **Jitter Buffer** (buffer de pelo menos 50ms) para compensar pequenas instabilidades do Wi-Fi e evitar cortes ou estalidos no áudio.

---

## 💡 Resolução de Problemas
* **Erro "Text section exceeds available space":** O seu *Partition Scheme* está configurado incorretamente. Mude para "Huge APP".
* **Sem som no modo Bluetooth:** Certifique-se de que a biblioteca `ESP32-A2DP` está instalada e que o seu telemóvel está pareado com o dispositivo chamado `Audio_Server_ESP32`.
* **OLED não liga:** Verifique as ligações dos pinos 21 e 22 e certifique-se de que a biblioteca `Adafruit_SSD1306` está configurada corretamente.
