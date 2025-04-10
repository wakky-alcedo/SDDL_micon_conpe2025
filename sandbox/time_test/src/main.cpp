#include <Arduino.h>

// #include "esp_sntp.h"
#include <WiFi.h>
// #include "time.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>

const char* ssid = "SDDLnet";
const char* password = "smallbear";

// const char* ntpServer1 = "ntp.nict.jp";
const char* ntpServer1 = "ntp1.noc.titech.ac.jp	";
// const char* ntpServer2 = "time.google.com";
const char* ntpServer2 = "ntp2.noc.titech.ac.jp	";
const char* ntpServer3 = "ntp.jst.mfeed.ad.jp";
const long  gmtOffset_sec = 9 * 3600;
const int   daylightOffset_sec = 0;

// TimeAPI設定
String timeApiUrl = "https://www.timeapi.io/api/Time/current/zone?timeZone=Asia/Tokyo";

// タイムゾーン設定（APIを変更した場合に使用）
String timeZone = "Asia/Tokyo";

// 関数のプロトタイプ宣言
void getTimeFromApi();
void setTimeZone(String newTimeZone);

void setup() {
  Serial.begin(115200);
  
  // Wi-Fi接続
  Serial.printf("Wi-Fi '%s'に接続中...", ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" 接続完了");
  
  // 初回時刻取得
  getTimeFromApi();
}

void loop() {
  // 10秒ごとに時刻を更新（API呼び出し頻度を考慮）
  getTimeFromApi();
  delay(10000);
}

void getTimeFromApi() {
  // Wi-Fi接続確認
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Wi-Fi接続が切断されました。再接続中...");
    WiFi.begin(ssid, password);
    return;
  }
  
  HTTPClient http;
  
  Serial.print("APIに接続中...: ");
  Serial.println(timeApiUrl);
  
  // HTTP開始
  http.begin(timeApiUrl);
  
  // GETリクエスト送信
  int httpResponseCode = http.GET();
  
  if (httpResponseCode > 0) {
    Serial.print("HTTPレスポンスコード: ");
    Serial.println(httpResponseCode);
    
    String payload = http.getString();
    
    // JSONレスポンスを解析
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, payload);
    
    if (error) {
      Serial.print("JSON解析エラー: ");
      Serial.println(error.c_str());
    } else {
      // 時刻情報を取得
      String year = doc["year"].as<String>();
      String month = doc["month"].as<String>();
      String day = doc["day"].as<String>();
      String hour = doc["hour"].as<String>();
      String minute = doc["minute"].as<String>();
      String seconds = doc["seconds"].as<String>();
      String milliSeconds = doc["milliSeconds"].as<String>();
      String dateTime = doc["dateTime"].as<String>();
      String date = doc["date"].as<String>();
      String time = doc["time"].as<String>();
      String timeZone = doc["timeZone"].as<String>();
      
      // 時刻情報を表示
      Serial.println("====== 時刻情報 ======");
      Serial.print("日時: ");
      Serial.println(dateTime);
      Serial.print("日付: ");
      Serial.println(date);
      Serial.print("時間: ");
      Serial.println(time);
      Serial.print("タイムゾーン: ");
      Serial.println(timeZone);
      Serial.println("年月日: " + year + "年" + month + "月" + day + "日");
      Serial.println("時分秒: " + hour + ":" + minute + ":" + seconds + "." + milliSeconds);
      Serial.println("=====================");
    }
  } else {
    Serial.print("HTTP GET エラー: ");
    Serial.println(httpResponseCode);
  }
  
  // HTTP終了
  http.end();
}

// タイムゾーンを変更する関数
void setTimeZone(String newTimeZone) {
  timeZone = newTimeZone;
  timeApiUrl = "https://www.timeapi.io/api/Time/current/zone?timeZone=" + timeZone;
  Serial.println("タイムゾーンを " + timeZone + " に変更しました");
}