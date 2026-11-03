#pragma once

#include <cstdint>
#include <functional>
#include <string>

namespace lms {

/// Seconds since 1970-01-01 UTC. The database stores it as text.
using Time = std::int64_t;
using Clock = std::function<Time()>;

constexpr int kMaxFailedLogins = 5;       // FR-01
constexpr Time kSecondsPerDay = 86400;    // BR1
constexpr int kMinPasswordLength = 8;

enum class Role { kAdmin, kLecturer, kHod, kStudent };
enum class MarkStatus { kMarked, kReturned, kModerated, kReleased };

std::string toString(Role r);
std::string toString(MarkStatus s);
Role parseRole(const std::string& text);
MarkStatus parseMarkStatus(const std::string& text);

/// "YYYY-MM-DD HH:MM:SS" in UTC.
std::string formatTime(Time t);
/// Accepts "YYYY-MM-DD HH:MM" or "YYYY-MM-DD HH:MM:SS", throws ValidationError.
Time parseTime(const std::string& text);
/// Current system time.
Time systemNow();
/// Mark with at most two decimals, e.g. "11" or "11.5".
std::string formatMark(double m);

}  // namespace lms
