/* ==============================================================================
 * DISCIPLINA: SISTEMAS EMBARCADOS / AUTOMAÇÃO E CONECTIVIDADE - IFPE GARANHUNS
 * PROFESSOR: LAURO ALVES
 * ETAPA: R6 – INTEGRAÇÃO DE SENSORES E ATUADORES
 * PROJETO: Controle Remoto Inteligente de Ar-Condicionados via ESP32 e IR
 * EQUIPE: Gustavo de Lima Souza, Raul Lopes e Jefferson Tenório
 * SIMULADOR WOKWI: https://wokwi.com/projects/476813449559690241
 * ==============================================================================
 *
 * ÍNDICE DOS REQUISITOS OBRIGATÓRIOS AVALIADOS PELO PROFESSOR:
 * [ITEM A] Configuração dos pinos                -> Linhas 27 a 33
 * [ITEM B] Inicialização dos sensores            -> Linhas 202 a 218 (setup)
 * [ITEM C] Variáveis utilizadas                  -> Linhas 34 a 65
 * [ITEM D] Comandos responsáveis pela leitura    -> Linhas 108 a 139
 * [ITEM E] Estruturas condicionais utilizadas    -> Linhas 141 a 188
 * [ITEM F] Acionamento de atuadores              -> Linhas 67 a 106
 * ==============================================================================
 */

#include <DHT.h>
#include <IRac.h>
#include <IRrecv.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <IRutils.h>

// [ITEM A] Pinos usados
constexpr uint8_t DHT_PIN = 4;        // dados do DHT22
constexpr uint8_t IR_TX_PIN = 15;     // LED IR (com transistor 2N2222)
constexpr uint8_t IR_RX_PIN = 14;     // receptor IR TSOP38238
constexpr uint8_t STATUS_LED_PIN = 2; // LED que acende/apaga junto com o ar
constexpr uint8_t RELAY_PIN = 5;      // rele que liga/desliga junto com o ar

// [ITEM C] Config e variaveis
constexpr uint8_t MIN_TEMP = 16;
constexpr uint8_t MAX_TEMP = 30;

constexpr unsigned long SENSOR_INTERVAL =
    2000; // o DHT22 so aceita leitura a cada 2s
constexpr unsigned long AUTO_INTERVAL = 30000; // espera minima do compressor
constexpr unsigned long DEMO_INTERVAL =
    8000; // espera curta pra caber no tempo do video
constexpr bool DEMO_MODE =
    true; // true = 8s (video), false = 30s (montagem final)

// Escolhe o intervalo conforme o modo
inline unsigned long getAutoInterval() {
  return DEMO_MODE ? DEMO_INTERVAL : AUTO_INTERVAL;
}

constexpr float HYSTERESIS =
    1.0f; // margem pra nao ficar ligando e desligando toda hora

struct AcState {
  bool power = false;     // ar ligado ou desligado
  bool automatic = false; // modo automatico
  uint8_t setpoint = 24;  // temperatura escolhida
};

AcState state;

float temperature = NAN;
float humidity = NAN;

unsigned long lastSensorRead = 0;
unsigned long lastAutoAction = 0;

DHT dht(DHT_PIN, DHT22);
IRac ac(IR_TX_PIN);
IRrecv irReceiver(IR_RX_PIN);
decode_results irResult;

// [ITEM F] Monta o comando e manda o sinal IR pro ar-condicionado (protocolo
// Samsung)
void sendAcCommand() {
  ac.next.protocol = decode_type_t::SAMSUNG_AC;
  ac.next.model = 1;
  ac.next.power = state.power;
  ac.next.mode = stdAc::opmode_t::kCool;
  ac.next.celsius = true;
  ac.next.degrees = state.setpoint;
  ac.next.fanspeed = stdAc::fanspeed_t::kAuto;
  ac.next.swingv = stdAc::swingv_t::kOff;

  ac.sendAc();

  Serial.printf(
      "[ATUADOR IR] Comando transmitido -> AC: %s | Temp Alvo: %u°C\n",
      state.power ? "ON (Ligado)" : "OFF (Desligado)", state.setpoint);
}

