/* ============================================================================
 *  ham_codes.h  —  коды, ИСПРАВЛЯЮЩИЕ ошибки (ЛР N2)
 *  Четыре кода: мажоритарный (повторение), Хэмминга (7,4),
 *  расширенный Хэмминга (8,4) SEC-DED, двумерный итерационный (код-произведение).
 *
 *  Файл без изменений компилируется для Arduino Uno, ESP32 и ПК (g++).
 *  Представление: кодовая комбинация хранится в младших n битах uint32_t,
 *  позиция 1 — САМАЯ ЛЕВАЯ (старший бит, передаётся первой), позиция n — правая.
 *  Доступ к позиции i даёт getBit(word, n, i).
 * ========================================================================== */
#ifndef HAM_CODES_H
#define HAM_CODES_H
#include <stdint.h>

/* ---- результат декодирования ---- */
typedef struct { uint32_t info; uint8_t status; int8_t pos; } DecResult;
enum { DEC_OK = 0, DEC_CORRECTED = 1, DEC_DETECTED = 2 };
/* DEC_OK        — синдром нулевой, ошибок не обнаружено
 * DEC_CORRECTED — ошибка обнаружена и исправлена (pos — исправленная позиция)
 * DEC_DETECTED  — ошибка обнаружена, но не исправлена (кратность вне возможностей кода) */

/* ---- исход приёма (определяется сравнением с переданным сообщением) ---- */
enum { OUT_OK = 0, OUT_CORRECTED = 1, OUT_DETECTED = 2, OUT_MISCORRECT = 3 };
/* OUT_OK         — принято без ошибок
 * OUT_CORRECTED  — ошибки были и исправлены верно
 * OUT_DETECTED   — ошибка обнаружена, комбинация забракована (переспрос)
 * OUT_MISCORRECT — ОШИБКА ДЕКОДИРОВАНИЯ: декодер молча выдал неверное сообщение */

/* ---- описание выбранного кода ---- */
typedef struct {
  char    id;        /* 'r' повторение, 'h' Хэмминг, 'x' расшир., 't' двумерный */
  uint8_t k, n;      /* информационных / всего разрядов */
  uint8_t q;         /* кратность повторения (код 'r') */
  uint8_t a, b;      /* размеры решётки данных (код 't') */
} CodeCfg;

/* ======================= служебные функции (готовы) ======================= */
static inline uint8_t  getBit(uint32_t w, uint8_t n, uint8_t i) { return (w >> (n - i)) & 1u; }
static inline uint32_t setBit(uint32_t w, uint8_t n, uint8_t i, uint8_t v) {
  uint32_t m = 1u << (n - i); return v ? (w | m) : (w & ~m);
}
static inline uint32_t flipBit(uint32_t w, uint8_t n, uint8_t i) { return w ^ (1u << (n - i)); }
static inline uint8_t  fecParity(uint32_t x) { x ^= x >> 16; x ^= x >> 8; x ^= x >> 4; return (0x6996u >> (x & 0xF)) & 1u; }
static inline uint8_t  fecWeight(uint32_t x) { uint8_t c = 0; while (x) { c += x & 1u; x >>= 1; } return c; }
static inline uint32_t fecMask(uint8_t n) { return (n >= 32) ? 0xFFFFFFFFu : ((1u << n) - 1u); }

/* =======================  ЗАДАНИЕ 1. Мажоритарный код  ====================
 * Каждый информационный бит повторяется q раз; декодер — голосование большинством.
 * n = k*q. Исправляет до (q-1)/2 ошибок в каждой группе из q разрядов. */
