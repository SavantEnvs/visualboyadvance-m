// Game Boy cheat-code parsers (GameShark and Game Genie). The input is a list of
// newline-separated codes; the first byte selects the parser. Codes are then applied by
// running a frame, which executes the resulting cheat memory patches.
#include <cstddef>
#include <cstdint>
#include <string>

#include "core/gb/gbCheats.h"
#include "vbam_env.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 4) {
        return -1;
    }
    if (!vbamGbBootBlank()) {
        return -1;
    }
    const bool gameshark = (data[0] & 1) == 0;
    std::string text((const char*)data + 1, size - 1);
    size_t pos = 0;
    int lines = 0;
    while (pos <= text.size() && lines < 32) {
        size_t nl = text.find('\n', pos);
        if (nl == std::string::npos) {
            nl = text.size();
        }
        std::string code = text.substr(pos, nl - pos);
        if (gameshark) {
            gbAddGsCheat(code.c_str(), "fuzz");
        } else {
            gbAddGgCheat(code.c_str(), "fuzz");
        }
        pos = nl + 1;
        lines++;
    }
    vbamGbRun(70000);
    gbCheatRemoveAll();
    vbamGbShutdown();
    return 0;
}
