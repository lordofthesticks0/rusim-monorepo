// Minimal Arduino.h shim for the PC simulator (firmware/display-v9/sim).
// It exists so the screen/fake-data sources compile unchanged on a native
// build. Hardware-shaped calls (pins, Serial1, WiFi) do not have meaning
// here and are either no-ops or stubbed separately.
#pragma once

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdarg.h>
#include <time.h>
#include <sys/select.h>
#include <unistd.h>
#include <errno.h>

#ifndef LOW
#define LOW 0
#define HIGH 1
#define INPUT 0
#define OUTPUT 1
#define INPUT_PULLUP 2
#endif

static inline uint32_t millis() {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return static_cast<uint32_t>(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

static inline void delay(uint32_t ms) {
  struct timespec ts = {ms / 1000, (ms % 1000) * 1000000L};
  nanosleep(&ts, nullptr);
}

static inline long random(long lo, long hi) {
  if (hi <= lo) return lo;
  return lo + rand() % (hi - lo);
}
static inline void randomSeed(unsigned long seed) { srand(seed); }
static inline int analogRead(int) { return 0; }
static inline void pinMode(int, int) {}
static inline void digitalWrite(int, int) {}
static inline int digitalRead(int) { return LOW; }

static inline long map(long x, long in_min, long in_max, long out_min, long out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

class SimSerial {
 public:
  void begin(unsigned long) {}
  int available() {
#ifdef _WIN32
    return 0;  // console stdin is blocking on Windows; skip in sim
#else
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(0, &rfds);
    struct timeval tv = {0, 0};
    return select(1, &rfds, nullptr, nullptr, &tv) > 0 ? 1 : 0;
#endif
  }
  int read() {
#ifdef _WIN32
    return -1;
#else
    return getchar();
#endif
  }
  void println() { printf("\n"); }
  void println(const char *s) { printf("%s\n", s); }
  void println(long v) { printf("%ld\n", v); }
  void print(const char *s) { printf("%s", s); }
  int printf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int n = vprintf(fmt, ap);
    va_end(ap);
    fflush(stdout);
    return n;
  }
};

extern SimSerial Serial;

// Stub for ESP.* used by screen_info.cpp
struct SimESPClass {
  uint32_t getFreeHeap() { return 0; }
  uint32_t getPsramSize() { return 0; }
};
extern SimESPClass ESP;

// `String` is not supported in the sim; grep first if a source needs it.
