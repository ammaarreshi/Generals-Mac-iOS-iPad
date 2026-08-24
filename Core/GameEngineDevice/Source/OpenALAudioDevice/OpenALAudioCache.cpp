#include "OpenALAudioCache.h"

extern "C" {
#include <libavcodec/avcodec.h>
}

#include "Common/AudioEventInfo.h"
#include "Common/AudioEventRTS.h"
#include "Common/file.h"
#include "Common/FileSystem.h"

#include <cstring>
#include <limits>

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
OpenALAudioFileCache::OpenALAudioFileCache() : m_maxSize(14*1024*1024), m_currentlyUsedSize(0)
{
}

Bool OpenALAudioFileCache::decodeFFmpeg(OpenAudioFile* file)
{
	std::vector<uint8_t> audioData;
	auto on_frame = [&audioData](AVFrame* frame, int stream_idx, int stream_type, void* user_data) {
		OpenAudioFile* file = static_cast<OpenAudioFile*>(user_data);
		if (stream_type != AVMEDIA_TYPE_AUDIO) {
			return;
		}

		const int frame_data_size = file->m_ffmpegFile->getSizeForSamples(frame->nb_samples);
		audioData.reserve(audioData.size() + frame_data_size);

		if (av_sample_fmt_is_planar(static_cast<AVSampleFormat>(frame->format))) {
			// Convert planar audio to interleaved
			int num_channels = file->m_ffmpegFile->getNumChannels();
			int bytes_per_sample = file->m_ffmpegFile->getBytesPerSample();
			for (int sample = 0; sample < frame->nb_samples; ++sample) {
				for (int channel = 0; channel < num_channels; ++channel) {
					const uint8_t* src = frame->data[channel] + sample * bytes_per_sample;
					audioData.insert(audioData.end(), src, src + bytes_per_sample);
				}
			}
		} else {
			// Directly copy interleaved audio
			audioData.insert(audioData.end(), frame->data[0], frame->data[0] + frame_data_size);
		}
		file->m_fileSize += frame_data_size;
		file->m_totalSamples += frame->nb_samples;
		};

	file->m_ffmpegFile->setFrameCallback(on_frame);
	file->m_ffmpegFile->setUserData(file);

	// Read all packets inside the file
	while (file->m_ffmpegFile->decodePacket()) {
	}

	// Fill the buffer with the audio data
	alBufferData(file->m_buffer, OpenALAudioManager::getALFormat(file->m_ffmpegFile->getNumChannels(), file->m_ffmpegFile->getBytesPerSample() * 8),
		audioData.data(), audioData.size(), file->m_ffmpegFile->getSampleRate());

	// Calculate the duration in MS
	file->m_duration = (file->m_totalSamples / (float)file->m_ffmpegFile->getSampleRate()) * 1000.0f;

	return true;
}

//-------------------------------------------------------------------------------------------------
OpenALAudioFileCache::~OpenALAudioFileCache()
{
	// Free all the samples that are open.
	OpenFilesHashIt it;
	for (it = m_openFiles.begin(); it != m_openFiles.end(); ++it) {
		if (it->second.m_openCount > 0) {
			DEBUG_CRASH(("Sample '%s' is still playing, and we're trying to quit.\n", it->second.m_eventInfo->m_audioName.str()));
		}

		releaseOpenAudioFile(&it->second);
		// Don't erase it from the map, cause it makes this whole process way more complicated, and 
		// we're about to go away anyways.
	}
}

