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

// FILE: MiniAudioStream.cpp ////////////////////////////////////////////////////////////////////////
// MiniAudioStream - Streaming audio for video playback via miniaudio
// Author: GeneralsX Contributors, June 2026

#include "MiniAudioDevice/MiniAudioStream.h"
#include "Lib/BaseType.h"
#include <cstring>

static ma_result MiniAudioStream_read(ma_data_source* pDataSource, void* pFramesOut, ma_uint64 frameCount, ma_uint64* pFramesRead)
{
    return ((MiniAudioStream*)pDataSource)->readPCM(pFramesOut, frameCount, pFramesRead);
}

static ma_result MiniAudioStream_seek(ma_data_source* pDataSource, ma_uint64 frameIndex)
{
    return ((MiniAudioStream*)pDataSource)->seekPCM(frameIndex);
}

static ma_result MiniAudioStream_get_data_format(ma_data_source* pDataSource, ma_format* pFormat, ma_uint32* pChannels, ma_uint32* pSampleRate, ma_channel* pChannelMap, size_t channelMapCap)
{
    return ((MiniAudioStream*)pDataSource)->getDataFormat(pFormat, pChannels, pSampleRate, pChannelMap, channelMapCap);
}

static ma_data_source_vtable g_MiniAudioStream_vtable = {
    MiniAudioStream_read,
    MiniAudioStream_seek,
    MiniAudioStream_get_data_format,
    NULL,
    NULL
};

MiniAudioStream::MiniAudioStream() :
    m_engine(NULL),
    m_sound(NULL),
    m_sampleRate(0),
    m_channels(0),
    m_format(ma_format_unknown),
    m_initialized(false),
    m_playing(false),
    m_soundCreated(false),
    m_readCursor(0)
{
    ma_data_source_config baseConfig = ma_data_source_config_init();
    baseConfig.vtable = &g_MiniAudioStream_vtable;
    ma_data_source_init(&baseConfig, &base);
}

MiniAudioStream::~MiniAudioStream()
{
    reset();
    ma_data_source_uninit(&base);
}

bool MiniAudioStream::bufferData(uint8_t *data, size_t data_size, ma_format format, int samplerate, int channels)
{
    if (data_size == 0) return false;

    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_initialized) {
        m_sampleRate = samplerate;
        m_channels   = channels;
        m_format     = format;
        m_initialized = true;
    }

    m_buffer.insert(m_buffer.end(), data, data + data_size);
    return true;
}

bool MiniAudioStream::isPlaying()
{
    if (!m_sound) return m_playing;
    return ma_sound_is_playing(m_sound) == MA_TRUE;
}

void MiniAudioStream::update()
{
    bool shouldCreate = false;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_playing && m_initialized && !m_soundCreated) {
            shouldCreate = true;
        }
    }

    if (shouldCreate) {
        createSound();
    }
}

void MiniAudioStream::reset()
{
    if (m_sound) {
        ma_sound_stop(m_sound);
        ma_sound_uninit(m_sound);
        free(m_sound);
        m_sound = NULL;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    m_initialized    = false;
    m_playing        = false;
    m_soundCreated   = false;
    m_buffer.clear();
    m_readCursor     = 0;
    m_sampleRate     = 0;
    m_channels       = 0;
    m_format         = ma_format_unknown;
}

void MiniAudioStream::play()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_playing = true;
    if (m_sound) ma_sound_start(m_sound);
}

void MiniAudioStream::createSound()
{
    if (!m_engine || !m_initialized) return;

    if (m_sound) {
        ma_sound_stop(m_sound);
        ma_sound_uninit(m_sound);
        free(m_sound);
        m_sound = NULL;
    }

    m_sound = (ma_sound *)malloc(sizeof(ma_sound));
    ma_result result = ma_sound_init_from_data_source(m_engine, &base,
        MA_SOUND_FLAG_NO_SPATIALIZATION | MA_SOUND_FLAG_NO_PITCH,
        NULL, m_sound);

    if (result != MA_SUCCESS) {
        free(m_sound);
        m_sound = NULL;
        return;
    }

    ma_sound_set_volume(m_sound, 1.0f);
    ma_sound_start(m_sound);
    m_soundCreated = true;
}

void MiniAudioStream::pause()
{
    m_playing = false;
    if (m_sound) ma_sound_stop(m_sound);
}

void MiniAudioStream::stop()
{
    pause();
    reset();
}

void MiniAudioStream::setVolume(float vol)
{
    if (m_sound) ma_sound_set_volume(m_sound, vol);
}

ma_result MiniAudioStream::readPCM(void* pFramesOut, ma_uint64 frameCount, ma_uint64* pFramesRead)
{
    if (!pFramesOut || !pFramesRead) return MA_INVALID_ARGS;

    std::lock_guard<std::mutex> lock(m_mutex);

    // Fallback to safe defaults if not fully initialized yet
    ma_format format = (m_format != ma_format_unknown) ? m_format : ma_format_s16;
    int channels = (m_channels != 0) ? m_channels : 2;

    size_t bytesPerFrame = ma_get_bytes_per_frame(format, channels);
    size_t bytesRequested = frameCount * bytesPerFrame;

    if (!m_initialized || m_buffer.empty()) {
        memset(pFramesOut, 0, bytesRequested);
        *pFramesRead = frameCount;
        return MA_SUCCESS;
    }

    size_t bytesAvailable = m_buffer.size() - m_readCursor;

    if (bytesAvailable == 0) {
        // Underflow - feed silence
        memset(pFramesOut, 0, bytesRequested);
        *pFramesRead = frameCount;
        return MA_SUCCESS;
    }

    size_t bytesToCopy = (bytesAvailable >= bytesRequested) ? bytesRequested : bytesAvailable;
    memcpy(pFramesOut, m_buffer.data() + m_readCursor, bytesToCopy);
    m_readCursor += bytesToCopy;

    if (bytesToCopy < bytesRequested) {
        memset((uint8_t*)pFramesOut + bytesToCopy, 0, bytesRequested - bytesToCopy);
    }

    *pFramesRead = frameCount;

    // Periodically reclaim consumed buffer memory (limit size to 256KB to keep footprint small)
    if (m_readCursor > 256 * 1024) {
        m_buffer.erase(m_buffer.begin(), m_buffer.begin() + m_readCursor);
        m_readCursor = 0;
    }

    return MA_SUCCESS;
}

ma_result MiniAudioStream::seekPCM(ma_uint64 frameIndex)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    size_t bytesPerFrame = ma_get_bytes_per_frame(m_format, m_channels);
    if (bytesPerFrame == 0) return MA_INVALID_ARGS;

    size_t targetByteOffset = frameIndex * bytesPerFrame;
    if (targetByteOffset <= m_buffer.size()) {
        m_readCursor = targetByteOffset;
    } else {
        m_readCursor = m_buffer.size();
    }
    return MA_SUCCESS;
}

ma_result MiniAudioStream::getDataFormat(ma_format* pFormat, ma_uint32* pChannels, ma_uint32* pSampleRate, ma_channel* pChannelMap, size_t channelMapCap)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    *pFormat = (m_format != ma_format_unknown) ? m_format : ma_format_s16;
    *pChannels = (m_channels != 0) ? m_channels : 2;
    *pSampleRate = (m_sampleRate != 0) ? m_sampleRate : 44100;

    if (pChannelMap != NULL) {
        ma_channel_map_init_standard(ma_standard_channel_map_default, pChannelMap, channelMapCap, *pChannels);
    }
    return MA_SUCCESS;
}
