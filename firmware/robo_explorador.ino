// ====================================================================
// ROBÔ EXPLORADOR ESPACIAL - ESP32 FÍSICO (ETAPA 02)
// Sensores: DHT22, LDR (Fotorresistor), HC-SR04 (Ultrassónico)
// Atuadores: LEDs (Verde/Vermelho), Motores DC
// Comunicação: HTTP GET (Firebase) + HTTP GET (CallmeBot/WhatsApp)
// ====================================================================

#include <WiFi.h>
#include <HTTPClient.h>
#include <time.h>
#include <UrlEncode.h>
#include "DHTesp.h"
#include "secrets.h"
// --------------------------------------------------------------------
// CONFIGURAÇÕES DE REDE, FIREBASE E CALLMEBOT
// --------------------------------------------------------------------
const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;
// URL do Firebase apontando exatamente para o nó do comando
String firebaseURL = "https://iot4-d7802-default-rtdb.firebaseio.com/comando.json";
const char* backendURL = BACKEND_URL;

// Dados do CallmeBot
String phoneNumber = CALLMEBOT_PHONE;
String apiKey = CALLMEBOT_API_KEY;

// --------------------------------------------------------------------
// MAPEAMENTO DOS PINOS (MANTIDOS EXATAMENTE OS SEUS PINOS ORIGINAIS)
// --------------------------------------------------------------------
const int PIN_LED_VERDE = 4;
const int PIN_LED_VERMELHO = 5;

const int PIN_DHT = 15;         // DHT22 (Temperatura e Umidade)
const int PIN_LDR = 35;         // Fotorresistor (Pino Analógico ADC1)

// Sensor Ultrassónico HC-SR04
const int PIN_TRIG = 12;
const int PIN_ECHO = 11;

// Motores DC
const int PIN_MOTOR_ESQ = 26;  
const int PIN_MOTOR_DIR = 27;  

// --------------------------------------------------------------------
// OBJETOS E VARIÁVEIS GLOBAIS
// --------------------------------------------------------------------
DHTesp dhtSensor;

unsigned long tempoAnteriorSensores = 0;
const long intervaloSensores = 2000; // Leitura dos sensores a cada 2s

unsigned long tempoAnteriorComando = 0;
const long intervaloComando = 300;   // Intervalo de leitura do comando (300ms)

bool alertaEnviado = false;
String comandoRemoto = "PARADO";
bool roboLigadoRemotamente = true;
float ultimaDistanciaMedida = 999.0; // Armazena a última distância lida

// Buffer em RAM para reenviar leituras quando a API estiver indisponivel.
const size_t MAX_LEITURAS_PENDENTES = 10;
String leiturasPendentes[MAX_LEITURAS_PENDENTES];
size_t quantidadeLeiturasPendentes = 0;

String timestampUTC() {
  time_t agora = time(nullptr);
  if (agora < 100000) return ""; // NTP ainda nao sincronizou; a API usara o horario do servidor.

  struct tm utc;
  gmtime_r(&agora, &utc);
  char texto[21];
  strftime(texto, sizeof(texto), "%Y-%m-%dT%H:%M:%SZ", &utc);
  return String(texto);
}

bool postarLeitura(const String& jsonPayload) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("API: Wi-Fi desconectado; leitura permanece na fila.");
    return false;
  }

  HTTPClient http;
  http.setTimeout(3000);
  if (!http.begin(backendURL)) {
    Serial.println("API: URL invalida. Confira BACKEND_URL em secrets.h.");
    return false;
  }
  http.addHeader("Content-Type", "application/json");
  int codigo = http.POST(jsonPayload);
  String resposta = http.getString();
  http.end();

  if (codigo == 201) {
    Serial.print("API: leitura armazenada (HTTP 201): ");
    Serial.println(resposta);
    return true;
  }

  Serial.print("API: envio nao confirmado. HTTP ");
  Serial.print(codigo);
  if (resposta.length() > 0) {
    Serial.print(" - ");
    Serial.print(resposta);
  }
  Serial.println();
  return false;
}

