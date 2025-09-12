#include <WiFi.h>            // Biblioteca para conexão Wi-Fi
#include <PubSubClient.h>    // Biblioteca para MQTT
#include "DHTesp.h"          // Biblioteca para sensor DHT22

// ------------------- CONFIGURAÇÕES -------------------
const int DHT_PIN = 9;       // Pino de dados do sensor DHT22
const int PIR_PIN = 3;       // Pino de dados do sensor PIR (movimento)
DHTesp dhtSensor;            // Objeto para leitura do DHT

// Dados da rede Wi-Fi (no Wokwi não precisa senha)
const char* ssid = "Wokwi-GUEST";
const char* password = "";

// Broker MQTT público (em produção, use broker com usuário/senha + TLS)
const char* mqtt_server = "broker.hivemq.com";

// Cliente Wi-Fi e MQTT
WiFiClient espClient;
PubSubClient client(espClient);

unsigned long lastMsg = 0;   // Controle de tempo para enviar dados
#define MSG_BUFFER_SIZE  (50)

float temp = 0;              // Variável para temperatura
float hum = 0;               // Variável para umidade
int value = 0;               // Variável para movimento (PIR)

// ------------------- FUNÇÃO: Conectar Wi-Fi -------------------
void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Conectando a rede Wi-Fi: ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  // Loop até conectar
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi conectado!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
}

// ------------------- FUNÇÃO: Callback MQTT -------------------
// Recebe mensagens de tópicos assinados
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Mensagem recebida em [");
  Serial.print(topic);
  Serial.print("]: ");

  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  // Exemplo de controle: ligar LED quando recebe "1"
  if ((char)payload[0] == '1') {
    digitalWrite(2, LOW);   // Liga LED (ativo em LOW no ESP)
  } else {
    digitalWrite(2, HIGH);  // Desliga LED
  }
}

// ------------------- FUNÇÃO: Reconeção MQTT -------------------
void reconnect() {
  // Mantém tentativa de reconectar até funcionar
  while (!client.connected()) {
    Serial.print("Tentando conectar ao MQTT...");
    String clientId = "ESP32Client-";
    clientId += String(random(0xffff), HEX);

    // Tenta conectar
    if (client.connect(clientId.c_str())) {
      Serial.println("Conectado ao broker MQTT!");
      client.publish("passaabola/status", "online");
      client.subscribe("passaabola/controle"); // Exemplo de tópico assinado
    } else {
      Serial.print("Falhou, rc=");
      Serial.print(client.state());
      Serial.println(" tentando de novo em 5s");
      delay(5000);
    }
  }
}

// ------------------- SETUP -------------------
void setup() {
  pinMode(2, OUTPUT);       // LED onboard
  pinMode(PIR_PIN, INPUT);  // Sensor PIR
  Serial.begin(115200);

  setup_wifi();             // Conecta Wi-Fi
  client.setServer(mqtt_server, 1883); // Configura broker MQTT
  client.setCallback(callback);        // Define função de callback
  dhtSensor.setup(DHT_PIN, DHTesp::DHT22); // Inicializa DHT22
}

// ------------------- LOOP PRINCIPAL -------------------
void loop() {
  if (!client.connected()) {
    reconnect(); // Garante conexão MQTT
  }
  client.loop(); // Mantém conexão ativa

  unsigned long now = millis();

  // A cada 2s envia leitura dos sensores
  if (now - lastMsg > 2000) {
    lastMsg = now;

    // ------ Leitura DHT22 ------
    TempAndHumidity data = dhtSensor.getTempAndHumidity();

    // Boas práticas: verificar se os dados são válidos
    if (!isnan(data.temperature) && !isnan(data.humidity)) {
      // Converte valores para string e envia via MQTT
      String temp = String(data.temperature, 2);
      String hum = String(data.humidity, 1);

      Serial.print("Temperatura: ");
      Serial.println(temp);
      client.publish("passaabola/sensor1/temperature", temp.c_str());

      Serial.print("Umidade: ");
      Serial.println(hum);
      client.publish("passaabola/sensor1/humidity", hum.c_str());
    } else {
      Serial.println("Erro na leitura do DHT22");
    }
  }

  // ------ Leitura PIR ------
  value = digitalRead(PIR_PIN);
  if (value == HIGH) {
    Serial.println("Movimento detectado!");
    client.publish("passaabola/sensor1/motion", "gol_detectado");
  } else {
    Serial.println("Sem movimento.");
    client.publish("passaabola/sensor1/motion", "no_motion");
  }
  
  delay(2000); // Boa prática: trocar por millis() futuramente
}
