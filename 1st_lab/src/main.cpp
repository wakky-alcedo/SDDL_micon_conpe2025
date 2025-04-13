#include <Arduino.h>

#include <WiFi.h>
#include <PubSubClient.h>
#include <WiFiUdp.h>
#include "DFRobotDFPlayerMini.h"
#include <SoftwareSerial.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// Wi-Fiの設定
// const char* ssid = "SDDLnet";
// const char* password = "smallbear";
const char* ssid = "Hippopotamus";
const char* password = "origami2827";

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
constexpr uint8_t threshold = 20; // ライトセンサーのしきい値を定義 (適宜調整してください)
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

SoftwareSerial mySoftwareSerial(22, 21); // RX, TX
HardwareSerial myHardwareSerial(2); // use HardwareSerial UART1
DFRobotDFPlayerMini myDFPlayer;

const String timeApiUrl = "https://www.timeapi.io/api/Time/current/zone?timeZone=Asia/Tokyo";
constexpr uint16_t eveningChimeTime = 17 * 60; // 17:00 (17時) の時間を分単位で定義
constexpr uint16_t workingEndChimeTime = 20 * 60; // 20:00 (20時) の時間を分単位で定義

// 関数のプロトタイプ宣言
void setup_wifi();
void callback(char* topic, byte* payload, unsigned int length);
void reconnect();
void publishStatus(bool lightStatus = false);
void publishTopic(const char* topic, String statusMessage);
int setTimeFromApi();
void printTime();

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
      dacWrite(speakerPin, 0);      
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

int setTimeFromApi() {
  // Wi-Fi接続確認
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Wi-Fi接続が切断されました。再接続中...");
    WiFi.begin(ssid, password);
    return -1; // エラー
  }
  
  HTTPClient http;
  
  Serial.print("APIに接続中...: ");
  Serial.println(timeApiUrl);
  
  // HTTP開始
  http.begin(timeApiUrl);
  
  // GETリクエスト送信
  int httpResponseCode = http.GET();

  http.end(); // HTTP終了
  
  if (httpResponseCode > 0) {
    Serial.print("HTTPレスポンスコード: ");
    Serial.println(String(httpResponseCode));
    
    String payload = http.getString();
    
    // JSONレスポンスを解析
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, payload);
    
    if (error) {
      Serial.print("JSON解析エラー: ");
      Serial.println(error.c_str());
    } else {
      // 時間をマイコンの時間に設定 clock_settimeを使用
      struct tm timeInfo;
      timeInfo.tm_year = doc["dateTime"].as<String>().substring(0, 4).toInt() - 1900; // 年を設定 (1900年からの経過年数)
      timeInfo.tm_mon = doc["dateTime"].as<String>().substring(5, 7).toInt() - 1; // 月を設定 (0-11の範囲)
      timeInfo.tm_mday = doc["dateTime"].as<String>().substring(8, 10).toInt(); // 日を設定 (1-31の範囲)
      timeInfo.tm_hour = doc["dateTime"].as<String>().substring(11, 13).toInt(); // 時を設定 (0-23の範囲)
      timeInfo.tm_min = doc["dateTime"].as<String>().substring(14, 16).toInt(); // 分を設定 (0-59の範囲)
      timeInfo.tm_sec = doc["dateTime"].as<String>().substring(17, 19).toInt(); // 秒を設定 (0-59の範囲)
      timeInfo.tm_isdst = -1; // 夏時間の設定 (自動判定)
      time_t epochTime = mktime(&timeInfo); // 時刻をエポック時間に変換
      struct timespec ts;
      ts.tv_sec = epochTime; // 秒を設定
      ts.tv_nsec = 0; // ナノ秒を設定 (0に設定)
      clock_settime(CLOCK_REALTIME, &ts); // システム時刻を設定
      return 0; // 成功
    }
  } else {
    Serial.print("HTTP GET エラー: ");
    Serial.println(String(httpResponseCode));
  }
  return -1; // エラー
}

