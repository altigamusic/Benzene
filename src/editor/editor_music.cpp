// Parse and play WAV files using the Windows Multimedia API (winmm.lib).
// This file is for the editor only and is entirely AI-generated.

#define WIN32_LEAN_AND_MEAN
#define WIN32_EXTRA_LEAN
#include "editor_music.h"

#include <commdlg.h>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <mmreg.h>
#include <mmsystem.h>
#include <string>
#include <vector>

namespace
{
struct WavData
{
    WAVEFORMATEX format = {};
    std::vector<unsigned char> data;
    std::string filePath;
};

WavData g_wav;
HWAVEOUT g_waveOut = nullptr;
WAVEHDR g_waveHeader = {};
bool g_hasPreparedHeader = false;
bool g_isPlaying = false;
DWORD g_startSystemMs = 0;
DWORD g_startByteOffset = 0;

bool readU32LE(const unsigned char* p, uint32_t* out)
{
    if (out == nullptr) return false;
    *out = static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) | (static_cast<uint32_t>(p[2]) << 16) |
           (static_cast<uint32_t>(p[3]) << 24);
    return true;
}

bool closeWaveOutDevice()
{
    if (g_waveOut == nullptr) return true;

    waveOutReset(g_waveOut);

    if (g_hasPreparedHeader)
    {
        waveOutUnprepareHeader(g_waveOut, &g_waveHeader, sizeof(g_waveHeader));
        g_hasPreparedHeader = false;
    }

    MMRESULT closeResult = waveOutClose(g_waveOut);
    g_waveOut = nullptr;
    g_waveHeader = {};
    g_isPlaying = false;
    g_startSystemMs = 0;
    g_startByteOffset = 0;

    return closeResult == MMSYSERR_NOERROR;
}

bool openWaveOutDevice()
{
    if (g_wav.data.empty()) return false;
    if (!closeWaveOutDevice()) return false;

    MMRESULT result = waveOutOpen(&g_waveOut, WAVE_MAPPER, &g_wav.format, 0, 0, CALLBACK_NULL);
    return result == MMSYSERR_NOERROR;
}

bool restartFromOffset(DWORD byteOffset)
{
    if (g_waveOut == nullptr) return false;
    if (g_wav.data.empty()) return false;

    DWORD maxOffset = static_cast<DWORD>(g_wav.data.size());
    byteOffset = std::min(byteOffset, maxOffset);

    const DWORD blockAlign = std::max<DWORD>(g_wav.format.nBlockAlign, 1);
    byteOffset -= byteOffset % blockAlign;

    waveOutReset(g_waveOut);

    if (g_hasPreparedHeader)
    {
        waveOutUnprepareHeader(g_waveOut, &g_waveHeader, sizeof(g_waveHeader));
        g_hasPreparedHeader = false;
    }

    g_waveHeader = {};
    g_waveHeader.lpData = reinterpret_cast<LPSTR>(g_wav.data.data() + byteOffset);
    g_waveHeader.dwBufferLength = maxOffset - byteOffset;

    MMRESULT prepareResult = waveOutPrepareHeader(g_waveOut, &g_waveHeader, sizeof(g_waveHeader));
    if (prepareResult != MMSYSERR_NOERROR) return false;

    g_hasPreparedHeader = true;

    MMRESULT writeResult = waveOutWrite(g_waveOut, &g_waveHeader, sizeof(g_waveHeader));
    if (writeResult != MMSYSERR_NOERROR)
    {
        waveOutUnprepareHeader(g_waveOut, &g_waveHeader, sizeof(g_waveHeader));
        g_hasPreparedHeader = false;
        return false;
    }

    g_startSystemMs = timeGetTime();
    g_startByteOffset = byteOffset;
    g_isPlaying = true;
    return true;
}

DWORD beatsToByteOffset(float beatTime, float bpm)
{
    if (g_wav.format.nAvgBytesPerSec == 0) return 0;
    if (bpm <= 0.0f) return 0;

    const float seconds = beatTime * 60.0f / bpm;
    const float positiveSeconds = std::max(0.0f, seconds);

    double rawOffset = std::max(0.0, static_cast<double>(positiveSeconds) * static_cast<double>(g_wav.format.nAvgBytesPerSec));

    DWORD byteOffset = static_cast<DWORD>(rawOffset);
    DWORD maxOffset = static_cast<DWORD>(g_wav.data.size());
    if (byteOffset > maxOffset) byteOffset = maxOffset;

    const DWORD blockAlign = std::max<DWORD>(g_wav.format.nBlockAlign, 1);
    byteOffset -= byteOffset % blockAlign;
    return byteOffset;
}

