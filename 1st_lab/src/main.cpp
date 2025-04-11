#include <Arduino.h>

#include <WiFi.h>
#include <PubSubClient.h>
#include <WiFiUdp.h>

// Wi-Fiの設定
const char* ssid = "SDDLnet";
const char* password = "smallbear";

// MQTTブローカーの設定
const char* mqtt_server = "192.168.0.206"; // MQTTブローカーのIPアドレス todo
const int mqtt_port = 1883;
const char* mqtt_client_id = "ESP32Client-1st_lab"; // クライアントIDはユニークにするためにプレフィックスを追加
const char* mqtt_user = "";         // MQTTブローカーに認証が必要な場合は設定
const char* mqtt_password = "";     // MQTTブローカーに認証が必要な場合は設定

// トピックの設定
const char* subscribeTopic_1l = "/esp32_1l/control";   // 1st lab       5ddからの通話開始
const char* publishTopic_1l = "/esp32_1l/status";      // 1st lab       ライト
const char* subscribeTopic_5dk = "/esp32_5dk/control"; // 5th dorm key
const char* publishTopic_5dk = "/esp32_5dk/status";    // 5th dorm key
const char* subscribeTopic_5dd = "/esp32_5dd/control"; // 5th dorm door
const char* publishTopic_5dd = "/esp32_5dd/status";    // 5th dorm door

WiFiClient espClient;
PubSubClient client(espClient);
unsigned long lastReconnectAttempt = 0;
unsigned long startTime = 0;

// constexpr int lightSensorVccPin = 22; // ライトセンサーのピン番号を定義 (GPIO34を使用)
constexpr int lightSensorPin = 35
; // ライトセンサーのピン番号を定義 (GPIO27を使用)
constexpr uint8_t threshold = 100; // ライトセンサーのしきい値を定義 (適宜調整してください)
constexpr uint8_t callLedPin = 4; // ライトセンサーのVccピンを定義 (GPIO22を使用)

String senderIP = ""; // 送信者のIPアドレスを格納する変数
const int receiverPort = 12345;

WiFiUDP udp;
const int speakerPin = 26;
const int sampleRate = 8000;
const int bitsPerSample = 16;
const int bytesPerSample = bitsPerSample / 8;
const int samplesPerPacket = 16;
const int packetSize = samplesPerPacket * bytesPerSample;
byte pcmBuffer[packetSize];

bool isCalling = false; // 通話中かどうかのフラグ

// 関数のプロトタイプ宣言
void setup_wifi();
void callback(char* topic, byte* payload, unsigned int length);
void reconnect();
void publishStatus(bool lightStatus = false);
void publishTopic(const char* topic, String statusMessage);

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.println("Topic: " + String(topic));
  String message = String((char*)payload).substring(0, length);
  Serial.println("Message: " + message);

  // 受信したトピックに応じて処理を分岐
  if (String(topic) == String(subscribeTopic_1l)) {
    Serial.println("-----------------------");
    Serial.println("Received message on topic: " + String(topic));
    // messageに"call start: "が含まれていたら、
    if (message.indexOf("call start: ") != -1) {
      Serial.println("Call started!");
      digitalWrite(callLedPin, LOW);
      // messageから"call start: "を取り除き、IPアドレスを取得
      String senderIP = message.substring(message.indexOf("call start: ") + 12);
      Serial.println("Sender IP Address: " + senderIP);
      // 自分のIPアドレスを発信
      String myIpAddress = WiFi.localIP().toString();
      publishTopic(publishTopic_1l, "IP: " + myIpAddress);
      // 通話開始フラグを立てる
      isCalling = true;
      udp.begin(receiverPort); // UDPを開始
    } else if (message.indexOf("call end: ") != -1) {
      Serial.println("Call ended!");
      digitalWrite(callLedPin, HIGH);
      isCalling = false; // 通話終了フラグをリセット
      udp.stop(); // UDPを停止
    }
  } else if (String(topic) == String(publishTopic_5dd)) {
    Serial.println("Received message on topic: " + String(topic));
  } else if (String(topic) == String(publishTopic_5dk)) {
    Serial.println("Received message on topic: " + String(topic));
  }


  Serial.println("-----------------------");
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // クライアントIDを生成 (MACアドレスの一部を使用)
    String clientId = mqtt_client_id;
    clientId += WiFi.macAddress();
    if (client.connect(clientId.c_str(), mqtt_user, mqtt_password)) {
      Serial.println("connected");
      client.subscribe(subscribeTopic_1l);
      client.subscribe(publishTopic_5dd);
      client.subscribe(publishTopic_5dk);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void publishStatus(bool lightStatus) {
  String statusMessage = "light status: " + String(lightStatus ? "on" : "off");
  publishTopic(publishTopic_1l, statusMessage);
}

void publishTopic(const char* topic, String statusMessage) {
  client.publish(topic, statusMessage.c_str());
  Serial.print("Published status: ");
  Serial.println(statusMessage);
}

void setup() {
  Serial.begin(115200);
  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
  startTime = millis();

  // pinMode(lightSensorVccPin, OUTPUT); // ライトセンサーのVccピンを出力に設定
  // digitalWrite(lightSensorVccPin, HIGH); // ライトセンサーのVccピンをHIGHに設定
  pinMode(lightSensorPin, INPUT); // ライトセンサーの入力ピンを設定
  pinMode(callLedPin, OUTPUT); // 通話LEDピンを出力に設定
  digitalWrite(callLedPin, HIGH); // 通話LEDを初期状態で点灯
  dacWrite(speakerPin, 0);
}

void loop() {
  if (!client.connected()) {
    long now = millis();
    if (now - lastReconnectAttempt > 5000) {
      lastReconnectAttempt = now;
      reconnect();
    }
  }
  client.loop();

  // ライトセンサー
  uint8_t lightStatus = analogRead(lightSensorPin); // ライトセンサーの値を読み取る
  static uint8_t showCount = 0; // statusを表示用のカウンタ
  showCount++;
  // if (showCount >= 255) { // 10回ごとに表示
  //   showCount = 0;
  //   Serial.println("Light Sensor Value: " + String(lightStatus));
  // }
  static bool prevLightStatus = false; // 前回のライトの状態を保存する変数
  if (prevLightStatus != lightStatus > threshold) {
    publishStatus(lightStatus > threshold);
  }
  prevLightStatus = lightStatus > threshold; // 現在の状態を保存

  // 通話中の処理
  if (isCalling) {
    // UDPで音声データを受信
    int packetSizeReceived = udp.parsePacket();
    if (packetSizeReceived == packetSize) {
      udp.read(pcmBuffer, packetSize);
      // 受信したPCMデータをDACに出力
      for (int i = 0; i < samplesPerPacket; i++) {
        int16_t pcmValue = (pcmBuffer[i * 2 + 1] << 8) | pcmBuffer[i * 2];
        // 16ビットのPCM値を8ビットのDACの範囲 (0-255) にマッピング
        int dacValue = map(pcmValue, -32768, 32767, 0, 255);
        dacWrite(speakerPin, dacValue);
        delayMicroseconds(1000000 / sampleRate);
      }
      Serial.printf("Received packet of size %d\n", packetSizeReceived);
    } else if (packetSizeReceived > 0) {
      Serial.printf("Received unexpected packet size: %d\n", packetSizeReceived);
    }
  }
  delay(1);
}