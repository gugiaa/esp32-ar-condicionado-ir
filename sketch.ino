#include <DHT.h>
#include <IRac.h>
#include <IRrecv.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <IRutils.h>

/* ==============================================================================
 * SISTEMAS EMBARCADOS - IFPE CAMPUS GARANHUNS
 * ETAPA R5: PROGRAMAÇÃO E LEITURA DOS SENSORES
 * PROJETO: Controle Remoto Inteligente de Ar-Condicionados via ESP32 e IR
 * EQUIPE: Gustavo de Lima, Raul Lopes e Jefferson Tenório
 * ==============================================================================
 */

// ==============================================================================
// [ITEM A] CONFIGURAÇÃO DOS PINOS
// ==============================================================================
// Definição dos pinos GPIO utilizados no microcontrolador ESP32 DevKit V4.
constexpr uint8_t DHT_PIN    = 4;   // GPIO4  -> Pino de dados (SDA) do sensor DHT22
constexpr uint8_t IR_TX_PIN  = 15;  // GPIO15 -> Sinal PWM de transmissão para o LED IR (via transistor NPN 2N2222)
constexpr uint8_t IR_RX_PIN  = 14;  // GPIO14 -> Sinal de dados do receptor infravermelho TSOP38238

// ==============================================================================
// [ITEM C] VARIÁVEIS UTILIZADAS (ESTRUTURAS, ESTADOS E CONSTANTES)
// ==============================================================================
// Limites operacionais de temperatura (°C) permitidos para ar-condicionado
constexpr uint8_t MIN_TEMP = 16;
constexpr uint8_t MAX_TEMP = 30;

// Temporizações não-bloqueantes baseadas em millis() (em milissegundos)
constexpr unsigned long SENSOR_INTERVAL = 2000;   // Intervalo de amostragem do DHT22 (2 segundos)
constexpr unsigned long AUTO_INTERVAL   = 30000;  // Intervalo mínimo entre comutações automáticas (30 segundos)

// Margem de histerese para evitar oscilações contínuas de liga/desliga no modo automático
constexpr float HYSTERESIS = 1.0f;

// Estrutura que encapsula o estado atual do sistema de climatização
struct AcState {
  bool power = false;       // true = Ligado, false = Desligado
  bool automatic = false;   // true = Modo automático ativo baseado na temperatura
  uint8_t setpoint = 24;    // Temperatura alvo desejada em graus Celsius
};

AcState state;              // Instância do estado global

// Variáveis para armazenamento das leituras ambientais
float temperature = NAN;    // Armazena a última temperatura lida (°C)
float humidity = NAN;       // Armazena a última umidade relativa lida (%)

// Marcadores de tempo para controle não-bloqueante
unsigned long lastSensorRead = 0;
unsigned long lastAutoAction = 0;

// Instâncias dos objetos de hardware
DHT dht(DHT_PIN, DHT22);           // Sensor de Temperatura e Umidade DHT22
IRac ac(IR_TX_PIN);                // Controlador universal de Ar-Condicionado IR
IRrecv irReceiver(IR_RX_PIN);      // Receptor e decodificador de pulsos IR
decode_results irResult;           // Buffer de resultados da decodificação IR

// ==============================================================================
// [ITEM F] ACIONAMENTO DE ATUADORES (TRANSMISSÃO IR)
// ==============================================================================
// Função responsável por codificar e disparar os pacotes infravermelhos simulando
// o controle remoto físico do ar-condicionado.
void sendAcCommand() {
  ac.next.protocol = decode_type_t::SAMSUNG_AC;
  ac.next.model = 1;
  ac.next.power = state.power;
  ac.next.mode = stdAc::opmode_t::kCool;
  ac.next.celsius = true;
  ac.next.degrees = state.setpoint;
  ac.next.fanspeed = stdAc::fanspeed_t::kAuto;
  ac.next.swingv = stdAc::swingv_t::kOff;

  // Disparo físico do trem de pulsos modulados em 38kHz no pino IR_TX_PIN (GPIO15)
  ac.sendAc();

  Serial.printf(
    "[IR ATUADOR] Comando enviado -> AC: %s | Temp Alvo: %u°C\n",
    state.power ? "LIGADO" : "DESLIGADO",
    state.setpoint
  );
}

// Altera o estado de potência e dispara o atuador IR se houver mudança de estado
void setPower(bool enabled) {
  if (state.power == enabled) return; // Não reenvia se já estiver no estado solicitado

  state.power = enabled;
  sendAcCommand();
}

// Altera o setpoint respeitando os limites operacionais e dispara o atuador IR
void setSetpoint(int value) {
  state.setpoint = constrain(value, MIN_TEMP, MAX_TEMP);
  Serial.printf("[CONFIG] Setpoint atualizado para: %u°C\n", state.setpoint);

  if (state.power) {
    sendAcCommand();
  }
}

