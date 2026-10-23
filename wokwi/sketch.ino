// Controle remoto do robo explorador (Wokwi -> Firebase -> ESP32 fisico).
#include <WiFi.h>
#include <HTTPClient.h>

const char* WIFI_SSID = "Wokwi-GUEST";
const char* WIFI_PASSWORD = "";
const char* FIREBASE_URL = "https://iot4-d7802-default-rtdb.firebaseio.com/comando.json";

const int PIN_LED_VERMELHO = 5;
const int PIN_LED_VERDE = 4;
const int PIN_BOTAO = 2;
const int PIN_JOYSTICK_X = 34;
const int PIN_JOYSTICK_Y = 35;

const unsigned long DEBOUNCE_MS = 250;
const unsigned long RETRY_MS = 1000;

bool sistemaLigado = true;
int estadoAnteriorBotao = HIGH;
unsigned long ultimaMudancaBotao = 0;
unsigned long ultimaTentativaEnvio = 0;
String ultimoComandoEnviado = "";
bool comandoLigarPendente = true;

bool enviarComando(const String& comando) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Firebase: Wi-Fi desconectado.");
    return false;
  }

  HTTPClient http;
  http.setTimeout(3000);
  if (!http.begin(FIREBASE_URL)) {
    Serial.println("Firebase: URL invalida.");
    return false;
  }

  http.addHeader("Content-Type", "application/json");
  int codigo = http.PUT("\"" + comando + "\"");
  String resposta = http.getString();
  http.end();

  if (codigo == 200) {
    Serial.print("Comando enviado: ");
    Serial.println(comando);
    return true;
  }

  Serial.print("Firebase: falha HTTP ");
  Serial.print(codigo);
  if (resposta.length()) {
    Serial.print(" - ");
    Serial.print(resposta);
  }
  Serial.println();
  return false;
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_LED_VERMELHO, OUTPUT);
  pinMode(PIN_LED_VERDE, OUTPUT);
  pinMode(PIN_BOTAO, INPUT_PULLUP);
  pinMode(PIN_JOYSTICK_X, INPUT);
  pinMode(PIN_JOYSTICK_Y, INPUT);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Conectando ao Wi-Fi do Wokwi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
    Serial.print(".");
  }
  Serial.println(" conectado.");
}

void loop() {
  unsigned long agora = millis();
  int estadoBotao = digitalRead(PIN_BOTAO);

  if (estadoBotao != estadoAnteriorBotao) {
    ultimaMudancaBotao = agora;
    estadoAnteriorBotao = estadoBotao;
  }

  static int estadoEstavelBotao = HIGH;
  if (agora - ultimaMudancaBotao >= DEBOUNCE_MS && estadoBotao != estadoEstavelBotao) {
    estadoEstavelBotao = estadoBotao;
    if (estadoEstavelBotao == LOW) {
      sistemaLigado = !sistemaLigado;
      comandoLigarPendente = sistemaLigado;
    }
  }

  digitalWrite(PIN_LED_VERDE, sistemaLigado ? HIGH : LOW);
  digitalWrite(PIN_LED_VERMELHO, sistemaLigado ? LOW : HIGH);

  String comandoDesejado;
  if (comandoLigarPendente) {
    comandoDesejado = "LIGAR";
  } else if (!sistemaLigado) {
    comandoDesejado = "DESLIGAR";
  } else {
    int x = analogRead(PIN_JOYSTICK_X);
    int y = analogRead(PIN_JOYSTICK_Y);
    comandoDesejado = "PARADO";

    if (y > 3000) comandoDesejado = "FRENTE";
    else if (y < 1000) comandoDesejado = "TRAS";
    else if (x > 3000) comandoDesejado = "ESQUERDA";
    else if (x < 1000) comandoDesejado = "DIREITA";
    else if (ultimoComandoEnviado == "DESLIGAR") comandoDesejado = "LIGAR";
  }

  if (comandoDesejado != ultimoComandoEnviado && agora - ultimaTentativaEnvio >= RETRY_MS) {
    ultimaTentativaEnvio = agora;
    if (enviarComando(comandoDesejado)) {
      ultimoComandoEnviado = comandoDesejado;
      if (comandoDesejado == "LIGAR") comandoLigarPendente = false;
    }
  }

  delay(20);
}
