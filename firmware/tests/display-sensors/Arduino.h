#pragma once

// Host-only Arduino surface for the real display model and UART parser.
// RX and TX are deliberately separate: outgoing heartbeats cannot loop back
// into the parser. Standard containers own all buffered bytes (no hardware).
#include <cmath>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <strings.h>
#include <deque>
#include <string>
#include <vector>

using std::isfinite;
using std::isnan;

namespace host {
extern uint32_t now_ms;
inline void advance_ms(uint32_t elapsed) { now_ms += elapsed; }
}  // namespace host

inline uint32_t millis() { return host::now_ms; }
inline void delay(uint32_t elapsed) { host::advance_ms(elapsed); }

static const uint32_t SERIAL_8N1 = 0;

class HostSerial {
 public:
  void begin(unsigned long, uint32_t, int, int) {}
  size_t setRxBufferSize(size_t size) { return size; }

  int available() const { return static_cast<int>(rx_.size()); }
  int read() {
    if (rx_.empty()) return -1;
    const uint8_t value = rx_.front();
    rx_.pop_front();
    return value;
  }

  void feed(const std::string &bytes) {
    for (unsigned char byte : bytes) rx_.push_back(byte);
  }
  void clear() {
    rx_.clear();
    tx_.clear();
  }
  const std::string &output() const { return tx_; }
  void clear_output() { tx_.clear(); }

  size_t write(uint8_t byte) {
    tx_.push_back(static_cast<char>(byte));
    return 1;
  }
  size_t write(const uint8_t *bytes, size_t size) {
    if (bytes == nullptr || size == 0) return 0;
    tx_.append(reinterpret_cast<const char *>(bytes), size);
    return size;
  }
  size_t print(char value) { return write(static_cast<uint8_t>(value)); }
  size_t print(const char *text) {
    if (text == nullptr) return 0;
    const size_t size = std::char_traits<char>::length(text);
    tx_.append(text, size);
    return size;
  }
  size_t println(const char *text) {
    const size_t size = print(text);
    return size + print('\n');
  }
  size_t printf(const char *format, ...) {
    va_list args;
    va_start(args, format);
    const int size = std::vsnprintf(nullptr, 0, format, args);
    va_end(args);
    if (size < 0) return 0;
    std::vector<char> buffer(static_cast<size_t>(size) + 1);
    va_start(args, format);
    std::vsnprintf(buffer.data(), buffer.size(), format, args);
    va_end(args);
    tx_.append(buffer.data(), static_cast<size_t>(size));
    return static_cast<size_t>(size);
  }

 private:
  std::deque<uint8_t> rx_;
  std::string tx_;
};

extern HostSerial Serial;
extern HostSerial Serial1;

// serial_cmd.cpp references these counters through MEM. No ESP runtime or
// allocation instrumentation is needed for the USB command-routing tests.
class HostESP {
 public:
  unsigned int getFreeHeap() const { return 0; }
  unsigned int getMinFreeHeap() const { return 0; }
  unsigned int getMaxAllocHeap() const { return 0; }
  unsigned int getFreePsram() const { return 0; }
  unsigned int getMinFreePsram() const { return 0; }
  unsigned int getMaxAllocPsram() const { return 0; }
};
extern HostESP ESP;
