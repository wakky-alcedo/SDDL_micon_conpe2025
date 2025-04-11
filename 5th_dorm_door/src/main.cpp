#include <Arduino.h>

#include <WiFi.h>
#include <PubSubClient.h>
#include <WiFiUdp.h>

#include "Hardware_Control_Assistant.hpp"

// Wi-Fiの設定
const char* ssid = "SDDLnet";
const char* password = "smallbear";

// MQTTブローカーの設定
const char* mqtt_server = "192.168.0.206";  // MQTTブローカーのIPアドレス todo
const int mqtt_port = 1883;                 // MQTTブローカーのポート番号
const char* mqtt_client_id = "ESP32Client-5th_dorm_door"; // クライアントIDはユニークにするためにプレフィックスを追加
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

const uint8_t enable_call_pin = 22; // GPIO22を使用 (通話開始用)
hca::Button_CHT enable_call_button(100); // 1秒間の長押しで通話開始

String receiverIP = "";  // 受信者のIPアドレスを格納する変数 通話直前に相手から送られてくる
const int receiverPort = 12345; // 受信者のポート番号
const int senderPort = 54321;

WiFiUDP udp;

const int micPin = 34;
const int sampleRate = 8000;
const int bitsPerSample = 16; // 1サンプルあたり16ビット
const int bytesPerSample = bitsPerSample / 8;
const int samplesPerPacket = 16; // 1パケットあたりのサンプル数
const int packetSize = samplesPerPacket * bytesPerSample; // パケットサイズ (バイト)

int16_t pcmValue;
static uint8_t pcmBuffer[packetSize];

bool isCalling = false; // 通話中かどうかのフラグ

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
  Serial.println("-----------------------");
  Serial.println("Topic: " + String(topic));
  String message = String((char*)payload).substring(0, length);
  Serial.println("Message: " + message);

  // 受信したトピックに応じて処理を分岐
  if (String(topic) == String(subscribeTopic_5dd)) {
    Serial.println("Received message for 5th dorm door: " + message);
  } else if (String(topic) == String(publishTopic_1l)) {
    Serial.println("Received message from 1st lab: " + message);
    // "IP address: "が含まれていたら、その後の部分を取得
    if (message.indexOf("IP: ") != -1) {
      receiverIP = message.substring(message.indexOf("IP: ") + 4);
      Serial.println("Receiver IP address: " + receiverIP);
      udp.begin(senderPort);
      isCalling = true; // 通話中フラグを立てる
    }
  } else if (String(topic) == String(publishTopic_5dk)) {
    Serial.println("Received message from 5th dorm key: " + message);
  } else {
    Serial.println("Unknown topic");
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
      // 購読するトピックを指定
      client.subscribe(subscribeTopic_5dd); // 5th dorm doorへのcontrol
      client.subscribe(publishTopic_5dk);   // 5th dorm keyのstatus
      client.subscribe(publishTopic_1l);    // 1st labのstatus
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
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

  pinMode(enable_call_pin, INPUT_PULLDOWN); // 通話開始用ピンをプルダウン入力に設定
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

  // 5秒ごとにステータスをPublish
  enable_call_button.update(digitalRead(enable_call_pin), millis()); // 通話開始用ピンの状態を更新
  if (enable_call_button.is_pushed()) { // 通話開始用ピンがHIGHの場合
    Serial.println("通話開始ボタンが押されました。");
    publishTopic(subscribeTopic_1l, "call start: " + String(WiFi.localIP().toString()));
    // ここではisCallingフラグを立てない（IPアドレスを受信したときに開始するため）
  } else if (enable_call_button.is_released()) { // 通話開始用ピンがLOWの場合
    Serial.println("通話開始ボタンが離されました。");
    publishTopic(subscribeTopic_1l, "call end: " + String(WiFi.localIP().toString()));
    isCalling = false; // 通話中フラグを下ろす
    udp.stop(); // UDP通信を停止
  }

  // 通話中
  if (isCalling) {
    for (int i = 0; i < samplesPerPacket; i++) {
      int rawValue = analogRead(micPin);
      // 12ビットのADC値を16ビットの符号付き整数にマッピング
      pcmValue = map(rawValue, 0, 4095, -32768, 32767);
      pcmBuffer[i * 2] = (pcmValue >> 0) & 0xFF;     // 下位バイト
      pcmBuffer[i * 2 + 1] = (pcmValue >> 8) & 0xFF; // 上位バイト
      delayMicroseconds(1000000 / sampleRate);
    }
  
    // const char* receiverIP = "192.168.0.218";  // 受信者のIPアドレスを指定
    // udp.beginPacket(receiverIP, receiverPort); // 受信者のIPアドレスとポート番号を指定
    udp.beginPacket(receiverIP.c_str(), receiverPort); // 受信者のIPアドレスとポート番号を指定
    udp.write(pcmBuffer, packetSize);
    udp.endPacket();
    Serial.printf("Sent %d bytes (PCM)\n", packetSize);
}
}