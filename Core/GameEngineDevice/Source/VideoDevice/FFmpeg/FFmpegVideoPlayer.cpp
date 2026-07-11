/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 TheSuperHackers
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

////////////////////////////////////////////////////////////////////////////////
//																																						//
//  (c) 2001-2003 Electronic Arts Inc.																				//
//																																						//
////////////////////////////////////////////////////////////////////////////////

//////// FFmpegVideoPlayer.cpp ///////////////////////////
// Stephan Vedder, April 2025
/////////////////////////////////////////////////

//----------------------------------------------------------------------------
//         Includes
//----------------------------------------------------------------------------

#include "Lib/BaseType.h"
#include "VideoDevice/FFmpeg/FFmpegVideoPlayer.h"
#include "Common/AudioAffect.h"
#include "Common/GameAudio.h"
#include "Common/GameMemory.h"
#include "Common/GlobalData.h"
#include "Common/Registry.h"
#include "Common/FileSystem.h"

#include "VideoDevice/FFmpeg/FFmpegFile.h"

extern "C" {
	#include <libavcodec/avcodec.h>
	#include <libswscale/swscale.h>
}

// GeneralsX @build Mr. Meeseeks 17/06/2026 Prioritize MiniAudio backend over OpenAL to prevent redefinition conflicts
#ifdef SAGE_USE_MINIAUDIO
#include "MiniAudioDevice/MiniAudioManager.h"
#include "MiniAudioDevice/MiniAudioStream.h"
#elif defined(SAGE_USE_OPENAL)
#include "OpenALAudioDevice/OpenALAudioManager.h"
#include "OpenALAudioDevice/OpenALAudioStream.h"
#endif

#include <chrono>

//----------------------------------------------------------------------------
//         Externals
//----------------------------------------------------------------------------



//----------------------------------------------------------------------------
//         Defines
//----------------------------------------------------------------------------
#define VIDEO_LANG_PATH_FORMAT "Data/%s/Movies/%s.%s"
#define VIDEO_PATH	"Data\\Movies"
#define VIDEO_EXT		"bik"



//----------------------------------------------------------------------------
//         Private Types
//----------------------------------------------------------------------------



//----------------------------------------------------------------------------
//         Private Data
//----------------------------------------------------------------------------



//----------------------------------------------------------------------------
//         Public Data
//----------------------------------------------------------------------------



//----------------------------------------------------------------------------
//         Private Prototypes
//----------------------------------------------------------------------------



//----------------------------------------------------------------------------
//         Private Functions
//----------------------------------------------------------------------------



//----------------------------------------------------------------------------
//         Public Functions
//----------------------------------------------------------------------------


//============================================================================
// FFmpegVideoPlayer::FFmpegVideoPlayer
//============================================================================

FFmpegVideoPlayer::FFmpegVideoPlayer()
{

}

//============================================================================
// FFmpegVideoPlayer::~FFmpegVideoPlayer
//============================================================================

FFmpegVideoPlayer::~FFmpegVideoPlayer()
{
	deinit();
}

//============================================================================
// FFmpegVideoPlayer::init
//============================================================================

void	FFmpegVideoPlayer::init()
{
	// Need to load the stuff from the ini file.
	VideoPlayer::init();

	initializeBinkWithMiles();
}

//============================================================================
// FFmpegVideoPlayer::deinit
//============================================================================

void FFmpegVideoPlayer::deinit()
{
	TheAudio->releaseHandleForBink();
	VideoPlayer::deinit();
}

//============================================================================
// FFmpegVideoPlayer::reset
//============================================================================

void	FFmpegVideoPlayer::reset()
{
	VideoPlayer::reset();
}

//============================================================================
// FFmpegVideoPlayer::update
//============================================================================

void	FFmpegVideoPlayer::update()
{
	VideoPlayer::update();

}

//============================================================================
// FFmpegVideoPlayer::loseFocus
//============================================================================

void	FFmpegVideoPlayer::loseFocus()
{
	VideoPlayer::loseFocus();
}

//============================================================================
// FFmpegVideoPlayer::regainFocus
//============================================================================

void	FFmpegVideoPlayer::regainFocus()
{
	VideoPlayer::regainFocus();
}