static uint32_t repEncode(uint32_t info, uint8_t k, uint8_t q) {
  uint32_t out = 0;
  for (uint8_t i = 1; i <= k; i++) {
    uint8_t bit = getBit(info, k, i);
    for (uint8_t j = 0; j < q; j++) out = (out << 1) | bit;
  }
  return out;
}
static DecResult repDecode(uint32_t word, uint8_t k, uint8_t q) {
  DecResult d = {0, DEC_OK, -1}; uint8_t n = k * q, corrected = 0;
  for (uint8_t i = 1; i <= k; i++) {
    uint8_t ones = 0;
    for (uint8_t j = 0; j < q; j++) ones += getBit(word, n, (i - 1) * q + 1 + j);
    uint8_t bit = (ones * 2 > q) ? 1 : 0;          /* большинство (q нечётно) */
    if (ones != 0 && ones != q) corrected = 1;     /* в группе была ошибка */
    d.info = (d.info << 1) | bit;
  }
  d.status = corrected ? DEC_CORRECTED : DEC_OK;
  return d;
}

/* =======================  ЗАДАНИЕ 2. Код Хэмминга (7,4)  ==================
 * Позиции 1..7. Проверочные биты p1,p2,p4 — на позициях 1,2,4;
 * информационные d1..d4 — на позициях 3,5,6,7.
 *   p1 (поз.1) покрывает позиции 1,3,5,7
 *   p2 (поз.2) покрывает позиции 2,3,6,7
 *   p4 (поз.4) покрывает позиции 4,5,6,7
 * Синдром s = (s4 s2 s1) в двоичном виде — НОМЕР искажённой позиции. d = 3, t = 1. */
static uint32_t hamEncode(uint32_t info) {
  uint8_t n = 7; uint32_t w = 0;
  w = setBit(w, n, 3, getBit(info, 4, 1));
  w = setBit(w, n, 5, getBit(info, 4, 2));
  w = setBit(w, n, 6, getBit(info, 4, 3));
  w = setBit(w, n, 7, getBit(info, 4, 4));
  w = setBit(w, n, 1, getBit(w, n, 3) ^ getBit(w, n, 5) ^ getBit(w, n, 7));
  w = setBit(w, n, 2, getBit(w, n, 3) ^ getBit(w, n, 6) ^ getBit(w, n, 7));
  w = setBit(w, n, 4, getBit(w, n, 5) ^ getBit(w, n, 6) ^ getBit(w, n, 7));
  return w;
}
static uint8_t hamExtract(uint32_t w) {                 /* позиции 3,5,6,7 -> 4 бита */
  uint8_t n = 7, info = 0;
  info = (info << 1) | getBit(w, n, 3); info = (info << 1) | getBit(w, n, 5);
  info = (info << 1) | getBit(w, n, 6); info = (info << 1) | getBit(w, n, 7);
  return info;
}
static DecResult hamDecode(uint32_t w) {
  uint8_t n = 7; DecResult r = {0, DEC_OK, -1};
  uint8_t s1 = getBit(w,n,1) ^ getBit(w,n,3) ^ getBit(w,n,5) ^ getBit(w,n,7);
  uint8_t s2 = getBit(w,n,2) ^ getBit(w,n,3) ^ getBit(w,n,6) ^ getBit(w,n,7);
  uint8_t s4 = getBit(w,n,4) ^ getBit(w,n,5) ^ getBit(w,n,6) ^ getBit(w,n,7);
  uint8_t syn = s1 | (s2 << 1) | (s4 << 2);
  if (syn) { w = flipBit(w, n, syn); r.status = DEC_CORRECTED; r.pos = syn; }
  r.info = hamExtract(w);
  return r;
}

/* ================  ЗАДАНИЕ 3. Расширенный Хэмминг (8,4), SEC-DED  =========
 * К коду (7,4) добавлен 8-й разряд — общая чётность всех семи. d = 4.
 * Правило декодирования по синдрому syn и общей чётности c всех 8 разрядов:
 *   c=1, syn!=0 -> одиночная ошибка на позиции syn — ИСПРАВИТЬ
 *   c=1, syn=0  -> ошибка в самом разряде общей чётности (поз.8) — ИСПРАВИТЬ
 *   c=0, syn!=0 -> двойная ошибка — ОБНАРУЖЕНА, не исправляется
 *   c=0, syn=0  -> ошибок нет */
