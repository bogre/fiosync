#ifndef RECORD_H
#define RECORD_H
#include <iomanip>
#include <iostream>
#include <algorithm>

enum FileType : unsigned int { unknown, csv, json, desc};
class Record {
  double price{0};
  int quantity{0};
  std::string id;
  std::string inputId;
  FileType streamType{};
  bool valid{false};

  inline void parse() noexcept {
    valid = false;
    std::string content{id};
    if (streamType == csv) {
      std::replace(content.begin(), content.end(),',',' ');
      std::istringstream is(content);
      is >> id >> quantity >> price;
      if (is.peek() == EOF) {
        valid = true;
      } else {
      }
      return;
    }
 }

  public:
  explicit Record(const std::string& strContent = "") : inputId(strContent) {}
  Record(const char* contentPtr, std::streamsize length,
         const std::string& streamId, FileType strmType)
      : id(contentPtr, length), inputId(streamId), streamType(strmType) {
    parse();
  }
  friend std::ostream& operator<<(std::ostream& os, const Record& r) {
    if (r.inputId == "stop") {
      os.setstate(std::ios::eofbit);
      return os;
    }
    os << '\"' << r.inputId << "\":" << r.id << ':' << r.quantity << ':'
       << std::fixed << std::setprecision(2) << r.price << "\n";
    return os;
  }
 inline bool Valid() {return valid;}
 inline std::string getSourceStreamId() { return inputId;}
};

#endif