//-------------------------------------------------------------------------------------------------
ALuint OpenALAudioFileCache::getBufferForFile(const OpenFileInfo &fileInfo)
{
	AudioEventRTS *eventToOpenFrom = fileInfo.event;

	AsciiString strToFind;
	if (eventToOpenFrom)
	{
		switch (eventToOpenFrom->getNextPlayPortion())
		{
		case PP_Attack:
			strToFind = eventToOpenFrom->getAttackFilename();
			break;
		case PP_Sound:
			strToFind = eventToOpenFrom->getFilename();
			break;
		case PP_Decay:
			strToFind = eventToOpenFrom->getDecayFilename();
			break;
		case PP_Done:
			return 0;
		}
	}
	else
	{
		if (fileInfo.filename)
		{
			strToFind = *fileInfo.filename;
		}
		else
		{
			DEBUG_CRASH(("No filename to open\n"));
			return 0;
		}
	}

	// GeneralsX @bugfix Marco 24/08/2026 Bail out early when there is no filename
	// to load. Some stock audio events ship with an empty "Sounds =" list -
	// TurretMoveLoop and RadarEvent among them. TurretMoveLoop is declared
	// "loop all random", so startNextLoop() regenerates the (empty) filename and
	// lands here again every frame, each time walking all the way down to
	// TheFileSystem->openFile("") before failing. Nothing was leaked, but the
	// lookup and the BIG-archive access were pure waste, several times a second.
	if (strToFind.isEmpty()) {
		return 0;
	}

	auto it = m_openFiles.find(strToFind);

	if (it != m_openFiles.end()) {
		++it->second.m_openCount;
		return it->second.m_buffer;
	}

	// Couldn't find the file, so actually open it.
	File* file = TheFileSystem->openFile(strToFind.str());
	if (!file) {
		DEBUG_ASSERTLOG(strToFind.isEmpty(), ("Missing Audio File: '%s'\n", strToFind.str()));
		return 0;
	}

	OpenAudioFile openedAudioFile;
	alGenBuffers(1, &openedAudioFile.m_buffer);
	openedAudioFile.m_eventInfo = eventToOpenFrom ? eventToOpenFrom->getAudioEventInfo() : NULL;
	openedAudioFile.m_ffmpegFile = new FFmpegFile();

	// This transfer ownership of file
	if (!openedAudioFile.m_ffmpegFile->open(file)) {
		releaseOpenAudioFile(&openedAudioFile);
		return 0;
	}

	if (eventToOpenFrom && eventToOpenFrom->isPositionalAudio()) {
		if (openedAudioFile.m_ffmpegFile->getNumChannels() > 1) {
			// GeneralsX @bugfix Bender 09/05/2026 Keep multichannel positional assets loadable so playback can fall back to non-spatial output.
			DEBUG_LOG(("OpenAL positional fallback armed for multichannel audio '%s' (%d channels)\n",
				strToFind.str(), openedAudioFile.m_ffmpegFile->getNumChannels()));
		}
	}

	if (!decodeFFmpeg(&openedAudioFile)) {
		releaseOpenAudioFile(&openedAudioFile);
		return 0;
	}

	openedAudioFile.m_ffmpegFile->close();

	// GeneralsX @bugfix Marco 24/08/2026 Account for the DECODED PCM size, not the
	// compressed file size. decodeFFmpeg() accumulates m_fileSize per frame; assigning
	// file->size() here overwrote it with a far smaller value, so the cache
	// under-reported its real footprint and evicted much later than intended.
	// (This is not what caused the buffer leak - see releaseOpenAudioFile - but
	// the accounting was wrong regardless.)
	m_currentlyUsedSize += openedAudioFile.m_fileSize;
	if (m_currentlyUsedSize > m_maxSize) {
		DEBUG_LOG(("Audio Cache is full, trying to free some space\n"));
		// We need to free some samples, or we're not going to be able to play this sound.
		if (!freeEnoughSpaceForSample(openedAudioFile)) {
			DEBUG_LOG(("Couldn't free enough space for sample\n"));
			m_currentlyUsedSize -= openedAudioFile.m_fileSize;
			releaseOpenAudioFile(&openedAudioFile);
			return 0;
		}
	}

	m_openFiles[strToFind] = openedAudioFile;
	return openedAudioFile.m_buffer;
}

//-------------------------------------------------------------------------------------------------
void OpenALAudioFileCache::closeBuffer(ALuint bufferToClose)
{
	if (!bufferToClose) {
		return;
	}

	OpenFilesHash::iterator it;
	for (it = m_openFiles.begin(); it != m_openFiles.end(); ++it) {
		if (it->second.m_buffer == bufferToClose) {
			--it->second.m_openCount;
			return;
		}
	}
}

float OpenALAudioFileCache::getBufferLength(ALuint handle)
{
	if (!handle) {
		return 0.0f;
	}

	for (auto it = m_openFiles.begin(); it != m_openFiles.end(); ++it) {
		if (it->second.m_buffer == handle) {
			return it->second.m_duration;
		}
	}

	return 0.0f;
}

