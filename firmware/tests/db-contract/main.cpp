// Host test for the network master's PostgREST/payload contract.
//
// Run: pio run -e db-contract -t exec
//
// This compiles firmware/network-master/db_contract.h unchanged, so it tests
// the exact parsing and formatting code that ships. It covers the contract
// cases from docs/plans-and-audits/network-master-database-plan.md §6 that do not
// need a server: named p_readings envelope, sparse bottle ids, null vs absent
// sensors, non-finite rejection, UTC timestamps, scalar acknowledgement and
// PostgREST error codes.

#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>

#include "db_contract.h"

static int g_failures = 0;
static int g_checks = 0;

#define CHECK(cond)                                                       \
  do {                                                                    \
    ++g_checks;                                                           \
    if (!(cond)) {                                                        \
      ++g_failures;                                                       \
      std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);         \
    }                                                                     \
  } while (0)

static bool contains(const char* hay, const char* needle) {
  return hay != nullptr && std::strstr(hay, needle) != nullptr;
}

static void test_timestamps() {
  char buf[24];
  CHECK(dbc::formatUtcTimestamp(1735689600LL, buf, sizeof(buf)));
  CHECK(std::strcmp(buf, "2025-01-01T00:00:00Z") == 0);
  CHECK(dbc::formatUtcTimestamp(1700000000LL, buf, sizeof(buf)));
  CHECK(std::strcmp(buf, "2023-11-14T22:13:20Z") == 0);
  // Leap day, and a value just before midnight (no day borrow).
  CHECK(dbc::formatUtcTimestamp(1709164800LL, buf, sizeof(buf)));
  CHECK(std::strcmp(buf, "2024-02-29T00:00:00Z") == 0);
  CHECK(!dbc::formatUtcTimestamp(1735689600LL, buf, 20));  // too small

  uint32_t epoch = 0;
  CHECK(dbc::parseIsoUtc("2026-11-10T23:19:45Z", epoch));
  CHECK(epoch == 1794352785u);
  CHECK(dbc::parseIsoUtc("2026-11-10T23:19:45.123456+00:00", epoch));
  CHECK(epoch == 1794352785u);
  CHECK(dbc::parseIsoUtc("2026-11-10T23:19:45+02:00", epoch));
  CHECK(epoch == 1794345585u);
  // No zone, wrong shape and out-of-range dates are refused.
  CHECK(!dbc::parseIsoUtc("2026-11-10T23:19:45", epoch));
  CHECK(!dbc::parseIsoUtc("not-a-date", epoch));
  CHECK(!dbc::parseIsoUtc("1999-12-31T23:59:59Z", epoch));
  CHECK(!dbc::parseIsoUtc("2026-13-01T00:00:00Z", epoch));
}

static void test_envelope() {
  dbc::PushRow rows[2] = {};
  // Bottles 1 and 3 only: ids 0 and 2, with no fabricated id 1.
  rows[0].bottleId = 0;
  rows[0].experimentId = 7;
  rows[0].sampleUnix = 1794352785LL;
  rows[0].ph = 6.8125f;
  rows[0].pressure = 981.0f;  // firmware source unit, no conversion
  rows[0].tempC = 39.0f;
  rows[0].co2 = 1200.0f;
  rows[0].ch4 = 0.0f;
  rows[0].nullMask = dbc::kNullCh4;  // present bottle, unavailable sensor

  rows[1].bottleId = 2;
  rows[1].experimentId = 7;
  rows[1].sampleUnix = 1794352785LL;
  rows[1].ph = 7.0f;
  rows[1].pressure = 984.5f;
  rows[1].tempC = 38.5f;
  rows[1].co2 = 1190.0f;
  rows[1].ch4 = 12.5f;
  rows[1].nullMask = 0;

  char out[1024];
  const size_t n = dbc::formatPushEnvelope(rows, 2, out, sizeof(out));
  CHECK(n > 0);
  CHECK(n == std::strlen(out));
  CHECK(contains(out, "{\"p_readings\":["));
  CHECK(out[n - 1] == '}' && out[n - 2] == ']');
  CHECK(contains(out, "\"bottle_id\":0"));
  CHECK(contains(out, "\"bottle_id\":2"));
  CHECK(!contains(out, "\"bottle_id\":1"));
  CHECK(contains(out, "\"experiment_id\":7"));
  CHECK(contains(out, "\"timestamp\":\"2026-11-10T23:19:45Z\""));
  CHECK(contains(out, "\"pressure\":981"));
  CHECK(contains(out, "\"ch4\":null"));
  CHECK(!contains(out, "nan") && !contains(out, "inf"));

  // A row without an experiment id omits the key (the SQL fallback is never
  // used for collected samples, but the formatter must not invent one).
  rows[0].experimentId = -1;
  CHECK(dbc::formatPushEnvelope(rows, 1, out, sizeof(out)) > 0);
  CHECK(!contains(out, "experiment_id"));

  // A buffer that cannot hold the complete envelope returns 0 and never a
  // truncated body that would be posted as if it were valid.
  rows[0].experimentId = 7;
  char tight[80];
  CHECK(dbc::formatPushEnvelope(rows, 2, tight, sizeof(tight)) == 0);
  CHECK(dbc::formatPushEnvelope(rows, 25, out, sizeof(out)) == 0);
}

