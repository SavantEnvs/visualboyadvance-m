// GBA cheat-code parsers (raw, GameShark v1/v3, CodeBreaker, "master" codes). The input is a
// list of newline-separated codes; the first byte selects the parser. Codes are then applied
// by running a frame, which executes the resulting cheat memory writes.
#include <cstddef>
#include <cstdint>
#include <string>

#include "core/gba/gbaCheats.h"
#include "vbam_env.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 4) {
        return -1;
    }
    if (!vbamGbaBootBlank()) {
        return -1;
    }
    const int kind = data[0] & 3;
    std::string text((const char*)data + 1, size - 1);
    size_t pos = 0;
    int lines = 0;
    while (pos <= text.size() && lines < 32) {
        size_t nl = text.find('\n', pos);
        if (nl == std::string::npos) {
            nl = text.size();
        }
        std::string code = text.substr(pos, nl - pos);
        switch (kind) {
        case 0: cheatsAddCheatCode(code.c_str(), "fuzz"); break;
        case 1: cheatsAddGSACode(code.c_str(), "fuzz", false); break;
        case 2: cheatsAddGSACode(code.c_str(), "fuzz", true); break;
        default: cheatsAddCBACode(code.c_str(), "fuzz"); break;
        }
        pos = nl + 1;
        lines++;
    }
    vbamGbaRun(1);
    cheatsDeleteAll(false);
    vbamGbaShutdown();
    return 0;
}