//-------------------------------------------------------------------------------------------------
void OpenALAudioFileCache::setMaxSize(UnsignedInt size)
{
	// Protect the function, in case we're trying to use this value elsewhere.

	// Hardcoded to 14MiB for now, this is a workaround for the limit
	//  set by the default config files being 4MB and causing needless reloads.
	//m_maxSize = size;
}

//-------------------------------------------------------------------------------------------------
void OpenALAudioFileCache::releaseOpenAudioFile(OpenAudioFile* fileToRelease)
{
	// GeneralsX @bugfix Marco 24/08/2026 Always detach the buffer from any source
	// still referencing it - not just when m_openCount > 0. The cache reference is
	// dropped (closeBuffer) before the source gets a new buffer assigned, so a
	// buffer with m_openCount == 0 can still be bound to a live OpenAL source.
	// alDeleteBuffers then fails with AL_INVALID_OPERATION and the buffer leaks;
	// measured: 1158 of 1158 deletions failed, ~26 MB/min lost.
	if (fileToRelease->m_buffer) {
		// This thing needs to be terminated IMMEDIATELY.
		TheAudio->closeAnySamplesUsingFile((const void*)(uintptr_t)fileToRelease->m_buffer);
	}

	if (fileToRelease->m_ffmpegFile) {
		// Free FFMPEG handles
		delete fileToRelease->m_ffmpegFile;
	}

	if (fileToRelease->m_buffer)
	{
		// Free the OpenAL buffer
		alDeleteBuffers(1, &fileToRelease->m_buffer);
	}
	fileToRelease->m_ffmpegFile = NULL;
	fileToRelease->m_buffer = 0;
	fileToRelease->m_eventInfo = NULL;
}

//-------------------------------------------------------------------------------------------------
Bool OpenALAudioFileCache::freeEnoughSpaceForSample(const OpenAudioFile& sampleThatNeedsSpace)
{

	Int spaceRequired = m_currentlyUsedSize - m_maxSize;
	Int runningTotal = 0;
	// GeneralsX @bugfix fbraz3 16/04/2026 Handle cache entries without AudioEventInfo (filename-only loads).
	const Int requestedPriority = sampleThatNeedsSpace.m_eventInfo
		? sampleThatNeedsSpace.m_eventInfo->m_priority
		: std::numeric_limits<Int>::min();

	std::list<AsciiString> filesToClose;
	// First, search for any samples that have ref counts of 0. They are low-hanging fruit, and 
	// should be considered immediately.
	OpenFilesHashIt it;
	for (it = m_openFiles.begin(); it != m_openFiles.end(); ++it) {
		if (it->second.m_openCount == 0) {
			// This is said low-hanging fruit.
			filesToClose.push_back(it->first);

			runningTotal += it->second.m_fileSize;

			if (runningTotal >= spaceRequired) {
				break;
			}
		}
	}

	// If we don't have enough space yet, then search through the events who have a count of 1 or more
	// and who are lower priority than this sound.
	// Mical said that at this point, sounds shouldn't care if other sounds are interruptable or not.
	// Kill any files of lower priority necessary to clear our the buffer.
	if (runningTotal < spaceRequired) {
		for (it = m_openFiles.begin(); it != m_openFiles.end(); ++it) {
			if (it->second.m_openCount > 0) {
				const Int candidatePriority = it->second.m_eventInfo
					? it->second.m_eventInfo->m_priority
					: std::numeric_limits<Int>::min();
				if (candidatePriority < requestedPriority) {
					filesToClose.push_back(it->first);
					runningTotal += it->second.m_fileSize;

					if (runningTotal >= spaceRequired) {
						break;
					}
				}
			}
		}
	}

	// We weren't able to find enough sounds to truncate. Therefore, this sound is not going to play.
	if (runningTotal < spaceRequired) {
		return FALSE;
	}

	std::list<AsciiString>::iterator ait;
	for (ait = filesToClose.begin(); ait != filesToClose.end(); ++ait) {
		OpenFilesHashIt itToErase = m_openFiles.find(*ait);
		if (itToErase != m_openFiles.end()) {
			releaseOpenAudioFile(&itToErase->second);
			m_currentlyUsedSize -= itToErase->second.m_fileSize;
			m_openFiles.erase(itToErase);
		}
	}

	return TRUE;
}