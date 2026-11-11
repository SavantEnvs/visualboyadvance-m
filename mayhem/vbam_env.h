#ifndef MAYHEM_VBAM_ENV_H_
#define MAYHEM_VBAM_ENV_H_

#include <cstddef>
#include <cstdint>

extern int vbamFramesDone;

bool vbamGbaBoot(const uint8_t* data, size_t size);
void vbamGbaRun(int frames);
void vbamGbaShutdown();

bool vbamGbBoot(const uint8_t* data, size_t size);
void vbamGbRun(int ticks);
void vbamGbShutdown();

void vbamBuildGbRom(uint8_t* rom, size_t size, const uint8_t* code, size_t codeLen);
bool vbamGbaBootBlank();
bool vbamGbBootBlank();

#endif
