# Controle Remoto Inteligente de Ar-Condicionados via ESP32 e IR

> **Instituto Federal de Pernambuco (IFPE) - Campus Garanhuns**  
> **Disciplina:** Sistemas Embarcados / Automação e Conectividade  
> **Professor:** Lauro Alves  
> **Etapa:** Entrega R5 – Programação e Leitura dos Sensores  
> **Equipe:** Gustavo de Lima Souza, Raul Lopes e Jefferson Tenório  

[![Simulação no Wokwi](https://img.shields.io/badge/Wokwi-Simulação%20Online-blue?logo=espressif)](https://wokwi.com/projects/476813449559690241)
[![Microcontrolador](https://img.shields.io/badge/MCU-ESP32%20DevKit%20V4-red?logo=espressif)](https://espressif.com)
[![Linguagem](https://img.shields.io/badge/Linguagem-C%2B%2B%20%2F%20Arduino-brightgreen)](https://www.arduino.cc/)

---

## 1. Visão Geral do Projeto

Este projeto tem como objetivo automatizar e modernizar o controle de aparelhos de ar-condicionado convencionais sem intervenções invasivas no equipamento original. O sistema utiliza um microcontrolador **ESP32** para coletar dados climáticos do ambiente via sensor digital **DHT22** e emitir comandos infravermelhos (IR) compatíveis com os protocolos dos fabricantes através de um circuito emissor (LED infravermelho acionado por transistor NPN 2N2222). Além disso, dispõe de um receptor infravermelho (**TSOP38238**) para a leitura e decodificação prévia de comandos de controles remotos comerciais.

---

## 2. Pinagem e Ligações de Hardware

| Componente | Pino do Componente | Pino do ESP32 | Descrição da Função |
| :--- | :--- | :--- | :--- |
| **Sensor DHT22** | SDA / Data | **GPIO 4** | Leitura digital de temperatura e umidade |
| **Sensor DHT22** | VCC | **3V3** | Alimentação positiva (3.3V) |
| **Sensor DHT22** | GND | **GND** | Aterramento comum |
| **Emissor IR (LED)** | Anodo (via Resistor 220Ω / Transistor) | **GPIO 15** | Pulso modulado PWM a 38 kHz para acionamento IR |
| **Emissor IR (LED)** | Catodo | **GND** | Aterramento comum |
| **Receptor IR (TSOP38238)** | DAT / Out | **GPIO 14** | Captura e decodificação de comandos IR |
| **Receptor IR (TSOP38238)** | VCC | **3V3** | Alimentação positiva |
| **Receptor IR (TSOP38238)** | GND | **GND** | Aterramento comum |

---

## 3. Requisitos Obrigatórios da Etapa R5 (Mapeamento A a F)

O código principal (`sketch.ino`) foi estruturado e comentado para atender explicitamente a todos os tópicos de avaliação exigidos pela disciplina:

### A. Configuração dos Pinos
Os pinos de I/O do ESP32 são definidos como constantes em tempo de compilação:
```cpp
constexpr uint8_t DHT_PIN    = 4;   // GPIO4  -> Pino de dados (SDA) do sensor DHT22
constexpr uint8_t IR_TX_PIN  = 15;  // GPIO15 -> Sinal PWM de transmissão para o LED IR
constexpr uint8_t IR_RX_PIN  = 14;  // GPIO14 -> Entrada de dados do receptor TSOP38238
```

### B. Inicialização dos Sensores
Executada dentro da rotina `setup()`, preparando os barramentos de comunicação e interrupções:
```cpp
void setup() {
  Serial.begin(115200);
  dht.begin();                // Inicializa o barramento do sensor DHT22
  irReceiver.enableIRIn();    // Inicializa a recepção do sinal infravermelho
}
```

### C. Variáveis Utilizadas
Uso de estruturas tipadas para retenção de estado e variáveis de controle temporal:
```cpp
// Estrutura de estado operacional
struct AcState {
  bool power = false;       // Estado de ligado/desligado
  bool automatic = false;   // Estado do modo de automação térmica
  uint8_t setpoint = 24;    // Temperatura desejada (°C)
};

AcState state;
float temperature = NAN;    // Temperatura ambiente lida (°C)
float humidity = NAN;       // Umidade relativa lida (%)

// Variáveis de controle de tempo não-bloqueante
unsigned long lastSensorRead = 0;
unsigned long lastAutoAction = 0;
```

### D. Comandos Responsáveis pela Leitura
As leituras são executadas de maneira periódica e não-bloqueante utilizando `millis()`:
- **Sensor DHT22:** `dht.readTemperature()` e `dht.readHumidity()`.
- **Receptor IR:** `irReceiver.decode(&irResult)` e posterior liberação de buffer com `irReceiver.resume()`.

### E. Estruturas Condicionais Utilizadas
O firmware implementa diversas condições essenciais para a segurança e controle do sistema:
1. **Validação de Integridade:** `if (isnan(newTemperature) || isnan(newHumidity))` protege contra falhas físicas na leitura do sensor.
2. **Temporização Não-bloqueante:** Garante que o loop principal continue responsivo enquanto respeita a taxa de amostragem de 2 segundos do DHT22.
3. **Controle Termostático com Histerese:**
   - Liga o resfriamento se: `temperature > (state.setpoint + HYSTERESIS)`.
   - Desliga o resfriamento se: `temperature < (state.setpoint - HYSTERESIS)`.
   - Inclui proteção contra chaveamento rápido do compressor (`AUTO_INTERVAL = 30000 ms`).
4. **Parser Serial:** Avalia comandos de controle recebidos pelo terminal (`on`, `off`, `t <temp>`, `auto`, `status`).

### F. Acionamento de Atuadores
O comando para o ar-condicionado é formatado e enviado através da biblioteca `IRremoteESP8266`:
```cpp
void sendAcCommand() {
  ac.next.protocol = decode_type_t::SAMSUNG_AC;
  ac.next.model = 1;
  ac.next.power = state.power;
  ac.next.mode = stdAc::opmode_t::kCool;
  ac.next.celsius = true;
  ac.next.degrees = state.setpoint;
  ac.next.fanspeed = stdAc::fanspeed_t::kAuto;
  ac.next.swingv = stdAc::swingv_t::kOff;

  ac.sendAc(); // Disparo dos pulsos infravermelhos modulados no pino IR_TX_PIN (GPIO 15)
}
```

---

## 4. Como Executar a Simulação

1. Acesse o projeto no Wokwi:  
   👉 [**https://wokwi.com/projects/476813449559690241**](https://wokwi.com/projects/476813449559690241)
2. Clique no botão de **Play** (Iniciar Simulação).
3. Abra o **Serial Monitor** para visualizar os logs de leitura de temperatura/umidade e interagir via comandos:
   - `on` : Liga o ar-condicionado via sinal IR.
   - `off` : Desliga o ar-condicionado via sinal IR.
   - `t 22` : Define a temperatura desejada para 22°C.
   - `auto` : Ativa/desativa o modo de controle termostático automático.
   - `status` : Exibe o estado operacional completo do sistema.
4. Para testar variações de temperatura no simulador:
   - Clique sobre o componente **DHT22** na tela e ajuste os seletores de temperatura e umidade.
5. Para testar o receptor infravermelho:
   - Clique no controle remoto virtual para emitir códigos para o sensor TSOP38238.

---

## 5. Estrutura do Repositório

```text
├── sketch.ino        # Código-fonte principal com documentação dos itens A a F
├── diagram.json      # Esquema elétrico e montagem gráfica da simulação Wokwi
├── libraries.txt     # Dependências de bibliotecas externas utilizadas
└── README.md         # Documentação técnica do sistema
```