void reenviarLeiturasPendentes() {
  if (quantidadeLeiturasPendentes == 0) return;
  if (!postarLeitura(leiturasPendentes[0])) return;

  for (size_t i = 1; i < quantidadeLeiturasPendentes; i++) {
    leiturasPendentes[i - 1] = leiturasPendentes[i];
  }
  leiturasPendentes[--quantidadeLeiturasPendentes] = "";
}

void registrarLeitura(const String& jsonPayload) {
  if (quantidadeLeiturasPendentes > 0 || !postarLeitura(jsonPayload)) {
    if (quantidadeLeiturasPendentes == MAX_LEITURAS_PENDENTES) {
      Serial.println("API: fila cheia; leitura mais recente descartada.");
      return;
    }
    leiturasPendentes[quantidadeLeiturasPendentes++] = jsonPayload;
    Serial.print("API: leitura guardada na fila. Pendentes: ");
    Serial.println(quantidadeLeiturasPendentes);
  }
}

String criarPayloadLeitura(float temperatura, float umidade, int luminosidade,
                           int presenca, int probabilidade) {
  String payload = "{";
  String timestamp = timestampUTC();
  if (timestamp.length() > 0) {
    payload += "\"timestamp\":\"" + timestamp + "\",";
  }
  payload += "\"temperatura_c\":" + String(temperatura, 1);
  payload += ",\"umidade_pct\":" + String(umidade, 1);
  payload += ",\"luminosidade\":" + String(luminosidade);
  payload += ",\"presenca\":" + String(presenca);
  payload += ",\"probabilidade_vida\":" + String(probabilidade);
  payload += "}";
  return payload;
}

// --------------------------------------------------------------------
// FUNÇÃO PARA LER DISTÂNCIA DO SENSOR ULTRASSÓNICO (HC-SR04)
// --------------------------------------------------------------------
float lerDistanciaHCSR04() {
  digitalWrite(PIN_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(PIN_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);

  // Timeout em 30ms (aproximadamente 5 metros de alcance máximo)
  long duracao = pulseIn(PIN_ECHO, HIGH, 30000); 
  if (duracao == 0) return 999.0; // Nenhum obstáculo detectado no alcance

  float distanciaCm = (duracao * 0.0343) / 2.0;
  return distanciaCm;
}

// --------------------------------------------------------------------
// FUNÇÃO PARA ATUALIZAR O ESTADO DOS MOTORES COM TRAVA DE SEGURANÇA
// --------------------------------------------------------------------
void atualizarMotores() {
  // Se o robô foi desligado pelo controle remoto, desliga os motores imediatamente
  if (!roboLigadoRemotamente) {
    digitalWrite(PIN_MOTOR_ESQ, LOW);
    digitalWrite(PIN_MOTOR_DIR, LOW);
    return;
  }

  // Lógica de Movimentação do Robô
  if (comandoRemoto == "FRENTE") {
    // Trava de Segurança: Não permite andar para frente se houver obstáculo a menos de 15cm
    if (ultimaDistanciaMedida > 0.0 && ultimaDistanciaMedida < 15.0) {
      Serial.println("⚠️ ALERTA DE SEGURANÇA: Obstáculo próximo! Motores parados.");
      digitalWrite(PIN_MOTOR_ESQ, LOW);
      digitalWrite(PIN_MOTOR_DIR, LOW);
    } else {
      digitalWrite(PIN_MOTOR_ESQ, HIGH);
      digitalWrite(PIN_MOTOR_DIR, HIGH);
    }
  } 
  else if (comandoRemoto == "ESQUERDA") {
    digitalWrite(PIN_MOTOR_ESQ, LOW);
    digitalWrite(PIN_MOTOR_DIR, HIGH);
  } 
  else if (comandoRemoto == "DIREITA") {
    digitalWrite(PIN_MOTOR_ESQ, HIGH);
    digitalWrite(PIN_MOTOR_DIR, LOW);
  } 
  else { // "TRAS" ou "PARADO"
    digitalWrite(PIN_MOTOR_ESQ, LOW);
    digitalWrite(PIN_MOTOR_DIR, LOW);
  }
}

// --------------------------------------------------------------------
// FUNÇÃO PARA BUSCAR O ÚLTIMO COMANDO NA NUVEM (HTTP GET)
// --------------------------------------------------------------------
void lerComandoDaNuvem() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(firebaseURL);
    http.setTimeout(1000); // Timeout de 1 segundo para não travar a execução

    int httpCode = http.GET();

    if (httpCode == 200) {
      String payload = http.getString();
      payload.replace("\"", ""); // Remove aspas do formato JSON
      payload.trim();
      
      if (payload.length() > 0) {
        comandoRemoto = payload;
      }

      // Atualiza o estado de ativado/desativado do robô
      if (comandoRemoto == "DESLIGAR") {
        roboLigadoRemotamente = false;
      } else if (comandoRemoto == "LIGAR") {
        roboLigadoRemotamente = true;
      }

      // Atualiza o estado dos motores com base no comando lido
      atualizarMotores();
    }
    http.end();
  }
}

