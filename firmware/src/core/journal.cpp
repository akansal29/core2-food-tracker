#include "journal.h"

#include <stdio.h>
#include <stdlib.h>

#include <algorithm>

#include "timeutil.h"

namespace ft {

uint32_t crc32(const char* data, size_t len, uint32_t crc) {
  crc = ~crc;
  for (size_t i = 0; i < len; ++i) {
    crc ^= static_cast<uint8_t>(data[i]);
    for (int k = 0; k < 8; ++k) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
  }
  return ~crc;
}

namespace {

std::string clean(const std::string& s) {
  std::string out = s;
  for (char& c : out) {
    if (c == '|') c = '/';
    if (c == '\n' || c == '\r') c = ' ';
  }
  return out;
}

std::string fmtFloat(float v) {
  char buf[24];
  snprintf(buf, sizeof(buf), "%.2f", static_cast<double>(v));
  return buf;
}

std::string finish(const std::string& body) {
  char buf[12];
  snprintf(buf, sizeof(buf), "|%08x", static_cast<unsigned>(crc32(body.data(), body.size())));
  return body + buf;
}

std::vector<std::string> split(const std::string& s, char sep) {
  std::vector<std::string> parts;
  size_t start = 0;
  for (size_t i = 0; i <= s.size(); ++i) {
    if (i == s.size() || s[i] == sep) {
      parts.emplace_back(s.substr(start, i - start));
      start = i + 1;
    }
  }
  return parts;
}

bool parseFloat(const std::string& s, float& out) {
  if (s.empty()) return false;
  char* end = nullptr;
  out = strtof(s.c_str(), &end);
  return end && *end == '\0';
}

bool parseInt64(const std::string& s, int64_t& out) {
  if (s.empty()) return false;
  char* end = nullptr;
  out = strtoll(s.c_str(), &end, 10);
  return end && *end == '\0';
}

}  // namespace

std::string encodeAdd(uint32_t seq, const Entry& e) {
  std::string body = std::to_string(seq) + "|A|" + clean(e.id) + "|" + std::to_string(e.ts) + "|" +
                     static_cast<char>(e.kind) + "|" + clean(e.foodId) + "|" + fmtFloat(e.servings) + "|" +
                     fmtFloat(e.kcal) + "|" + fmtFloat(e.protein) + "|" + fmtFloat(e.waterOz) + "|" + clean(e.name);
  return finish(body);
}

std::string encodeEdit(uint32_t seq, const Entry& e) {
  std::string body = std::to_string(seq) + "|E|" + clean(e.id) + "|" + std::to_string(e.ts) + "|" +
                     fmtFloat(e.servings) + "|" + fmtFloat(e.kcal) + "|" + fmtFloat(e.protein) + "|" +
                     fmtFloat(e.waterOz);
  return finish(body);
}

std::string encodeDelete(uint32_t seq, const std::string& id) {
  return finish(std::to_string(seq) + "|D|" + clean(id));
}

std::string makeEntryId(int64_t ts, uint32_t seq) {
  char buf[40];
  snprintf(buf, sizeof(buf), "e_%lld_%04u", static_cast<long long>(ts), static_cast<unsigned>(seq % 10000));
  return buf;
}

bool Ledger::applyLine(const std::string& line) {
  size_t bar = line.rfind('|');
  if (bar == std::string::npos || line.size() - bar != 9) return false;
  uint32_t want = static_cast<uint32_t>(strtoul(line.c_str() + bar + 1, nullptr, 16));
  if (crc32(line.data(), bar) != want) return false;

  std::vector<std::string> f = split(line.substr(0, bar), '|');
  if (f.size() < 3 || f[1].size() != 1) return false;
  int64_t seq = 0;
  if (!parseInt64(f[0], seq) || seq < 0) return false;
  const char op = f[1][0];

  bool ok = false;
  if (op == 'A' && f.size() >= 11) {
    Entry e;
    e.id = f[2];
    if (!parseInt64(f[3], e.ts) || f[4].size() != 1) return false;
    e.kind = static_cast<EntryKind>(f[4][0]);
    e.foodId = f[5];
    if (!parseFloat(f[6], e.servings) || !parseFloat(f[7], e.kcal) || !parseFloat(f[8], e.protein) ||
        !parseFloat(f[9], e.waterOz))
      return false;
    // Name is the rest (it was cleaned of '|', but join defensively).
    e.name = f[10];
    for (size_t i = 11; i < f.size(); ++i) e.name += "/" + f[i];
    if (find(e.id)) return false;  // duplicate id: never double-count
    add(e);
    ok = true;
  } else if (op == 'E' && f.size() == 8) {
    Entry e;
    e.id = f[2];
    if (!parseInt64(f[3], e.ts) || !parseFloat(f[4], e.servings) || !parseFloat(f[5], e.kcal) ||
        !parseFloat(f[6], e.protein) || !parseFloat(f[7], e.waterOz))
      return false;
    ok = edit(e);
  } else if (op == 'D' && f.size() == 3) {
    ok = remove(f[2]);
  }
  if (ok && static_cast<uint32_t>(seq) > lastSeq_) lastSeq_ = static_cast<uint32_t>(seq);
  return ok;
}

size_t Ledger::replay(const std::string& contents, size_t* badLines) {
  size_t good = 0, bad = 0, start = 0;
  while (start < contents.size()) {
    size_t nl = contents.find('\n', start);
    if (nl == std::string::npos) {  // torn final line
      ++bad;
      break;
    }
    std::string line = contents.substr(start, nl - start);
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (!line.empty()) {
      if (applyLine(line)) ++good; else ++bad;
    }
    start = nl + 1;
  }
  if (badLines) *badLines = bad;
  return good;
}

void Ledger::add(const Entry& e) { entries_.push_back(e); }

bool Ledger::edit(const Entry& e) {
  for (Entry& x : entries_) {
    if (x.id == e.id) {
      x.ts = e.ts;
      x.servings = e.servings;
      x.kcal = e.kcal;
      x.protein = e.protein;
      x.waterOz = e.waterOz;
      x.edited = true;
      return true;
    }
  }
  return false;
}

bool Ledger::remove(const std::string& id) {
  auto it = std::find_if(entries_.begin(), entries_.end(), [&](const Entry& x) { return x.id == id; });
  if (it == entries_.end()) return false;
  entries_.erase(it);
  return true;
}

const Entry* Ledger::find(const std::string& id) const {
  for (const Entry& x : entries_)
    if (x.id == id) return &x;
  return nullptr;
}

Totals Ledger::totalsFor(uint32_t day) const {
  Totals t;
  for (const Entry& e : entries_) {
    if (trackingDay(e.ts) != day) continue;
    t.kcal += e.kcal;
    t.protein += e.protein;
    t.waterOz += e.waterOz;
    if (e.kind != EntryKind::Water) {
      ++t.foodCount;
      if (e.ts > t.lastFoodTs) t.lastFoodTs = e.ts;
    }
  }
  return t;
}

std::vector<const Entry*> Ledger::entriesFor(uint32_t day) const {
  std::vector<const Entry*> out;
  for (const Entry& e : entries_)
    if (trackingDay(e.ts) == day) out.push_back(&e);
  std::sort(out.begin(), out.end(), [](const Entry* a, const Entry* b) { return a->ts > b->ts; });
  return out;
}

}  // namespace ft
