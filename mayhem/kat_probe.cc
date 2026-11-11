// Known-answer probes for the headless core. Each line is "<name>: <value> PASS|FAIL";
// mayhem/test.sh counts them (and requires every expected name to be present).
#include <cstdint>
#include <cstdio>
#include <cstdarg>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <vector>
#include <zlib.h>

#include "core/base/patch.h"
#include "core/base/system.h"
#include "core/gb/gb.h"
#include "core/gb/gbGlobals.h"
#include "core/gba/gba.h"
#include "core/gba/gbaGlobals.h"
#include "vbam_env.h"

static int g_fail = 0;

static void report(const char* name, bool ok, const char* fmt, ...) __attribute__((format(printf, 3, 4)));
static void report(const char* name, bool ok, const char* fmt, ...) {
    char buf[128];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    printf("%s: %s %s\n", name, buf, ok ? "PASS" : "FAIL");
    if (!ok) {
        g_fail++;
    }
}

static void put32(std::vector<uint8_t>& v, size_t off, uint32_t x) { memcpy(&v[off], &x, 4); }

static void kat_gba_fib() {
    std::vector<uint8_t> rom(0x400, 0);
    static const uint32_t code[] = {
        0xE3A00000,  // mov r0, #0
        0xE3A01001,  // mov r1, #1
        0xE3A0200A,  // mov r2, #10
        0xE0803001,  // loop: add r3, r0, r1
        0xE1A00001,  //       mov r0, r1
        0xE1A01003,  //       mov r1, r3
        0xE2522001,  //       subs r2, r2, #1
        0x1AFFFFFA,  //       bne loop
        0xE3A04402,  // mov r4, #0x02000000
        0xE5840000,  // str r0, [r4]
        0xEAFFFFFE,  // b .
    };
    for (size_t i = 0; i < sizeof(code) / 4; i++) {
        put32(rom, 4 * i, code[i]);
    }
    uint32_t v = 0xFFFFFFFF;
    if (vbamGbaBoot(rom.data(), rom.size())) {
        vbamGbaRun(1);
        memcpy(&v, g_workRAM, 4);
        vbamGbaShutdown();
    }
    report("gba_arm_fib10", v == 55, "workram[0]=%u", v);
}

static void kat_gb_add() {
    std::vector<uint8_t> rom(0x8000, 0);
    static const uint8_t code[] = {0x3E, 0x12, 0xC6, 0x34, 0xEA, 0x00, 0xC0, 0x18, 0xFE};
    vbamBuildGbRom(rom.data(), rom.size(), code, sizeof(code));
    int v = -1;
    if (vbamGbBoot(rom.data(), rom.size())) {
        vbamGbRun(70000);
        v = gbMemoryMap[0xC][0];
        vbamGbShutdown();
    }
    report("gb_ld_add_store", v == 0x46, "wram[c000]=0x%02x", v);
}

static void kat_ips() {
    const char* tmpdir = getenv("TMPDIR");
    char path[4096];
    snprintf(path, sizeof(path), "%s/vbam-kat-XXXXXX.ips", tmpdir && *tmpdir ? tmpdir : "/tmp");
    int fd = mkstemps(path, 4);
    // "PATCH", record @0x000010 len 4 "ABCD", RLE record @0x000020 len 0 rle-len 8 'Z', "EOF"
    static const uint8_t ips[] = {'P', 'A', 'T', 'C', 'H', 0, 0, 0x10, 0, 4, 'A', 'B', 'C', 'D',
                                  0,   0, 0x20, 0, 0, 0, 8, 'Z', 'E', 'O', 'F'};
    ssize_t w = fd >= 0 ? write(fd, ips, sizeof(ips)) : -1;
    if (fd >= 0) {
        close(fd);
    }
    uint8_t* rom = (uint8_t*)calloc(1, 0x100);
    int size = 0x100;
    bool ok = w == (ssize_t)sizeof(ips) && applyPatch(path, &rom, &size);
    uint32_t sum = 0;
    for (int i = 0; ok && i < 0x100; i++) {
        sum = sum * 131 + rom[i];
    }
    bool match = ok && memcmp(rom + 0x10, "ABCD", 4) == 0 && memcmp(rom + 0x20, "ZZZZZZZZ", 8) == 0 &&
                 rom[0x0F] == 0 && rom[0x28] == 0;
    unlink(path);
    free(rom);
    report("ips_patch_apply", match, "size=%d", size);
}

static void kat_suite_frame() {
    FILE* f = fopen("src/core/test/suite.gba", "rb");
    std::vector<uint8_t> rom;
    if (f) {
        fseek(f, 0, SEEK_END);
        long n = ftell(f);
        fseek(f, 0, SEEK_SET);
        rom.resize((size_t)n);
        if (fread(rom.data(), 1, rom.size(), f) != rom.size()) {
            rom.clear();
        }
        fclose(f);
    }
    uLong crc = 0;
    if (!rom.empty() && vbamGbaBoot(rom.data(), rom.size())) {
        vbamGbaRun(40);
        crc = crc32(0, g_pix, 4 * 241 * 162);
        vbamGbaShutdown();
    }
    report("gba_suite_rom_frame40_crc", crc != 0, "crc32=%08lx", (unsigned long)crc);
}

int main() {
    kat_gba_fib();
    kat_gb_add();
    kat_ips();
    kat_suite_frame();
    printf("kat_probe: %d failing\n", g_fail);
    return g_fail ? 1 : 0;
}