//============================================================================
// FFmpegVideoPlayer::createStream
//============================================================================

VideoStreamInterface* FFmpegVideoPlayer::createStream( File* file )
{

	if ( file == nullptr )
	{
		return nullptr;
	}

	FFmpegFile* ffmpegHandle = NEW FFmpegFile();
	if(!ffmpegHandle->open(file))
	{
		delete ffmpegHandle;
		return nullptr;
	}

	FFmpegVideoStream *stream = NEW FFmpegVideoStream(ffmpegHandle);

	if ( stream )
	{

		stream->m_next = m_firstStream;
		stream->m_player = this;
		m_firstStream = stream;

		// never let volume go to 0, as Bink will interpret that as "play at full volume".
		Int mod = (Int) ((TheAudio->getVolume(AudioAffect_Speech) * 0.8f) * 100) + 1;
		[[maybe_unused]]  Int volume = (32768 * mod) / 100;
		DEBUG_LOG(("FFmpegVideoPlayer::createStream() - About to set volume (%g -> %d -> %d",
			TheAudio->getVolume(AudioAffect_Speech), mod, volume));
		//BinkSetVolume( stream->m_handle,0, volume);
		DEBUG_LOG(("FFmpegVideoPlayer::createStream() - set volume"));
	}

	return stream;
}

//============================================================================
// FFmpegVideoPlayer::open
//============================================================================

VideoStreamInterface*	FFmpegVideoPlayer::open( AsciiString movieTitle )
{
	VideoStreamInterface*	stream = nullptr;

	const Video* pVideo = getVideo(movieTitle);
	if (pVideo) {
		DEBUG_LOG(("FFmpegVideoPlayer::createStream() - About to open bink file"));

		if (TheGlobalData->m_modDir.isNotEmpty())
		{
			char filePath[ _MAX_PATH ];
			snprintf( filePath, ARRAY_SIZE(filePath), "%s%s\\%s.%s", TheGlobalData->m_modDir.str(), VIDEO_PATH, pVideo->m_filename.str(), VIDEO_EXT );
			File* file =  TheFileSystem->openFile(filePath);
			DEBUG_ASSERTLOG(!file, ("opened bink file %s", filePath));
			if (file)
			{
				return createStream( file );
			}
		}

		char localizedFilePath[ _MAX_PATH ];
		snprintf( localizedFilePath, ARRAY_SIZE(localizedFilePath), VIDEO_LANG_PATH_FORMAT, GetRegistryLanguage().str(), pVideo->m_filename.str(), VIDEO_EXT );
		File* file =  TheFileSystem->openFile(localizedFilePath);
		DEBUG_ASSERTLOG(!file, ("opened localized bink file %s", localizedFilePath));
		if (!file)
		{
			char filePath[ _MAX_PATH ];
			snprintf( filePath, ARRAY_SIZE(filePath), "%s\\%s.%s", VIDEO_PATH, pVideo->m_filename.str(), VIDEO_EXT );
			file = TheFileSystem->openFile(filePath);
			DEBUG_ASSERTLOG(!file, ("opened bink file %s", filePath));
		}

		DEBUG_LOG(("FFmpegVideoPlayer::createStream() - About to create stream"));
		stream = createStream( file );
	}

	return stream;
}

//============================================================================
// FFmpegVideoPlayer::load
//============================================================================

VideoStreamInterface*	FFmpegVideoPlayer::load( AsciiString movieTitle )
{
	return open(movieTitle); // load() used to have the same body as open(), so I'm combining them.  Munkee.
}

//============================================================================
//============================================================================
void FFmpegVideoPlayer::notifyVideoPlayerOfNewProvider( Bool nowHasValid )
{
	if (!nowHasValid) {
		TheAudio->releaseHandleForBink();
		//BinkSetSoundTrack(0, 0);
	} else {
		initializeBinkWithMiles();
	}
}

//============================================================================
//============================================================================
void FFmpegVideoPlayer::initializeBinkWithMiles()
{
	Int retVal = 0;
	void *driver = TheAudio->getHandleForBink();

	if ( driver )
	{
		//retVal = BinkSoundUseDirectSound(driver);
	}
	if( !driver || retVal == 0)
	{
		//BinkSetSoundTrack ( 0,0 );
	}
}

