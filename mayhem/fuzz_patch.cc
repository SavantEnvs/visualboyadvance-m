// ROM patch loaders (IPS / UPS / BPS / PPF). The first input byte picks the format, the rest
// is the patch file, applied to a fixed 16 KiB base image.
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

#include "core/base/patch.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    static const char* const kSuffix[] = {".ips", ".ups", ".bps", ".ppf"};
    if (size < 2) {
        return -1;
    }
    const char* suffix = kSuffix[data[0] & 3];
    data++;
    size--;

    const char* tmpdir = getenv("TMPDIR");
    char path[4096];
    snprintf(path, sizeof(path), "%s/vbam-patch-XXXXXX%s", tmpdir && *tmpdir ? tmpdir : "/tmp",
             suffix);
    int fd = mkstemps(path, 4);
    if (fd < 0) {
        return -1;
    }
    size_t off = 0;
    while (off < size) {
        ssize_t w = write(fd, data + off, size - off);
        if (w <= 0) {
            break;
        }
        off += (size_t)w;
    }
    close(fd);

    const int kBase = 0x4000;
    uint8_t* rom = (uint8_t*)malloc(kBase);
    for (int i = 0; i < kBase; i++) {
        rom[i] = (uint8_t)(i * 31 + (i >> 8));
    }
    int romSize = kBase;
    applyPatch(path, &rom, &romSize);
    free(rom);
    unlink(path);
    return 0;
}
