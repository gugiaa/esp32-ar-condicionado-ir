/* ==============================================================================
 * DISCIPLINA: SISTEMAS EMBARCADOS / AUTOMAÇÃO E CONECTIVIDADE - IFPE GARANHUNS
 * PROFESSOR: LAURO ALVES
 * ETAPA: R5 – PROGRAMAÇÃO E LEITURA DOS SENSORES
 * PROJETO: Controle Remoto Inteligente de Ar-Condicionados via ESP32 e IR
 * EQUIPE: Gustavo de Lima Souza, Raul Lopes e Jefferson Tenório
 * SIMULADOR WOKWI: https://wokwi.com/projects/476813449559690241
 * ==============================================================================
 * 
 * ÍNDICE DOS REQUISITOS OBRIGATÓRIOS AVALIADOS PELO PROFESSOR:
 * [ITEM A] Configuração dos pinos                -> Linhas 24 a 31
 * [ITEM B] Inicialização dos sensores            -> Linhas 221 a 241 (setup)
 * [ITEM C] Variáveis utilizadas                  -> Linhas 36 a 75
 * [ITEM D] Comandos responsáveis pela leitura    -> Linhas 116 a 157
 * [ITEM E] Estruturas condicionais utilizadas    -> Linhas 121, 128, 163-185, 194-214
 * [ITEM F] Acionamento de atuadores              -> Linhas 81 a 111
 * ==============================================================================
 */

#include <DHT.h>
#include <IRac.h>
#include <IRrecv.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <IRutils.h>

// ==============================================================================
// [ITEM A] CONFIGURAÇÃO DOS PINOS
// ==============================================================================
// Definição dos pinos GPIO utilizados no ESP32 para cada componente de hardware:
constexpr uint8_t DHT_PIN    = 4;   // GPIO 4  -> Pino de dados digital (SDA) do Sensor DHT22
constexpr uint8_t IR_TX_PIN  = 15;  // GPIO 15 -> Pino de emissão IR PWM (ligado ao transistor 2N2222 e LED IR)
constexpr uint8_t IR_RX_PIN  = 14;  // GPIO 14 -> Pino de sinal de recepção do receptor IR TSOP38238


// ==============================================================================
// [ITEM C] VARIÁVEIS UTILIZADAS
// ==============================================================================
// Constantes de calibração operacional de temperatura (°C)
constexpr uint8_t MIN_TEMP = 16;     // Limite inferior permitido para o ar-condicionado
constexpr uint8_t MAX_TEMP = 30;     // Limite superior permitido para o ar-condicionado

// Constantes de temporização não-bloqueante (em milissegundos)
constexpr unsigned long SENSOR_INTERVAL = 2000;   // Intervalo de amostragem do DHT22 (2 segundos)
constexpr unsigned long AUTO_INTERVAL   = 30000;  // Intervalo de proteção do compressor (30 segundos)

// Margem de histerese térmica (°C) para prevenir oscilações frequentes de liga/desliga
constexpr float HYSTERESIS = 1.0f;

// Estrutura para controle de estado do ar-condicionado
struct AcState {
  bool power = false;       // true = Ligado, false = Desligado
  bool automatic = false;   // true = Modo automático ativo
  uint8_t setpoint = 24;    // Temperatura desejada (°C)
};

AcState state;              // Instância global do estado do sistema

// Variáveis para armazenamento das grandezas físicas lidas
float temperature = NAN;    // Armazena a última temperatura ambiente (°C)
float humidity = NAN;       // Armazena a última umidade relativa do ar (%)

// Variáveis para temporização não-bloqueante via millis()
unsigned long lastSensorRead = 0; // Armazena timestamp da última leitura do sensor
unsigned long lastAutoAction = 0; // Armazena timestamp do último chaveamento automático

