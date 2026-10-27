/* ============================================================================
 *  ЛР N2, часть 1 — терминальный стенд кодов, исправляющих ошибки.
 *  Совместим с Arduino Uno и ESP32 (WEMOS D1 R32). Монитор порта: 115200 бод,
 *  окончание строки «Новая строка». Все вычисления ведёт микроконтроллер.
 *
 *  Студент реализует 8 функций в ham_codes.h (задания 1–4). Этот файл
 *  (командный интерфейс) изменять не требуется.
 * ========================================================================== */
#include "ham_codes.h"

CodeCfg  code = { 'h', 4, 0, 3, 2, 2 };   /* текущий код; по умолчанию Хэмминг (7,4) */
uint32_t lastInfo = 0, lastWord = 0;      /* последнее закодированное сообщение/слово */

/* ----- вспомогательный вывод кодовой комбинации с разделителями ----- */
void printWord(uint32_t w, uint8_t n) {
  for (uint8_t i = 1; i <= n; i++) {
    Serial.print((char)('0' + getBit(w, n, i)));
    /* визуальные разделители: группы повторения, инфо|контроль, строки решётки */
    if (code.id == 'r' && i % code.q == 0 && i < n) Serial.print(' ');
    if (code.id == 'h' && (i == 4)) Serial.print('|');
    if (code.id == 'x' && (i == 4 || i == 7)) Serial.print('|');
    if (code.id == 't' && i % (code.b + 1) == 0 && i < n) Serial.print(' ');
  }
}
const char* statusStr(uint8_t s) {
  if (s == DEC_OK) return "ошибок нет";
  if (s == DEC_CORRECTED) return "ОШИБКА ИСПРАВЛЕНА";
  return "ОШИБКА ОБНАРУЖЕНА (не исправляется)";
}
const char* codeName() {
  switch (code.id) { case 'r': return "repetition"; case 'h': return "Hamming";
                     case 'x': return "ext.Hamming"; case 't': return "2D-product";
                     case 'H': return "Hamming(15,11)"; }
  return "?";
}
uint8_t codeT() { /* корректирующая способность t */
  switch (code.id) { case 'r': return (code.q - 1) / 2; case 'h': return 1;
                     case 'x': return 1; case 't': return 1; case 'H': return 1; }
  return 0;
}
uint8_t codeD() { /* кодовое расстояние d */
  switch (code.id) { case 'r': return code.q; case 'h': return 3; case 'x': return 4;
                     case 't': return 4; case 'H': return 3; }
  return 0;
}

void cmdInfo() {
  uint8_t n = codeN(&code), k = codeK(&code);
  Serial.print("Код: "); Serial.print(codeName());
  Serial.print("  k="); Serial.print(k); Serial.print("  n="); Serial.print(n);
  Serial.print("  R="); Serial.print((float)k / n, 3);
  Serial.print("  L="); Serial.print(1.0f - (float)k / n, 3);
  Serial.print("  d="); Serial.print(codeD());
  Serial.print("  испр. t="); Serial.println(codeT());
}

/* ----- разобрать строку из 0/1 в число (игнорируя пробелы и '|') ----- */
uint32_t parseBits(const String& s, uint8_t& len) {
  uint32_t v = 0; len = 0;
  for (uint16_t i = 0; i < s.length(); i++) {
    char ch = s[i];
    if (ch == '0' || ch == '1') { v = (v << 1) | (ch - '0'); len++; }
  }
  return v;
}

void cmdEnc(uint32_t info) {
  lastInfo = info & fecMask(codeK(&code));
  lastWord = codeEncode(&code, lastInfo);
  Serial.print("сообщение : "); printWord(lastInfo, codeK(&code)); Serial.println();
  Serial.print("кодовое   : "); printWord(lastWord, codeN(&code));
  Serial.print("   вес "); Serial.println(fecWeight(lastWord));
}

void cmdDec(uint32_t word) {
  uint8_t n = codeN(&code);
  DecResult d = codeDecode(&code, word);
  Serial.print("принято   : "); printWord(word, n); Serial.println();
  Serial.print("декодер   : "); Serial.print(statusStr(d.status));
  if (d.status == DEC_CORRECTED) { Serial.print(", позиция "); Serial.print(d.pos); }
  Serial.println();
  Serial.print("сообщение : "); printWord(d.info, codeK(&code)); Serial.println();
}

