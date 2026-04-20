// Parse and play WAV files using the Windows Multimedia API (winmm.lib).
// This file is for the editor only and is entirely AI-generated.

#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>

namespace EditorMusic
{
bool OpenFileDialogAndLoad(HWND ownerWindow);
bool LoadWavFile(const char* filePath);
void Update(bool isPlaying, float beatTime, float bpm);
void Shutdown();
const char* GetLoadedFilePath();
} // namespace EditorMusic
