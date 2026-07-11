/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include "always.h"
#include "MiniAudioDevice/MiniAudioCompat.h"
#include <stdint.h>
#include <vector>
#include <mutex>

class MiniAudioStream final
{
public:
    ma_data_source_base base; // MUST BE FIRST

    MiniAudioStream();
    ~MiniAudioStream();

    // Called by MiniAudioManager to provide engine reference at construction
    void setEngine(ma_engine *engine) { m_engine = engine; }

    // Push decoded PCM data (called by FFmpegVideoStream::onFrame)
    bool bufferData(uint8_t *data, size_t data_size, ma_format format, int samplerate, int channels);

    bool isPlaying();
    // Must be called every frame from FFmpegVideoStream::update()
    void update();
    void reset();

    void play();
    void pause();
    void stop();
    void setVolume(float vol);

    // Custom data source callbacks
    ma_result readPCM(void* pFramesOut, ma_uint64 frameCount, ma_uint64* pFramesRead);
    ma_result seekPCM(ma_uint64 frameIndex);
    ma_result getDataFormat(ma_format* pFormat, ma_uint32* pChannels, ma_uint32* pSampleRate, ma_channel* pChannelMap, size_t channelMapCap);

protected:
    void createSound();

    std::vector<uint8_t> m_buffer;
    std::mutex m_mutex;
    ma_engine *m_engine;
    ma_sound *m_sound;
    int m_sampleRate;
    int m_channels;
    ma_format m_format;
    bool m_initialized;
    bool m_playing;
    bool m_soundCreated;
    size_t m_readCursor;
};