static void test_ack_and_errors() {
  long n = -1;
  CHECK(dbc::parseScalarCount("4", 1, n) && n == 4);
  CHECK(dbc::parseScalarCount(" 0 \n", 4, n) && n == 0);
  CHECK(dbc::parseScalarCount("24", 2, n) && n == 24);
  CHECK(!dbc::parseScalarCount("", 0, n));
  CHECK(!dbc::parseScalarCount("abc", 3, n));
  CHECK(!dbc::parseScalarCount("4.0", 3, n));
  CHECK(!dbc::parseScalarCount("-1", 2, n));
  CHECK(!dbc::parseScalarCount("4,5", 3, n));
  CHECK(!dbc::parseScalarCount("{\"count\":4}", 11, n));

  char code[8];
  const char* auth = "{\"code\":\"28000\",\"message\":\"No live device session.\"}";
  CHECK(dbc::parseErrorCode(auth, std::strlen(auth), code, sizeof(code)));
  CHECK(std::strcmp(code, "28000") == 0);
  const char* payload = "{\"code\":\"22023\",\"message\":\"bottle_id 9 out of range\"}";
  CHECK(dbc::parseErrorCode(payload, std::strlen(payload), code, sizeof(code)));
  CHECK(std::strcmp(code, "22023") == 0);
  const char* nocode = "{\"message\":\"plain\"}";
  CHECK(!dbc::parseErrorCode(nocode, std::strlen(nocode), code, sizeof(code)));
}

static void test_field_parsing() {
  const char* owner = "{\"owner_name\":\"Rumen lab\",\"bottle_count\":4}";
  const char* end = owner + std::strlen(owner);
  const char* vb = nullptr;
  const char* ve = nullptr;
  char text[32];
  int32_t iv = -1;
  CHECK(dbc::findField(owner, end, "owner_name", vb, ve));
  CHECK(dbc::parseString(vb, ve, text, sizeof(text)));
  CHECK(std::strcmp(text, "Rumen lab") == 0);
  CHECK(dbc::findField(owner, end, "bottle_count", vb, ve));
  CHECK(dbc::parseInt(vb, ve, iv) && iv == 4);

  // A decoy key inside a string value must not win.
  const char* decoy = "{\"note\":\"\\\"bottle_id\\\":9\",\"bottle_id\":3}";
  const char* dend = decoy + std::strlen(decoy);
  CHECK(dbc::findField(decoy, dend, "bottle_id", vb, ve));
  CHECK(dbc::parseInt(vb, ve, iv) && iv == 3);

  // Explicit null is not a number; the caller keeps its default.
  const char* nul = "{\"duration_h\":null}";
  const char* nend = nul + std::strlen(nul);
  CHECK(dbc::findField(nul, nend, "duration_h", vb, ve));
  CHECK(!dbc::parseInt(vb, ve, iv));

  // Escaped string content is decoded, bounded and NUL-terminated.
  const char* esc = "{\"owner_name\":\"a\\\"b\\\\c\"}";
  const char* eend = esc + std::strlen(esc);
  CHECK(dbc::findField(esc, eend, "owner_name", vb, ve));
  CHECK(dbc::parseString(vb, ve, text, sizeof(text)));
  CHECK(std::strcmp(text, "a\"b\\c") == 0);
  char tiny[3];
  CHECK(dbc::parseString(vb, ve, tiny, sizeof(tiny)));
  CHECK(std::strlen(tiny) == 2);
}

static void test_array_iteration() {
  const char* body =
      "[{\"experiment_id\":7,\"duration_h\":48,\"setpoints\":{\"t\":39.5}},"
      "{\"experiment_id\":6,\"duration_h\":null,\"sample_interval_s\":30}]";
  const size_t len = std::strlen(body);
  size_t cursor = 0;
  const char* ob = nullptr;
  const char* oe = nullptr;
  int seen = 0;
  int32_t ids[2] = {-1, -1};
  while (dbc::nextObject(body, len, cursor, ob, oe)) {
    const char* vb = nullptr;
    const char* ve = nullptr;
    if (dbc::findField(ob, oe, "experiment_id", vb, ve)) {
      CHECK(dbc::parseInt(vb, ve, ids[seen]));
    }
    ++seen;
  }
  CHECK(seen == 2);
  CHECK(ids[0] == 7 && ids[1] == 6);
  CHECK(!dbc::nextObject(body, len, cursor, ob, oe));

  // Empty array iterates zero times; a truncated body iterates nothing.
  cursor = 0;
  CHECK(!dbc::nextObject("[]", 2, cursor, ob, oe));
  cursor = 0;
  CHECK(!dbc::nextObject("[{\"experiment_id\":7", 18, cursor, ob, oe));

  // Setpoints are kept as bounded raw text, never interpreted.
  cursor = 0;
  CHECK(dbc::nextObject(body, len, cursor, ob, oe));
  const char* vb = nullptr;
  const char* ve = nullptr;
  CHECK(dbc::findField(ob, oe, "setpoints", vb, ve));
  char raw[32];
  CHECK(dbc::parseRaw(vb, ve, raw, sizeof(raw)));
  CHECK(std::strcmp(raw, "{\"t\":39.5}") == 0);
}

