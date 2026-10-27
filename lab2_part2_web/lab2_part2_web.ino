/* ============================================================================
 *  ЛР N2, часть 2 — веб-стенд кодов, исправляющих ошибки (только ESP32).
 *  ESP32 поднимает точку доступа (или подключается к сети аудитории) и отдаёт
 *  страницу стенда. Все вычисления — кодирование, декодирование, эксперимент —
 *  выполняет микроконтроллер, браузер только отображает результат (JSON).
 *
 *  Светодиоды:  GPIO16 зелёный  — принято верно / ошибка исправлена
 *               GPIO17 жёлтый   — ошибка обнаружена, не исправлена
 *               GPIO18 красный  — ошибка декодирования (выдано неверное слово)
 *
 *  Студент реализует indicate() (задание 7) и переносит свой ham_codes.h
 *  из части 1 (с выполненными заданиями 1–4).
 * ========================================================================== */
#include <WiFi.h>
#include <WebServer.h>
#include "ham_codes.h"
#include "page_html.h"

#define USE_SOFTAP 1                    /* 1 — своя точка доступа; 0 — сеть аудитории */
const char* AP_SSID = "ESP32-FEC-Lab";
const char* AP_PASS = "12345678";
const char* STA_SSID = "YOUR_SSID";
const char* STA_PASS = "YOUR_PASS";

const int LED_GREEN = 16, LED_YELLOW = 17, LED_RED = 18;
WebServer server(80);

/* ----- текущий код из параметров запроса ----- */
CodeCfg cfgFromArgs() {
  CodeCfg c = { 'h', 4, 0, 3, 2, 2 };
  if (server.hasArg("code")) c.id = server.arg("code")[0];
  if (server.hasArg("q"))    c.q  = server.arg("q").toInt();
  if (server.hasArg("a"))    c.a  = server.arg("a").toInt();
  if (server.hasArg("b"))    c.b  = server.arg("b").toInt();
  return c;
}
uint32_t bitsToVal(const String& s) { uint32_t v = 0; for (uint16_t i = 0; i < s.length(); i++)
  if (s[i] == '0' || s[i] == '1') v = (v << 1) | (s[i] - '0'); return v; }
String valToBits(uint32_t v, uint8_t n) { String s; for (uint8_t i = 1; i <= n; i++) s += (char)('0' + getBit(v, n, i)); return s; }

/* ==========================================================================
 *  ЗАДАНИЕ 7. Светодиодная индикация исхода приёма.
 *  Горит ровно один светодиод; при OUT_MISCORRECT (ошибка декодирования)
 *  красный мигает три раза. Реализуйте функцию самостоятельно.
 * ========================================================================== */
void indicate(uint8_t outcome) {
  digitalWrite(LED_GREEN, LOW); digitalWrite(LED_YELLOW, LOW); digitalWrite(LED_RED, LOW);
  switch (outcome) {
    case OUT_OK:
    case OUT_CORRECTED:  digitalWrite(LED_GREEN, HIGH); break;
    case OUT_DETECTED:   digitalWrite(LED_YELLOW, HIGH); break;
    case OUT_MISCORRECT:
      for (int i = 0; i < 3; i++) { digitalWrite(LED_RED, HIGH); delay(120);
                                    digitalWrite(LED_RED, LOW); delay(120); }
      digitalWrite(LED_RED, HIGH); break;
  }
}

/* ----------------------------- маршруты API ------------------------------ */
void handleRoot() { server.send_P(200, "text/html", PAGE_HTML); }

void handleInfo() {
  CodeCfg c = cfgFromArgs(); uint8_t n = codeN(&c), k = codeK(&c);
  uint8_t d = (c.id == 'r') ? c.q : ((c.id == 'h' || c.id == 'H') ? 3 : 4);
  uint8_t t = (c.id == 'r') ? (c.q - 1) / 2 : 1;
  String j = "{\"n\":" + String(n) + ",\"k\":" + String(k) +
             ",\"R\":" + String((float)k / n, 3) + ",\"L\":" + String(1.0f - (float)k / n, 3) +
             ",\"d\":" + String(d) + ",\"t\":" + String(t) + "}";
  server.send(200, "application/json", j);
}

void handleEncode() {
  CodeCfg c = cfgFromArgs();
  uint32_t info = bitsToVal(server.arg("data")) & fecMask(codeK(&c));
  uint32_t w = codeEncode(&c, info);
  server.send(200, "application/json",
    "{\"word\":\"" + valToBits(w, codeN(&c)) + "\",\"weight\":" + String(fecWeight(w)) + "}");
}

void handleCheck() {
  CodeCfg c = cfgFromArgs(); uint8_t n = codeN(&c);
  uint32_t sentInfo = bitsToVal(server.arg("sent")) & fecMask(codeK(&c));
  uint32_t word = bitsToVal(server.arg("word"));
  DecResult d = codeDecode(&c, word);
  uint8_t out = classify(sentInfo, d);
  indicate(out);                                   /* задание 7 */
  const char* st = (d.status == DEC_OK) ? "ok" : (d.status == DEC_CORRECTED) ? "corrected" : "detected";
  String j = "{\"status\":\"" + String(st) + "\",\"pos\":" + String(d.pos) +
             ",\"info\":\"" + valToBits(d.info, codeK(&c)) + "\",\"outcome\":" + String(out) + "}";
  server.send(200, "application/json", j);
}

void handleExp() {
  CodeCfg c = cfgFromArgs();
  double p0 = server.arg("p").toFloat();
  uint32_t N = server.arg("N").toInt(); if (N == 0) N = 100000;
  FecStat s; uint32_t t0 = millis(); fecExperiment(&c, p0, N, &s); uint32_t ms = millis() - t0;
  String j = "{\"N\":" + String(N) + ",\"corr\":" + String(s.nOk + s.nCorr) +
             ",\"det\":" + String(s.nDet) + ",\"mis\":" + String(s.nMis) +
             ",\"false\":" + String(s.nFalse) +
             ",\"Pdek\":" + String((double)s.nMis / N, 8) +
             ",\"Pdet\":" + String((double)s.nDet / N, 8) +
             ",\"ms\":" + String(ms) + "}";
  server.send(200, "application/json", j);
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_GREEN, OUTPUT); pinMode(LED_YELLOW, OUTPUT); pinMode(LED_RED, OUTPUT);
  indicate(OUT_OK);                                /* зелёный при старте */

#if USE_SOFTAP
  WiFi.mode(WIFI_AP); WiFi.softAP(AP_SSID, AP_PASS);
  Serial.print("AP \""); Serial.print(AP_SSID); Serial.print("\"  IP: ");
  Serial.println(WiFi.softAPIP());
#else
  WiFi.mode(WIFI_STA); WiFi.begin(STA_SSID, STA_PASS);
  while (WiFi.status() != WL_CONNECTED) { delay(300); Serial.print('.'); }
  Serial.print("\nIP: "); Serial.println(WiFi.localIP());
#endif

  server.on("/", handleRoot);
  server.on("/api/info", handleInfo);
  server.on("/api/encode", handleEncode);
  server.on("/api/check", handleCheck);
  server.on("/api/exp", handleExp);
  server.begin();
  Serial.println("HTTP server started");
}
void loop() { server.handleClient(); }
