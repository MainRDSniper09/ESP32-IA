#include "TimeUtils.h"

#include <stdio.h>
#include <string.h>

namespace agent {
namespace timeutil {

namespace {

bool isDigit(char c) { return c >= '0' && c <= '9'; }

int twoDigits(const std::string& s, size_t pos) { return (s[pos] - '0') * 10 + (s[pos + 1] - '0'); }

bool isLeap(int y) { return (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0); }

int daysInMonth(int y, int m) {
  static const int kDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (m == 2 && isLeap(y)) return 29;
  return kDays[m - 1];
}

}  // namespace

void toLocal(time_t t, struct tm& out) {
#if defined(_WIN32)
  out = *localtime(&t);  // MinGW (solo pruebas en Windows)
#else
  localtime_r(&t, &out);
#endif
}

bool parseHHMM(const std::string& s, int& hour, int& minute) {
  if (s.size() != 5 || s[2] != ':') return false;
  if (!isDigit(s[0]) || !isDigit(s[1]) || !isDigit(s[3]) || !isDigit(s[4])) return false;
  int h = twoDigits(s, 0);
  int m = twoDigits(s, 3);
  if (h > 23 || m > 59) return false;
  hour = h;
  minute = m;
  return true;
}

bool parseDate(const std::string& s, int& year, int& month, int& day) {
  if (s.size() != 10 || s[4] != '-' || s[7] != '-') return false;
  for (size_t i = 0; i < s.size(); ++i) {
    if (i == 4 || i == 7) continue;
    if (!isDigit(s[i])) return false;
  }
  int y = (s[0] - '0') * 1000 + (s[1] - '0') * 100 + (s[2] - '0') * 10 + (s[3] - '0');
  int m = twoDigits(s, 5);
  int d = twoDigits(s, 8);
  if (y < 2024 || y > 2099 || m < 1 || m > 12 || d < 1 || d > daysInMonth(y, m)) return false;
  year = y;
  month = m;
  day = d;
  return true;
}

bool resolveLocalTime(time_t now, int hour, int minute, const std::string& date, time_t& out) {
  struct tm tmv;
  toLocal(now, tmv);
  if (!date.empty()) {
    int y, mo, d;
    if (!parseDate(date, y, mo, d)) return false;
    tmv.tm_year = y - 1900;
    tmv.tm_mon = mo - 1;
    tmv.tm_mday = d;
  }
  tmv.tm_hour = hour;
  tmv.tm_min = minute;
  tmv.tm_sec = 0;
  tmv.tm_isdst = -1;  // que mktime determine el horario de verano
  time_t t = mktime(&tmv);
  if (t == static_cast<time_t>(-1)) return false;
  if (t <= now) {
    if (!date.empty()) return false;  // fecha explícita en el pasado
    t = addLocalDays(t, 1);
  }
  out = t;
  return true;
}

time_t addLocalDays(time_t t, int days) {
  struct tm tmv;
  toLocal(t, tmv);
  tmv.tm_mday += days;
  tmv.tm_isdst = -1;
  return mktime(&tmv);
}

std::string toIso8601(time_t t) {
  struct tm lt, gt;
  toLocal(t, lt);
#if defined(_WIN32)
  gt = *gmtime(&t);
#else
  gmtime_r(&t, &gt);
#endif
  // Desfase local-UTC calculado a mano (el %z de strftime no es portable).
  int dayDiff = lt.tm_yday - gt.tm_yday;
  if (lt.tm_year != gt.tm_year) dayDiff = lt.tm_year > gt.tm_year ? 1 : -1;
  long off = dayDiff * 86400L + (lt.tm_hour - gt.tm_hour) * 3600L + (lt.tm_min - gt.tm_min) * 60L;
  char sign = off < 0 ? '-' : '+';
  if (off < 0) off = -off;

  char buf[64];
  snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02d%c%02ld:%02ld", lt.tm_year + 1900, lt.tm_mon + 1,
           lt.tm_mday, lt.tm_hour, lt.tm_min, lt.tm_sec, sign, off / 3600, (off % 3600) / 60);
  return std::string(buf);
}

std::string toHHMM(time_t t) {
  struct tm tmv;
  toLocal(t, tmv);
  char buf[16];
  snprintf(buf, sizeof(buf), "%02d:%02d", tmv.tm_hour, tmv.tm_min);
  return std::string(buf);
}

std::string toShort(time_t t) {
  struct tm tmv;
  toLocal(t, tmv);
  char buf[32];
  snprintf(buf, sizeof(buf), "%02d/%02d %02d:%02d", tmv.tm_mday, tmv.tm_mon + 1, tmv.tm_hour, tmv.tm_min);
  return std::string(buf);
}

}  // namespace timeutil
}  // namespace agent