// --------------------------------------------------------------------
// FUNÇÃO PARA ENVIAR MENSAGEM VIA CALLMEBOT (HTTP GET)
// --------------------------------------------------------------------
void sendMessage(String message) {
  if (WiFi.status() == WL_CONNECTED) {
    // Montagem da URL conforme o padrão do CallmeBot
    String url = "https://api.callmebot.com/whatsapp.php?phone=" + phoneNumber + 
                 "&apikey=" + apiKey + 
                 "&text=" + urlEncode(message);

    HTTPClient http;
    http.begin(url);
    http.setTimeout(3000); // Timeout de 3 segundos para a API do WhatsApp

    int httpResponseCode = http.GET(); // Método HTTP GET para requisição com URL formatada

    if (httpResponseCode == 200) {
      Serial.println("-> Alerta enviado com sucesso via WhatsApp (CallmeBot)!");
    } else {
      Serial.print("-> Erro CallmeBot. Código HTTP: ");
      Serial.println(httpResponseCode);
    }

    http.end();
  }
}

// --------------------------------------------------------------------
// SETUP ATUALIZADO PARA ESP32-S3
// --------------------------------------------------------------------
void setup() {
  // Inicializa a comunicação Serial
  Serial.begin(115200);

  // Aguarda até 3 segundos para a porta USB Serial do ESP32-S3 estabilizar
  unsigned long tempoInicioSerial = millis();
  while (!Serial && (millis() - tempoInicioSerial < 3000)) {
    delay(10);
  }

  Serial.println("\n--- Inicializando Robô Explorador Físico (ESP32-S3) ---");

  // Configuração dos Pinos
  pinMode(PIN_LED_VERDE, OUTPUT);
  pinMode(PIN_LED_VERMELHO, OUTPUT);

  pinMode(PIN_MOTOR_ESQ, OUTPUT);
  pinMode(PIN_MOTOR_DIR, OUTPUT);

  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_ECHO, INPUT);

  // Inicializa o Sensor DHT22
  dhtSensor.setup(PIN_DHT, DHTesp::DHT22);

  // Conexão Wi-Fi
  WiFi.begin(ssid, password);
  Serial.print("Conectando ao Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConectado com sucesso!");
  Serial.print("IP do Robô: ");
  Serial.println(WiFi.localIP());
  Serial.print("API de leituras: ");
  Serial.println(backendURL);
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
}

