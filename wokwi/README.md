# Controle remoto Wokwi

O código foi organizado a partir do controle de teste compartilhado em [Missopad](https://missopad.com/ULISSESFUNCIONANDO/CAMADASERVI%C3%87O).

Este projeto simula o controle remoto. O ESP32 do Wokwi envia os comandos `LIGAR`, `DESLIGAR`, `FRENTE`, `TRAS`, `ESQUERDA`, `DIREITA` e `PARADO` ao Firebase Realtime Database. O firmware físico consulta o mesmo nó `/comando.json`.

## Arquivos

- `sketch.ino`: firmware do controle.
- `diagram.json`: ESP32, joystick, botão e LEDs de estado.

## Importar no Wokwi

Crie um projeto ESP32 no Wokwi e substitua o sketch e o diagrama pelos arquivos desta pasta. O projeto usa a rede simulada `Wokwi-GUEST` e o endereço do Firebase configurado no código. Inicie a simulação e use o joystick; o botão alterna entre ligado e desligado. Confira no Monitor Serial o comando e o código HTTP.

O circuito usa joystick em GPIO 34/35, botão em GPIO 2, LED verde em GPIO 4 e LED vermelho em GPIO 5.

## Fluxo de dados

Wokwi envia comandos ao Firebase; o ESP32 físico lê os comandos e envia as leituras dos sensores ao backend Flask local, que salva no SQLite. O Wokwi não envia as leituras do circuito físico ao endereço privado do computador.