void cmdFlip(int* pos, int np) {
  uint8_t n = codeN(&code);
  uint32_t e = 0;
  for (int i = 0; i < np; i++) if (pos[i] >= 1 && pos[i] <= n) e ^= (1u << (n - pos[i]));
  uint32_t recv = lastWord ^ e;
  DecResult d = codeDecode(&code, recv);
  uint8_t out = classify(lastInfo, d);
  Serial.print("передано  : "); printWord(lastWord, n); Serial.println();
  Serial.print("ошибки    : "); printWord(e, n); Serial.print("   кратность "); Serial.println(fecWeight(e));
  Serial.print("принято   : "); printWord(recv, n); Serial.println();
  Serial.print("декодер   : "); Serial.println(statusStr(d.status));
  Serial.print("сообщение : "); printWord(d.info, codeK(&code)); Serial.println();
  Serial.print("итог      : ");
  switch (out) {
    case OUT_OK:        Serial.println("принято верно"); break;
    case OUT_CORRECTED: Serial.println("ошибка исправлена"); break;
    case OUT_DETECTED:  Serial.println("ошибка обнаружена, комбинация бракуется (переспрос)"); break;
    case OUT_MISCORRECT:Serial.println("ОШИБКА ДЕКОДИРОВАНИЯ: выдано неверное сообщение"); break;
  }
}

void cmdList() {
  uint8_t k = codeK(&code); uint32_t M = 1u << k;
  if (M > 64) { Serial.println("слишком много комбинаций (> 64)"); return; }
  for (uint32_t m = 0; m < M; m++) {
    printWord(m, k); Serial.print("  ->  "); printWord(codeEncode(&code, m), codeN(&code)); Serial.println();
  }
}

void cmdExp(double p0, uint32_t N) {
  FecStat s; uint32_t t0 = millis();
  fecExperiment(&code, p0, N, &s);
  uint32_t ms = millis() - t0;
  double p0meas = (double)s.bitsErr / ((double)N * codeN(&code));
  Serial.print("код "); Serial.print(codeName());
  Serial.print("  N="); Serial.print(N); Serial.print("  p0="); Serial.print(p0, 4);
  Serial.print(" (изм. "); Serial.print(p0meas, 4); Serial.println(")");
  Serial.print("  верно/исправлено : "); Serial.println(s.nOk + s.nCorr);
  Serial.print("  обнаружено (брак): "); Serial.println(s.nDet);
  Serial.print("  ОШИБОК ДЕКОДИР.  : "); Serial.println(s.nMis);
  Serial.print("  ложных тревог    : "); Serial.println(s.nFalse);
  Serial.print("  P_дек = "); Serial.println((double)s.nMis / N, 8);
  Serial.print("  время, мс = "); Serial.println(ms);
}

void cmdSweep(uint32_t N) {
  const double ps[] = {0.2, 0.1, 0.05, 0.02, 0.01, 0.005, 0.002, 0.001};
  Serial.println("p0;N;nMis;nDet;P_dek;P_det");
  for (uint8_t i = 0; i < 8; i++) {
    FecStat s; fecExperiment(&code, ps[i], N, &s);
    Serial.print(ps[i], 4); Serial.print(';'); Serial.print(N); Serial.print(';');
    Serial.print(s.nMis); Serial.print(';'); Serial.print(s.nDet); Serial.print(';');
    Serial.print((double)s.nMis / N, 8); Serial.print(';');
    Serial.println((double)s.nDet / N, 8);
  }
}

void cmdCmp(double p0, uint32_t N) {
  const char ids[] = {'r', 'h', 'x', 't'};
  CodeCfg save = code;
  Serial.println("код;n;k;R;P_dek;P_det;мс");
  for (uint8_t i = 0; i < 4; i++) {
    code.id = ids[i];
    FecStat s; uint32_t t0 = millis(); fecExperiment(&code, p0, N, &s); uint32_t ms = millis() - t0;
    Serial.print(codeName()); Serial.print(';'); Serial.print(codeN(&code)); Serial.print(';');
    Serial.print(codeK(&code)); Serial.print(';'); Serial.print((float)codeK(&code) / codeN(&code), 3); Serial.print(';');
    Serial.print((double)s.nMis / N, 8); Serial.print(';'); Serial.print((double)s.nDet / N, 8); Serial.print(';');
    Serial.println(ms);
  }
  code = save;
}

/* ---- задание 10: демонстрация перемежения против пакетов ошибок ---- */
void cmdBurst(uint8_t L) {
  if (L == 0 || L > 20) L = 7;
  uint8_t ok0, fl0, ok1, fl1;
  fecInterleaveDemo(L, 0, &ok0, &fl0);
  fecInterleaveDemo(L, 1, &ok1, &fl1);
  Serial.print("Пакет из "); Serial.print(L);
  Serial.println(" подряд идущих ошибок; 7 слов Хэмминга (7,4), глубина 7:");
  Serial.print("  без перемежения : восстановлено "); Serial.print(ok0);
  Serial.print("/7, сбоев "); Serial.println(fl0);
  Serial.print("  с перемежением  : восстановлено "); Serial.print(ok1);
  Serial.print("/7, сбоев "); Serial.println(fl1);
}

