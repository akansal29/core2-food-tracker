// Append-only journal of food and water entries (guide Section 8).
//
// Every change is a new text line:   <seq>|<op>|<fields...>|<crc32 hex>
//   A  add     id|ts|kind|food_id|servings|kcal|protein|water_oz|name
//   E  edit    id|ts|servings|kcal|protein|water_oz
//   D  delete  id
// The CRC covers everything before the final '|'. A line with a bad CRC or a
// missing newline (torn write after power loss) is ignored on replay.
#pragma once
#include <stdint.h>

#include <string>
#include <vector>

namespace ft {

enum class EntryKind : char { Food = 'f', Combo = 'c', Custom = 'u', Water = 'w' };

struct Entry {
  std::string id;       // e.g. e_1792640000_0007 (unique; also guards against duplicates)
  int64_t ts = 0;       // local seconds
  EntryKind kind = EntryKind::Food;
  std::string foodId;   // empty for water and custom
  std::string name;     // snapshot at logging time
  float servings = 1;
  float kcal = 0;       // for the logged amount (snapshot)
  float protein = 0;    // grams, for the logged amount
  float waterOz = 0;    // US fl oz
  bool edited = false;
};

struct Totals {
  float kcal = 0;
  float protein = 0;
  float waterOz = 0;
  uint32_t foodCount = 0;
  int64_t lastFoodTs = 0;
};

uint32_t crc32(const char* data, size_t len, uint32_t crc = 0);

std::string encodeAdd(uint32_t seq, const Entry& e);
std::string encodeEdit(uint32_t seq, const Entry& e);
std::string encodeDelete(uint32_t seq, const std::string& id);

// In-memory view rebuilt by replaying the journal.
class Ledger {
 public:
  // Applies one journal line (without trailing newline). Returns false if the
  // line is corrupt or refers to an unknown entry; the ledger is unchanged then.
  bool applyLine(const std::string& line);

  // Replays a whole journal file's contents. Returns the number of good lines.
  // A final line without '\n' is treated as torn and skipped.
  size_t replay(const std::string& contents, size_t* badLines = nullptr);

  // Direct mutation used after the line has been durably written.
  void add(const Entry& e);
  bool edit(const Entry& e);
  bool remove(const std::string& id);

  const Entry* find(const std::string& id) const;
  Totals totalsFor(uint32_t trackingDay) const;
  std::vector<const Entry*> entriesFor(uint32_t trackingDay) const;  // newest first
  size_t size() const { return entries_.size(); }
  const std::vector<Entry>& all() const { return entries_; }
  uint32_t lastSeq() const { return lastSeq_; }

 private:
  std::vector<Entry> entries_;
  uint32_t lastSeq_ = 0;
};

// Builds a unique entry id from a timestamp and sequence number.
std::string makeEntryId(int64_t ts, uint32_t seq);

}  // namespace ft