static uint32_t extEncode(uint32_t info) {
  uint8_t n = 8; uint32_t h = hamEncode(info); uint32_t w = h << 1;   /* поз.1..7 */
  w = setBit(w, n, 8, fecParity(h));                                  /* поз.8 */
  return w;
}
static DecResult extDecode(uint32_t w) {
  uint8_t n = 8; DecResult r = {0, DEC_OK, -1};
  uint8_t s1 = getBit(w,n,1) ^ getBit(w,n,3) ^ getBit(w,n,5) ^ getBit(w,n,7);
  uint8_t s2 = getBit(w,n,2) ^ getBit(w,n,3) ^ getBit(w,n,6) ^ getBit(w,n,7);
  uint8_t s4 = getBit(w,n,4) ^ getBit(w,n,5) ^ getBit(w,n,6) ^ getBit(w,n,7);
  uint8_t syn = s1 | (s2 << 1) | (s4 << 2);
  uint8_t c = fecParity(w & 0xFFu);
  if (c) { if (syn) w = flipBit(w, n, syn); else w = flipBit(w, n, 8);
           r.status = DEC_CORRECTED; r.pos = syn ? syn : 8; }
  else   { if (syn) r.status = DEC_DETECTED; else r.status = DEC_OK; }
  r.info = (getBit(w,n,3) << 3) | (getBit(w,n,5) << 2) | (getBit(w,n,6) << 1) | getBit(w,n,7);
  return r;
}

/* ================  ЗАДАНИЕ 4. Двумерный итерационный код  ================
 * Данные a*b бит укладываются в решётку a x b; добавляются чётности
 * по строкам и столбцам и угловая чётность. n = (a+1)(b+1), d = 4, t = 1.
 * Декодер: искажённая строка (одна) и столбец (один) задают позицию ошибки
 * на пересечении; иначе ошибка только обнаруживается. */
static uint32_t twoDEncode(uint32_t info, uint8_t a, uint8_t b) {
  uint8_t R = a + 1, C = b + 1; uint8_t g[8][8]; uint32_t w = 0;
  for (uint8_t r = 0; r < R; r++) for (uint8_t c = 0; c < C; c++) g[r][c] = 0;
  for (uint8_t r = 0; r < a; r++) for (uint8_t c = 0; c < b; c++) g[r][c] = getBit(info, a*b, r*b + c + 1);
  for (uint8_t r = 0; r < a; r++) { uint8_t p = 0; for (uint8_t c = 0; c < b; c++) p ^= g[r][c]; g[r][b] = p; }
  for (uint8_t c = 0; c < b; c++) { uint8_t p = 0; for (uint8_t r = 0; r < a; r++) p ^= g[r][c]; g[a][c] = p; }
  { uint8_t p = 0; for (uint8_t c = 0; c < b; c++) p ^= g[a][c]; g[a][b] = p; }
  for (uint8_t r = 0; r < R; r++) for (uint8_t c = 0; c < C; c++) w = (w << 1) | g[r][c];
  return w;
}
static DecResult twoDDecode(uint32_t word, uint8_t a, uint8_t b) {
  uint8_t R = a + 1, C = b + 1, n = R * C; DecResult res = {0, DEC_OK, -1}; uint8_t g[8][8];
  for (uint8_t r = 0; r < R; r++) for (uint8_t c = 0; c < C; c++) g[r][c] = getBit(word, n, r*C + c + 1);
  uint8_t nr = 0, nc = 0, rr = 0, cc = 0;
  for (uint8_t r = 0; r < R; r++) { uint8_t p = 0; for (uint8_t c = 0; c < C; c++) p ^= g[r][c]; if (p) { nr++; rr = r; } }
  for (uint8_t c = 0; c < C; c++) { uint8_t p = 0; for (uint8_t r = 0; r < R; r++) p ^= g[r][c]; if (p) { nc++; cc = c; } }
  if (nr == 0 && nc == 0) res.status = DEC_OK;
  else if (nr == 1 && nc == 1) { g[rr][cc] ^= 1; res.status = DEC_CORRECTED; res.pos = rr*C + cc + 1; }
  else res.status = DEC_DETECTED;
  uint32_t info = 0;
  for (uint8_t r = 0; r < a; r++) for (uint8_t c = 0; c < b; c++) info = (info << 1) | g[r][c];
  res.info = info;
  return res;
}