void help() {
  Serial.println(F("=== Терминальный стенд ЛР2: коды, исправляющие ошибки ==="));
  Serial.println(F("code r [q]   выбрать код с повторением (кратность q=3 или 5)"));
  Serial.println(F("code h       код Хэмминга (7,4)"));
  Serial.println(F("code x       расширенный код Хэмминга (8,4), SEC-DED"));
  Serial.println(F("code t [a b] двумерный итерационный код (решётка a x b)"));
  Serial.println(F("code H       код Хэмминга (15,11)            [задание 10]"));
  Serial.println(F("burst L      пакет из L ошибок: с перемежением и без [задание 10]"));
  Serial.println(F("info         параметры текущего кода: n, k, R, L, d, t"));
  Serial.println(F("enc 1011     закодировать k информационных бит"));
  Serial.println(F("dec 0110011  декодировать принятую комбинацию из n бит"));
  Serial.println(F("flip 2 5     исказить позиции 2 и 5 последней комбинации и декодировать"));
  Serial.println(F("list         таблица кодовых комбинаций (не более 64)"));
  Serial.println(F("exp 0.05 20000   статистический эксперимент в ДСК"));
  Serial.println(F("sweep 20000  зависимость P_дек(p0) в формате CSV"));
  Serial.println(F("cmp 0.05 20000   эксперимент для всех четырёх кодов"));
  Serial.println(F("var ...      параметры варианта (см. методичку)"));
  Serial.println(F("seed 123     начальное значение генератора"));
}

void handle(String line) {
  line.trim(); if (line.length() == 0) return;
  int sp = line.indexOf(' ');
  String cmd = (sp < 0) ? line : line.substring(0, sp);
  String arg = (sp < 0) ? ""   : line.substring(sp + 1);
  arg.trim();

  if (cmd == "help" || cmd == "?") { help(); }
  else if (cmd == "code") {
    char id = arg.length() ? arg[0] : 'h';
    code.id = id;
    if (id == 'r') { int q = arg.substring(1).toInt(); if (q == 3 || q == 5) code.q = q; }
    if (id == 't') {
      int s2 = arg.indexOf(' '); if (s2 > 0) { code.a = arg.substring(s2 + 1).toInt();
        int s3 = arg.indexOf(' ', s2 + 1); if (s3 > 0) code.b = arg.substring(s3 + 1).toInt(); }
    }
    cmdInfo();
  }
  else if (cmd == "info")  cmdInfo();
  else if (cmd == "enc")   { uint8_t l; cmdEnc(parseBits(arg, l)); }
  else if (cmd == "dec")   { uint8_t l; cmdDec(parseBits(arg, l)); }
  else if (cmd == "flip")  {
    int pos[8], np = 0; int start = 0;
    while (start < (int)arg.length() && np < 8) {
      int s2 = arg.indexOf(' ', start);
      String tok = (s2 < 0) ? arg.substring(start) : arg.substring(start, s2);
      tok.trim(); if (tok.length()) pos[np++] = tok.toInt();
      if (s2 < 0) break; start = s2 + 1;
    }
    cmdFlip(pos, np);
  }
  else if (cmd == "list")  cmdList();
  else if (cmd == "exp")   { int s2 = arg.indexOf(' '); cmdExp(arg.substring(0, s2).toFloat(), arg.substring(s2 + 1).toInt()); }
  else if (cmd == "sweep") cmdSweep(arg.toInt());
  else if (cmd == "cmp")   { int s2 = arg.indexOf(' '); cmdCmp(arg.substring(0, s2).toFloat(), arg.substring(s2 + 1).toInt()); }
  else if (cmd == "burst") cmdBurst(arg.toInt());
  else if (cmd == "seed")  { fecSeed(arg.toInt()); Serial.println("seed set"); }
  else if (cmd == "var")   { Serial.println("параметры варианта приняты (см. методичку, раздел 4)"); }
  else Serial.println("неизвестная команда, наберите help");
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {}
  delay(200);
  help();
  cmdInfo();
}
void loop() {
  static String buf;
  while (Serial.available()) {
    char ch = Serial.read();
    if (ch == '\n' || ch == '\r') { if (buf.length()) { handle(buf); buf = ""; } }
    else buf += ch;
  }
}
