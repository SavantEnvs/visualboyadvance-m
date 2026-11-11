// Frontend stubs the VBA-M core expects an embedder to provide, plus the
// helpers every harness shares (headless boot of a GB / GBA core).
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>

#include "core/base/system.h"
#include "core/gb/gb.h"
#include "core/gb/gbGlobals.h"
#include "core/gb/gbSound.h"
#include "core/gba/gba.h"
#include "core/gba/gbaEeprom.h"
#include "core/gba/gbaFlash.h"
#include "core/gba/gbaGlobals.h"
#include "core/gba/gbaSound.h"

#include "vbam_env.h"

struct CoreOptions coreOptions;

void systemMessage(int, const char*, ...) {}
void log(const char*, ...) {}
bool systemPauseOnFrame() { return false; }
void systemGbPrint(uint8_t*, int, int, int, int, int) {}
void systemScreenCapture(int) {}
void systemDrawScreen() {}
void systemSendScreen() {}
bool systemReadJoypads() { return true; }
uint32_t systemReadJoypad(int) { return 0; }
uint32_t systemGetClock() { return 0; }
void systemSetTitle(const char*) {}

namespace {
class NullSoundDriver : public SoundDriver {
  public:
    bool init(long) override { return true; }
    void pause() override {}
    void reset() override {}
    void resume() override {}
    void write(uint16_t*, int) override {}
    void setThrottle(unsigned short) override {}
};
}  // namespace

std::unique_ptr<SoundDriver> systemSoundInit() {
    return std::unique_ptr<SoundDriver>(new NullSoundDriver);
}
void systemOnWriteDataToSoundBuffer(const uint16_t*, int) {}
void systemOnSoundShutdown() {}
void systemScreenMessage(const char*) {}
void systemUpdateMotionSensor() {}
int systemGetSensorX() { return 0; }
int systemGetSensorY() { return 0; }
int systemGetSensorZ() { return 0; }
uint8_t systemGetSensorDarkness() { return 0; }
void systemCartridgeRumble(bool) {}
void systemPossibleCartridgeRumble(bool) {}
void updateRumbleFrame() {}
bool systemCanChangeSoundQuality() { return false; }
void systemShowSpeed(int) {}
void system10Frames() {}
int vbamFramesDone = 0;
void systemFrame() { ++vbamFramesDone; }
void systemGbBorderOn() {}
static void nullDbgOutput(const char*, uint32_t) {}
static void nullDbgSignal(int, int) {}
void (*dbgOutput)(const char* s, uint32_t addr) = nullDbgOutput;
void (*dbgSignal)(int sig, int number) = nullDbgSignal;

uint8_t systemColorMap8[0x10000];
uint16_t systemColorMap16[0x10000];
uint32_t systemColorMap32[0x10000];
uint16_t systemGbPalette[24];
int systemRedShift = 0;
int systemGreenShift = 0;
int systemBlueShift = 0;
int systemColorDepth = 32;
int systemVerbose = 0;
int systemFrameSkip = 0;
int systemSaveUpdateCounter = 0;
int systemSpeed = 0;
int emulating = 0;

namespace {

void setupColorMap() {
    systemRedShift = 19;
    systemGreenShift = 11;
    systemBlueShift = 3;
    for (int i = 0; i < 0x10000; i++) {
        systemColorMap32[i] = (uint32_t)(((i & 0x1f) << systemRedShift) |
                                         (((i & 0x3e0) >> 5) << systemGreenShift) |
                                         (((i & 0x7c00) >> 10) << systemBlueShift));
    }
}

}  // namespace

// ---- GBA ----

bool vbamGbaBoot(const uint8_t* data, size_t size) {
    static bool soundReady = false;
    if (size == 0 || size > 0x2000000) {
        return false;
    }
    if (!soundReady) {
        setupColorMap();
        soundInit();
        soundReady = true;
    }
    coreOptions.saveType = GBA_SAVE_AUTO;
    coreOptions.skipBios = true;
    if (CPULoadRomData((const char*)data, (int)size) == 0) {
        return false;
    }
    flashDetectSaveType((int)size);
    if (coreOptions.saveType == GBA_SAVE_EEPROM) {
        eepromSetSize(SIZE_EEPROM_512);
    }
    CPUInit("", false);
    CPUReset();
    emulating = 1;
    return true;
}

void vbamGbaRun(int frames) {
    const int kFrameCycles = 280896;
    for (int i = 0; i < frames; i++) {
        CPULoop(kFrameCycles);
    }
}

void vbamGbaShutdown() {
    emulating = 0;
    CPUCleanUp();
}

// ---- GB ----

bool vbamGbBoot(const uint8_t* data, size_t size) {
    static bool soundReady = false;
    if (size == 0) {
        return false;
    }
    if (!soundReady) {
        setupColorMap();
        soundInit();
        soundReady = true;
    }
    gbBorderOn = false;
    systemGbPalette[0] = 0x7FFF;
    systemGbPalette[1] = 0x56B5;
    systemGbPalette[2] = 0x294A;
    systemGbPalette[3] = 0x0000;
    for (int i = 4; i < 24; i++) {
        systemGbPalette[i] = systemGbPalette[i & 3];
    }
    if (!gbLoadRomData((const char*)data, size)) {
        return false;
    }
    gbGetHardwareType();
    gbCPUInit("", false);
    gbReset();
    emulating = 1;
    return true;
}

void vbamGbRun(int ticks) {
    GBSystem.emuMain(ticks);
}

void vbamGbShutdown() {
    emulating = 0;
    gbCleanUp();
}

// ---- fixed synthetic cartridges (for harnesses that fuzz something other than the ROM) ----

bool vbamGbaBootBlank() {
    static uint8_t rom[0x400];
    memset(rom, 0, sizeof(rom));
    const uint32_t spin = 0xEAFFFFFE;  // b .
    memcpy(rom, &spin, sizeof(spin));
    return vbamGbaBoot(rom, sizeof(rom));
}

void vbamBuildGbRom(uint8_t* rom, size_t size, const uint8_t* code, size_t codeLen) {
    static const uint8_t kLogo[0x30] = {
        0xce, 0xed, 0x66, 0x66, 0xcc, 0x0d, 0x00, 0x0b, 0x03, 0x73, 0x00, 0x83,
        0x00, 0x0c, 0x00, 0x0d, 0x00, 0x08, 0x11, 0x1f, 0x88, 0x89, 0x00, 0x0e,
        0xdc, 0xcc, 0x6e, 0xe6, 0xdd, 0xdd, 0xd9, 0x99, 0xbb, 0xbb, 0x67, 0x63,
        0x6e, 0x0e, 0xec, 0xcc, 0xdd, 0xdc, 0x99, 0x9f, 0xbb, 0xb9, 0x33, 0x3e,
    };
    memset(rom, 0, size);
    memcpy(rom + 0x104, kLogo, sizeof(kLogo));
    memcpy(rom + 0x100, "\x00\xC3\x50\x01", 4);  // nop; jp 0x0150
    memcpy(rom + 0x150, code, codeLen);
    memcpy(rom + 0x134, "VBAMKAT", 7);
    uint8_t sum = 0;
    for (size_t a = 0x134; a <= 0x14c; a++) {
        sum = (uint8_t)(sum - rom[a] - 1);
    }
    rom[0x14d] = sum;
}

bool vbamGbBootBlank() {
    static uint8_t rom[0x8000];
    static const uint8_t spin[] = {0x18, 0xFE};  // jr -2
    vbamBuildGbRom(rom, sizeof(rom), spin, sizeof(spin));
    return vbamGbBoot(rom, sizeof(rom));
}
