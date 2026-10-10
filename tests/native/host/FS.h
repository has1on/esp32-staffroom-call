#pragma once
// RAM-backed fake filesystem: exercises the actual Roster.h without touching a TF card.
#include <map>
#include <memory>
#include <string>
#include <cstdint>
#define FILE_READ "r"
class File {
    struct Cursor { std::string data; size_t position = 0; };
    std::shared_ptr<Cursor> cursor;
public:
    File() = default;
    explicit File(const std::string &data) : cursor(std::make_shared<Cursor>()) { cursor->data = data; }
    explicit operator bool() const { return bool(cursor); }
    int read() { return !cursor || cursor->position >= cursor->data.size() ? -1 :
                       static_cast<unsigned char>(cursor->data[cursor->position++]); }
    uint32_t size() const { return cursor ? uint32_t(cursor->data.size()) : 0; }
    bool seek(uint32_t position) {
        if (!cursor || position > cursor->data.size()) return false;
        cursor->position = position; return true;
    }
};
namespace fs {
class FS {
public:
    std::map<std::string,std::string> files;
    File open(const char *path,const char *) {
        auto it = files.find(path);
        return it == files.end() ? File() : File(it->second);
    }
};
}
