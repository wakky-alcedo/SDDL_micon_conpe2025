#include <Arduino.h>

#include <WiFi.h>
#include <PubSubClient.h>

// Wi-Fiの設定
const char* ssid = "SDDLnet";
const char* password = "smallbear";

// MQTTブローカーの設定
const char* mqtt_server = "192.168.0.206";
const int mqtt_port = 1883;
const char* mqtt_client_id = "ESP32Client-"; // クライアントIDはユニークにするためにプレフィックスを追加
const char* mqtt_user = "";         // MQTTブローカーに認証が必要な場合は設定
const char* mqtt_password = "";     // MQTTブローカーに認証が必要な場合は設定

// トピックの設定
const char* subscribeTopic = "/esp32/control";
const char* publishTopic = "/esp32/status";

WiFiClient espClient;
PubSubClient client(espClient);
unsigned long lastReconnectAttempt = 0;
unsigned long startTime = 0;

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
  Serial.print("Message arrived in topic: ");
  Serial.println(topic);
  Serial.print("Message:");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println(""); // 改行を追加
  // payloadを文字列に変換
  String message = String((char*)payload).substring(0, length);
  // payloadがonの場合、LEDを点灯
  if (message == "on") {
    Serial.println("LED ON");
    // LEDを点灯する処理をここに追加
  } else if (message == "off") {
    Serial.println("LED OFF");
    // LEDを消灯する処理をここに追加
  } else {
    Serial.println("Unknown command");
  }


  Serial.println();
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
      client.subscribe(subscribeTopic);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void publishStatus() {
  unsigned long currentTime = millis() - startTime;
  String statusMessage = "Uptime: " + String(currentTime / 1000) + " seconds";
  client.publish(publishTopic, statusMessage.c_str());
  Serial.print("Published status: ");
  Serial.println(statusMessage);
}

void setup() {
  Serial.begin(115200);
  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
  startTime = millis();
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
  if (millis() - lastReconnectAttempt > 5000 && client.connected()) {
    publishStatus();
    lastReconnectAttempt = millis(); // publish後もタイマーをリセット
  }
  delay(100); // CPU負荷軽減のためเล็กน้อยの遅延
}