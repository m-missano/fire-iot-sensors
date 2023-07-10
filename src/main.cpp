#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h> 
#include <Wire.h>
#include <SparkFunCCS811.h> 
#include <Adafruit_BME280.h>
#include <BH1750.h>
#include "ClosedCube_HDC1080.h"


//#define CCS811_ADDR 0x5B //Default I2C Address
#define CCS811_ADDR 0x5A //Alternate I2C Address
#define BME280_ADDR 0x76
#define PIN_NOT_WAKE 5
#define SEALEVELPRESSURE_HPA (1013.25)

#ifndef STASSID
#define STASSID "STASSID"
#define STAPSK "STAPSK"
#endif

// Definicao ssid e password da rede
const char* ssid = STASSID;
const char* password = STAPSK;

const char* host = "192.168.1.1";
const uint16_t port = 3000;

// Dados do servidor MQTT
const char* mqtt_server = "mqtt.prod.konkerlabs.net";

// Definicao de usuario e senha na konker
const char* USER = "USER";
const char* PWD = "PWD";

// Definicao dos canais utilizados
const char* PUB_BME280_TEMP_C = "data/USER/pub/bme280_temperature";
const char* PUB_BME280_HUMID = "data/USER/pub/bme280_humidity";
const char* PUB_BME280_PRESSURE = "data/USER/pub/bme280_pressure";
const char* PUB_BME280_ALTITUDE = "data/USER/pub/bme280_altitude";
const char* PUB_CCS811_CO2 = "data/USER/pub/ccs811_co2";
const char* PUB_CCS811_TVOC = "data/USER/pub/ccs811_tvoc";
const char* PUB_BH1750_LUX = "data/USER/pub/bh1750_lux";
const char* SUB = "data/4lfsmrgncib0/sub/conf";

char bufferJ[256];
char *mensagem;

char *jsonMQTTmsgDATA(const char *device_id, const char *metric, float value) {
	const int capacity = JSON_OBJECT_SIZE(3);
	StaticJsonDocument<capacity> jsonMSG;
	jsonMSG["deviceId"] = device_id;
	jsonMSG["metric"] = metric;
	jsonMSG["value"] = value;
	serializeJson(jsonMSG, bufferJ);
	return bufferJ;
}

WiFiClient espClient;
PubSubClient client(espClient);

// Declaracao dos sensores
CCS811 ccs811(CCS811_ADDR);
BH1750 lightMeter(0x23);
Adafruit_BME280 bme;

// Declaracao das variaveis de armazenamento dos dados dos sensores
float ccs811_co2;
float ccs811_tvoc;
float bme280_temp_c;
float bme280_humid; 
float bme280_altitude; 
float bme280_pressure; 
float bh1750_lux;

// Criando a funcao de callback
// Essa funcao eh rodada quando uma mensagem eh recebida via MQTT.
// Nesse caso ela eh muito simples: imprima via serial o que voce recebeu
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
}

void reconnect() {
  // Entra no Loop ate estar conectado
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Usando um ID unico
    // Tentando conectar
    if (client.connect(USER, USER, PWD)) {
      Serial.println("connected");
      // Subscrevendo no topico esperado
      client.subscribe(SUB);
    } else {
      Serial.print("Falhou! Codigo rc=");
      Serial.print(client.state());
      Serial.println(" Tentando novamente em 5 segundos");
      // Esperando 5 segundos para tentar novamente
      delay(5000);
    }
  }
}

void setup_wifi() {
  delay(10);
  // Agora vamos nos conectar em uma rede Wifi
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    //Imprimindo pontos na tela ate a conexao ser estabelecida!
    Serial.print(".");
  }

  // Imprime que a conexao foi bem sucedida
  Serial.println("");
  Serial.println("WiFi conectado");
  Serial.println("Endereco de IP: ");
  Serial.println(WiFi.localIP());
}

// Imprime as informacoes coletadas nas variaveis de armazenamento
void printInfoSerial() {

  Serial.print("CO2: ");
  Serial.print(ccs811_co2);
  Serial.println(" ppm ");

  Serial.print("TVOC : ");
  Serial.print(ccs811_tvoc);
  Serial.println(" ppb ");

  Serial.println("");

  Serial.println("HDC1080 data:");
  Serial.print("Temp: ");
  Serial.print(bme280_temp_c, 2);
  Serial.print(" C ");

  Serial.print(" RH: ");
  Serial.print(bme280_humid, 2);
  Serial.println(" %");

  Serial.print(bme280_pressure);
  Serial.println(" hPa");

  Serial.print("Approx. Altitude = ");
  Serial.print(bme280_altitude);
  Serial.println(" m");

  Serial.println();

  Serial.print("Light: ");
  Serial.print(bh1750_lux);
  Serial.println(" lx");
  Serial.println();

}

