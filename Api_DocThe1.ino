#include <WiFi.h>
#include <HTTPClient.h>
#include <SPI.h>
#include <MFRC522.h>
#include <ArduinoJson.h>

#define SS_PIN 5    // SDA nối D5
#define RST_PIN 22  // RST nối D22

const char* ssid = "WIFI 201";
const char* password = "88888888";
const char* serverUrl = "http://192.168.0.112:5250/api/HocSinh/info/rfid/";
const char* phanCongUrl = "http://192.168.0.112:5250/api/phancong/current";

WiFiClient client;
MFRC522 mfrc522(SS_PIN, RST_PIN);

String maPhanCong = ""; // Khởi tạo rỗng

void setup() {
    Serial.begin(115200);
    WiFi.begin(ssid, password);

    Serial.println("🔄 Đang kết nối WiFi...");
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.print(".");
    }

    Serial.println("\n✅ Đã kết nối WiFi!");
    Serial.print("📡 Địa chỉ IP: ");
    Serial.println(WiFi.localIP());

    SPI.begin();
    mfrc522.PCD_Init();
    Serial.println("🎯 Hệ thống sẵn sàng quét thẻ RFID...");
    Serial.println("Đang chờ mã phân công từ server...");
}

void loop() {
    // Kiểm tra thẻ RFID
    if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) {
        return;
    }

    // Lấy maPhanCong mới nhất từ server trước khi quẹt thẻ
    fetchMaPhanCong();

    if (maPhanCong == "") {
        Serial.println("⛔ Chưa nhận được mã phân công từ server!");
        delay(3000);
        return;
    }

    String uid = "";
    for (byte i = 0; i < mfrc522.uid.size; i++) {
        uid += String(mfrc522.uid.uidByte[i], HEX);
    }
    uid.toUpperCase();

    Serial.print("📡 UID Đọc được: ");
    Serial.println(uid);
    Serial.print("📋 Mã phân công hiện tại: ");
    Serial.println(maPhanCong);

    getHocSinhInfo(uid);

    delay(1000); // Tránh đọc liên tục
}

void fetchMaPhanCong() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("⛔ WiFi bị mất kết nối!");
        return;
    }

    HTTPClient http;
    http.begin(client, phanCongUrl);
    http.addHeader("Accept", "*/*");

    int httpCode = http.GET();
    Serial.print("🔍 Mã lỗi HTTP: ");
    Serial.println(httpCode);

    if (httpCode == 200) {
        String response = http.getString();
        Serial.print("🔍 Phản hồi từ server: ");
        Serial.println(response);

        StaticJsonDocument<200> doc;
        DeserializationError error = deserializeJson(doc, response);
        if (!error && doc.containsKey("maPhanCong")) {
            maPhanCong = doc["maPhanCong"].as<String>();
            Serial.print("✅ Nhận mã phân công từ server: ");
            Serial.println(maPhanCong);
        } else {
            Serial.println("⛔ Lỗi phân tích JSON hoặc không có maPhanCong!");
            if (error) {
                Serial.print("🔍 Lỗi JSON: ");
                Serial.println(error.c_str());
            }
            maPhanCong = ""; // Đặt lại rỗng nếu có lỗi
        }
    } else {
        Serial.print("⛔ Lỗi lấy maPhanCong! Mã lỗi: ");
        Serial.println(httpCode);
        maPhanCong = ""; // Đặt lại rỗng nếu server lỗi
    }
    http.end();
}

void getHocSinhInfo(String uid) {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("⛔ WiFi bị mất kết nối!");
        return;
    }

    HTTPClient http;
    String fullUrl = String(serverUrl) + uid + "?maPhanCong=" + maPhanCong;

    Serial.print("🔗 Gửi yêu cầu đến: ");
    Serial.println(fullUrl);

    http.begin(client, fullUrl);
    http.addHeader("Accept", "*/*");

    int httpCode = http.GET();
    Serial.print("🔍 Mã lỗi HTTP: ");
    Serial.println(httpCode);

    if (httpCode == 200) {
        String response = http.getString();
        Serial.println("✅ Nhận thông tin học sinh và điểm danh:");
        Serial.println(response);
        printHocSinhInfo(response);
    } else {
        Serial.println("⛔ Lỗi lấy thông tin! Mã lỗi: " + String(httpCode));
        if (httpCode == 404) {
            Serial.println("⛔ Không tìm thấy học sinh với UID này!");
        } else if (httpCode == 400) {
            String response = http.getString();
            Serial.println("⛔ Lỗi yêu cầu: " + response);
        }
    }
    http.end();
}

void printHocSinhInfo(String jsonString) {
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, jsonString);
    if (error) {
        Serial.println("⛔ Lỗi phân tích cú pháp JSON!");
        return;
    }

    if (!doc["success"].as<bool>()) {
        Serial.println("⛔ " + doc["message"].as<String>());
        return;
    }

    JsonObject data = doc["data"];
    Serial.println("=== Thông tin học sinh ===");
    Serial.print("Mã học sinh: "); Serial.println(data["maHocSinh"].as<String>());
    Serial.print("RFID UID: "); Serial.println(data["rfiD_UID"].as<String>());
    Serial.print("Họ tên: "); Serial.println(data["hoTen"].as<String>());
    Serial.print("Ngày sinh: "); Serial.println(data["ngaySinh"].as<String>());
    Serial.print("Giới tính: "); Serial.println(data["gioiTinh"].as<String>());
    Serial.print("Mã lớp: "); Serial.println(data["maLop"].as<String>());
    Serial.println("========================");
}