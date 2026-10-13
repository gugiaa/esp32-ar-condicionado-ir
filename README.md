# Controle de Ar-Condicionado com ESP32 e IR

IFPE - Campus Garanhuns | Sistemas Embarcados (prof. Lauro Alves) | Etapa R6
Equipe: Gustavo de Lima Souza, Raul Lopes e Jefferson Tenório

A ideia é ligar e desligar um ar-condicionado comum usando um ESP32, sem mexer no
aparelho. O ESP32 lê a temperatura com um DHT22 e manda o comando por
infravermelho, como se fosse o controle remoto (protocolo Samsung). Também tem
um receptor IR que mostra os códigos recebidos de controles comuns.

## Ligação

| Componente | ESP32 | Obs |
| :--- | :--- | :--- |
| DHT22 (dados) | GPIO 4 | sensor de temperatura e umidade |
| DHT22 (VCC / GND) | 3V3 / GND | alimentação |
| LED IR (via resistor 220Ω e transistor) | GPIO 15 | emite o comando |
| LED IR (catodo) | GND | - |
| Receptor IR TSOP38238 (DAT / VCC / GND) | GPIO 14 / 3V3 / GND | lê controles remotos |
| LED azul "AC ON" (via resistor 220Ω) | GPIO 2 | acende junto com o ar |
| Relé (IN / VCC / GND) | GPIO 5 / 3V3 / GND | simula o compressor |

O LED azul e o relé acompanham o mesmo estado do IR. Como o sinal
infravermelho não dá pra ver, eles servem pra mostrar no vídeo que o sistema
reagiu.

## Itens da avaliação (R5, mantidos na R6)

- A. Pinos definidos no começo do `sketch.ino` (GPIO 4, 15, 14, 2 e 5).
- B. `setup()` inicializa o serial, o DHT22 e o receptor IR, além das saídas do LED e do relé.
- C. Struct `AcState` (ligado/desligado, automático, setpoint), temperatura/umidade lidas e tempos de controle.
- D. Leitura com `dht.readTemperature()` / `dht.readHumidity()` e `irReceiver.decode()` com `resume()`.
- E. Validação da leitura, intervalo sem travar o loop com `millis()`, liga/desliga com histerese de 1°C, proteção do compressor e comandos do serial.
- F. Envio do comando com `ac.sendAc()` em `sendAcCommand()`, chamado por `setPower()`.

Na R6 o `setPower()` também atualiza o relé e o LED, e o intervalo de proteção
cai de 30 s pra 8 s (`DEMO_MODE`) pra dar tempo de gravar os dois testes.
Na montagem final é só colocar `DEMO_MODE = false`.

## Como rodar no Wokwi

Projeto: https://wokwi.com/projects/476813449559690241

1. Abre o link e aperta o Play.
2. Abre o Serial Monitor (115200).
3. Comandos:
   - `on` : liga o ar pelo IR.
   - `off` : desliga o ar pelo IR.
   - `t 22` : escolhe a temperatura (de 16 a 30).
   - `auto` : liga/desliga o modo automático.
   - `status` : mostra temperatura, estado do ar e do modo auto.
4. Pra mudar a temperatura, clica no DHT22 e arrasta o controle de temperatura.
5. Pra testar o receptor IR, clica nos botões do controle remoto virtual.

## Arquivos

```text
├── sketch.ino     # código do ESP32
├── diagram.json   # montagem no Wokwi
├── libraries.txt  # DHT sensor library, Adafruit Unified Sensor, IRremoteESP8266
└── README.md
```
