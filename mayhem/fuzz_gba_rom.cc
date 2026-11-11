// GBA emulator: boot arbitrary cartridge bytes headlessly and run a few frames.
#include <cstddef>
#include <cstdint>

#include "vbam_env.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 16) {
        return -1;
    }
    if (!vbamGbaBoot(data, size)) {
        return -1;
    }
    vbamGbaRun(3);
    vbamGbaShutdown();
    return 0;
}