// Printa o erro do sensor
void printSensorError() {
  uint8_t error = ccs811.getErrorRegister();

  if ( error == 0xFF ) { //comm error
    Serial.println("Failed to get ERROR_ID register.");
  } else {
    Serial.print("Error: ");
    if (error & 1 << 5) Serial.print("HeaterSupply");
    if (error & 1 << 4) Serial.print("HeaterFault");
    if (error & 1 << 3) Serial.print("MaxResistance");
    if (error & 1 << 2) Serial.print("MeasModeInvalid");
    if (error & 1 << 1) Serial.print("ReadRegInvalid");
    if (error & 1 << 0) Serial.print("MsgInvalid");
    Serial.println();
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println();

  Wire.begin();

  // Inicia o ccs811
  ccs811.begin();
  Serial.print("CCS811 begin: ");
  Serial.println();

  // Inicia o bme280
  bme.begin(BME280_ADDR);  
  
  // Inicia o BH1750
  if (lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE)) {
    Serial.println(F("BH1750 Advanced begin"));
  } else {
    Serial.println(F("Error initialising BH1750"));
  }

  // Seta o wifi e o mqtt
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  delay(500);
}

//---------------------------------------------------------------
void loop() {

  //O programa em si eh muito simples: 
  //se nao estiver conectado no Broker MQTT, se conecte!
  if (!client.connected()) {
    reconnect();
  }

  // Checa se ha dados do ccs811 disponiveis
  if (ccs811.dataAvailable()) {
    
    ccs811.readAlgorithmResults();

    // Recupera os valores do bme280
    bme280_temp_c = bme.readTemperature();
    bme280_pressure = bme.readPressure()/100.0F;
    bme280_altitude = bme.readAltitude(SEALEVELPRESSURE_HPA);
    bme280_humid = bme.readHumidity();
    
    // Envia os valores de umidade e temperatura para o ccs811
    ccs811.setEnvironmentalData(bme280_humid, bme280_temp_c);

    // Recupera o valor de luminosidade do bh1750
    if (lightMeter.measurementReady()) {
      bh1750_lux = lightMeter.readLightLevel();
    }

    // Recupera os valores do ccs811
    ccs811_co2 = ccs811.getCO2();
    ccs811_tvoc = ccs811.getTVOC();

    // Imprime as informacoes coletadas
    printInfoSerial();

    //Enviando via MQTT o resultado calculado de temperatura
    mensagem = jsonMQTTmsgDATA("bme280_temperature", "Celsius", bme280_temp_c);
    client.publish(PUB_BME280_TEMP_C, mensagem); 
    client.loop();  

    //Enviando via MQTT o resultado calculado de pressao
    mensagem = jsonMQTTmsgDATA("bme280_pressure", "Hectopascal", bme280_pressure);
    client.publish(PUB_BME280_PRESSURE, mensagem); 
    client.loop();  

    //Enviando via MQTT o resultado calculado de altitude
    mensagem = jsonMQTTmsgDATA("bme280_altitude", "Meters", bme280_altitude);
    client.publish(PUB_BME280_ALTITUDE, mensagem); 
    client.loop();  

    //Enviando via MQTT o resultado calculado de umidade
    mensagem = jsonMQTTmsgDATA("bme280_humidity", "Percentage", bme280_humid);
    client.publish(PUB_BME280_HUMID, mensagem); 
    client.loop();
    
    //Enviando via MQTT o resultado calculado de co2
    mensagem = jsonMQTTmsgDATA("ccs811_co2", "Particles per million", ccs811_co2);
    client.publish(PUB_CCS811_CO2, mensagem); 
    client.loop();  

    //Enviando via MQTT o resultado calculado de tvoc
    mensagem = jsonMQTTmsgDATA("ccs811_tvoc", "Particles per billion", ccs811_tvoc);
    client.publish(PUB_CCS811_TVOC, mensagem); 
    client.loop();    

    //Enviando via MQTT o resultado calculado de luminosidade
    mensagem = jsonMQTTmsgDATA("bh1750_lux", "Lux", bh1750_lux);
    client.publish(PUB_BH1750_LUX, mensagem); 
    client.loop();    
    
  } else if (ccs811.checkForStatusError()) {
    // Se o ccs811 encontrou um erro interno, mostra o erro
    printSensorError();
  }

  // Delay para a proxima leitura
  delay(60000); 
}

