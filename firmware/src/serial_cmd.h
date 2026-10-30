// USB serial command protocol (guide Section 9): one JSON object per line.
// Supports: hello, time_get, time_set, stats, put_begin / put_chunk / put_commit (foods.json).
#pragma once

namespace serial_cmd {

void poll();       // call every loop; handles complete lines
bool isSyncing();  // true while a food list upload is in progress

}  // namespace serial_cmd
