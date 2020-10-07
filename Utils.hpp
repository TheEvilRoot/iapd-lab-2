#ifndef __LAB_2_UTILS_HPP
#define __LAB_2_UTILS_HPP

#include <functional>

struct SizeValue {
private:
  static const std::array<std::string, 5> sizeSuffixes;

  ULONGLONG _bytes;
  ULONGLONG _value{ 0 };
  std::string _suffix{ };

  size_t applySuffix() {
    for (auto sIdx = 0; sIdx < sizeSuffixes.size(); sIdx++) {
      if (_value < 1024) {
        return sIdx;
      }
      _value /= 1024;
    }
    return sizeSuffixes.size() - 1;
  }
public:
  explicit SizeValue(ULONGLONG v) :
    _bytes{ v }, _value{ v }, _suffix{ sizeSuffixes[applySuffix()] } { }

  auto bytes() const { return _bytes; }
  auto value() const { return _value; }
  auto suffix() const { return _suffix; }

  friend std::ostream& operator<<(std::ostream& o, const SizeValue& v) {
    o << v.value() << " " << v.suffix();
    return o;
  }

};

auto getErrorMessage(DWORD errorCode) {
  if (errorCode == ERROR_SUCCESS) return std::string();
  LPSTR messageBuffer{ nullptr };
  auto size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
    NULL, errorCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), reinterpret_cast<LPSTR>(&messageBuffer), 0, NULL);
  std::string message(messageBuffer, size);
  auto idx = message.find_first_of('\n');
  if (idx != std::string::npos)
    message = message.substr(0, idx + 1);

  LocalFree(messageBuffer);
  return message;
}

inline auto trim(const std::string& str) {
  auto first = str.find_first_not_of(" \n\t");
  if (first == std::string::npos) {
    return str;
  }
  auto last = str.find_last_not_of(" \n\t");
  return str.substr(first, (last - first + 1));
}

template<typename T>
inline auto all_of(const T* arr, size_t size, std::function<bool(T)> pred) {
  bool flag = true;
  for (auto i = 0; i < size; i++) flag = flag & pred(arr[i]);
  return flag;
}

inline std::string stringifyBool(bool b) {
  return (b ? "Yes" : "No");
}

const std::array<std::string,5> SizeValue::sizeSuffixes = {
    "B", "Kb", "Mb", "Gb", "Tb"
};

#endif