/* ===============  ЗАДАНИЕ 10 (доп.). Код Хэмминга (15,11)  ===============
 * Обобщённый код Хэмминга: n = 2ʳ − 1, k = n − r. Проверочные биты — на
 * позициях-степенях двойки (1,2,4,8,…), информационные — на остальных.
 * При r = 3 получается код (7,4), при r = 4 — код (15,11). Синдром из r бит —
 * двоичный номер искажённой позиции. */
static inline uint8_t fecIsPow2(uint8_t p) { return p && ((p & (p - 1)) == 0); }
static uint32_t hammingEncode(uint32_t info, uint8_t r) {
  uint8_t n = (1u << r) - 1, k = n - r, di = 1; uint32_t w = 0;
  for (uint8_t p = 1; p <= n; p++) { if (fecIsPow2(p)) continue; w = setBit(w, n, p, getBit(info, k, di)); di++; }
  for (uint8_t j = 0; j < r; j++) { uint8_t pp = 1u << j, par = 0;
    for (uint8_t p = 1; p <= n; p++) { if (p == pp) continue; if (p & pp) par ^= getBit(w, n, p); }
    w = setBit(w, n, pp, par); }
  return w;
}
static DecResult hammingDecode(uint32_t w, uint8_t r) {
  uint8_t n = (1u << r) - 1; DecResult d = {0, DEC_OK, -1}; uint8_t s = 0;
  for (uint8_t j = 0; j < r; j++) { uint8_t pp = 1u << j, par = 0;
    for (uint8_t p = 1; p <= n; p++) if (p & pp) par ^= getBit(w, n, p);
    s |= par << j; }
  if (s) { w = flipBit(w, n, s); d.status = DEC_CORRECTED; d.pos = s; }
  uint32_t info = 0;
  for (uint8_t p = 1; p <= n; p++) if (!fecIsPow2(p)) info = (info << 1) | getBit(w, n, p);
  d.info = info;
  return d;
}

/* ===============  ЗАДАНИЕ 10 (доп.). Перемежение (interleaving)  =========
 * Семь кодовых слов Хэмминга (7,4) записываются строками матрицы 7×7 и
 * передаются по столбцам (глубина перемежения 7). Пакет из L подряд идущих
 * ошибок в канале «размазывается» по разным словам: при L ≤ 7 на каждое слово
 * приходится не более одной ошибки, и код с t = 1 исправляет все семь слов.
 * Без перемежения тот же пакет сосредоточен в одном-двух словах и ломает их. */
static void fecInterleaveDemo(uint8_t L, uint8_t interleave, uint8_t* corrected, uint8_t* failed) {
  uint32_t cw[7]; for (uint8_t i = 0; i < 7; i++) cw[i] = hamEncode(i & 0xF);
  uint8_t stream[49];
  for (uint8_t i = 0; i < 7; i++) for (uint8_t j = 0; j < 7; j++) {
    uint8_t idx = interleave ? (j * 7 + i) : (i * 7 + j);
    stream[idx] = getBit(cw[i], 7, j + 1);
  }
  for (uint8_t b = 0; b < L && (10 + b) < 49; b++) stream[10 + b] ^= 1;   /* пакет ошибок */
  uint8_t ok = 0, fl = 0;
  for (uint8_t i = 0; i < 7; i++) {
    uint32_t r = 0;
    for (uint8_t j = 0; j < 7; j++) { uint8_t idx = interleave ? (j * 7 + i) : (i * 7 + j); r = (r << 1) | stream[idx]; }
    if (hamDecode(r).info == (uint32_t)(i & 0xF)) ok++; else fl++;
  }
  *corrected = ok; *failed = fl;
}