//============================================================================
// FFmpegVideoStream::FFmpegVideoStream
//============================================================================

FFmpegVideoStream::FFmpegVideoStream(FFmpegFile* file)
: m_ffmpegFile(file)
{
	m_ffmpegFile->setFrameCallback(onFrame);
	m_ffmpegFile->setUserData(this);

	// GeneralsX @build Mr. Meeseeks 17/06/2026 Prioritize MiniAudio backend over OpenAL to prevent redefinition conflicts
#ifdef SAGE_USE_MINIAUDIO
	MiniAudioStream* audioStream = (MiniAudioStream*)TheAudio->getHandleForBink();
	audioStream->reset();
#elif defined(SAGE_USE_OPENAL)
	// Release the audio handle if it's already in use
	OpenALAudioStream* audioStream = (OpenALAudioStream*)TheAudio->getHandleForBink();
	audioStream->reset();
#endif

	// Decode until we have our first video frame
	while (m_good && m_gotFrame == false)
		m_good = m_ffmpegFile->decodePacket();

#ifdef SAGE_USE_MINIAUDIO
	audioStream->setVolume(1.0f);
	audioStream->play();
#elif defined(SAGE_USE_OPENAL)
	// Start audio playback
	// GeneralsX @bugfix fbraz3 23/04/2026 Ensure video stream starts with audible gain even after prior source reuse.
	// Issue: https://github.com/fbraz3/GeneralsX/issues/38
	audioStream->setVolume(1.0f);
	audioStream->play();
#endif

	m_startTime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

//============================================================================
// FFmpegVideoStream::~FFmpegVideoStream
//============================================================================

FFmpegVideoStream::~FFmpegVideoStream()
{
	// GeneralsX @bugfix fbraz3 20/04/2026 Stop and clear queued OpenAL buffers so video audio
	// does not bleed into gameplay after the loading screen closes the stream.
	// Issue: https://github.com/fbraz3/GeneralsX/issues/38
#ifdef SAGE_USE_MINIAUDIO
	if (TheAudio)
	{
		MiniAudioStream* audioStream = (MiniAudioStream*)TheAudio->getHandleForBink();
		if (audioStream)
			audioStream->reset();
	}
#elif defined(SAGE_USE_OPENAL)
	if (TheAudio)
	{
		OpenALAudioStream* audioStream = (OpenALAudioStream*)TheAudio->getHandleForBink();
		if (audioStream)
			audioStream->reset();
	}
#endif
	av_freep(&m_audioBuffer);
	av_frame_free(&m_frame);
	sws_freeContext(m_swsContext);
	delete m_ffmpegFile;
}

void FFmpegVideoStream::onFrame(AVFrame *frame, int stream_idx, int stream_type, void *user_data)
{
	FFmpegVideoStream *videoStream = static_cast<FFmpegVideoStream *>(user_data);
	if (stream_type == AVMEDIA_TYPE_VIDEO) {
		av_frame_free(&videoStream->m_frame);
		videoStream->m_frame = av_frame_clone(frame);
		videoStream->m_gotFrame = true;
	}
#if defined(SAGE_USE_OPENAL) || defined(SAGE_USE_MINIAUDIO)
	else if (stream_type == AVMEDIA_TYPE_AUDIO) {
#ifdef SAGE_USE_MINIAUDIO
		MiniAudioStream* audioStream = (MiniAudioStream*)TheAudio->getHandleForBink();
#elif defined(SAGE_USE_OPENAL)
		OpenALAudioStream* audioStream = (OpenALAudioStream*)TheAudio->getHandleForBink();
#endif
		AVSampleFormat sampleFmt = static_cast<AVSampleFormat>(frame->format);
		const int bytesPerSample = av_get_bytes_per_sample(sampleFmt);
		const int frameSize = av_samples_get_buffer_size(nullptr, frame->ch_layout.nb_channels, frame->nb_samples, sampleFmt, 1);
		if (frameSize <= 0 || bytesPerSample <= 0 || frame->ch_layout.nb_channels <= 0 || frame->nb_samples <= 0)
			return;

		int outputBitsPerSample = bytesPerSample * 8;
		int outputFrameSize = frameSize;
		uint8_t* frameData = frame->data[0];

		// GeneralsX @bugfix fbraz3 23/04/2026 Convert float32 FFmpeg output to PCM16 for robust OpenAL compatibility.
		// Issue: https://github.com/fbraz3/GeneralsX/issues/38
		if (sampleFmt == AV_SAMPLE_FMT_FLT || sampleFmt == AV_SAMPLE_FMT_FLTP)
		{
			outputBitsPerSample = 16;
			outputFrameSize = frame->nb_samples * frame->ch_layout.nb_channels * (int)sizeof(int16_t);
			videoStream->m_audioBuffer = static_cast<uint8_t*>(av_realloc(videoStream->m_audioBuffer, outputFrameSize));
			if (videoStream->m_audioBuffer == nullptr)
			{
				DEBUG_LOG(("Failed to allocate converted audio buffer"));
				return;
			}
			int16_t *dst = reinterpret_cast<int16_t *>(videoStream->m_audioBuffer);
			if (sampleFmt == AV_SAMPLE_FMT_FLTP)
			{
				for (int sample_idx = 0; sample_idx < frame->nb_samples; ++sample_idx)
				{
					for (int channel_idx = 0; channel_idx < frame->ch_layout.nb_channels; ++channel_idx)
					{
						const float sample = reinterpret_cast<const float *>(frame->data[channel_idx])[sample_idx];
						const float clamped = (sample < -1.0f) ? -1.0f : ((sample > 1.0f) ? 1.0f : sample);
						*dst++ = static_cast<int16_t>(clamped * 32767.0f);
					}
				}
			}
			else
			{
				const float *src = reinterpret_cast<const float *>(frame->data[0]);
				const int totalSamples = frame->nb_samples * frame->ch_layout.nb_channels;
				for (int i = 0; i < totalSamples; ++i)
				{
					const float clamped = (src[i] < -1.0f) ? -1.0f : ((src[i] > 1.0f) ? 1.0f : src[i]);
					*dst++ = static_cast<int16_t>(clamped * 32767.0f);
				}
			}
			frameData = videoStream->m_audioBuffer;
		}
		// The format is planar - convert it to interleaved
		else if (av_sample_fmt_is_planar(sampleFmt))
		{
			videoStream->m_audioBuffer = static_cast<uint8_t*>(av_realloc(videoStream->m_audioBuffer, outputFrameSize));
			if (videoStream->m_audioBuffer == nullptr)
			{
				DEBUG_LOG(("Failed to allocate audio buffer"));
				return;
			}
			// Write the samples into our audio buffer
			for (int sample_idx = 0; sample_idx < frame->nb_samples; sample_idx++)
			{
				int byte_offset = sample_idx * bytesPerSample;
				for (int channel_idx = 0; channel_idx < frame->ch_layout.nb_channels; channel_idx++)
				{
					uint8_t* dst = &videoStream->m_audioBuffer[byte_offset * frame->ch_layout.nb_channels + channel_idx * bytesPerSample];
					uint8_t* src = &frame->data[channel_idx][byte_offset];
					memcpy(dst, src, bytesPerSample);
				}
			}
			frameData = videoStream->m_audioBuffer;
		}

#ifdef SAGE_USE_MINIAUDIO
		ma_format format = ma_format_unknown;
		if (outputBitsPerSample == 8) format = ma_format_u8;
		else if (outputBitsPerSample == 16) format = ma_format_s16;
		else if (outputBitsPerSample == 32) format = ma_format_f32;
		audioStream->bufferData(frameData, outputFrameSize, format, frame->sample_rate, frame->ch_layout.nb_channels);
		audioStream->update();
#elif defined(SAGE_USE_OPENAL)
		ALenum format = OpenALAudioManager::getALFormat(frame->ch_layout.nb_channels, outputBitsPerSample);
		audioStream->bufferData(frameData, outputFrameSize, format, frame->sample_rate);
		audioStream->update();
#endif
	}
#endif
}


//============================================================================
// FFmpegVideoStream::update
//============================================================================

void FFmpegVideoStream::update()
{
#ifdef SAGE_USE_MINIAUDIO
	MiniAudioStream* audioStream = (MiniAudioStream*)TheAudio->getHandleForBink();
	if (!audioStream->isPlaying()) {
		audioStream->play();
	}
	audioStream->update();
#elif defined(SAGE_USE_OPENAL)
	// Resume audio playback only if not already playing.
	// Calling alSourcePlay() on an AL_PLAYING source restarts it from the
	// first unprocessed buffer (OpenAL 1.1 spec §4.3.9), which causes
	// effective silence when invoked every game frame.
	OpenALAudioStream* audioStream = (OpenALAudioStream*)TheAudio->getHandleForBink();
	if (!audioStream->isPlaying()) {
		audioStream->play();
	}
#endif
	//BinkWait( m_handle );
}

//============================================================================
// FFmpegVideoStream::isFrameReady
//============================================================================

Bool FFmpegVideoStream::isFrameReady()
{
	uint64_t time = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
	bool ready = (time - m_startTime) >= m_ffmpegFile->getFrameTime() * frameIndex();
	return ready;

	//return !BinkWait( m_handle );
}

//============================================================================
// FFmpegVideoStream::frameDecompress
//============================================================================

void FFmpegVideoStream::frameDecompress()
{
	//BinkDoFrame( m_handle );
}

//============================================================================
// FFmpegVideoStream::frameRender
//============================================================================

void FFmpegVideoStream::frameRender( VideoBuffer *buffer )
{
	if (buffer == nullptr) {
		return;
	}

	if (m_frame == nullptr) {
		return;
	}

	if (m_frame->data == nullptr) {
		return;
	}

	AVPixelFormat dst_pix_fmt;

	switch (buffer->format()) {
		case VideoBuffer::TYPE_R8G8B8:
			dst_pix_fmt = AV_PIX_FMT_RGB24;
			break;
		case VideoBuffer::TYPE_X8R8G8B8:
			dst_pix_fmt = AV_PIX_FMT_BGR0;
			break;
		case VideoBuffer::TYPE_R5G6B5:
			dst_pix_fmt = AV_PIX_FMT_RGB565;
			break;
		case VideoBuffer::TYPE_X1R5G5B5:
			dst_pix_fmt = AV_PIX_FMT_RGB555;
			break;
		default:
			return;
	}

	m_swsContext = sws_getCachedContext(m_swsContext,
		width(),
		height(),
		static_cast<AVPixelFormat>(m_frame->format),
		buffer->width(),
		buffer->height(),
		dst_pix_fmt,
		SWS_BICUBIC,
		nullptr,
		nullptr,
		nullptr);

	uint8_t *buffer_data = static_cast<uint8_t *>(buffer->lock());
	if (buffer_data == nullptr) {
		DEBUG_LOG(("Failed to lock videobuffer"));
		return;
	}

	int dst_strides[] = { (int)buffer->pitch() };
	uint8_t *dst_data[] = { buffer_data };
	[[maybe_unused]] int result =
		sws_scale(m_swsContext, m_frame->data, m_frame->linesize, 0, height(), dst_data, dst_strides);
	DEBUG_ASSERTLOG(result >= 0, ("Failed to scale frame"));
	buffer->unlock();
}

//============================================================================
// FFmpegVideoStream::frameNext
//============================================================================

void FFmpegVideoStream::frameNext()
{
	m_gotFrame = false;
	// Decode until we have our next video frame
	while (m_good && m_gotFrame == false)
		m_good = m_ffmpegFile->decodePacket();
}

//============================================================================
// FFmpegVideoStream::frameIndex
//============================================================================

Int FFmpegVideoStream::frameIndex()
{
	return m_ffmpegFile->getCurrentFrame();
}

//============================================================================
// FFmpegVideoStream::totalFrames
//============================================================================

Int	FFmpegVideoStream::frameCount()
{
	return m_ffmpegFile->getNumFrames();
}

//============================================================================
// FFmpegVideoStream::frameGoto
//============================================================================

void FFmpegVideoStream::frameGoto( Int index )
{
	m_ffmpegFile->seekFrame(index);
}

//============================================================================
// VideoStream::height
//============================================================================

Int		FFmpegVideoStream::height()
{
	return m_ffmpegFile->getHeight();
}

//============================================================================
// VideoStream::width
//============================================================================

Int		FFmpegVideoStream::width()
{
	return m_ffmpegFile->getWidth();
}


