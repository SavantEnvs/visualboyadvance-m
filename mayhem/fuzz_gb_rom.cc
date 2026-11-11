// Game Boy / Color / Super Game Boy emulator: boot arbitrary cartridge bytes and run a few frames.
#include <cstddef>
#include <cstdint>

#include "vbam_env.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 0x150) {
        return -1;
    }
    if (!vbamGbBoot(data, size)) {
        return -1;
    }
    for (int i = 0; i < 3; i++) {
        vbamGbRun(70000);
    }
    vbamGbShutdown();
    return 0;
}