// --------------------------------------------------------------------
// LOOP PRINCIPAL
// --------------------------------------------------------------------
void loop() {
  unsigned long tempoAtual = millis();

  // 1. Consulta o comando do controle remoto na nuvem a cada 300ms
  if (tempoAtual - tempoAnteriorComando >= intervaloComando) {
    tempoAnteriorComando = tempoAtual;
    lerComandoDaNuvem();
  }

  // 2. Executa rotina dos sensores e cálculo de vida a cada 2 segundos
  if (tempoAtual - tempoAnteriorSensores >= intervaloSensores) {
    tempoAnteriorSensores = tempoAtual;

    // Reenvia no maximo uma leitura pendente por ciclo de sensores.
    reenviarLeiturasPendentes();

    // Leitura do Sensor DHT22
    TempAndHumidity dadosDHT = dhtSensor.getTempAndHumidity();
    float temperatura = dadosDHT.temperature;
    float umidade = dadosDHT.humidity;

    // Proteção contra leitura com erro (NaN)
    if (isnan(temperatura)) temperatura = 0.0;
    if (isnan(umidade)) umidade = 0.0;

    // Leitura dos demais sensores
    int valorLuz = analogRead(PIN_LDR);
    ultimaDistanciaMedida = lerDistanciaHCSR04();
    
    // Considera presença detectada se houver objeto/obstáculo a menos de 20cm
    bool presencaDetectada = (ultimaDistanciaMedida > 0.0 && ultimaDistanciaMedida < 20.0);

    // Algoritmo de Cálculo da Probabilidade de Vida
    int probabilidadeVida = 0;

    if (temperatura >= 15.0 && temperatura <= 30.0) probabilidadeVida += 25;
    if (umidade >= 40.0 && umidade <= 70.0) probabilidadeVida += 25;
    if (valorLuz > 1500) probabilidadeVida += 20;
    if (presencaDetectada) probabilidadeVida += 30;

    // Impressão contínua no Monitor Serial
    Serial.println("\n==============================================");
    Serial.print("Temperatura: "); Serial.print(temperatura, 1); Serial.println(" °C");
    Serial.print("Umidade: "); Serial.print(umidade, 1); Serial.println(" %");
    Serial.print("Intensidade de Luz (LDR): "); Serial.println(valorLuz);
    Serial.print("Distância (HC-SR04): "); Serial.print(ultimaDistanciaMedida, 1); Serial.println(" cm");
    Serial.print("Sensor de Presença: "); Serial.println(presencaDetectada ? "Presença detectada" : "Sem presença");
    Serial.print("Comando Atual Recebido: "); Serial.println(comandoRemoto);
    Serial.print("Probabilidade de Vida: "); Serial.print(probabilidadeVida); Serial.println("%");

    String payloadLeitura = criarPayloadLeitura(
      temperatura, umidade, valorLuz, presencaDetectada ? 1 : 0, probabilidadeVida
    );
    registrarLeitura(payloadLeitura);

    // Lógica de Atuação e Sinalização
    if (!roboLigadoRemotamente) {
      digitalWrite(PIN_LED_VERDE, LOW);
      digitalWrite(PIN_LED_VERMELHO, HIGH);
      Serial.println("Estado do Robô: DESLIGADO REMOTAMENTE");
    } 
    else if (probabilidadeVida <= 75) {
      digitalWrite(PIN_LED_VERDE, HIGH);
      digitalWrite(PIN_LED_VERMELHO, LOW);
      Serial.println("Estado do Robô: EXPLORAÇÃO NORMAL");
      alertaEnviado = false; // Reseta a trava para novos alertas futuros
    } 
    else {
      // Estado de Alerta (Probabilidade > 75%)
      digitalWrite(PIN_LED_VERDE, LOW);
      digitalWrite(PIN_LED_VERMELHO, HIGH);
      Serial.println("ALERTA! Alta probabilidade de vida detectada!");

      // Envia a mensagem via WhatsApp apenas uma vez ao entrar no estado de alerta
      if (!alertaEnviado) {
        sendMessage("Alerta! Alta probabilidade de vida detectada no planeta.");
        alertaEnviado = true;
      }
    }
    Serial.println("==============================================");
  }
}
