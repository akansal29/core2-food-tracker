// USB serial command protocol (guide Section 9): one JSON object per line.
// Milestone 1 supports: hello, time_get, time_set, stats.
#pragma once

namespace serial_cmd {

void poll();  // call every loop; handles complete lines

}  // namespace serial_cmd