// Instâncias dos periféricos de hardware
DHT dht(DHT_PIN, DHT22);           // Objeto do sensor de temperatura e umidade DHT22
IRac ac(IR_TX_PIN);                // Objeto para controle universal de ar-condicionado IR
IRrecv irReceiver(IR_RX_PIN);      // Objeto receptor de infravermelho
decode_results irResult;           // Buffer estruturado para armazenar dados do pacote IR decodificado


// ==============================================================================
// [ITEM F] ACIONAMENTO DE ATUADORES, QUANDO HOUVER
// ==============================================================================
// Monta o protocolo de controle remoto e dispara o sinal infravermelho para o ar-condicionado
void sendAcCommand() {
  ac.next.protocol = decode_type_t::SAMSUNG_AC; // Protocolo do ar-condicionado (Samsung)
  ac.next.model = 1;                            // Modelo do equipamento
  ac.next.power = state.power;                  // Estado de energia (ligado/desligado)
  ac.next.mode = stdAc::opmode_t::kCool;        // Modo de operação (Refrigeração)
  ac.next.celsius = true;                       // Escala em Celsius
  ac.next.degrees = state.setpoint;             // Temperatura desejada
  ac.next.fanspeed = stdAc::fanspeed_t::kAuto;  // Velocidade do ventilador automática
  ac.next.swingv = stdAc::swingv_t::kOff;       // Aletas de direcionamento desativadas

  // [ATUADOR FÍSICO]: Disparo dos pulsos infravermelhos modulados em 38kHz no pino IR_TX_PIN (GPIO 15)
  ac.sendAc();

  Serial.printf(
    "[ATUADOR IR] Comando transmitido -> AC: %s | Temp Alvo: %u°C\n",
    state.power ? "ON (Ligado)" : "OFF (Desligado)",
    state.setpoint
  );
}

// Altera o estado de ligado/desligado e aciona o atuador IR
void setPower(bool enabled) {
  if (state.power == enabled) return; // Evita reenvio desnecessário se não houve mudança

  state.power = enabled;
  sendAcCommand();                    // Aciona o atuador
}

// Altera a temperatura desejada e atualiza o equipamento caso esteja ligado
void setSetpoint(int value) {
  state.setpoint = constrain(value, MIN_TEMP, MAX_TEMP);

  Serial.printf("[CONFIG] Temperatura ajustada para: %u°C\n", state.setpoint);

  if (state.power) {
    sendAcCommand();                  // Aciona o atuador
  }
}


// ==============================================================================
// [ITEM D] COMANDOS RESPONSÁVEIS PELA LEITURA DOS SENSORES
// ==============================================================================
// Executa a leitura periódica do sensor DHT22 (temperatura e umidade)
void updateSensor() {
  // [ITEM E] Estrutura condicional para respeitar o intervalo de leitura sem congelar o processamento
  if (millis() - lastSensorRead < SENSOR_INTERVAL) return;

  lastSensorRead = millis();

  // [ITEM D]: Comandos de leitura do sensor DHT22
  const float newTemperature = dht.readTemperature();
  const float newHumidity = dht.readHumidity();

  // [ITEM E] Estrutura condicional de validação de leitura (detecção de falha ou desconexão)
  if (isnan(newTemperature) || isnan(newHumidity)) {
    Serial.println("[SENSOR DHT] Erro na leitura dos dados!");
    return;
  }

  // Atualização dos valores das variáveis
  temperature = newTemperature;
  humidity = newHumidity;

  Serial.printf(
    "[SENSOR DHT] Leitura realizada -> Temperatura: %.1f°C | Umidade: %.0f%%\n",
    temperature,
    humidity
  );
}

// Executa a leitura e decodificação de comandos recebidos pelo receptor infravermelho
void readIrCommand() {
  // [ITEM D]: Comando de leitura e verificação do buffer do receptor IR
  if (!irReceiver.decode(&irResult)) return;

  Serial.println("[SENSOR IR-RX] Sinal infravermelho detectado:");
  Serial.println(resultToHumanReadableBasic(&irResult));

  // Libera o receptor IR para processar o próximo comando
  irReceiver.resume();
}


