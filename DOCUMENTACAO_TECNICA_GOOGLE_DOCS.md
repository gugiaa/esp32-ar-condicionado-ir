# DOCUMENTAÇÃO TÉCNICA DO PROJETO

**DISCIPLINA:** SISTEMAS EMBARCADOS / AUTOMAÇÃO E CONECTIVIDADE  
**INSTITUIÇÃO:** Instituto Federal de Pernambuco (IFPE) – Campus Garanhuns  
**PROFESSOR:** Lauro Alves  
**ETAPAS:** R7, R8 e R9 – Documentação, Correções e Demonstração Final  
**PROJETO:** Controle Remoto Inteligente de Ar-Condicionados via ESP32 e IR  
**EQUIPE:** Gustavo de Lima Souza, Raul Lopes e Jefferson Tenório  
**SIMULADOR VIRTUAL (WOKWI):** https://wokwi.com/projects/476813449559690241  
**REPOSITÓRIO GITHUB:** https://github.com/gugiaa/esp32-ar-condicionado-ir  

---

## 1. Descrição do Projeto

### 1.1 Objetivo Geral
Desenvolver um dispositivo embarcado autônomo baseado no microcontrolador ESP32 capaz de monitorar as condições termo-higrométricas do ambiente e acionar aparelhos de ar-condicionado comerciais através de comandos ópticos infravermelhos (IR), proporcionando automação climática inteligente sem a necessidade de intervenção estrutural ou violação da garantia dos condicionadores de ar originais.

### 1.2 Problema Solucionado
Ambientes corporativos, acadêmicos e residenciais frequentemente sofrem com desperdício energético decorrente de aparelhos de ar-condicionado deixados ligados desnecessariamente ou operando em temperaturas excessivamente baixas. Além disso, a substituição de aparelhos convencionais por modelos com Wi-Fi nativo ou integração predial demanda alto custo financeiro. 

A solução proposta atua como um intermediário universal não invasivo (retrofit): o sistema analisa a temperatura do ambiente em tempo real e, através de emulação óptica dos protocolos proprietários de controle remoto, gerencia o ciclo térmico do equipamento com histerese calculada, prolongando a vida útil do compressor e reduzindo o consumo de energia elétrica.

### 1.3 Funcionalidades Propostas
- **Monitoramento Contínuo:** Aquisição periódica de temperatura (°C) e umidade relativa (%) via sensor digital de alta precisão.
- **Transmissão Infravermelha Universal (IR):** Disparo de pulsos modulados a 38 kHz reproduzindo fielmente os frames de dados de controles remotos comerciais (ex.: protocolo Samsung AC).
- **Modo Termostático Automático com Histerese:** Controle dinâmico liga/desliga baseado em setpoint configurável e faixa de histerese de ±1,0 °C, prevenindo ciclos curtos e desgaste de componentes mecânicos.
- **Recepção e Clonagem de Sinais IR:** Captura de sinais de controles físicos pré-existentes para mapeamento e diagnóstico via receptor dedicado.
- **Interface de Controle Serial:** Terminal de supervisão e parametrização com comandos intuitivos (`on`, `off`, `t <temp>`, `auto`, `status`).

---

## 2. Componentes Utilizados e Especificações

| Componente | Modelo / Referência | Quantidade | Função Específica no Projeto |
| :--- | :--- | :---: | :--- |
| **Microcontrolador** | ESP32 DevKit V4 (WROOM-32) | 1 | Núcleo de processamento dual-core (240 MHz), leitura dos barramentos digitais e temporização das rotinas. |
| **Sensor Termo-higrométrico** | DHT22 (AM2302) | 1 | Transdutor digital capacitivo para leitura de temperatura (-40 a 80 °C, ±0,5 °C) e umidade (0 a 100% RH). |
| **Emissor Infravermelho** | LED Infravermelho 940 nm (5 mm) | 1 | Transmissor de feixe óptico na faixa invisível com modulação de portadora em 38 kHz. |
| **Driver de Potência** | Transistor Bipolar NPN 2N2222 | 1 | Chaveamento rápido de alta corrente para o LED IR, garantindo alcance de emissão sem sobrecarregar a GPIO do ESP32. |
| **Resistor de Base** | Resistor de Filme de Carbono 1kΩ | 1 | Polarização e limitação da corrente de base do transistor 2N2222. |
| **Resistor de Coletor** | Resistor de 220Ω (ou 100Ω) | 1 | Limitação da corrente de pulso sobre o LED infravermelho. |
| **Receptor Infravermelho** | TSOP38238 | 1 | Módulo receptor com demodulador interno de 38 kHz para decodificação e aprendizado de pacotes IR. |
| **Matriz de Contatos** | Protoboard 830 pontos | 1 | Montagem mecânica e barramento de distribuição de energia do protótipo físico. |
| **Cabeamento** | Jumpers Macho-Macho / Macho-Fêmea | 1 kit | Conexão ponto a ponto entre os terminais dos sensores, atuadores e placas. |
| **Cabo de Comunicação / Alimentação** | Cabo Micro-USB / USB | 1 | Alimentação do microcontrolador (5V DC) e comunicação serial com o computador. |

---

## 3. Funcionamento, Montagem e Programação

