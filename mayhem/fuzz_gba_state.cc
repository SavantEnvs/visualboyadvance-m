// GBA save-state loader (the in-memory gzip-framed form used for rewind / .sgm files):
// restore an arbitrary state blob onto a freshly booted core, then run a frame.
#include <cstddef>
#include <cstdint>
#include <vector>

#include "core/base/system.h"
#include "core/gba/gba.h"
#include "vbam_env.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 16) {
        return -1;
    }
    if (!vbamGbaBootBlank()) {
        return -1;
    }
    std::vector<char> blob(data, data + size);
    if (GBASystem.emuReadMemState(blob.data(), (int)blob.size())) {
        vbamGbaRun(1);
    }
    vbamGbaShutdown();
    return 0;
}
