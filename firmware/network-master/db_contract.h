#pragma once

// Pure transport-contract helpers for the network master's Supabase/PostgREST
// path. Nothing here touches Arduino, WiFi or the heap, so the same code is
// compiled into the firmware and into the native `db-contract` test env
// (firmware/tests/db-contract/). Keeping it header-only means the test cannot
// drift from what ships.
//
// Contract summary (docs/llm-generated/network-master-database-plan.md §3):
//   - push_readings takes ONE named argument, so the body is
//     {"p_readings":[...]}, never a bare array.
//   - timestamps are UTC "YYYY-MM-DDTHH:MM:SSZ" at sample time.
//   - null sensor values are JSON null, which is how a present bottle reports
//     an unavailable sensor.
//   - the RPC answer is a bare inserted-row count ("4"), which a replay can
//     legitimately return as 0.

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace dbc {

// Sensor null bits. A set bit means "this sensor produced no value for a
// bottle that was otherwise present".
constexpr uint8_t kNullPh = 0x01;
constexpr uint8_t kNullPressure = 0x02;
constexpr uint8_t kNullTemp = 0x04;
constexpr uint8_t kNullCo2 = 0x08;
constexpr uint8_t kNullCh4 = 0x10;
constexpr uint8_t kNullAll = 0x1F;

// One bottle as delivered by the Mega frame parser. bottleId is the 0-based
// database id (Mega bottles 1-4 map to 0-3).
struct BottleSample {
  uint8_t bottleId;
  float ph;
  float pressure;
  float tempC;
  float co2;
  float ch4;
  uint8_t nullMask;
};

// One immutable queued row. experimentId is always >= 0 for real uploads; a
// negative value means "omit the key", which the SQL would resolve to the
// highest configured experiment. The firmware never uses that fallback for
// collected samples because it would mislabel an outage spanning a run change.
struct PushRow {
  uint8_t bottleId;
  int32_t experimentId;
  int64_t sampleUnix;
  float ph;
  float pressure;
  float tempC;
  float co2;
  float ch4;
  uint8_t nullMask;
};

// True for values that can be serialised as a JSON number. NaN and infinities
// are rejected because JSON has no representation for them and the server's
// `::real` cast would accept a NaN silently.
inline bool finiteValue(float v) { return std::isfinite(v); }

// Clears null bits for fields that hold NaN/infinity, so a broken sensor is
// reported as null instead of poisoning the round. Returns how many fields
// were converted.
inline uint8_t sanitizeNonFinite(BottleSample& s) {
  uint8_t fixed = 0;
  if (!(s.nullMask & kNullPh) && !finiteValue(s.ph)) {
    s.nullMask |= kNullPh;
    s.ph = 0.0f;
    fixed++;
  }
  if (!(s.nullMask & kNullPressure) && !finiteValue(s.pressure)) {
    s.nullMask |= kNullPressure;
    s.pressure = 0.0f;
    fixed++;
  }
  if (!(s.nullMask & kNullTemp) && !finiteValue(s.tempC)) {
    s.nullMask |= kNullTemp;
    s.tempC = 0.0f;
    fixed++;
  }
  if (!(s.nullMask & kNullCo2) && !finiteValue(s.co2)) {
    s.nullMask |= kNullCo2;
    s.co2 = 0.0f;
    fixed++;
  }
  if (!(s.nullMask & kNullCh4) && !finiteValue(s.ch4)) {
    s.nullMask |= kNullCh4;
    s.ch4 = 0.0f;
    fixed++;
  }
  return fixed;
}

// --- calendar arithmetic ----------------------------------------------------
// Howard Hinnant's days_from_civil / civil_from_days. Used instead of
// gmtime_r/timegm so the wire timestamp is identical on host and ESP32 and
// independent of libc timezone state.

inline int64_t daysFromCivil(int64_t y, unsigned m, unsigned d) {
  y -= m <= 2;
  const int64_t era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = (unsigned)(y - era * 400);            // [0, 399]
  const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;  // [0, 365]
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;           // [0, 146096]
  return era * 146097 + (int64_t)doe - 719468;
}

inline void civilFromDays(int64_t z, int& y, unsigned& m, unsigned& d) {
  z += 719468;
  const int64_t era = (z >= 0 ? z : z - 146096) / 146097;
  const unsigned doe = (unsigned)(z - era * 146097);                      // [0, 146096]
  const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  const int64_t yr = (int64_t)yoe + era * 400;
  const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);           // [0, 365]
  const unsigned mp = (5 * doy + 2) / 153;                                // [0, 11]
  d = doy - (153 * mp + 2) / 5 + 1;                                       // [1, 31]
  m = mp < 10 ? mp + 3 : mp - 9;                                          // [1, 12]
  y = (int)(yr + (m <= 2));
}