void printTime() {
  time_t now = time(nullptr);
  struct tm* timeInfo = localtime(&now);
  Serial.print("Current time: ");
  Serial.println(String(timeInfo->tm_hour) + ":" + String(timeInfo->tm_min) + ":" + String(timeInfo->tm_sec));
}

void setup() {
  Serial.begin(115200);
  delay(100);

  // Wi-Fi接続
  setup_wifi();
  // MQTT接続
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
  reconnect();

  // 時刻を設定
  while (setTimeFromApi() != 0) {
    Serial.println("時刻の設定に失敗しました。再試行中...");
    delay(5000); // 5秒待機
  }
  printTime(); // 時刻を表示

  // ピンの設定
  pinMode(lightSensorPin, INPUT); // ライトセンサーの入力ピンを設定
  pinMode(callLedPin, OUTPUT); // 通話LEDピンを出力に設定
  digitalWrite(callLedPin, HIGH); // 通話LEDを初期状態で点灯
  dacWrite(speakerPin, 0);

  // DFPlayer Miniの初期化
  Serial.println("MP3 Player");
  mySoftwareSerial.begin(9600);
  // myHardwareSerial.begin(9600, SERIAL_8N1, 22, 21); // RX, TX

  while(!myDFPlayer.begin(mySoftwareSerial)) { //Use software serial to communicate with mp3 module
    Serial.print(".");
    delay(500);
  }
  myDFPlayer.volume(20); // Set volume value (0-30).
  Serial.println("MP3 Player ready.");
  // myDFPlayer.play(1);  //Play the first mp3
  // Serial.println("Playing track 1.");
}

void loop() {
  static uint16_t slowLoopCount = 0; // statusを表示用のカウンタ
  slowLoopCount++;
  if (slowLoopCount >= 1000) { // ADCの処理頻度が高いと値が安定しない

    // MQTT接続確認
    if (!client.connected()) {
      long now = millis();
      if (now - lastReconnectAttempt > 5000) {
        lastReconnectAttempt = now;
        reconnect();
      }
    }
    client.loop();

    // ライトセンサー
    slowLoopCount = 0;
    uint8_t lightValue = analogRead(lightSensorPin);
    Serial.println("Light Sensor Value: " + String(lightValue));
    static bool prevLightStatus = false; // 前回のライトの状態を保存する変数
    if (prevLightStatus != lightValue > threshold) {
      publishStatus(lightValue > threshold);
    }
    prevLightStatus = lightValue > threshold; // 現在の状態を保存

    // 時刻を取得して、チャイムを鳴らす
    time_t now = time(nullptr);
    struct tm* timeInfo = localtime(&now);
    uint16_t currentTime = timeInfo->tm_hour * 60 + timeInfo->tm_min; // 現在の時間を分単位で取得
    static uint16_t prevTime = 0; // 前回の時間を保存する変数
    if (prevTime != currentTime) {
      Serial.println("Current time: " + String(timeInfo->tm_hour) + ":" + String(timeInfo->tm_min) + ":" + String(timeInfo->tm_sec));
      if (currentTime == eveningChimeTime) {
        Serial.println("Evening chime time!");
        myDFPlayer.play(1); // 1番の音声を再生
      } else if (currentTime == workingEndChimeTime) {
        Serial.println("Working end chime time!");
        myDFPlayer.play(2); // 2番の音声を再生
        Serial.println("Working end chime time!");
      } else if (currentTime == 0) {
        setTimeFromApi(); // 0時になったらAPIから時刻を取得、再設定
        Serial.println("Time reset from API.");
      }
      prevTime = currentTime; // 現在の時間を保存
    }
  }

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
      Serial.println("Received packet of size " + String(packetSizeReceived));
    } else if (packetSizeReceived > 0) {
      Serial.println("Received packet of unexpected size: " + String(packetSizeReceived));
    }
  }
  delay(1);
}