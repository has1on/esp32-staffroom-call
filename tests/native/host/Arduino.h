#pragma once
// Minimal host-only adapter. Never copied into Door/ or used on the ESP32.
#include <cstdint>
#include <cstdlib>
#include <string>
#include <type_traits>
class String {
    std::string value;
public:
    String() = default;
    String(const char *s) : value(s ? s : "") {}
    String(char c) : value(1, c) {}
    String(const std::string &s) : value(s) {}
    template<class T, std::enable_if_t<std::is_integral_v<T> && !std::is_same_v<T,char>, int> = 0>
    String(T n) : value(std::to_string(n)) {}
    const char *c_str() const { return value.c_str(); }
    size_t length() const { return value.size(); }
    bool isEmpty() const { return value.empty(); }
    char operator[](size_t i) const { return value[i]; }
    String &operator+=(const String &s) { value += s.value; return *this; }
    String &operator+=(char c) { value += c; return *this; }
    friend String operator+(String a, const String &b) { a += b; return a; }
    friend bool operator==(const String &a,const String &b) { return a.value == b.value; }
    friend bool operator!=(const String &a,const String &b) { return !(a == b); }
};
inline void yield() {}