// Formats "YYYY-MM-DDTHH:MM:SSZ" (always 20 characters + NUL). Returns false
// when the value is outside 1970..9999 or the buffer is too small.
inline bool formatUtcTimestamp(int64_t unixSeconds, char* out, size_t cap) {
  if (out == nullptr || cap < 21) return false;
  int64_t days = unixSeconds / 86400;
  int64_t secs = unixSeconds % 86400;
  if (secs < 0) {
    secs += 86400;
    days -= 1;
  }
  int y = 0;
  unsigned m = 0, d = 0;
  civilFromDays(days, y, m, d);
  if (y < 1970 || y > 9999) return false;
  int n = std::snprintf(out, cap, "%04d-%02u-%02uT%02u:%02u:%02uZ", y, m, d,
                        (unsigned)(secs / 3600), (unsigned)((secs % 3600) / 60),
                        (unsigned)(secs % 60));
  return n == 20;
}

// Parses an ISO-8601 timestamp that MUST carry an explicit zone ("Z" or
// "+HH:MM"/"-HH:MM"). Postgres returns e.g. "2026-11-10T23:19:45.123456+00:00".
// Fractional seconds are accepted and ignored. Rejects anything outside
// 2000..2100 as a clock/sanity error.
inline bool parseIsoUtc(const char* s, uint32_t& outEpoch) {
  if (s == nullptr) return false;
  int Y = 0, M = 0, D = 0, h = 0, mi = 0, se = 0, consumed = 0;
  if (std::sscanf(s, "%4d-%2d-%2dT%2d:%2d:%2d%n", &Y, &M, &D, &h, &mi, &se,
                  &consumed) != 6) {
    return false;
  }
  if (M < 1 || M > 12 || D < 1 || D > 31 || h < 0 || h > 23 || mi < 0 ||
      mi > 59 || se < 0 || se > 60) {
    return false;
  }
  const char* p = s + consumed;
  if (*p == '.') {
    ++p;
    while (*p >= '0' && *p <= '9') ++p;
  }
  int64_t offset = 0;
  if (*p == 'Z' || *p == 'z') {
    ++p;
  } else if (*p == '+' || *p == '-') {
    const int sign = (*p == '-') ? -1 : 1;
    int oh = 0, om = 0;
    int used = 0;
    if (std::sscanf(p + 1, "%2d:%2d%n", &oh, &om, &used) != 2) return false;
    if (oh < 0 || oh > 23 || om < 0 || om > 59) return false;
    offset = sign * (oh * 3600 + om * 60);
    p += 1 + used;
  } else {
    return false;  // no zone: refuse rather than guess
  }
  if (*p != '\0') return false;

  int64_t t = daysFromCivil(Y, (unsigned)M, (unsigned)D) * 86400 +
              h * 3600 + mi * 60 + se - offset;
  if (t < 946684800LL || t > 4102444800LL) return false;  // 2000..2100
  outEpoch = (uint32_t)t;
  return true;
}

// --- payload formatting -----------------------------------------------------

inline void formatValue(char* out, size_t cap, float v, bool isNull) {
  if (cap == 0) return;
  if (isNull) {
    std::snprintf(out, cap, "null");
  } else {
    std::snprintf(out, cap, "%.6g", (double)v);
  }
}