### 3.1 Arquitetura e Lógica do Sistema
O firmware foi inteiramente estruturado com o paradigma de programação assíncrona orientada a temporizadores (`millis()`), banindo atrasos bloqueantes (`delay()`). O loop principal executa de forma cooperativa quatro tarefas independentes:
1. `handleCommand()`: Processamento do buffer serial de entrada em tempo real.
2. `updateSensor()`: Amostragem a cada 2000 ms do barramento One-Wire do DHT22 com verificação de integridade (`isnan`).
3. `updateAutomaticControl()`: Avaliação das condições termostáticas se o modo automático estiver ativo.
4. `readIrCommand()`: Verificação de interrupção externa de pacotes capturados pelo receptor TSOP38238.

### 3.2 Tabela de Conexões e Pinagem (ESP32 DevKit V4)

| Componente | Pino do Componente | Pino do ESP32 | Nível Lógico / Alimentação |
| :--- | :--- | :--- | :--- |
| **Sensor DHT22** | VCC (Pino 1) | **3V3** | 3.3V DC regulado |
| **Sensor DHT22** | SDA / Data (Pino 2) | **GPIO 4** | Sinal digital bidirecional |
| **Sensor DHT22** | GND (Pino 4) | **GND** | 0V (Terra comum) |
| **LED Emissor IR** | Anodo | **5V / VBUS** | Via Resistor limitador e Coletor do 2N2222 |
| **Transistor 2N2222** | Base (via Resistor 1kΩ) | **GPIO 15** | PWM / Portadora modulada 38 kHz |
| **Transistor 2N2222** | Emissor | **GND** | 0V (Terra comum) |
| **Receptor TSOP38238** | OUT (Pino 1) | **GPIO 14** | Entrada digital de pulsos demodulados |
| **Receptor TSOP38238** | GND (Pino 2) | **GND** | 0V (Terra comum) |
| **Receptor TSOP38238** | VCC (Pino 3) | **3V3** | 3.3V DC regulado |

### 3.3 Estrutura do Código-Fonte (Requisitos A a F)
O código gravado no microcontrolador organiza-se nas seções avaliadas:
- **A. Configuração dos Pinos:** Constantes `constexpr uint8_t` definindo GPIO 4, GPIO 15 e GPIO 14.
- **B. Inicialização dos Sensores:** Chamadas a `dht.begin()` e `irReceiver.enableIRIn()` na função `setup()`.
- **C. Variáveis Utilizadas:** Estrutura `AcState` (contendo flags `power`, `automatic` e inteiro `setpoint`), variáveis de ponto flutuante `temperature` e `humidity`, e temporizadores de controle de tempo.
- **D. Comandos de Leitura:** `dht.readTemperature()`, `dht.readHumidity()` e `irReceiver.decode(&irResult)`.
- **E. Estruturas Condicionais:** Filtro de descarte `isnan()`, proteção contra oscilação rápida com intervalo mínimo de chaveamento (`AUTO_INTERVAL`) e controle termostático com histerese (`temperature > setpoint + HYSTERESIS` e `temperature < setpoint - HYSTERESIS`).
- **F. Acionamento de Atuadores:** Emissão óptica via biblioteca especializada `IRremoteESP8266` com a chamada `ac.sendAc()`.

---

## 4. Testes Realizados e Resultados Observados

### 4.1 Teste 1: Sensoriamento e Estabilidade de Amostragem
- **Procedimento:** Monitoramento de temperatura e umidade em bancada com variação térmica induzida (aproximação de fluxo de ar quente/frio).
- **Resultado:** O sensor respondeu de forma contínua com atualização estável a cada 2 segundos. Com a filtragem de `isnan()`, nenhum valor espúrio ou corrompido foi propagado para a lógica de decisão.

### 4.2 Teste 2: Controle Termostático com Histerese
- **Procedimento:** Parametrização do setpoint em 24 °C e histerese em 1,0 °C, variando a temperatura ambiente artificialmente de 22 °C a 27 °C.
- **Resultado:** 
  - Ao subir para 25,2 °C (> 24 + 1 °C), o sistema disparou o comando IR de ligar o ar-condicionado.
  - Pequenas flutuações entre 23,5 °C e 24,5 °C não provocaram chaveamento, confirmando a eficácia da histerese.
  - Ao atingir 22,8 °C (< 24 - 1 °C), o sistema acionou o comando IR de desligar o equipamento.

### 4.3 Teste 3: Alcance Óptico e Eficiência do Driver IR
- **Procedimento:** Teste de recepção do comando emitido pelo LED IR contra receptor TSOP38238 posicionado a distâncias crescentes (1m, 3m e 5m).
- **Resultado:** Com o circuito chaveado por transistor 2N2222, o sinal foi recebido e decodificado com 100% de integridade a 5 metros de distância em linha de visada direta.

---

## 5. Registros Visuais e Links de Demonstração

### 5.1 Diagrama em Blocos da Arquitetura
```text
[ Sensor DHT22 ] ----(GPIO 4)----+
                                 |
[ Receptor TSOP38238 ] -(GPIO 14)-> [ ESP32 DevKit V4 ] --(GPIO 15)--> [ Driver 2N2222 + LED IR ] ---> [ Ar-Condicionado ]
                                 |                                                  
[ Terminal Serial / PC ] <------>+
```

### 5.2 Recursos e Links Oficiais
- **Simulação Interativa Wokwi:**  
  https://wokwi.com/projects/476813449559690241  
  *(Permite testar interativamente a leitura dos sensores, manipulação de temperatura e disparo de pulsos no navegador)*
- **Repositório Público GitHub:**  
  https://github.com/gugiaa/esp32-ar-condicionado-ir  
  *(Código-fonte completo, instruções de compilação e esquema de montagem)*