// ==============================================================================
// [ITEM E] ESTRUTURAS CONDICIONAIS UTILIZADAS (AUTOMAÇÃO E PARSER SERIAL)
// ==============================================================================
// Lógica de automação termostática para ligar/desligar o ar-condicionado por temperatura
void updateAutomaticControl() {
  // Condicional de guarda: ignora se o modo automático estiver desligado ou leitura for inválida
  if (!state.automatic || isnan(temperature)) return;

  // Condicional de proteção: evita chaveamento antes do intervalo mínimo de segurança
  if (millis() - lastAutoAction < AUTO_INTERVAL && lastAutoAction != 0) {
    return;
  }

  // Condicional 1: Se o ar estiver DESLIGADO e a temperatura ambiente superar o setpoint + histerese -> LIGA
  if (!state.power && temperature > state.setpoint + HYSTERESIS) {
    lastAutoAction = millis();
    Serial.printf("[AUTO CONDICIONAL] Temp (%.1f°C) > Setpoint+Histerese -> Acionando ar-condicionado (ON)\n", temperature);
    setPower(true);
  }
  // Condicional 2: Se o ar estiver LIGADO e a temperatura ambiente baixar do setpoint - histerese -> DESLIGA
  else if (state.power && temperature < state.setpoint - HYSTERESIS) {
    lastAutoAction = millis();
    Serial.printf("[AUTO CONDICIONAL] Temp (%.1f°C) < Setpoint-Histerese -> Desligando ar-condicionado (OFF)\n", temperature);
    setPower(false);
  }
}

// Interpreta comandos recebidos via monitor Serial
void handleCommand() {
  if (!Serial.available()) return;

  String command = Serial.readStringUntil('\n');
  command.trim();

  // Condicionais de tratamento de comandos de entrada
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
    printStatus();
  }
  else if (command.startsWith("t ")) {
    setSetpoint(command.substring(2).toInt());
  }
  else {
    Serial.println("Comandos disponíveis: on | off | t <16-30> | auto | status");
  }
}

// Exibe o status consolidado do sistema no monitor serial
void printStatus() {
  Serial.printf(
    "[STATUS] Temp: %.1f°C | Umid: %.0f%% | AC: %s | Setpoint: %u°C | Modo Auto: %s\n",
    temperature,
    humidity,
    state.power ? "ON" : "OFF",
    state.setpoint,
    state.automatic ? "ON" : "OFF"
  );
}


// ==============================================================================
// [ITEM B] INICIALIZAÇÃO DOS SENSORES (E PERIFÉRICOS)
// ==============================================================================
void setup() {
  // Inicialização da interface Serial para depuração e comando
  Serial.begin(115200);

  // [ITEM B]: Inicialização do sensor de temperatura e umidade DHT22
  dht.begin();

  // [ITEM B]: Inicialização e ativação das interrupções do receptor infravermelho
  irReceiver.enableIRIn();

  Serial.println();
  Serial.println("=========================================================");
  Serial.println(" IFPE Garanhuns - Sistemas Embarcados (Entrega R5)      ");
  Serial.println(" Projeto: Controle Remoto de Ar-Condicionado via ESP32  ");
  Serial.println("=========================================================");
  Serial.println("Comandos seriais: on | off | t <16-30> | auto | status  ");
  Serial.println("=========================================================");
  Serial.println();
}


// ==============================================================================
// CICLO PRINCIPAL (LOOP)
// ==============================================================================
void loop() {
  handleCommand();          // Processa comandos seriais recebidos
  updateSensor();           // [ITEM D] Leitura periódica do sensor DHT22
  updateAutomaticControl(); // [ITEM E] Execução das regras condicionais automáticas
  readIrCommand();          // [ITEM D] Leitura do receptor de infravermelho
}