// Formats one round as {"p_readings":[ ... ]} in `out`. Rows may have distinct
// timestamps but are normally one collection round. Returns bytes written
// (excluding NUL) or 0 when the buffer is too small for a complete envelope.
// A partial envelope is never returned: a truncated body must not be posted.
inline size_t formatPushEnvelope(const PushRow* rows, size_t n, char* out,
                                 size_t cap) {
  if (rows == nullptr || out == nullptr || cap == 0 || n > 24) return 0;
  size_t pos = 0;
  int w = std::snprintf(out, cap, "{\"p_readings\":[");
  if (w <= 0 || (size_t)w >= cap) return 0;
  pos = (size_t)w;
  for (size_t k = 0; k < n; ++k) {
    const PushRow& r = rows[k];
    char ts[24];
    if (!formatUtcTimestamp(r.sampleUnix, ts, sizeof(ts))) return 0;
    char ph[16], pr[16], te[16], co[16], ch[16];
    formatValue(ph, sizeof(ph), r.ph, (r.nullMask & kNullPh) != 0);
    formatValue(pr, sizeof(pr), r.pressure, (r.nullMask & kNullPressure) != 0);
    formatValue(te, sizeof(te), r.tempC, (r.nullMask & kNullTemp) != 0);
    formatValue(co, sizeof(co), r.co2, (r.nullMask & kNullCo2) != 0);
    formatValue(ch, sizeof(ch), r.ch4, (r.nullMask & kNullCh4) != 0);
    char obj[256];
    if (r.experimentId < 0) {
      w = std::snprintf(obj, sizeof(obj),
                        "%s{\"bottle_id\":%u,\"timestamp\":\"%s\",\"ph\":%s,"
                        "\"pressure\":%s,\"temp\":%s,\"co2\":%s,\"ch4\":%s}",
                        k ? "," : "", (unsigned)r.bottleId, ts, ph, pr, te, co, ch);
    } else {
      w = std::snprintf(obj, sizeof(obj),
                        "%s{\"bottle_id\":%u,\"experiment_id\":%ld,"
                        "\"timestamp\":\"%s\",\"ph\":%s,\"pressure\":%s,"
                        "\"temp\":%s,\"co2\":%s,\"ch4\":%s}",
                        k ? "," : "", (unsigned)r.bottleId, (long)r.experimentId,
                        ts, ph, pr, te, co, ch);
    }
    if (w <= 0 || (size_t)w >= sizeof(obj)) return 0;
    if (pos + (size_t)w + 3 > cap) return 0;  // "]}" + NUL
    std::memcpy(out + pos, obj, (size_t)w);
    pos += (size_t)w;
  }
  w = std::snprintf(out + pos, cap - pos, "]}");
  if (w != 2) return 0;
  return pos + 2;
}

// --- response parsing -------------------------------------------------------

inline const char* skipWs(const char* p, const char* end) {
  while (p < end &&
         (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')) {
    ++p;
  }
  return p;
}

// Locates "key":<value> inside [begin,end) and returns the value span
// (leading whitespace removed, exclusive end at a top-level ',' or closing
// bracket). String and nested-object contents are skipped, so a key name
// inside a string value cannot match.
inline bool findField(const char* begin, const char* end, const char* key,
                      const char*& vb, const char*& ve) {
  if (begin == nullptr || end == nullptr || key == nullptr || begin >= end) {
    return false;
  }
  const size_t klen = std::strlen(key);
  const char* p = begin;
  while (p < end) {
    const char* q = (const char*)std::memchr(p, '"', (size_t)(end - p));
    if (q == nullptr) return false;
    const char* ke = q + 1;
    while (ke < end && *ke != '"') {
      if (*ke == '\\' && ke + 1 < end) ++ke;
      ++ke;
    }
    if (ke >= end) return false;
    const bool match =
        (size_t)(ke - q - 1) == klen && std::memcmp(q + 1, key, klen) == 0;
    const char* after = skipWs(ke + 1, end);
    if (match && after < end && *after == ':') {
      const char* v = skipWs(after + 1, end);
      const char* e = v;
      int depth = 0;
      bool instr = false;
      while (e < end) {
        const char c = *e;
        if (instr) {
          if (c == '\\' && e + 1 < end) {
            e += 2;
            continue;
          }
          if (c == '"') instr = false;
        } else if (c == '"') {
          instr = true;
        } else if (c == '{' || c == '[') {
          ++depth;
        } else if (c == '}' || c == ']') {
          if (depth == 0) break;
          --depth;
        } else if (c == ',' && depth == 0) {
          break;
        }
        ++e;
      }
      vb = v;
      ve = e;
      return true;
    }
    p = ke + 1;
  }
  return false;
}

// Parses a JSON string span into a bounded NUL-terminated buffer. Returns
// false for a non-string (including null) or an unterminated value.
inline bool parseString(const char* vb, const char* ve, char* out, size_t cap) {
  if (out == nullptr || cap == 0) return false;
  out[0] = '\0';
  const char* p = skipWs(vb, ve);
  if (p >= ve || *p != '"') return false;
  ++p;
  size_t o = 0;
  while (p < ve && *p != '"') {
    char c = *p++;
    if (c == '\\' && p < ve) {
      const char esc = *p++;
      switch (esc) {
        case 'n': c = '\n'; break;
        case 'r': c = '\r'; break;
        case 't': c = '\t'; break;
        case 'b': c = '\b'; break;
        case 'f': c = '\f'; break;
        case 'u':
          // Owner names may carry non-ASCII escapes. The firmware only logs
          // these, so replacing each \uXXXX with '?' is honest and bounded.
          for (int i = 0; i < 4 && p < ve; ++i) ++p;
          c = '?';
          break;
        default: c = esc; break;
      }
    }
    if (o + 1 < cap) out[o++] = c;
  }
  if (p >= ve || *p != '"') return false;
  out[o] = '\0';
  return true;
}

// Parses an integer span ("5", "-1"). Anything else (null, floats, strings,
// trailing junk) fails so a caller keeps its default.
inline bool parseInt(const char* vb, const char* ve, int32_t& out) {
  const char* p = skipWs(vb, ve);
  bool neg = false;
  if (p < ve && (*p == '-' || *p == '+')) {
    neg = (*p == '-');
    ++p;
  }
  if (p >= ve || *p < '0' || *p > '9') return false;
  int64_t v = 0;
  while (p < ve && *p >= '0' && *p <= '9') {
    v = v * 10 + (*p - '0');
    if (v > 2147483647LL) return false;
    ++p;
  }
  p = skipWs(p, ve);
  if (p != ve) return false;
  out = (int32_t)(neg ? -v : v);
  return true;
}

// Copies a raw span (e.g. the setpoints object) into a bounded buffer.
inline bool parseRaw(const char* vb, const char* ve, char* out, size_t cap) {
  if (out == nullptr || cap == 0) return false;
  const char* p = skipWs(vb, ve);
  const char* e = ve;
  while (e > p && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r' ||
                   e[-1] == '\n')) {
    --e;
  }
  size_t len = (size_t)(e - p);
  if (len >= cap) len = cap - 1;
  std::memcpy(out, p, len);
  out[len] = '\0';
  return true;
}

