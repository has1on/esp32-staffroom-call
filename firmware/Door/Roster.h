#pragma once
#include <Arduino.h>
#include <FS.h>
#include "CommonCore.h"
#include "NetworkValidation.h"

struct Person { String id, className, maskedName, displayName; };
inline String cell(const std::vector<std::string> &row, int index) {
    return index >= 0 && size_t(index) < row.size() ? String(row[index].c_str()) : String();
}
inline bool doorIdentifier(const String &id) {
    if (!staffroom::identifier(std::string(id.c_str()))) return false;
    for (size_t i = 0; i < id.length(); ++i)
        if (id[i] >= 'a' && id[i] <= 'z') return false; // keyboard is uppercase
    return true;
}
inline bool readStudentHeader(File &file, std::vector<std::string> &header,
                              String &error) {
    std::string csvError;
    if (!file || !staffroom::csvRecord(file, header, csvError)) {
        error = "students_missing_or_invalid";
        return false;
    }
    staffroom::stripBom(header);
    if (header != std::vector<std::string>{"studentId", "class", "maskedName"}) {
        error = "students_header_invalid";
        return false;
    }
    return true;
}
inline bool studentsReady(fs::FS &fs, String &error) {
    File file = fs.open("/students.csv", FILE_READ);
    std::vector<std::string> header;
    return readStudentHeader(file, header, error);
}
inline bool findStudent(fs::FS &fs, const String &id, Person &out, String &error) {
    if (!staffroom::studentId5(id.c_str())) { error = "invalid_id"; return false; }
    File file = fs.open("/students.csv", FILE_READ);
    std::vector<std::string> header, row;
    if (!readStudentHeader(file, header, error)) return false;
    std::string csvError;
    bool found = false;
    Person candidate;
    size_t records = 0;
    // Scan to EOF, rather than return the first hit: an ambiguous student ID
    // must be rejected instead of potentially calling for the wrong student.
    while (staffroom::csvRecord(file, row, csvError)) {
        if (row.size() == 1 && row[0].empty()) continue;
        if (row.size() != 3 || staffroom::studentUiId(cell(row, 0).c_str()).empty() ||
            cell(row, 1).isEmpty() || cell(row, 2).isEmpty()) {
            error = "students_invalid"; return false;
        }
        if (String(staffroom::studentUiId(cell(row, 0).c_str()).c_str()) == id) {
            if (found) { error = "duplicate_student"; return false; }
            candidate.id = id;
            candidate.className = cell(row, 1);
            candidate.maskedName = cell(row, 2);
            found = true;
        }
        if (++records % 32 == 0) yield();
    }
    if (!csvError.empty()) { error = "students_invalid"; return false; }
    if (!found) { error = "id_not_found"; return false; }
    out = candidate;
    return true;
}
inline bool loadTeachers(fs::FS &fs, Person *out, size_t &count, String &error) {
    count = 0;
    File file = fs.open("/teachers.csv", FILE_READ);
    std::vector<std::string> header, row;
    std::string csvError;
    if (!file || !staffroom::csvRecord(file, header, csvError)) {
        error = "teachers_missing_or_invalid"; return false;
    }
    staffroom::stripBom(header);
    if (header != std::vector<std::string>{"teacherId", "displayName"}) {
        error = "teachers_header_invalid"; return false;
    }
    while (staffroom::csvRecord(file, row, csvError)) {
        if (row.size() == 1 && row[0].empty()) continue;
        if (row.size() != 2 || count >= 15 || !doorIdentifier(cell(row, 0)) ||
            cell(row, 1).isEmpty()) { error = "teachers_invalid_or_over_15"; return false; }
        for (size_t i = 0; i < count; ++i) {
            if (out[i].id == cell(row, 0)) { error = "duplicate_teacher"; return false; }
        }
        out[count].id = cell(row, 0);
        out[count].displayName = cell(row, 1);
        ++count;
    }
    if (!csvError.empty() || count == 0) { error = "teachers_invalid"; return false; }
    return true;
}
