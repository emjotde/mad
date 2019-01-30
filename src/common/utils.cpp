#include "common/utils.h"
#include "common/logging.h"
#include "CLI/StringTools.hpp"

#include <stdio.h>
#include <array>
#include <iostream>
#include <sstream>
#include <string>
#ifdef __unix__
#include <unistd.h>
#endif
#include <codecvt>
#include <cwctype>

namespace marian {
namespace utils {

void trim(std::string& s) {
  CLI::detail::trim(s, " \t\n");
}

void trimRight(std::string& s) {
  CLI::detail::rtrim(s, " \t\n");
}

void trimLeft(std::string& s) {
  CLI::detail::ltrim(s, " \t\n");
}

// @TODO: use more functions from CLI instead of own implementations
void split(const std::string& line,
           std::vector<std::string>& pieces,
           const std::string& del /*= " "*/,
           bool keepEmpty) {
  size_t begin = 0;
  size_t pos = 0;
  std::string token;
  while((pos = line.find(del, begin)) != std::string::npos) {
    if(pos >= begin) {
      token = line.substr(begin, pos - begin);
      if(token.size() > 0 || keepEmpty)
        pieces.push_back(token);
    }
    begin = pos + del.size();
  }
  if(pos >= begin) {
    token = line.substr(begin, pos - begin);
    if(token.size() > 0 || keepEmpty)
      pieces.push_back(token);
  }
}

std::vector<std::string> split(const std::string& line,
                               const std::string& del /*= " "*/,
                               bool keepEmpty) {
  std::vector<std::string> pieces;
  split(line, pieces, del, keepEmpty);
  return pieces;
}

// @TODO: splitAny() shares all but 2 expressions with split(). Merge them.
void splitAny(const std::string& line,
              std::vector<std::string>& pieces,
              const std::string& del /*= " "*/,
              bool keepEmpty) {
  size_t begin = 0;
  size_t pos = 0;
  std::string token;
  while((pos = line.find_first_of(del, begin)) != std::string::npos) {
    if(pos >= begin) {
      token = line.substr(begin, pos - begin);
      if(token.size() > 0 || keepEmpty)
        pieces.push_back(token);
    }
    begin = pos + 1;
  }
  if(pos >= begin) {
    token = line.substr(begin, pos - begin);
    if(token.size() > 0 || keepEmpty)
      pieces.push_back(token);
  }
}

std::vector<std::string> splitAny(const std::string& line,
                                  const std::string& del /*= " "*/,
                                  bool keepEmpty) {
  std::vector<std::string> pieces;
  splitAny(line, pieces, del, keepEmpty);
  return pieces;
}

std::string join(const std::vector<std::string>& words,
                 const std::string& del /*= " "*/) {
  std::stringstream ss;
  if(words.empty()) {
    return "";
  }

  ss << words[0];
  for(size_t i = 1; i < words.size(); ++i) {
    ss << del << words[i];
  }

  return ss.str();
}

std::string exec(const std::string& cmd) {
  std::array<char, 128> buffer;
  std::string result;
#ifdef _WIN32
#define popen _popen
#define pclose _pclose
#endif
  std::shared_ptr<std::FILE> pipe(popen(cmd.c_str(), "r"), pclose);
  if(!pipe)
    ABORT("popen() failed!");

  while(!std::feof(pipe.get())) {
    if(std::fgets(buffer.data(), 128, pipe.get()) != NULL)
      result += buffer.data();
  }
  return result;
}

std::pair<std::string, int> hostnameAndProcessId() { // helper to get hostname:pid
#ifdef _WIN32
  std::string hostname = getenv("COMPUTERNAME");
  auto processId = (int)GetCurrentProcessId();
#else
  static std::string hostname = [](){ // not sure if gethostname() is expensive. This way we call it only once.
    char hostnamebuf[HOST_NAME_MAX + 1] = { 0 };
    gethostname(hostnamebuf, sizeof(hostnamebuf));
    return std::string(hostnamebuf);
  }();
  auto processId = (int)getpid();
#endif
  return{ hostname, processId };
}

// format a long number with comma separators
std::string withCommas(size_t n) {
  std::string res = std::to_string(n);
  for (int i = (int)res.size() - 3; i > 0; i -= 3)
    res.insert(i, ",");
  return res;
}

bool beginsWith(const std::string& text, const std::string& prefix) {
  return text.size() >= prefix.size()
         && !text.compare(0, prefix.size(), prefix);
}

bool endsWith(const std::string& text, const std::string& suffix) {
  return text.size() >= suffix.size()
         && !text.compare(text.size() - suffix.size(), suffix.size(), suffix);
}

static std::wstring utf8ToWString(std::string const& s) {
  std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
  return converter.from_bytes(s);
}

static std::string toUTF8String(std::wstring const& s) {
  std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
  return converter.to_bytes(s);
}

// convert a UTF-8 string to all-caps
#if 0
// @BUGBUG: This does not work for non-ASCII characters.
std::string utf8ToUpper(const std::string& s) {
  auto ws = utf8ToWString(s);
  for (auto& c : ws)
    c = std::towupper(c);
  return toUTF8String(ws);
}
#else
struct UTF8Mapper : std::map<wchar_t, wchar_t> { // Hack because Philly does not have UTF-8 locale installed
  UTF8Mapper() {
    /*
      env LC_ALL=en_US.UTF-8 sed 's/\(.\)/\1\n/g'   TEXT_FILE_CONTAINING_ALL_CHARS > l
      env LC_ALL=en_US.UTF-8 sed 's/\(.\)/\U\1\n/g' TEXT_FILE_CONTAINING_ALL_CHARS > u
      paste l u | env LC_ALL=en_US.UTF-8 sort -u > x
      cat x | awk '{if($1 != $2){print}}' > y
      cat y | tr -d '\r' \
        | od -w10000 -t x1 \
        | head -1 \
        | sed -e 's/^0000000 /{{".x/g' -e 's/ 09 /",".x/g' -e 's/ 0a /"},{".x/g' -e 's/ 0a$/"}/' -e 's/ /.x/g' \
        | tr '.' '\\' \
        | xclip
    */
    std::vector<std::pair<std::string, std::string>> map8{ {"\xc9\x92","\xe2\xb1\xb0"},{"\x61","\x41"},{"\xc3\xa1","\xc3\x81"},{"\xc3\xa0","\xc3\x80"},{"\xe1\xba\xaf","\xe1\xba\xae"},{"\xe1\xba\xb1","\xe1\xba\xb0"},{"\xe1\xba\xb5","\xe1\xba\xb4"},{"\xe1\xba\xb3","\xe1\xba\xb2"},{"\xe1\xba\xb7","\xe1\xba\xb6"},{"\xc4\x83","\xc4\x82"},{"\xe1\xba\xa5","\xe1\xba\xa4"},{"\xe1\xba\xa7","\xe1\xba\xa6"},{"\xe1\xba\xab","\xe1\xba\xaa"},{"\xe1\xba\xa9","\xe1\xba\xa8"},{"\xe1\xba\xad","\xe1\xba\xac"},{"\xc3\xa2","\xc3\x82"},{"\xc7\x8e","\xc7\x8d"},{"\xc7\xbb","\xc7\xba"},{"\xc3\xa5","\xc3\x85"},{"\xc7\x9f","\xc7\x9e"},{"\xc3\xa4","\xc3\x84"},{"\xc3\xa3","\xc3\x83"},{"\xc4\x85","\xc4\x84"},{"\xc4\x81","\xc4\x80"},{"\xe1\xba\xa3","\xe1\xba\xa2"},{"\xc8\x83","\xc8\x82"},{"\xe1\xba\xa1","\xe1\xba\xa0"},{"\xc7\xa3","\xc7\xa2"},{"\xc3\xa6","\xc3\x86"},{"\x62","\x42"},{"\xe1\xb8\x87","\xe1\xb8\x86"},{"\x63","\x43"},{"\xc4\x87","\xc4\x86"},{"\xc4\x89","\xc4\x88"},{"\xc4\x8d","\xc4\x8c"},{"\xc4\x8b","\xc4\x8a"},{"\xc3\xa7","\xc3\x87"},{"\x64","\x44"},{"\xc4\x8f","\xc4\x8e"},{"\xc4\x91","\xc4\x90"},{"\xe1\xb8\x91","\xe1\xb8\x90"},{"\xe1\xb8\x8d","\xe1\xb8\x8c"},{"\xe1\xb8\x8f","\xe1\xb8\x8e"},{"\xc3\xb0","\xc3\x90"},{"\x65","\x45"},{"\xc3\xa9","\xc3\x89"},{"\xc3\xa8","\xc3\x88"},{"\xc4\x95","\xc4\x94"},{"\xe1\xba\xbf","\xe1\xba\xbe"},{"\xe1\xbb\x81","\xe1\xbb\x80"},{"\xe1\xbb\x85","\xe1\xbb\x84"},{"\xe1\xbb\x83","\xe1\xbb\x82"},{"\xe1\xbb\x87","\xe1\xbb\x86"},{"\xc3\xaa","\xc3\x8a"},{"\xc4\x9b","\xc4\x9a"},{"\xc3\xab","\xc3\x8b"},{"\xe1\xba\xbd","\xe1\xba\xbc"},{"\xc4\x97","\xc4\x96"},{"\xc4\x99","\xc4\x98"},{"\xe1\xb8\x97","\xe1\xb8\x96"},{"\xc4\x93","\xc4\x92"},{"\xe1\xba\xbb","\xe1\xba\xba"},{"\xc8\x87","\xc8\x86"},{"\xe1\xba\xb9","\xe1\xba\xb8"},{"\xc7\x9d","\xc6\x8e"},{"\x66","\x46"},{"\x67","\x47"},{"\xc7\xb5","\xc7\xb4"},{"\xc4\x9f","\xc4\x9e"},{"\xc4\x9d","\xc4\x9c"},{"\xc7\xa7","\xc7\xa6"},{"\xc4\xa1","\xc4\xa0"},{"\xc4\xa3","\xc4\xa2"},{"\xc9\xa0","\xc6\x93"},{"\x68","\x48"},{"\xc4\xa5","\xc4\xa4"},{"\xc4\xa7","\xc4\xa6"},{"\xe1\xb8\xa9","\xe1\xb8\xa8"},{"\xe1\xb8\xa5","\xe1\xb8\xa4"},{"\xe1\xb8\xab","\xe1\xb8\xaa"},{"\x69","\x49"},{"\xc4\xb1","\x49"},{"\xc3\xad","\xc3\x8d"},{"\xc3\xac","\xc3\x8c"},{"\xc4\xad","\xc4\xac"},{"\xc3\xae","\xc3\x8e"},{"\xc7\x90","\xc7\x8f"},{"\xc3\xaf","\xc3\x8f"},{"\xc4\xa9","\xc4\xa8"},{"\xc4\xaf","\xc4\xae"},{"\xc4\xab","\xc4\xaa"},{"\xe1\xbb\x89","\xe1\xbb\x88"},{"\xc8\x8b","\xc8\x8a"},{"\xe1\xbb\x8b","\xe1\xbb\x8a"},{"\x6a","\x4a"},{"\xc4\xb5","\xc4\xb4"},{"\x6b","\x4b"},{"\xe1\xb8\xb1","\xe1\xb8\xb0"},{"\xc4\xb7","\xc4\xb6"},{"\xe1\xb8\xb3","\xe1\xb8\xb2"},{"\xc6\x99","\xc6\x98"},{"\x6c","\x4c"},{"\xc4\xba","\xc4\xb9"},{"\xc4\xbe","\xc4\xbd"},{"\xc5\x82","\xc5\x81"},{"\xc4\xbc","\xc4\xbb"},{"\xe1\xb8\xb7","\xe1\xb8\xb6"},{"\x6d","\x4d"},{"\xe1\xb8\xbf","\xe1\xb8\xbe"},{"\xe1\xb9\x83","\xe1\xb9\x82"},{"\xc5\x8b","\xc5\x8a"},{"\x6e","\x4e"},{"\xc5\x84","\xc5\x83"},{"\xc5\x88","\xc5\x87"},{"\xc3\xb1","\xc3\x91"},{"\xe1\xb9\x85","\xe1\xb9\x84"},{"\xc5\x86","\xc5\x85"},{"\xe1\xb9\x87","\xe1\xb9\x86"},{"\xe1\xb9\x89","\xe1\xb9\x88"},{"\xc5\x93","\xc5\x92"},{"\x6f","\x4f"},{"\xc3\xb3","\xc3\x93"},{"\xc3\xb2","\xc3\x92"},{"\xc5\x8f","\xc5\x8e"},{"\xe1\xbb\x91","\xe1\xbb\x90"},{"\xe1\xbb\x93","\xe1\xbb\x92"},{"\xe1\xbb\x95","\xe1\xbb\x94"},{"\xe1\xbb\x99","\xe1\xbb\x98"},{"\xc3\xb4","\xc3\x94"},{"\xc7\x92","\xc7\x91"},{"\xc3\xb6","\xc3\x96"},{"\xc5\x91","\xc5\x90"},{"\xc3\xb5","\xc3\x95"},{"\xc3\xb8","\xc3\x98"},{"\xc7\xab","\xc7\xaa"},{"\xc5\x8d","\xc5\x8c"},{"\xe1\xbb\x8f","\xe1\xbb\x8e"},{"\xc8\x8f","\xc8\x8e"},{"\xe1\xbb\x8d","\xe1\xbb\x8c"},{"\xe1\xbb\x9b","\xe1\xbb\x9a"},{"\xe1\xbb\x9d","\xe1\xbb\x9c"},{"\xe1\xbb\xa1","\xe1\xbb\xa0"},{"\xe1\xbb\x9f","\xe1\xbb\x9e"},{"\xe1\xbb\xa3","\xe1\xbb\xa2"},{"\xc6\xa1","\xc6\xa0"},{"\xc9\x94","\xc6\x86"},{"\x70","\x50"},{"\xe1\xb9\x95","\xe1\xb9\x94"},{"\x71","\x51"},{"\x72","\x52"},{"\xc5\x95","\xc5\x94"},{"\xc5\x99","\xc5\x98"},{"\xc5\x97","\xc5\x96"},{"\xe1\xb9\x9b","\xe1\xb9\x9a"},{"\xe1\xb9\x9f","\xe1\xb9\x9e"},{"\x73","\x53"},{"\xc5\x9b","\xc5\x9a"},{"\xc5\x9d","\xc5\x9c"},{"\xc5\xa1","\xc5\xa0"},{"\xc5\x9f","\xc5\x9e"},{"\xe1\xb9\xa3","\xe1\xb9\xa2"},{"\x74","\x54"},{"\xc5\xa5","\xc5\xa4"},{"\xc5\xa3","\xc5\xa2"},{"\xe1\xb9\xad","\xe1\xb9\xac"},{"\xe1\xb9\xaf","\xe1\xb9\xae"},{"\xc8\x95","\xc8\x94"},{"\x75","\x55"},{"\xc3\xba","\xc3\x9a"},{"\xc3\xb9","\xc3\x99"},{"\xc5\xad","\xc5\xac"},{"\xc3\xbb","\xc3\x9b"},{"\xc7\x94","\xc7\x93"},{"\xc5\xaf","\xc5\xae"},{"\xc7\x98","\xc7\x97"},{"\xc7\x9c","\xc7\x9b"},{"\xc3\xbc","\xc3\x9c"},{"\xc5\xb1","\xc5\xb0"},{"\xc5\xa9","\xc5\xa8"},{"\xc5\xb3","\xc5\xb2"},{"\xc5\xab","\xc5\xaa"},{"\xe1\xbb\xa7","\xe1\xbb\xa6"},{"\xe1\xbb\xa5","\xe1\xbb\xa4"},{"\xe1\xb9\xb3","\xe1\xb9\xb2"},{"\xe1\xbb\xa9","\xe1\xbb\xa8"},{"\xe1\xbb\xab","\xe1\xbb\xaa"},{"\xe1\xbb\xaf","\xe1\xbb\xae"},{"\xe1\xbb\xad","\xe1\xbb\xac"},{"\xe1\xbb\xb1","\xe1\xbb\xb0"},{"\xc6\xb0","\xc6\xaf"},{"\x76","\x56"},{"\x77","\x57"},{"\xc5\xb5","\xc5\xb4"},{"\x78","\x58"},{"\xe1\xba\x8b","\xe1\xba\x8a"},{"\x79","\x59"},{"\xc3\xbd","\xc3\x9d"},{"\xe1\xbb\xb3","\xe1\xbb\xb2"},{"\xc5\xb7","\xc5\xb6"},{"\xc3\xbf","\xc5\xb8"},{"\xe1\xbb\xb9","\xe1\xbb\xb8"},{"\x7a","\x5a"},{"\xc5\xba","\xc5\xb9"},{"\xc5\xbe","\xc5\xbd"},{"\xc5\xbc","\xc5\xbb"},{"\xc6\xb6","\xc6\xb5"},{"\xe1\xba\x93","\xe1\xba\x92"},{"\xe1\xba\x95","\xe1\xba\x94"},{"\xc8\xa5","\xc8\xa4"},{"\xc3\xbe","\xc3\x9e"},{"\xca\x92","\xc6\xb7"},{"\xce\xb1","\xce\x91"},{"\xce\xac","\xce\x86"},{"\xce\xb2","\xce\x92"},{"\xce\xb3","\xce\x93"},{"\xce\xb4","\xce\x94"},{"\xce\xb5","\xce\x95"},{"\xce\xad","\xce\x88"},{"\xce\xb6","\xce\x96"},{"\xce\xb7","\xce\x97"},{"\xce\xae","\xce\x89"},{"\xce\xb8","\xce\x98"},{"\xce\xb9","\xce\x99"},{"\xce\xaf","\xce\x8a"},{"\xcf\x8a","\xce\xaa"},{"\xce\xba","\xce\x9a"},{"\xce\xbb","\xce\x9b"},{"\xce\xbc","\xce\x9c"},{"\xce\xbd","\xce\x9d"},{"\xce\xbe","\xce\x9e"},{"\xce\xbf","\xce\x9f"},{"\xcf\x8c","\xce\x8c"},{"\xcf\x80","\xce\xa0"},{"\xcf\x83","\xce\xa3"},{"\xcf\x82","\xce\xa3"},{"\xcf\x84","\xce\xa4"},{"\xcf\x85","\xce\xa5"},{"\xcf\x8d","\xce\x8e"},{"\xcf\x8b","\xce\xab"},{"\xcf\x86","\xce\xa6"},{"\xcf\x87","\xce\xa7"},{"\xcf\x88","\xce\xa8"},{"\xcf\x89","\xce\xa9"},{"\xcf\x8e","\xce\x8f"},{"\xd0\xb0","\xd0\x90"},{"\xd3\x93","\xd3\x92"},{"\xd3\x95","\xd3\x94"},{"\xd0\xb1","\xd0\x91"},{"\xd0\xb2","\xd0\x92"},{"\xd0\xb3","\xd0\x93"},{"\xd2\x93","\xd2\x92"},{"\xd2\x91","\xd2\x90"},{"\xd0\xb4","\xd0\x94"},{"\xd1\x93","\xd0\x83"},{"\xd1\x92","\xd0\x82"},{"\xd0\xb5","\xd0\x95"},{"\xd1\x90","\xd0\x80"},{"\xd3\x99","\xd3\x98"},{"\xd1\x94","\xd0\x84"},{"\xd1\x91","\xd0\x81"},{"\xd0\xb6","\xd0\x96"},{"\xd0\xb7","\xd0\x97"},{"\xd2\x99","\xd2\x98"},{"\xd1\x95","\xd0\x85"},{"\xd0\xb8","\xd0\x98"},{"\xd3\xa3","\xd3\xa2"},{"\xd1\x96","\xd0\x86"},{"\xd1\x97","\xd0\x87"},{"\xd0\xb9","\xd0\x99"},{"\xd1\x98","\xd0\x88"},{"\xd0\xba","\xd0\x9a"},{"\xd2\x9b","\xd2\x9a"},{"\xd3\x84","\xd3\x83"},{"\xd2\xa1","\xd2\xa0"},{"\xd0\xbb","\xd0\x9b"},{"\xd1\x99","\xd0\x89"},{"\xd0\xbc","\xd0\x9c"},{"\xd0\xbd","\xd0\x9d"},{"\xd2\xa3","\xd2\xa2"},{"\xd1\x9a","\xd0\x8a"},{"\xd0\xbe","\xd0\x9e"},{"\xd3\xa7","\xd3\xa6"},{"\xd3\xa9","\xd3\xa8"},{"\xd0\xbf","\xd0\x9f"},{"\xd1\x80","\xd0\xa0"},{"\xd1\x81","\xd0\xa1"},{"\xd2\xab","\xd2\xaa"},{"\xd1\x82","\xd0\xa2"},{"\xd1\x9c","\xd0\x8c"},{"\xd1\x9b","\xd0\x8b"},{"\xd1\x83","\xd0\xa3"},{"\xd3\xb1","\xd3\xb0"},{"\xd2\xb1","\xd2\xb0"},{"\xd2\xaf","\xd2\xae"},{"\xd1\x9e","\xd0\x8e"},{"\xd1\x84","\xd0\xa4"},{"\xd1\x85","\xd0\xa5"},{"\xd2\xb3","\xd2\xb2"},{"\xd2\xbb","\xd2\xba"},{"\xd1\x86","\xd0\xa6"},{"\xd1\x87","\xd0\xa7"},{"\xd1\x9f","\xd0\x8f"},{"\xd1\x88","\xd0\xa8"},{"\xd1\x89","\xd0\xa9"},{"\xd1\x8a","\xd0\xaa"},{"\xd1\x8b","\xd0\xab"},{"\xd1\x8c","\xd0\xac"},{"\xd1\x8d","\xd0\xad"},{"\xd1\x8e","\xd0\xae"},{"\xd1\x8f","\xd0\xaf"},{"\xd5\xa1","\xd4\xb1"},{"\xd5\xa3","\xd4\xb3"},{"\xd5\xa5","\xd4\xb5"},{"\xd5\xab","\xd4\xbb"},{"\xd5\xac","\xd4\xbc"},{"\xd5\xb2","\xd5\x82"},{"\xd5\xb8","\xd5\x88"},{"\xd5\xbd","\xd5\x8d"},{"\xd5\xbe","\xd5\x8e"},{"\xd5\xbf","\xd5\x8f"},{"\xd6\x80","\xd5\x90"},{"\xd6\x81","\xd5\x91"} };
    for (auto p8 : map8) {
      auto from = utf8ToWString(p8.first);
      auto to   = utf8ToWString(p8.second);
      ABORT_IF(from.size() != 1 || to.size() != 1, "Incorrect character encoding??");
      insert(std::make_pair(from.front(), to.front()));
    }
  }
  wchar_t towupper(wchar_t c) const {
    auto iter = find(c);
    if (iter == end())
      return c;
    else
      return iter->second;
  }
};
std::string utf8ToUpper(const std::string& s) {
  static UTF8Mapper mapper;
  auto ws = utf8ToWString(s);
  for (auto& c : ws)
    c = mapper.towupper(c);
  return toUTF8String(ws);
}
#endif

double parseDouble(std::string s) {
  double res;
  char c; // dummy char--if we succeed to parse this, then there were extraneous characters after the number
  auto rc = sscanf(s.c_str(), "%lf%c", &res, &c);
  ABORT_IF(rc != 1, "Mal-formed number: {}", s);
  return res;
}

// parses a user-friendly number that can have commas and (some) units
double parseNumber(std::string param) {
  // get unit prefix
  double factor = 1.;
  if (!param.empty() && param.back() >= 'A') {
    switch (param.back()) {
    case 'k': factor = 1.e3;  break;
    case 'M': factor = 1.e6;  break;
    case 'G': factor = 1.e9;  break;
    case 'T': factor = 1.e12; break;
    default: ABORT("Invalid or unsupported unit prefix '{}' in {}", param.back(), param);
    }
    param.pop_back();
  }
  // we allow users to place commas in numbers (note: we are not actually verifying that they are in the right place)
  std::remove_if(param.begin(), param.end(), [](char c) { return c == ','; });
  return factor * parseDouble(param);
}

}  // namespace utils
}  // namespace marian
