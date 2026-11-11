// ROM archive extraction (zip / 7z / rar / gz / bz2 / xz / tar ...) through fex, as the
// frontends do when a user opens a compressed cartridge. Format is sniffed from the header.
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>

#include "core/base/file_util.h"

static bool acceptAny(const char*) { return true; }

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 8) {
        return -1;
    }
    const char* tmpdir = getenv("TMPDIR");
    char path[4096];
    snprintf(path, sizeof(path), "%s/vbam-arc-XXXXXX", tmpdir && *tmpdir ? tmpdir : "/tmp");
    int fd = mkstemp(path);
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

    int outSize = 0;
    uint8_t* image = utilLoad(path, acceptAny, nullptr, outSize);
    free(image);
    unlink(path);
    return 0;
}
