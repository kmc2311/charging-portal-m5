#include <M5Core2.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>

// ========= Wi-Fi設定（ここだけ書き換え） =========
const char* WIFI_SSID = "KMC_Main";
const char* WIFI_PASS = "dD9BenjQ";

// ========= GPIO（あなたの配線に合わせて変更OK） =========
// LEDテープ
const int RED_PIN   = 26;
const int GREEN_PIN = 27;
const int BLUE_PIN  = 14;

// 超音波
const int TRIG_PIN  = 32;
const int ECHO_PIN  = 33;

// ========= 動作設定 =========
const int thresholdCm = 20;
const unsigned long transitionMs = 30UL * 1000UL;
const unsigned long graceMs = 3000;
const unsigned long updateIntervalMs = 20;
const unsigned long blinkIntervalMs  = 300;
const unsigned long echoTimeoutUs    = 30000;

// 共通アノードなら true（0=点灯/255=消灯）
const bool COMMON_ANODE = true;

// ========= PWM（LEDC） =========
const int PWM_FREQ = 5000;
const int PWM_BITS = 8;
const int CH_R = 0, CH_G = 1, CH_B = 2;

// ========= 状態 =========
bool active = false;
unsigned long startMs = 0;
unsigned long lastSeenMs = 0;
unsigned long lastUpdateMs = 0;
bool blinkOn = true;
unsigned long lastBlinkMs = 0;

String state = "IDLE";
int progress255 = 0;

// ========= サーバ =========
WebServer server(80);
WebSocketsServer ws(81);

// HTTPでアクセスした時の案内ページ（最低限）
const char* INDEX_HTML = R"HTML(
<!doctype html>
<html lang="ja">
<head>
  <meta charset="utf-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1" />
  <title>M5 Charging Portal</title>
  <style>
    body{font-family:system-ui,-apple-system,Segoe UI,Roboto,Arial; margin:24px}
    code{background:#eee; padding:2px 6px; border-radius:6px}
  </style>
</head>
<body>
  <h1>M5 Charging Portal</h1>
  <p>このM5Stackは状態をWebSocketで配信中です。</p>
  <p>PC/スマホ側のサイトで host に <code>このM5のIP</code> を入れてConnectしてください。</p>
  <p>WebSocket: <code>ws://(this ip):81/</code></p>
</body>
</html>
)HTML";

// ========= 便利関数 =========
int pwmFromLevel(int level) {
  level = constrain(level, 0, 255);
  return COMMON_ANODE ? (255 - level) : level;
}

void setRGB(int r, int g, int b) {
  ledcWrite(CH_R, pwmFromLevel(r));
  ledcWrite(CH_G, pwmFromLevel(g));
  ledcWrite(CH_B, pwmFromLevel(b));
}

void ledOff() { setRGB(0,0,0); }

int measureDistanceCm() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  unsigned long duration = pulseIn(ECHO_PIN, HIGH, echoTimeoutUs);
  if (duration == 0) return -1;
  return (int)(duration * 0.034 / 2.0);
}

String currentJSON() {
  String s = "{\"state\":\"";
  s += state;
  s += "\",\"progress\":";
  s += String(progress255);
  s += "}";
  return s;
}

// ★修正版：一時オブジェクトを直接渡さない
void broadcast() {
  String msg = currentJSON();
  ws.broadcastTXT(msg);
}

// ========= WebSocketイベント =========
// ★修正版：一時オブジェクトを直接渡さない
void onWsEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t len) {
  if (type == WStype_CONNECTED) {
    String msg = currentJSON();
    ws.sendTXT(num, msg);
  }
}

void setup() {
  M5.begin();
  Serial.begin(115200);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  // PWM（LEDC）初期化
  ledcSetup(CH_R, PWM_FREQ, PWM_BITS);
  ledcSetup(CH_G, PWM_FREQ, PWM_BITS);
  ledcSetup(CH_B, PWM_FREQ, PWM_BITS);
  ledcAttachPin(RED_PIN, CH_R);
  ledcAttachPin(GREEN_PIN, CH_G);
  ledcAttachPin(BLUE_PIN, CH_B);

  ledOff();

  // Wi-Fi接続
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setTextSize(2);
  M5.Lcd.setCursor(10, 10);
  M5.Lcd.print("Connecting WiFi...");

  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
  }

  IPAddress ip = WiFi.localIP();
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setCursor(10, 10);
  M5.Lcd.print("WiFi OK");
  M5.Lcd.setCursor(10, 40);
  M5.Lcd.print(ip.toString());

  // HTTP
  server.on("/", [](){
    server.send(200, "text/html; charset=utf-8", INDEX_HTML);
  });
  server.on("/state", [](){
    String msg = currentJSON();
    server.send(200, "application/json; charset=utf-8", msg);
  });
  server.begin();

  // WebSocket
  ws.begin();
  ws.onEvent(onWsEvent);

  // 初期状態
  state = "IDLE";
  progress255 = 0;
  broadcast();
}

void loop() {
  M5.update();
  server.handleClient();
  ws.loop();

  unsigned long now = millis();
  int distance = measureDistanceCm();
  bool detectedNow = (distance > 0 && distance <= thresholdCm);

  if (detectedNow) lastSeenMs = now;

  // 検知開始
  if (detectedNow && !active) {
    active = true;
    startMs = now;
    lastSeenMs = now;
    lastUpdateMs = 0;
    blinkOn = true;
    lastBlinkMs = now;

    setRGB(255,0,0);
    state = "CHARGING";
    progress255 = 0;
    broadcast();
  }

  if (active) {
    // 猶予切れ → IDLE
    if (!detectedNow && (now - lastSeenMs > graceMs)) {
      active = false;
      ledOff();
      state = "IDLE";
      progress255 = 0;
      broadcast();
      delay(10);
      return;
    }

    if (now - lastUpdateMs >= updateIntervalMs) {
      lastUpdateMs = now;

      float p = (transitionMs == 0) ? 1.0f : (float)(now - startMs) / (float)transitionMs;
      if (p > 1.0f) p = 1.0f;

      int g = (int)(p * 255.0f);
      int r = 255 - g;

      if (p < 1.0f) {
        setRGB(r, g, 0);
        state = "CHARGING";
        progress255 = g;
        broadcast();
      } else {
        // 完了：緑点滅
        if (now - lastBlinkMs >= blinkIntervalMs) {
          lastBlinkMs = now;
          blinkOn = !blinkOn;
        }
        if (blinkOn) setRGB(0,255,0);
        else         ledOff();

        state = "COMPLETE";
        progress255 = 255;
        broadcast();
      }
    }
  }

  delay(10);
}
