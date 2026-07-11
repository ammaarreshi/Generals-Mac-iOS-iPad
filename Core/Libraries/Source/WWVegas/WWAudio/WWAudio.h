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

/***********************************************************************************************
 ***              C O N F I D E N T I A L  ---  W E S T W O O D  S T U D I O S               ***
 ***********************************************************************************************
 *                                                                                             *
 *                 Project Name : WWAudio.h                                                    *
 *                                                                                             *
 *                     $Archive:: /Commando/Code/WWAudio/WWAudio.h          $*
 *                                                                                             *
 *                       Author:: Patrick Smith                                                *
 *                                                                                             *
 *                     $Modtime:: 8/14/01 2:27p                                               $*
 *                                                                                             *
 *                    $Revision:: 19                                                          $*
 *                                                                                             *
 *---------------------------------------------------------------------------------------------*
 * Functions:                                                                                  *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#pragma once

#include "always.h"

// TheSuperHackers @build 09/02/2026 Conditionally include Miles (Windows only)
// GeneralsX @feature fbraz 11/06/2026 Also use MilesStub when MiniAudio is enabled
#if !defined(SAGE_USE_OPENAL) && !defined(SAGE_USE_MINIAUDIO)
#pragma warning (push, 3)
#include "mss.h"
#pragma warning (pop)
#else
#include "MilesStub.h"
#endif

#include "Vector.h"
#include "SoundBuffer.h"
#include "AudioEvents.h"
#include "wwstring.h"

/////////////////////////////////////////////////////////////////////////////////
// Forward declaration
/////////////////////////////////////////////////////////////////////////////////
class AudibleSoundClass;
class Sound3DClass;
class Sound2DTriggerClass;
class StreamSoundClass;
class FileClass;
class SoundSceneClass;
class FileFactoryClass;
class SoundSceneObjClass;
class LogicalListenerClass;
class LogicalSoundClass;
class Matrix3D;


/////////////////////////////////////////////////////////////////////////////////
//	Class IDs
/////////////////////////////////////////////////////////////////////////////////
typedef enum
{
	CLASSID_UNKNOWN			= 0,
	CLASSID_2D,
	CLASSID_3D,
	CLASSID_LISTENER,
	CLASSID_PSEUDO3D,
	CLASSID_2DTRIGGER,
	CLASSID_LOGICAL,
	CLASSID_FILTERED,
	CLASSID_COUNT
} SOUND_CLASSID;


/////////////////////////////////////////////////////////////////////////////////
//	Default values
/////////////////////////////////////////////////////////////////////////////////
const int DEF_2D_SAMPLE_COUNT		= 16;
const int DEF_3D_SAMPLE_COUNT		= 16;
const float DEF_MUSIC_VOL			= 1.0F;
const float DEF_SFX_VOL				= 1.0F;
const int DEF_CACHE_SIZE			= 1024;
const int DEF_MAX_2D_BUFFER_SIZE	= 20000;
const int DEF_MAX_3D_BUFFER_SIZE	= 100000;


/////////////////////////////////////////////////////////////////////////////////
//	Constants
/////////////////////////////////////////////////////////////////////////////////
enum
{
	MAX_CACHE_HASH		= 256,
	CACHE_HASH_MASK	= 0x000000FF
};


/////////////////////////////////////////////////////////////////////////////////
//
//	WWAudioClass
//
//	Main controlling entity for all music and sound effects in a game.  Used
// to:
//
//		-- Select hardware devices
//		-- Modify quality preferences
//		-- Modify global volume settings
//		-- Allocate new sounds
//		-- Cache reuseable sounds
//		-- Play music and sound effects
//
//
/////////////////////////////////////////////////////////////////////////////////
class WWAudioClass
{
public:

	//////////////////////////////////////////////////////////////////////
	//	Public data types
	//////////////////////////////////////////////////////////////////////
	typedef enum
	{
		DRIVER2D_ERROR			= 0,
		DRIVER2D_DSOUND,
		DRIVER2D_WAVEOUT,
		DRIVER2D_COUNT
	} DRIVER_TYPE_2D;

	typedef enum
	{
		DRIVER3D_ERROR			= 0,
		DRIVER3D_D3DSOUND,
		DRIVER3D_EAX,
		DRIVER3D_A3D,
		DRIVER3D_RSX,
		DRIVER3D_PSEUDO,
		DRIVER3D_DOLBY,
		DRIVER3D_COUNT
	} DRIVER_TYPE_3D;

	typedef struct _DRIVER_INFO_STRUCT
	{
		HPROVIDER	driver;
		char *		name;
	} DRIVER_INFO_STRUCT;

	//////////////////////////////////////////////////////////////////////
	//	Friend classes
	//////////////////////////////////////////////////////////////////////
	friend class AudibleSoundClass;
	friend class Sound3DClass;
	friend class Listener3DClass;

	//////////////////////////////////////////////////////////////////////
	//	Public constructors/destructors
	//////////////////////////////////////////////////////////////////////
	WWAudioClass ();
	virtual ~WWAudioClass ();

	//////////////////////////////////////////////////////////////////////
	//	Static methods
	//////////////////////////////////////////////////////////////////////
	static WWAudioClass *	Get_Instance ()		{ return _theInstance; }

	//////////////////////////////////////////////////////////////////////
	//	Initialization methods
	//////////////////////////////////////////////////////////////////////

	//
	//	Note:  After call Initialize () you can begin using the library, you don't
	// need to explicitly call Open_2D_Device () or Select_3D_Device ().  Those
	// methods were provided as a means of opening devices other than the default.
	//
	//	The Initialize () method defaults to a stereo, 16bit, 44100hz 2D DirectSound
	// driver and a RSX 3D provider.  If RSX isn't available, it trys A3D, then
	//	EAX, then D3DSound, then whatever driver is first available.
	//
	void					Initialize (bool stereo = true, int bits = 16, int hertz = 44100);
	void					Initialize (const char *registry_subkey_name);
	void					Shutdown ();

	//////////////////////////////////////////////////////////////////////
	//	Driver methods
	//////////////////////////////////////////////////////////////////////
	HDIGDRIVER			Get_2D_Driver () const	{ return m_Driver2D; }
	HPROVIDER			Get_3D_Driver () const	{ return m_Driver3D; }
	HPROVIDER			Get_Reverb_Filter () const { return m_ReverbFilter; }

	//////////////////////////////////////////////////////////////////////
	//	2D Hardware/driver selection methods
	//////////////////////////////////////////////////////////////////////
	DRIVER_TYPE_2D		Open_2D_Device (LPWAVEFORMAT format = nullptr);
	DRIVER_TYPE_2D		Open_2D_Device (bool stereo, int bits, int hertz);
	bool					Close_2D_Device ();
	int					Get_Playback_Rate () const		{ return m_PlaybackRate; }
	int					Get_Playback_Bits () const		{ return m_PlaybackBits; }
	bool					Get_Playback_Stereo () const		{ return m_PlaybackStereo; }

	//////////////////////////////////////////////////////////////////////
	//	3D Hardware/driver selection methods
	//////////////////////////////////////////////////////////////////////

	// Device information
	int					Get_3D_Device_Count () const								{ return m_Driver3DList.Count (); }
	bool					Get_3D_Device (int index, DRIVER_INFO_STRUCT **info)	{ (*info) = m_Driver3DList[index]; return true; }
	bool					Is_3D_Device_Available (DRIVER_TYPE_3D type)				{ return Find_3D_Device (type) >= 0; }
	int					Find_3D_Device (DRIVER_TYPE_3D type);

	// Device selection
	bool					Select_3D_Device (int index);
	bool					Select_3D_Device (const char *device_name, HPROVIDER provider);
	bool					Select_3D_Device (DRIVER_TYPE_3D type);

	bool					Select_3D_Device (const char *device_name);

	//////////////////////////////////////////////////////////////////////
	//	Registry settings
	//////////////////////////////////////////////////////////////////////
	bool					Load_From_Registry (const char *subkey_name);
	bool					Load_From_Registry (const char *subkey_name, StringClass &device_name, bool &is_stereo, int &bits, int &hertz, bool &sound_enabled, bool &music_enabled, float &sound_volume, float &music_volume);
	bool					Save_To_Registry (const char *subkey_name);
	bool					Save_To_Registry (const char *subkey_name, const StringClass &device_name, bool is_stereo, int bits, int hertz, bool sound_enabled, bool music_enabled, float sound_volume, float music_volume);

	//////////////////////////////////////////////////////////////////////
	//	File interface methods
	//////////////////////////////////////////////////////////////////////

	//
	//	Note:  The user is responsible for freeing this file factory, the
	// sound library does not.
	//
	void					Set_File_Factory (FileFactoryClass *ffactory)			{ m_FileFactory = ffactory; }

	//////////////////////////////////////////////////////////////////////
	//	Preference settings methods
	//////////////////////////////////////////////////////////////////////
	bool					Set_Max_2D_Sample_Count (int count = DEF_2D_SAMPLE_COUNT);
	int					Get_Max_2D_Sample_Count () const;
	int					Get_Avail_2D_Sample_Count () const;

	bool					Set_Max_3D_Sample_Count (int count = DEF_3D_SAMPLE_COUNT);
	int					Get_Max_3D_Sample_Count () const;
	int					Get_Avail_3D_Sample_Count () const;

	//
	//	Reverb Support:  Only works with Create Labs EAX chipset.
	//
	float					Get_Effects_Level ()	{ return m_EffectsLevel; }

	//	See ENVIRONMENT_ defines in MSS.h for a list of possible values.
	int					Get_Reverb_Room_Type ()		{ return m_ReverbRoomType; }
	void					Set_Reverb_Room_Type (int type);

	//////////////////////////////////////////////////////////////////////
	//	Volume methods
	//////////////////////////////////////////////////////////////////////
	void					Set_Sound_Effects_Volume (float volume = DEF_SFX_VOL);
	float					Get_Sound_Effects_Volume () const						{ return m_SoundVolume; }

	void					Set_Music_Volume (float volume = DEF_MUSIC_VOL);
	float					Get_Music_Volume () const									{ return m_MusicVolume; }

	void					Allow_Sound_Effects (bool onoff = true);
	bool					Are_Sound_Effects_On () const							{ return m_AreSoundEffectsEnabled; }

	void					Allow_Music (bool onoff = true);
	bool					Is_Music_On () const										{ return m_IsMusicEnabled; }

	//////////////////////////////////////////////////////////////////////
	//	Update methods
	//////////////////////////////////////////////////////////////////////
	void					On_Frame_Update (unsigned int milliseconds);

	//////////////////////////////////////////////////////////////////////
	//	Callback methods
	//////////////////////////////////////////////////////////////////////
	void					Register_EOS_Callback (LPFNEOSCALLBACK callback, DWORD user_param);
	void					UnRegister_EOS_Callback (LPFNEOSCALLBACK callback);

	void					Register_Text_Callback (LPFNTEXTCALLBACK callback, DWORD user_param);
	void					UnRegister_Text_Callback (LPFNTEXTCALLBACK callback);

	void					Fire_Text_Callback (AudibleSoundClass *sound_obj, const StringClass &text);

	//////////////////////////////////////////////////////////////////////
	//	Sound allocation methods
	//////////////////////////////////////////////////////////////////////

	//
	//	Note:  The string_id is either the filename (for file-based sounds)
	// or the string_id passed into the create-from-memory methods.
	// It is important to note that the AudibleSoundClass object is not cached, but
	// the raw sound data the AudibleSoundClass object plays.
	//
	bool					Is_Sound_Cached (const char *string_id);

	//
	//	Note:  We differentiate between a 'music' object and
	// a 'sound effect' for volume control.  A music object
	// is simply a sound effect with a 'SOUND_TYPE' of TYPE_MUSIC.
	// When the user adjusts the 'music' volume, we loop through
	// the sound effects marked 'music' and update their volume.
	// Same for 'sound effect' volume.
	//

	//
	//	Note:  The sound data these objects support can be PCM WAV,
	// ADPCM WAV, VOC, or MP3.
	//
	AudibleSoundClass *		Create_Sound_Effect (FileClass &file, const char *string_id);
	AudibleSoundClass *		Create_Sound_Effect (const char *filename);
	AudibleSoundClass *		Create_Sound_Effect (const char *string_id, unsigned char *raw_wave_data, unsigned long bytes);

	//
	//	Note:  3D sound effects should be inserted into the SoundScene.
	// They should not be manually played/stopped/etc.
	//
	//	True 3D sound effects require mono, uncompressed, WAV data.
	// If the supplied data is not in this format the sound will
	// become a pseudo-3D sound.
	//
	//	Pseudo-3D sounds are not true 3D sounds.  They have the
	// same properties as 3D 'sound effects' but do not use 3D
	// hardware, are not restricted to mono, uncompressed, WAV data,
	// and do not calculate doppler and reverb effects.
	//
	Sound3DClass *		Create_3D_Sound (FileClass &file, const char *string_id, int classid_hint = CLASSID_3D);
	Sound3DClass *		Create_3D_Sound (const char *filename, int classid_hint = CLASSID_3D);
	Sound3DClass *		Create_3D_Sound (const char *string_id, unsigned char *raw_wave_data, unsigned long bytes, int classid_hint = CLASSID_3D);


	//////////////////////////////////////////////////////////////////////
	//	Logical-sound related methods
	//////////////////////////////////////////////////////////////////////
	LogicalSoundClass *		Create_Logical_Sound ();
	LogicalListenerClass *	Create_Logical_Listener ();

	//
	//	Logical type identification for use with the definition system
	//
	void							Add_Logical_Type (int id, LPCTSTR display_name);
	void							Reset_Logical_Types ();
	int							Get_Logical_Type_Count () const	{ return m_LogicalTypes.Count (); }
	int							Get_Logical_Type (int index, StringClass &name);

	//////////////////////////////////////////////////////////////////////
	//	Definition related methods
	//////////////////////////////////////////////////////////////////////

	//
	//	Sound creation methods
	//
	int						Create_Instant_Sound (int definition_id, const Matrix3D &tm, RefCountClass *user_obj = nullptr, uint32 user_data = 0, int classid_hint = CLASSID_3D);
	int						Create_Instant_Sound (const char *def_name, const Matrix3D &tm, RefCountClass *user_obj = nullptr, uint32 user_data = 0, int classid_hint = CLASSID_3D);
	AudibleSoundClass *	Create_Continuous_Sound (int definition_id, RefCountClass *user_obj = nullptr, uint32 user_data = 0, int classid_hint = CLASSID_3D);
	AudibleSoundClass *	Create_Continuous_Sound (const char *def_name, RefCountClass *user_obj = nullptr, uint32 user_data = 0, int classid_hint = CLASSID_3D);
	AudibleSoundClass *	Create_Sound (int definition_id, RefCountClass *user_obj = nullptr, uint32 user_data = 0, int classid_hint = CLASSID_3D);
	AudibleSoundClass *	Create_Sound (const char *def_name, RefCountClass *user_obj = nullptr, uint32 user_data = 0, int classid_hint = CLASSID_3D);

	//////////////////////////////////////////////////////////////////////
	//	Sound object lookup
	//////////////////////////////////////////////////////////////////////
	SoundSceneObjClass *	Find_Sound_Object (uint32 sound_obj_id);

	//////////////////////////////////////////////////////////////////////
	//	Sound scene methods (for 3D sounds)
	//////////////////////////////////////////////////////////////////////
	SoundSceneClass *	Get_Sound_Scene () const		{ return m_SoundScene; }

	//////////////////////////////////////////////////////////////////////
	//	Cache methods
	//////////////////////////////////////////////////////////////////////

	//
	// Note:  The cache sizes are in kilo-bytes.  The default
	// is currently set to 1MB.
	//
	void					Set_Cache_Size (int kbytes = DEF_CACHE_SIZE)	{ m_MaxCacheSize = (kbytes * 1024); }
	int					Get_Cache_Size () const						{ return m_MaxCacheSize / 1024; }
	int					Get_Current_Cache_Size () const				{ return m_CurrentCacheSize; }

	//
	// This settings determines whether a sound buffer is loaded
	// into memory or streamed from file.
	//
	//	Note:  True 3D sounds can't be streamed, so if the buffer size
	// is larger then the following setting, the sound is created as
	// a pseudo-3d sound effect and streamed.
	//
	void					Set_Max_2D_Sound_Buffer (int bytes = DEF_MAX_2D_BUFFER_SIZE)	{ m_Max2DBufferSize = bytes; }
	void					Set_Max_3D_Sound_Buffer (int bytes = DEF_MAX_3D_BUFFER_SIZE)	{ m_Max3DBufferSize = bytes; }

	//////////////////////////////////////////////////////////////////////
	//	Play control methods
	//////////////////////////////////////////////////////////////////////
	bool					Simple_Play_2D_Sound_Effect (const char *filename, float priority = 1.0F, float volume = DEF_SFX_VOL);
	bool					Simple_Play_2D_Sound_Effect (FileClass &file, float priority = 1.0F, float volume = DEF_SFX_VOL);

	//////////////////////////////////////////////////////////////////////
	//	Playlist methods
	//////////////////////////////////////////////////////////////////////
	bool						Add_To_Playlist (AudibleSoundClass *sound);
	bool						Remove_From_Playlist (AudibleSoundClass *sound);
	int						Get_Playlist_Count () const			{ return m_Playlist.Count (); }
	AudibleSoundClass *	Get_Playlist_Entry (int index) const;
	AudibleSoundClass *	Peek_Playlist_Entry (int index) const	{ return m_Playlist[index]; }
	void						Flush_Playlist ();
	bool						Is_Sound_In_Playlist (AudibleSoundClass *sound_obj);

	//////////////////////////////////////////////////////////////////////
	//	Statistics methods
	//////////////////////////////////////////////////////////////////////

	//
	//	This CPU percent includes total CPU being used for:
	//		Mixing
	//		Format conversions
	//		ADPCM or MP3 decompression
	//		System buffer fills
	//
	float					Get_Digital_CPU_Percent () const;

	//////////////////////////////////////////////////////////////////////
	//	Debug methods
	//////////////////////////////////////////////////////////////////////
	bool					Is_Disabled () const;

	// Um somtimes you need to get rid of all the completed sounds without
	// being in the update render function and without totally shutting down
	// the sound system.  This is primarily because completed (non static) sounds
	// still may have a reference to the object they're attached to.
	void					Free_Completed_Sounds ();

protected:

	//////////////////////////////////////////////////////////////////////
	//	Protected methods
	//////////////////////////////////////////////////////////////////////
	void						Build_3D_Driver_List ();
	void						Free_3D_Driver_List ();
	void						Reprioritize_Playlist ();
	bool						Validate_3D_Sound_Buffer (SoundBufferClass *buffer);
	FileClass *				Get_File (LPCTSTR filename);
	void						Return_File (FileClass *file);

	//////////////////////////////////////////////////////////////////////
	//	Handle management
	//////////////////////////////////////////////////////////////////////
	void						Allocate_2D_Handles ();
	void						Release_2D_Handles ();
	void						Allocate_3D_Handles ();
	void						Release_3D_Handles ();
	HSAMPLE					Get_2D_Sample (const AudibleSoundClass &sound_obj);
	H3DSAMPLE				Get_3D_Sample (const Sound3DClass &sound_obj);
	H3DPOBJECT				Get_Listener_Handle ();
	void						ReAssign_2D_Handles ();
	void						ReAssign_3D_Handles ();
	void						Remove_2D_Sound_Handles ();
	void						Remove_3D_Sound_Handles ();
	bool						Is_OK_To_Give_Handle (const AudibleSoundClass &sound_obj);

	//////////////////////////////////////////////////////////////////////
	//	Cache methods
	//////////////////////////////////////////////////////////////////////
	SoundBufferClass *	Get_Sound_Buffer (FileClass &file, const char *string_id, bool is_3d);
	SoundBufferClass *	Get_Sound_Buffer (const char *filename, bool is_3d);
	SoundBufferClass *	Find_Cached_Buffer (const char *string_id);
	SoundBufferClass *	Create_Sound_Buffer (FileClass &file, const char *string_id, bool is_3d);
	SoundBufferClass *	Create_Sound_Buffer (unsigned char *file_image, unsigned long bytes, const char *string_id, bool is_3d);
	bool						Cache_Buffer (SoundBufferClass *buffer, const char *string_id);
	bool						Free_Cache_Space (int bytes);
	void						Flush_Cache ();

	//////////////////////////////////////////////////////////////////////
	//	Miles File Callbacks
	//////////////////////////////////////////////////////////////////////
	static U32 AILCALLBACK	File_Open_Callback (char const *filename, void **file_handle);
	static void AILCALLBACK	File_Close_Callback (void *file_handle);
	static S32 AILCALLBACK	File_Seek_Callback (void *file_handle, S32 offset, U32 type);
	static U32 AILCALLBACK	File_Read_Callback (void *file_handle, void *buffer, U32 bytes);

private:

	//////////////////////////////////////////////////////////////////////
	//	Static member data
	//////////////////////////////////////////////////////////////////////
	static WWAudioClass *						_theInstance;
	static HANDLE									_TimerSyncEvent;

	//////////////////////////////////////////////////////////////////////
	//	Private data types
	//////////////////////////////////////////////////////////////////////
	typedef struct _CACHE_ENTRY_STRUCT
	{
		char *					string_id;
		SoundBufferClass *	buffer;

		_CACHE_ENTRY_STRUCT ()
			: string_id (0), buffer (nullptr) {}

		_CACHE_ENTRY_STRUCT &operator= (const _CACHE_ENTRY_STRUCT &src) { string_id = ::strdup (src.string_id); REF_PTR_SET (buffer, src.buffer); return *this; }
		bool operator== (const _CACHE_ENTRY_STRUCT &src) { return false; }
		bool operator!= (const _CACHE_ENTRY_STRUCT &src) { return true; }
	} CACHE_ENTRY_STRUCT;


	typedef struct _LOGICAL_TYPE_STRUCT
	{
		StringClass				display_name;
		int						id;

		_LOGICAL_TYPE_STRUCT ()
			: id (0) {}

		_LOGICAL_TYPE_STRUCT (int _id, LPCTSTR name)
			:	display_name (name), id (_id) {}

		bool operator== (const _LOGICAL_TYPE_STRUCT &src) { return false; }
		bool operator!= (const _LOGICAL_TYPE_STRUCT &src) { return true; }
	} LOGICAL_TYPE_STRUCT;

	//////////////////////////////////////////////////////////////////////
	//	Private member data
	//////////////////////////////////////////////////////////////////////
	int													m_PlaybackRate;
	int													m_PlaybackBits;
	bool													m_PlaybackStereo;
	float													m_MusicVolume;
	float													m_SoundVolume;
	int													m_Max2DSamples;
	int													m_Max3DSamples;
	int													m_Max2DBufferSize;
	int													m_Max3DBufferSize;
	HTIMER												m_UpdateTimer;
	bool													m_IsMusicEnabled;
	bool													m_AreSoundEffectsEnabled;
	FileFactoryClass *								m_FileFactory;

	// Callback lists
	AudioCallbackListClass<LPFNEOSCALLBACK>	m_EOSCallbackList;
	AudioCallbackListClass<LPFNTEXTCALLBACK>	m_TextCallbackList;

	// Sound scene management
	SoundSceneClass *									m_SoundScene;

	//	Driver information
	HDIGDRIVER											m_Driver2D;
	HPROVIDER											m_Driver3D;
	HPROVIDER											m_Driver3DPseudo;
	HPROVIDER											m_ReverbFilter;
	DynamicVectorClass<DRIVER_INFO_STRUCT *>	m_Driver3DList;

	// Available sample handles
	DynamicVectorClass<HSAMPLE>					m_2DSampleHandles;
	DynamicVectorClass<H3DSAMPLE>					m_3DSampleHandles;

	// Playlist management
	DynamicVectorClass<AudibleSoundClass *>	m_Playlist;
	DynamicVectorClass<AudibleSoundClass *>	m_CompletedSounds;

	// Buffer caching
	DynamicVectorClass<CACHE_ENTRY_STRUCT>		m_CachedBuffers[MAX_CACHE_HASH];
	int													m_MaxCacheSize;
	int													m_CurrentCacheSize;

	// Logical type management
	DynamicVectorClass<LOGICAL_TYPE_STRUCT>	m_LogicalTypes;

	//	Reverb support
	float													m_EffectsLevel;
	int													m_ReverbRoomType;
};