// Iterates the top-level objects of a JSON array. cursor starts at 0.
// ob..oe is the object span, braces included.
inline bool nextObject(const char* body, size_t len, size_t& cursor,
                       const char*& ob, const char*& oe) {
  if (body == nullptr) return false;
  const char* end = body + len;
  const char* p = body + cursor;
  while (p < end) {
    p = skipWs(p, end);
    if (p >= end) return false;
    if (*p != '{') {
      ++p;
      continue;
    }
    const char* s = p;
    int depth = 0;
    bool instr = false;
    while (p < end) {
      const char c = *p;
      if (instr) {
        if (c == '\\' && p + 1 < end) {
          p += 2;
          continue;
        }
        if (c == '"') instr = false;
      } else if (c == '"') {
        instr = true;
      } else if (c == '{') {
        ++depth;
      } else if (c == '}') {
        --depth;
        if (depth == 0) {
          ++p;
          ob = s;
          oe = p;
          cursor = (size_t)(p - body);
          return true;
        }
      }
      ++p;
    }
    return false;
  }
  return false;
}

// Validates the scalar acknowledgement of push_readings: a bare non-negative
// integer. A replay may answer 0, so 0 is valid; a JSON body, an error object
// or a float is not.
inline bool parseScalarCount(const char* text, size_t len, long& out) {
  if (text == nullptr) return false;
  const char* end = text + len;
  const char* p = skipWs(text, end);
  if (p >= end || *p < '0' || *p > '9') return false;
  long v = 0;
  while (p < end && *p >= '0' && *p <= '9') {
    if (v > 210000000L) return false;
    v = v * 10 + (*p - '0');
    ++p;
  }
  p = skipWs(p, end);
  if (p != end) return false;
  out = v;
  return true;
}

// Extracts the PostgreSQL SQLSTATE from a PostgREST error body
// ({"code":"28000","message":"...","details":null,...}). Returns "" when the
// body carries no code.
inline bool parseErrorCode(const char* body, size_t len, char* out, size_t cap) {
  if (body == nullptr || out == nullptr || cap == 0) return false;
  out[0] = '\0';
  const char* end = body + len;
  const char* vb = nullptr;
  const char* ve = nullptr;
  if (!findField(body, end, "code", vb, ve)) return false;
  return parseString(vb, ve, out, cap);
}

}  // namespace dbc