static void test_sanitize() {
  dbc::BottleSample s = {};
  s.bottleId = 3;
  s.ph = std::numeric_limits<float>::quiet_NaN();
  s.pressure = std::numeric_limits<float>::infinity();
  s.tempC = 39.0f;
  s.co2 = -std::numeric_limits<float>::infinity();
  s.ch4 = 5.0f;
  s.nullMask = dbc::kNullPh;  // already-null stays null, not double counted
  const uint8_t fixed = dbc::sanitizeNonFinite(s);
  CHECK(fixed == 2);  // pressure and co2
  CHECK((s.nullMask & dbc::kNullPressure) != 0);
  CHECK((s.nullMask & dbc::kNullCo2) != 0);
  CHECK((s.nullMask & dbc::kNullTemp) == 0);
  CHECK((s.nullMask & dbc::kNullCh4) == 0);
  CHECK(s.pressure == 0.0f);
  CHECK(s.tempC == 39.0f);
}

static dbc::PushRow row(uint8_t bottle, int64_t ts) {
  dbc::PushRow r = {};
  r.bottleId = bottle;
  r.sampleUnix = ts;
  return r;
}

static void test_bottle_range() {
  // In-range rule: ids are 0-based, an unknown count accepts everything.
  CHECK(dbc::bottleInRange(0, 2));
  CHECK(dbc::bottleInRange(1, 2));
  CHECK(!dbc::bottleInRange(2, 2));
  CHECK(!dbc::bottleInRange(3, 2));
  CHECK(dbc::bottleInRange(3, 4));
  CHECK(dbc::bottleInRange(3, 24));
  CHECK(dbc::bottleInRange(3, -1));
  CHECK(dbc::bottleInRange(3, 0));

  // Purge on a ring whose live rows wrap past the end of the array. Head sits
  // at slot 6 of 8: three rounds of four bottles = 12 rows is too many, so use
  // 2 rounds x 3 bottles = 6 rows at slots 6,7,0,1,2,3.
  dbc::PushRow ring[8] = {};
  const size_t cap = 8;
  size_t head = 6, count = 0, tail = 6;
  const uint8_t ids[6] = {0, 1, 2, 0, 1, 2};
  const int64_t ts[6] = {100, 100, 100, 130, 130, 130};
  for (size_t i = 0; i < 6; i++) {
    ring[tail] = row(ids[i], ts[i]);
    tail = (tail + 1) % cap;
    count++;
  }
  CHECK(tail == 4);

  size_t removed = dbc::purgeOutOfRangeRows(ring, cap, head, tail, count, 2);
  CHECK(removed == 2);
  CHECK(count == 4);
  CHECK(tail == (head + 4) % cap);
  // Order is preserved: 0,1 at t=100 then 0,1 at t=130.
  const uint8_t wantId[4] = {0, 1, 0, 1};
  const int64_t wantTs[4] = {100, 100, 130, 130};
  for (size_t i = 0; i < 4; i++) {
    CHECK(ring[(head + i) % cap].bottleId == wantId[i]);
    CHECK(ring[(head + i) % cap].sampleUnix == wantTs[i]);
  }

  // Nothing to remove: unchanged. Everything out of range: empty.
  removed = dbc::purgeOutOfRangeRows(ring, cap, head, tail, count, 4);
  CHECK(removed == 0 && count == 4);
  removed = dbc::purgeOutOfRangeRows(ring, cap, head, tail, count, 1);
  CHECK(removed == 2 && count == 2);
  CHECK(ring[head].bottleId == 0 && ring[(head + 1) % cap].bottleId == 0);
  size_t emptyTail = 0, emptyCount = 0;
  CHECK(dbc::purgeOutOfRangeRows(ring, cap, head, emptyTail, emptyCount, 1) == 0);
}

int main() {
  test_bottle_range();
  test_timestamps();
  test_envelope();
  test_ack_and_errors();
  test_field_parsing();
  test_array_iteration();
  test_sanitize();
  std::printf("%d checks, %d failures\n", g_checks, g_failures);
  return g_failures == 0 ? 0 : 1;
}
