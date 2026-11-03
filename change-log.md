# Histórico de mudanças

## 23452de (13/10) - etapa R6

Commit `feat: atividade R6.` (`sketch.ino`, `diagram.json`, `README.md`).

- `sketch.ino`: cabeçalho com `ETAPA: R6` e índice A-F certo. LED no GPIO 2 e relé no GPIO 5, que
  ligam/desligam junto com o ar em `setPower()`. `DEMO_MODE = true`
  deixa a espera do compressor em 8 s pra dar tempo de gravar o vídeo
  (na montagem final volta pra 30 s colocando `false`). Comentários
  encurtados; textos do Serial mantidos iguais pros testes do vídeo.
- `diagram.json`: LED azul "AC ON" (GPIO 2) e módulo relé (GPIO 5).
  O resto do circuito não mudou.
- `README.md`: texto simplificado e pinos novos na tabela.

## 6d90224 (08/10) - requisitos A até F no código

- `sketch.ino` reorganizado com as marcações `[ITEM A]` a `[ITEM F]`
  pedidas na avaliação, sem mudar o funcionamento.

## 2879163 (08/10) - entrega R5

- Primeira versão: `README.md`, `sketch.ino` (ESP32 + DHT22 + IR),
  `diagram.json` da montagem no Wokwi e `libraries.txt`.