// Troca o estado e atualiza o rele e o LED junto
void setPower(bool enabled) {
  if (state.power == enabled)
    return;

  state.power = enabled;
  digitalWrite(RELAY_PIN, enabled ? HIGH : LOW);
  digitalWrite(STATUS_LED_PIN, enabled ? HIGH : LOW);
  sendAcCommand();
}

// Troca a temperatura escolhida (sempre entre 16 e 30)
void setSetpoint(int value) {
  state.setpoint = constrain(value, MIN_TEMP, MAX_TEMP);

  Serial.printf("[CONFIG] Temperatura ajustada para: %u°C\n", state.setpoint);

  if (state.power) {
    sendAcCommand();
  }
}

// [ITEM D] Le temperatura e umidade de 2 em 2 segundos
void updateSensor() {
  if (millis() - lastSensorRead < SENSOR_INTERVAL)
    return;
  lastSensorRead = millis();

  const float newTemperature = dht.readTemperature();
  const float newHumidity = dht.readHumidity();

  if (isnan(newTemperature) || isnan(newHumidity)) {
    Serial.println("[SENSOR DHT] Erro na leitura dos dados!");
    return;
  }

  temperature = newTemperature;
  humidity = newHumidity;

  Serial.printf("[SENSOR DHT] Leitura realizada -> Temperatura: %.1f°C | "
                "Umidade: %.0f%%\n",
                temperature, humidity);
}

// Le o que chega no receptor IR (controle remoto)
void readIrCommand() {
  if (!irReceiver.decode(&irResult))
    return;

  Serial.println("[SENSOR IR-RX] Sinal infravermelho detectado:");
  Serial.println(resultToHumanReadableBasic(&irResult));

  irReceiver.resume();
}

// [ITEM E] No automatico: liga se esquentar, desliga se esfriar
void updateAutomaticControl() {
  if (!state.automatic || isnan(temperature))
    return;

  const unsigned long effectiveInterval = getAutoInterval();
  if (millis() - lastAutoAction < effectiveInterval && lastAutoAction != 0) {
    return;
  }

  if (!state.power && temperature > state.setpoint + HYSTERESIS) {
    lastAutoAction = millis();
    Serial.printf("[AUTO CONDICIONAL] Temp (%.1f°C) > Setpoint+Histerese -> "
                  "Acionando ar-condicionado (ON)\n",
                  temperature);
    setPower(true);
  } else if (state.power && temperature < state.setpoint - HYSTERESIS) {
    lastAutoAction = millis();
    Serial.printf("[AUTO CONDICIONAL] Temp (%.1f°C) < Setpoint-Histerese -> "
                  "Desligando ar-condicionado (OFF)\n",
                  temperature);
    setPower(false);
  }
}

// Comandos digitados no monitor serial
void handleCommand() {
  if (!Serial.available())
    return;

  String command = Serial.readStringUntil('\n');
  command.trim();

  if (command == "on") {
    setPower(true);
  } else if (command == "off") {
    setPower(false);
  } else if (command == "auto") {
    state.automatic = !state.automatic;
    Serial.printf("[AUTO] Modo Automático: %s\n",
                  state.automatic ? "ATIVADO" : "DESATIVADO");
  } else if (command == "status") {
    printStatus();
  } else if (command.startsWith("t ")) {
    setSetpoint(command.substring(2).toInt());
  } else {
    Serial.println(
        "Comandos disponíveis: on | off | t <16-30> | auto | status");
  }
}

// Mostra como o sistema esta no momento
void printStatus() {
  Serial.printf("[STATUS] Temp: %.1f°C | Umid: %.0f%% | AC: %s | Setpoint: "
                "%u°C | Modo Auto: %s\n",
                temperature, humidity, state.power ? "ON" : "OFF",
                state.setpoint, state.automatic ? "ON" : "OFF");
}

// [ITEM B] Inicializacao
void setup() {
  Serial.begin(115200);

  pinMode(STATUS_LED_PIN, OUTPUT);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(STATUS_LED_PIN, LOW);
  digitalWrite(RELAY_PIN, LOW);

  dht.begin();
  irReceiver.enableIRIn();

  Serial.println();
  Serial.println("Controle de ar-condicionado via ESP32 - IFPE Garanhuns");
  Serial.println("Comandos: on | off | t <16-30> | auto | status");
  Serial.println();
}

void loop() {
  handleCommand();
  updateSensor();
  updateAutomaticControl();
  readIrCommand();
}