bool parseWavFile(const char* filePath, WavData* outWav)
{
    if (filePath == nullptr || outWav == nullptr) return false;

    std::ifstream file(filePath, std::ios::binary);
    if (!file.good()) return false;

    std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    if (bytes.size() < 12) return false;

    if (memcmp(bytes.data(), "RIFF", 4) != 0 || memcmp(bytes.data() + 8, "WAVE", 4) != 0) return false;

    bool hasFmt = false;
    bool hasData = false;

    WAVEFORMATEX parsedFormat = {};
    std::vector<unsigned char> parsedData;

    size_t offset = 12;
    while (offset + 8 <= bytes.size())
    {
        const unsigned char* chunk = bytes.data() + offset;
        uint32_t chunkSize = 0;
        readU32LE(chunk + 4, &chunkSize);

        const size_t chunkDataOffset = offset + 8;
        if (chunkDataOffset + chunkSize > bytes.size()) break;

        if (memcmp(chunk, "fmt ", 4) == 0)
        {
            if (chunkSize < 16) return false;

            memset(&parsedFormat, 0, sizeof(parsedFormat));
            const size_t toCopy = std::min<size_t>(chunkSize, sizeof(WAVEFORMATEX));
            memcpy(&parsedFormat, bytes.data() + chunkDataOffset, toCopy);

            if (chunkSize > 16)
                parsedFormat.cbSize = static_cast<WORD>(std::min<uint32_t>(chunkSize - 16, 0xFFFFu));
            else
                parsedFormat.cbSize = 0;

            if (parsedFormat.nBlockAlign == 0 || parsedFormat.nAvgBytesPerSec == 0) return false;
            if (!(parsedFormat.wFormatTag == WAVE_FORMAT_PCM || parsedFormat.wFormatTag == WAVE_FORMAT_IEEE_FLOAT ||
                    parsedFormat.wFormatTag == WAVE_FORMAT_EXTENSIBLE))
            {
                return false;
            }

            hasFmt = true;
        }
        else if (memcmp(chunk, "data", 4) == 0)
        {
            parsedData.assign(bytes.begin() + static_cast<long long>(chunkDataOffset),
                bytes.begin() + static_cast<long long>(chunkDataOffset + chunkSize));
            hasData = true;
        }

        offset = chunkDataOffset + chunkSize;
        if (chunkSize % 2 != 0) offset++;
    }

    if (!hasFmt || !hasData) return false;

    outWav->format = parsedFormat;
    outWav->data = std::move(parsedData);
    outWav->filePath = filePath;
    return true;
}
} // namespace

namespace EditorMusic
{

bool OpenFileDialogAndLoad(HWND ownerWindow)
{
    char selectedPath[MAX_PATH] = {};

    OPENFILENAMEA ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = ownerWindow;
    ofn.lpstrFile = selectedPath;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = "WAV files (*.wav)\0*.wav\0All files (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

    if (!GetOpenFileNameA(&ofn)) return false;

    return LoadWavFile(selectedPath);
}

bool LoadWavFile(const char* filePath)
{
    WavData loadedWav;
    if (!parseWavFile(filePath, &loadedWav)) return false;

    g_wav = std::move(loadedWav);
    g_waveHeader = {};
    g_hasPreparedHeader = false;
    g_isPlaying = false;

    return openWaveOutDevice();
}

void Update(bool isPlaying, float beatTime, float bpm)
{
    if (g_waveOut == nullptr || g_wav.data.empty()) return;

    if (!isPlaying)
    {
        if (g_isPlaying)
        {
            waveOutReset(g_waveOut);
            g_isPlaying = false;
        }
        return;
    }

    const DWORD desiredOffset = beatsToByteOffset(beatTime, bpm);

    if (!g_isPlaying)
    {
        restartFromOffset(desiredOffset);
        return;
    }

    const DWORD now = timeGetTime();
    const DWORD elapsedMs = now - g_startSystemMs;
    const DWORD elapsedBytes = static_cast<DWORD>((static_cast<unsigned long long>(elapsedMs) * g_wav.format.nAvgBytesPerSec) / 1000ULL);

    DWORD expectedOffset = g_startByteOffset + elapsedBytes;
    DWORD maxOffset = static_cast<DWORD>(g_wav.data.size());
    if (expectedOffset > maxOffset) expectedOffset = maxOffset;

    const DWORD thresholdBytes = std::max<DWORD>(g_wav.format.nAvgBytesPerSec / 10, g_wav.format.nBlockAlign);
    const DWORD distance = (desiredOffset > expectedOffset) ? (desiredOffset - expectedOffset) : (expectedOffset - desiredOffset);

    const bool didFinishBuffer = (g_waveHeader.dwFlags & WHDR_DONE) != 0;

    if (didFinishBuffer || distance > thresholdBytes)
    {
        restartFromOffset(desiredOffset);
    }
}

void Shutdown()
{
    closeWaveOutDevice();
    g_wav = {};
}

const char* GetLoadedFilePath() { return g_wav.filePath.empty() ? "" : g_wav.filePath.c_str(); }

} // namespace EditorMusic
