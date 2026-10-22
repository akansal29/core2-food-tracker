// Persistent storage on internal flash (LittleFS). Guide Section 8.
#pragma once
#include <string>

#include "app.h"

namespace storage {

bool begin();  // mounts (formats on first boot), creates folders

// Journal: append one line and flush before returning. Returns false on any failure.
bool appendJournal(const std::string& line);
// Replays /data/journal.log into the ledger. Returns false if the file could not be read.
bool loadJournal(ft::Ledger& ledger, uint32_t& badLines);

// Settings: atomic replace (write temp file, then rename).
bool saveSettings(const app::Settings& s);
bool loadSettings(app::Settings& s);

// Generic atomic write used by settings now and food state later.
bool writeFileAtomic(const char* path, const std::string& contents);

size_t usedBytes();
size_t totalBytes();
size_t journalBytes();

}  // namespace storage
