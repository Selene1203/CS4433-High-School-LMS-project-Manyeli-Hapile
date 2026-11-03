#include "lms/types.hpp"

#include <chrono>
#include <cstdio>
#include <ctime>

#include "lms/error.hpp"

namespace lms {

std::string toString(Role r) {
  switch (r) {
    case Role::kAdmin: return "ADMIN";
    case Role::kLecturer: return "LECTURER";
    case Role::kHod: return "HOD";
    case Role::kStudent: return "STUDENT";
  }
  return "";
}

std::string toString(MarkStatus s) {
  switch (s) {
    case MarkStatus::kMarked: return "MARKED";
    case MarkStatus::kReturned: return "RETURNED";
    case MarkStatus::kModerated: return "MODERATED";
    case MarkStatus::kReleased: return "RELEASED";
  }
  return "";
}

Role parseRole(const std::string& t) {
  if (t == "ADMIN") return Role::kAdmin;
  if (t == "LECTURER") return Role::kLecturer;
  if (t == "HOD") return Role::kHod;
  if (t == "STUDENT") return Role::kStudent;
  throw ValidationError("unknown role: " + t);
}

MarkStatus parseMarkStatus(const std::string& t) {
  if (t == "MARKED") return MarkStatus::kMarked;
  if (t == "RETURNED") return MarkStatus::kReturned;
  if (t == "MODERATED") return MarkStatus::kModerated;
  if (t == "RELEASED") return MarkStatus::kReleased;
  throw ValidationError("unknown mark status: " + t);
}

std::string formatTime(Time t) {
  std::time_t tt = static_cast<std::time_t>(t);
  std::tm tm{};
  gmtime_r(&tt, &tm);
  char buf[64];
  std::snprintf(buf, sizeof buf, "%04d-%02d-%02d %02d:%02d:%02d",
                tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour,
                tm.tm_min, tm.tm_sec);
  return buf;
}

namespace {

bool between(int v, int lo, int hi) { return v >= lo && v <= hi; }

}  // namespace

Time parseTime(const std::string& text) {
  int y = 0, mo = 0, d = 0, h = 0, mi = 0, s = 0;
  int n = std::sscanf(text.c_str(), "%d-%d-%d %d:%d:%d", &y, &mo, &d, &h, &mi,
                      &s);
  if (n < 5) throw ValidationError("date must look like 2026-11-30 17:00");
  // with five fields scanned, s keeps its starting value of zero
  bool inRange = between(mo, 1, 12) && between(d, 1, 31) && between(h, 0, 23) &&
                 between(mi, 0, 59) && between(s, 0, 59) && between(y, 1970, 2200);
  if (!inRange) throw ValidationError("date is out of range");
  std::tm tm{};
  tm.tm_year = y - 1900;
  tm.tm_mon = mo - 1;
  tm.tm_mday = d;
  tm.tm_hour = h;
  tm.tm_min = mi;
  tm.tm_sec = s;
  std::time_t tt = timegm(&tm);
  // timegm rolls 31 Feb into March, so compare it back.
  std::tm back{};
  gmtime_r(&tt, &back);
  if (back.tm_mday != d || back.tm_mon != mo - 1) {
    throw ValidationError("that day does not exist");
  }
  return static_cast<Time>(tt);
}

Time systemNow() {
  return static_cast<Time>(std::chrono::duration_cast<std::chrono::seconds>(
                               std::chrono::system_clock::now()
                                   .time_since_epoch())
                               .count());
}

std::string formatMark(double m) {
  char buf[32];
  std::snprintf(buf, sizeof buf, "%.2f", m);
  std::string s = buf;
  while (!s.empty() && s.back() == '0') s.pop_back();
  if (!s.empty() && s.back() == '.') s.pop_back();
  return s;
}

}  // namespace lms