// ==============================================================================
// [ITEM D] COMANDOS RESPONSÁVEIS PELA LEITURA DOS SENSORES
// ==============================================================================
// Leitura periódica do sensor DHT22 com intervalo não-bloqueante
void updateSensor() {
  // [ITEM E] Condicional de controle temporal (não-bloqueante)
  if (millis() - lastSensorRead < SENSOR_INTERVAL) return;

  lastSensorRead = millis();

  // Execução da leitura digital dos canais de temperatura e umidade
  const float newTemperature = dht.readTemperature();
  const float newHumidity    = dht.readHumidity();

  // [ITEM E] Condicional de validação de integridade dos dados (detecção de falha física)
  if (isnan(newTemperature) || isnan(newHumidity)) {
    Serial.println("[DHT AVISO] Falha ao ler os dados do sensor DHT22!");
    return;
  }

  // Atualização dos dados validados
  temperature = newTemperature;
  humidity    = newHumidity;

  Serial.printf(
    "[SENSOR DHT22] Temperatura: %.1f°C | Umidade: %.0f%%\n",
    temperature,
    humidity
  );
}

// Leitura assíncrona do receptor infravermelho
void readIrCommand() {
  // Executa a leitura caso haja pacote IR recebido no buffer
  if (!irReceiver.decode(&irResult)) return;

  Serial.println("[SENSOR IR-RX] Código recebido do controle:");
  Serial.println(resultToHumanReadableBasic(&irResult));

  // Libera o receptor para a próxima leitura
  irReceiver.resume();
}

// ==============================================================================
// [ITEM E] ESTRUTURAS CONDICIONAIS UTILIZADAS (LÓGICA DE CONTROLE E AUTOMAÇÃO)
// ==============================================================================
// Lógica de decisão termostática automática com histerese para controle do ar-condicionado
void updateAutomaticControl() {
  // Condicional de guarda: ignora se o modo automático estiver desligado ou leitura for inválida
  if (!state.automatic || isnan(temperature)) return;

  // Proteção contra ciclos curtos de acionamento do compressor (mínimo AUTO_INTERVAL)
  if (millis() - lastAutoAction < AUTO_INTERVAL && lastAutoAction != 0) {
    return;
  }

  // Condicional de resfriamento: Temperatura ambiente superior ao setpoint + histerese
  if (!state.power && temperature > (state.setpoint + HYSTERESIS)) {
    lastAutoAction = millis();
    Serial.printf("[AUTO DECISÃO] Temp (%.1f°C) > Setpoint+%0.1f°C -> Ligando ar-condicionado.\n",
                  temperature, HYSTERESIS);
    setPower(true);
  }
  // Condicional de desligamento: Temperatura ambiente inferior ao setpoint - histerese
  else if (state.power && temperature < (state.setpoint - HYSTERESIS)) {
    lastAutoAction = millis();
    Serial.printf("[AUTO DECISÃO] Temp (%.1f°C) < Setpoint-%0.1f°C -> Desligando ar-condicionado.\n",
                  temperature, HYSTERESIS);
    setPower(false);
  }
}

// Processamento de comandos recebidos via monitor Serial
void handleCommand() {
  if (!Serial.available()) return;

  String command = Serial.readStringUntil('\n');
  command.trim();

  // [ITEM E] Condicionais de interpretação dos comandos de entrada
  if (command == "on") {
    setPower(true);
  }
  else if (command == "off") {
    setPower(false);
  }
  else if (command == "auto") {
    state.automatic = !state.automatic;
    Serial.printf("[AUTO] Modo Automático: %s\n", state.automatic ? "ATIVADO" : "DESATIVADO");
  }
  else if (command == "status") {
    Serial.printf(
      "[STATUS ATUAL] Temp: %.1f°C | Umid: %.0f%% | AC: %s | Alvo: %u°C | Modo Auto: %s\n",
      temperature, humidity,
      state.power ? "ON" : "OFF",
      state.setpoint,
      state.automatic ? "ON" : "OFF"
    );
  }
  else if (command.startsWith("t ")) {
    setSetpoint(command.substring(2).toInt());
  }
  else {
    Serial.println("[AJUDA] Comandos disponíveis: on | off | t <16-30> | auto | status");
  }
}

// ==============================================================================
// [ITEM B] INICIALIZAÇÃO DOS SENSORES E PERIFÉRICOS
// ==============================================================================
void setup() {
  Serial.begin(115200);

  // Inicialização do barramento do sensor DHT22
  dht.begin();

  // Inicialização e habilitação da interrupção do receptor infravermelho
  irReceiver.enableIRIn();

  Serial.println();
  Serial.println("==================================================");
  Serial.println(" IFPE - Controle de Ar-Condicionado Inteligente  ");
  Serial.println(" Entrega R5: Programação e Leitura de Sensores   ");
  Serial.println("==================================================");
  Serial.println("Comandos: 'on', 'off', 't 22', 'auto', 'status'");
  Serial.println("==================================================");
  Serial.println();
}

// ==============================================================================
// LOOP PRINCIPAL DO MICROCONTROLADOR
// ==============================================================================
void loop() {
  handleCommand();          // Interpreta comandos seriais recebidos
  updateSensor();           // Realiza a leitura periódica dos sensores (Item D)
  updateAutomaticControl(); // Avalia regras condicionais de controle (Item E)
  readIrCommand();          // Processa sinais infravermelhos recebidos (Item D)
}