/* =====================  диспетчер по выбранному коду  ===================== */
static uint8_t codeK(const CodeCfg* cfg) {
  if (cfg->id == 't') return cfg->a * cfg->b;
  if (cfg->id == 'H') return 11;
  return cfg->k;
}
static uint8_t codeN(const CodeCfg* cfg) {
  switch (cfg->id) {
    case 'r': return cfg->k * cfg->q;
    case 'h': return 7;
    case 'x': return 8;
    case 't': return (cfg->a + 1) * (cfg->b + 1);
    case 'H': return 15;
  }
  return 0;
}
static uint32_t codeEncode(const CodeCfg* cfg, uint32_t info) {
  switch (cfg->id) {
    case 'r': return repEncode(info, cfg->k, cfg->q);
    case 'h': return hamEncode(info);
    case 'x': return extEncode(info);
    case 't': return twoDEncode(info, cfg->a, cfg->b);
    case 'H': return hammingEncode(info, 4);
  }
  return 0;
}
static DecResult codeDecode(const CodeCfg* cfg, uint32_t word) {
  DecResult z = {0, DEC_OK, -1};
  switch (cfg->id) {
    case 'r': return repDecode(word, cfg->k, cfg->q);
    case 'h': return hamDecode(word);
    case 'x': return extDecode(word);
    case 't': return twoDDecode(word, cfg->a, cfg->b);
    case 'H': return hammingDecode(word, 4);
  }
  return z;
}

/* =====================  классификация исхода приёма  ===================== */
static uint8_t classify(uint32_t sent_info, DecResult d) {
  if (d.status == DEC_DETECTED) return OUT_DETECTED;
  if (d.info == sent_info)      return (d.status == DEC_CORRECTED) ? OUT_CORRECTED : OUT_OK;
  return OUT_MISCORRECT;
}

/* ==========================  генератор ДСК (xorshift32)  ================== */
static uint32_t g_rng = 2463534242u;
static inline void     fecSeed(uint32_t s) { g_rng = s ? s : 1u; }
static inline uint32_t fecRand(void) { uint32_t x = g_rng; x ^= x << 13; x ^= x >> 17; x ^= x << 5; return g_rng = x; }
/* вектор ошибок: каждый из n разрядов искажается независимо с вероятностью p0 */
static uint32_t fecErrorVector(uint8_t n, double p0) {
  uint32_t e = 0, thr = (uint32_t)(p0 * 4294967296.0);
  for (uint8_t i = 0; i < n; i++) { e <<= 1; if (fecRand() < thr) e |= 1u; }
  return e;
}

/* ==========================  статистический эксперимент  ================= */
typedef struct {
  uint32_t N, nOk, nCorr, nDet, nMis, nFalse; /* nFalse: e=0, а декодер изменил слово (ошибка в программе) */
  uint32_t bitsErr;                            /* суммарное число искажённых разрядов канала */
} FecStat;
static void fecExperiment(const CodeCfg* cfg, double p0, uint32_t N, FecStat* st) {
  uint8_t n = codeN(cfg); uint32_t kmask = fecMask(codeK(cfg));
  st->N = N; st->nOk = st->nCorr = st->nDet = st->nMis = st->nFalse = 0; st->bitsErr = 0;
  for (uint32_t i = 0; i < N; i++) {
    uint32_t info = fecRand() & kmask;
    uint32_t x = codeEncode(cfg, info);
    uint32_t e = fecErrorVector(n, p0);
    st->bitsErr += fecWeight(e);
    DecResult d = codeDecode(cfg, x ^ e);
    uint8_t o = classify(info, d);
    if (e == 0 && d.info != info) st->nFalse++;
    switch (o) { case OUT_OK: st->nOk++; break; case OUT_CORRECTED: st->nCorr++; break;
                 case OUT_DETECTED: st->nDet++; break; default: st->nMis++; }
  }
}

#endif /* HAM_CODES_H */
