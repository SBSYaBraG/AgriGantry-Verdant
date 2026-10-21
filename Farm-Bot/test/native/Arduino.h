// Host-only Arduino shim. Exercises the real firmware without attached motors.
#pragma once
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <deque>
#include <functional>
#include <sstream>
#include <string>
#include <vector>

#define HIGH 1
#define LOW 0
#define INPUT_PULLUP 2
#define OUTPUT 1
#define F(value) value
#define constrain(value, lower, upper) ((value) < (lower) ? (lower) : ((value) > (upper) ? (upper) : (value)))
using std::min;
using std::max;
using byte = uint8_t;

class String {
  std::string value;
public:
  String(const char* text = "") : value(text) {}
  String(std::string text) : value(text) {}
  unsigned int length() const { return (unsigned int)value.size(); }
  const char* c_str() const { return value.c_str(); }
  char operator[](size_t index) const { return value[index]; }
  bool operator==(const char* text) const { return value == text; }
  bool startsWith(const char* prefix) const { return value.find(prefix) == 0; }
  String substring(size_t index) const { return String(value.substr(index)); }
  void toUpperCase() { for (char& c : value) c = (char)std::toupper((unsigned char)c); }
  void trim() {
    size_t first = value.find_first_not_of(" \t\r\n");
    value = first == std::string::npos ? "" : value.substr(first, value.find_last_not_of(" \t\r\n") - first + 1);
  }
};

class FakeSerial {
public:
  std::deque<char> input;
  std::string output;
  void begin(unsigned long) {}
  int available() const { return (int)input.size(); }
  int read() { char c = input.front(); input.pop_front(); return c; }
  void inject(const std::string& text) { for (char c : text) input.push_back(c); }
  template<class T> void print(T value) { std::ostringstream out; out << value; output += out.str(); }
  template<class T> void println(T value) { print(value); output += '\n'; }
  void println() { output += '\n'; }
};

struct PinWrite { uint8_t pin; uint8_t level; };
extern FakeSerial Serial;
extern uint8_t pinLevels[70];
extern uint8_t pinModes[70];
extern unsigned long pulseCount[70];
extern std::vector<PinWrite> writes;
extern uint64_t fakeTimeUs;
extern std::function<void()> onAdvance;
extern std::function<void(uint8_t)> onPulse;
void pinMode(uint8_t pin, uint8_t mode);
int digitalRead(uint8_t pin);
void digitalWrite(uint8_t pin, uint8_t level);
unsigned long millis();
unsigned long micros();
void delay(unsigned long ms);
void delayMicroseconds(unsigned int us);
