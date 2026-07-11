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

////////////////////////////////////////////////////////////////////////////////
//																																						//
//  (c) 2001-2003 Electronic Arts Inc.																				//
//																																						//
////////////////////////////////////////////////////////////////////////////////

// FILE: INI.cpp //////////////////////////////////////////////////////////////////////////////////
// Author: Colin Day, November 2001
// Desc:   INI Reader
///////////////////////////////////////////////////////////////////////////////////////////////////

// INCLUDES ///////////////////////////////////////////////////////////////////////////////////////
#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine
#define DEFINE_DEATH_NAMES

#include "WWMath/wwmath.h"
#include "Common/INI.h"
#include "Common/INIException.h"

#include "Common/DamageFX.h"
#include "Common/file.h"
#include "Common/FileSystem.h"
#include "Common/GameAudio.h"
#include "Common/Science.h"
#include "Common/SpecialPower.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "Common/Upgrade.h"
#include "Common/Xfer.h"
#include "Common/XferCRC.h"

#include "GameClient/Anim2D.h"
#include "GameClient/Color.h"
#include "GameClient/FXList.h"
#include "GameClient/GameText.h"
#include "GameClient/Image.h"
#include "GameClient/ParticleSys.h"
#include "GameLogic/Armor.h"
#include "GameLogic/ExperienceTracker.h"
#include "GameLogic/FPUControl.h"
#include "GameLogic/ObjectCreationList.h"
#include "GameLogic/ScriptEngine.h"
#include "GameLogic/Weapon.h"

#if __cplusplus >= 201611L && !defined(__APPLE__)
#define USE_STD_FROM_CHARS_PARSING 1
#else
#define USE_STD_FROM_CHARS_PARSING 0
#endif

#if USE_STD_FROM_CHARS_PARSING
#include <charconv>
#include <cstdlib>
#include <string_view>
#include <type_traits>
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////
// PRIVATE DATA ///////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

static Xfer *s_xfer = nullptr;

//-------------------------------------------------------------------------------------------------
/** This is the table of data types we can have in INI files.  To add a new data type
	* block make a new entry in this table and add an appropriate parsing function */
//-------------------------------------------------------------------------------------------------
extern void parseReallyLowMHz( INI* ini);		// yeah, so sue me (srj)
struct BlockParse
{
	const char *token;
	INIBlockParse parse;
};
static const BlockParse theTypeTable[] =
{
	{ "AIData",                         INI::parseAIDataDefinition },
	{ "Animation",                      INI::parseAnim2DDefinition },
	{ "Armor",                          INI::parseArmorDefinition },
	{ "AudioEvent",                     INI::parseAudioEventDefinition },
	{ "AudioSettings",                  INI::parseAudioSettingsDefinition },
	{ "BenchProfile",                   INI::parseBenchProfile },
	{ "Bridge",                         INI::parseTerrainBridgeDefinition },
	{ "Campaign",                       INI::parseCampaignDefinition },
	{ "ChallengeGenerals",              INI::parseChallengeModeDefinition },
	{ "CommandButton",                  INI::parseCommandButtonDefinition },
	{ "CommandMap",                     INI::parseMetaMapDefinition },
	{ "CommandSet",                     INI::parseCommandSetDefinition },
	{ "ControlBarResizer",              INI::parseControlBarResizerDefinition },
	{ "ControlBarScheme",               INI::parseControlBarSchemeDefinition },
	{ "CrateData",                      INI::parseCrateTemplateDefinition },
	{ "Credits",                        INI::parseCredits },
	{ "DamageFX",                       INI::parseDamageFXDefinition },
	{ "DialogEvent",                    INI::parseDialogDefinition },
	{ "DrawGroupInfo",                  INI::parseDrawGroupNumberDefinition },
	{ "DynamicGameLOD",                 INI::parseDynamicGameLODDefinition },
	{ "EvaEvent",                       INI::parseEvaEvent },
	{ "FXList",                         INI::parseFXListDefinition },
	{ "GameData",                       INI::parseGameDataDefinition },
	{ "HeaderTemplate",                 INI::parseHeaderTemplateDefinition },
	{ "InGameUI",                       INI::parseInGameUIDefinition },
	{ "LODPreset",                      INI::parseLODPreset },
	{ "Language",                       INI::parseLanguageDefinition },
	{ "Locomotor",                      INI::parseLocomotorTemplateDefinition },
	{ "MapCache",                       INI::parseMapCacheDefinition },
	{ "MapData",                        INI::parseMapDataDefinition },
	{ "MappedImage",                    INI::parseMappedImageDefinition },
	{ "MiscAudio",                      INI::parseMiscAudio },
	{ "Mouse",                          INI::parseMouseDefinition },
	{ "MouseCursor",                    INI::parseMouseCursorDefinition },
	{ "MultiplayerColor",               INI::parseMultiplayerColorDefinition },
	{ "MultiplayerSettings",            INI::parseMultiplayerSettingsDefinition },
	{ "MultiplayerStartingMoneyChoice", INI::parseMultiplayerStartingMoneyChoiceDefinition },
	{ "MusicTrack",                     INI::parseMusicTrackDefinition },
	{ "Object",                         INI::parseObjectDefinition },
	{ "ObjectCreationList",             INI::parseObjectCreationListDefinition },
	{ "ObjectReskin",                   INI::parseObjectReskinDefinition },
	{ "OnlineChatColors",               INI::parseOnlineChatColorDefinition },
	{ "ParticleSystem",                 INI::parseParticleSystemDefinition },
	{ "PlayerTemplate",                 INI::parsePlayerTemplateDefinition },
	{ "Rank",                           INI::parseRankDefinition },
	{ "ReallyLowMHz",                   parseReallyLowMHz },
	{ "Road",                           INI::parseTerrainRoadDefinition },
	{ "Science",                        INI::parseScienceDefinition },
	{ "ScriptAction",                   ScriptEngine::parseScriptAction },
	{ "ScriptCondition",                ScriptEngine::parseScriptCondition },
	{ "ShellMenuScheme",                INI::parseShellMenuSchemeDefinition },
	{ "SpecialPower",                   INI::parseSpecialPowerDefinition },
	{ "StaticGameLOD",                  INI::parseStaticGameLODDefinition },
	{ "Terrain",                        INI::parseTerrainDefinition },
	{ "Upgrade",                        INI::parseUpgradeDefinition },
	{ "Video",                          INI::parseVideoDefinition },
	{ "WaterSet",                       INI::parseWaterSettingDefinition },
	{ "WaterTransparency",              INI::parseWaterTransparencyDefinition },
	{ "Weapon",                         INI::parseWeaponTemplateDefinition },
	{ "Weather",                        INI::parseWeatherDefinition },
	{ "WebpageURL",                     INI::parseWebpageURLDefinition },
	{ "WindowTransition",               INI::parseWindowTransitions },
};


///////////////////////////////////////////////////////////////////////////////////////////////////
// PRIVATE FUNCTIONS //////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
Bool INI::isValidINIFilename( const char *filename )
{
	if( filename == nullptr )
		return FALSE;

	return endsWithNoCase(filename, ".ini");
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// PUBLIC FUNCTIONS ///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
INI::INI()
{

	m_readBuffer = nullptr;
	m_readBufferNext = 0;
	m_readBufferUsed = 0;
	m_filename					= "None";
	m_loadType					= INI_LOAD_INVALID;
	m_lineNum						= 0;
	m_buffer[0]					= 0;
	m_endOfFile					= FALSE;
#ifdef DEBUG_CRASHING
	m_curBlockStart[0]	= 0;
#endif

}

//-------------------------------------------------------------------------------------------------
UnsignedInt INI::loadFileDirectory( AsciiString fileDirName, INILoadType loadType, Xfer *pXfer, Bool subdirs )
{
	// GeneralsX @feature BenderAI 20/02/2026 Debug hang investigation
	fprintf(stderr, "[INI] loadFileDirectory('%s') START\n", fileDirName.str());
	fflush(stderr);
	
	UnsignedInt filesRead = 0;

	AsciiString iniDir = fileDirName;
	AsciiString iniFile = fileDirName;

	char ext[] = ".ini";

	if (iniDir.endsWithNoCase(ext))
	{
		iniDir.truncateBy(ARRAY_SIZE(ext)-1);
	}

	if (!iniFile.endsWithNoCase(ext))
	{
		iniFile.concat(ext);
	}

	fprintf(stderr, "[INI] loadFileDirectory - checking iniFile: '%s'\n", iniFile.str());
	fflush(stderr);
	
	if (TheFileSystem->doesFileExist(iniFile.str()))
	{
		fprintf(stderr, "[INI] loadFileDirectory - loading iniFile: '%s' START\n", iniFile.str());
		fflush(stderr);
		filesRead += load(iniFile, loadType, pXfer);
		fprintf(stderr, "[INI] loadFileDirectory - loading iniFile: '%s' END\n", iniFile.str());
		fflush(stderr);
	}

	// Load any additional ini files from a "filename" directory and its subdirectories.
	fprintf(stderr, "[INI] loadFileDirectory - calling loadDirectory('%s') START\n", iniDir.str());
	fflush(stderr);
	filesRead += loadDirectory(iniDir, loadType, pXfer, subdirs);
	fprintf(stderr, "[INI] loadFileDirectory - calling loadDirectory('%s') END\n", iniDir.str());
	fflush(stderr);

	// Expect to open and load at least one file.
	if (filesRead == 0)
	{
		fprintf(stderr, "[INI] ERROR: No files read from directory '%s'\n", fileDirName.str());
		fflush(stderr);
		throw INI_CANT_OPEN_FILE;
	}

	fprintf(stderr, "[INI] loadFileDirectory('%s') END - filesRead=%d\n", fileDirName.str(), filesRead);
	fflush(stderr);
	return filesRead;
}

//-------------------------------------------------------------------------------------------------
/** Load all INI files in the specified directory (and subdirectories if indicated).
	* If we are to load subdirectories, we will load them *after* we load all the
	* files in the current directory */
//-------------------------------------------------------------------------------------------------
UnsignedInt INI::loadDirectory( AsciiString dirName, INILoadType loadType, Xfer *pXfer, Bool subdirs )
{
	// GeneralsX @feature BenderAI 20/02/2026 Debug hang investigation
	fprintf(stderr, "[INI] loadDirectory('%s') START\n", dirName.str());
	fflush(stderr);
	
	UnsignedInt filesRead = 0;

	// sanity
	if( dirName.isEmpty() )
	{
		fprintf(stderr, "[INI] ERROR: Empty directory name in loadDirectory\n");
		fflush(stderr);
		throw INI_INVALID_DIRECTORY;
	}

	FilenameList filenameList;
	dirName.concat('\\');
	TheFileSystem->getFileListInDirectory(dirName, "*.ini", filenameList, subdirs);
	// Load the INI files in the dir now, in a sorted order.  This keeps things the same between machines
	// in a network game.
	FilenameList::const_iterator it = filenameList.begin();
	while (it != filenameList.end())
	{
		AsciiString tempname;
		tempname = (*it).str() + dirName.getLength();

		if ((tempname.find('\\') == nullptr) && (tempname.find('/') == nullptr)) {
			// this file doesn't reside in a subdirectory, load it first.
			filesRead += load( *it, loadType, pXfer );
		}
		++it;
	}

	it = filenameList.begin();
	while (it != filenameList.end())
	{
		AsciiString tempname;
		tempname = (*it).str() + dirName.getLength();

		if ((tempname.find('\\') != nullptr) || (tempname.find('/') != nullptr)) {
			filesRead += load( *it, loadType, pXfer );
		}
		++it;
	}

	return filesRead;
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void INI::prepFile( AsciiString filename, INILoadType loadType )
{
	// if we have a file open already -- we can't do another one
	if( m_readBuffer != nullptr )
	{

		DEBUG_CRASH(( "INI::load, cannot open file '%s', file already open", filename.str() ));
		throw INI_FILE_ALREADY_OPEN;

	}

	// open the file
	File* file = TheFileSystem->openFile(filename.str(), File::READ);
	if( file == nullptr )
	{

		DEBUG_CRASH(( "INI::load, cannot open file '%s'", filename.str() ));
		throw INI_CANT_OPEN_FILE;

	}

	m_readBufferNext = 0;
	m_readBufferUsed = file->size();
	m_readBuffer = file->readEntireAndClose();

	// save our filename
	m_filename = filename;

	// save our load type
	m_loadType = loadType;
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void INI::unPrepFile()
{
	// delete the buffer
	delete[] m_readBuffer;
	m_readBuffer = nullptr;
	m_readBufferNext = 0;
	m_readBufferUsed = 0;

	m_filename = "None";
	m_loadType = INI_LOAD_INVALID;
	m_lineNum = 0;
	m_endOfFile = FALSE;
	s_xfer = nullptr;
}

//-------------------------------------------------------------------------------------------------
static INIBlockParse findBlockParse(const char* token)
{
	for (size_t i = 0; i < ARRAY_SIZE(theTypeTable); ++i)
	{
		if (strcmp(theTypeTable[i].token, token) == 0)
		{
			return theTypeTable[i].parse;
		}
	}

	return nullptr;
}

//-------------------------------------------------------------------------------------------------
static INIFieldParseProc findFieldParse(const FieldParse* parseTable, const char* token, int& offset, const void*& userData)
{
	const FieldParse* parse = parseTable;
	for (; parse->token; ++parse)
	{
		if (strcmp( parse->token, token ) == 0)
		{
			offset = parse->offset;
			userData = parse->userData;
			return parse->parse;
		}
	}

	if (!parse->token && parse->parse)
	{
		offset = parse->offset;
		userData = token;
		return parse->parse;
	}
	else
	{
		return nullptr;
	}
}

//-------------------------------------------------------------------------------------------------
/** Load and parse an INI file */
//-------------------------------------------------------------------------------------------------
UnsignedInt INI::load( AsciiString filename, INILoadType loadType, Xfer *pXfer )
{
	// GeneralsX @feature BenderAI 20/02/2026 Debug hang investigation
	fprintf(stderr, "[INI] load('%s') START\n", filename.str());
	fflush(stderr);
	
	setFPMode(); // so we have consistent Real values for GameLogic -MDC

	s_xfer = pXfer;
	fprintf(stderr, "[INI] load - calling prepFile('%s') START\n", filename.str());
	fflush(stderr);
	prepFile(filename, loadType);
	fprintf(stderr, "[INI] load - prepFile completed\n");
	fflush(stderr);

	try
	{

		// read all lines in the file
		DEBUG_ASSERTCRASH( m_endOfFile == FALSE, ("INI::load, EOF at the beginning!") );
		UnsignedInt lineCount = 0;
		while( m_endOfFile == FALSE )
		{
			lineCount++;
			if ((lineCount % 100) == 0) {
				fprintf(stderr, "[INI] load - processed %d lines\n", lineCount);
				fflush(stderr);
			}
			
			// read this line
			readLine();

			AsciiString currentLine = m_buffer;

			// the first word is the type of data we're processing
			const char *token = strtok( m_buffer, getSeps() );
			if( token )
			{
				INIBlockParse parse = findBlockParse(token);
				if (parse)
				{
					#ifdef DEBUG_CRASHING
					static_assert(ARRAY_SIZE(m_curBlockStart) >= ARRAY_SIZE(m_buffer), "Incorrect array size");
					strcpy(m_curBlockStart, m_buffer);
					#endif
					try {
						(*parse)( this );

					} catch (...) {
						DEBUG_CRASH(("Error parsing block '%s' in INI file '%s'", token, m_filename.str()) );
						char buff[1024];
						snprintf(buff, ARRAY_SIZE(buff), "Error parsing INI file '%s' (Line: '%s')\n",
							m_filename.str(), currentLine.str());

						throw INIException(buff);
					}
					#ifdef DEBUG_CRASHING
						strcpy(m_curBlockStart, "NO_BLOCK");
					#endif
				}
				else
				{
					DEBUG_CRASH( ("[LINE: %d - FILE: '%s'] Unknown block '%s'",
														 getLineNum(), getFilename().str(), token ) );
					throw INI_UNKNOWN_TOKEN;
				}

			}

		}
		fprintf(stderr, "[INI] load - processed total %d lines\n", lineCount);
		fflush(stderr);
	}
	catch (...)
	{
		fprintf(stderr, "[INI] ERROR in load('%s') - exception caught\n", filename.str());
		fflush(stderr);
		unPrepFile();

		// propagate the exception.
		throw;
	}

	unPrepFile();

	fprintf(stderr, "[INI] load('%s') END\n", filename.str());
	fflush(stderr);
	return 1;
}

//-------------------------------------------------------------------------------------------------
/** Read a line from the already open file.  Any comments will be removed and
	* therefore ignored from any given line
	* 
	* TheSuperHackers @performance xezon 18/01/2026 The file contents are now read directly from a
	* full File Ram buffer into the INI Line Buffer without a third buffer in between.
	*/
//-------------------------------------------------------------------------------------------------
void INI::readLine()
{
	// sanity
	DEBUG_ASSERTCRASH( m_readBuffer, ("readLine(), read buffer is null") );

	if (m_endOfFile)
	{
		*m_buffer = 0;
	}
	else
	{
		// read up till the newline or semicolon character, or until out of space
		char *p = m_buffer;
		while (p != m_buffer+INI_MAX_CHARS_PER_LINE)
		{
			// test end of read buffer
			if (m_readBufferNext==m_readBufferUsed)
			{
				m_endOfFile = true;
				*p = 0;
				break;
			}

			// get next character
			*p = m_readBuffer[m_readBufferNext++];

			// check for new line
			if (*p == '\n')
			{
				*p = 0;
				break;
			}

			DEBUG_ASSERTCRASH(*p != '\t', ("tab characters are not allowed in INI files (%s). please check your editor settings. Line Number %d", m_filename.str(), getLineNum()));

			// if this is a semicolon, that represents the start of a comment
			if (*p == ';')
			{
				*p = 0;
			}

			// make whitespace characters actual spaces
			else if (*p > 0 && *p < 32)
			{
				*p = ' ';
			}

			p++;
		}

		*p = 0;

		// increase our line count
		m_lineNum++;

		// check for at the max
		if ( p == m_buffer+INI_MAX_CHARS_PER_LINE )
		{
			DEBUG_CRASH( ("Buffer too small (%d) and was truncated, increase INI_MAX_CHARS_PER_LINE", INI_MAX_CHARS_PER_LINE) );
		}
	}

	if (s_xfer)
	{
		s_xfer->xferUser( m_buffer, sizeof( char ) * strlen( m_buffer ) );
		//DEBUG_LOG(("Xfer val is now 0x%8.8X in %s, line %s", ((XferCRC *)s_xfer)->getCRC(), m_filename.str(), m_buffer));
	}
}

//-------------------------------------------------------------------------------------------------
/** Parse UnsignedByte from buffer and assign at location 'store' */
//-------------------------------------------------------------------------------------------------
void INI::parseUnsignedByte( INI* ini, void * /*instance*/, void *store, const void* /*userData*/ )
{
	const char *token = ini->getNextToken();
	Int value = scanInt(token);
	if (value < 0 || value > 255)
	{
		DEBUG_CRASH(("Bad value INI::parseUnsignedByte"));
		throw ERROR_BUG;
	}
	*(Byte *)store = (Byte)value;
}

//-------------------------------------------------------------------------------------------------
/** Parse signed short from buffer and assign at location 'store' */
//-------------------------------------------------------------------------------------------------
void INI::parseShort( INI* ini, void * /*instance*/, void *store, const void* /*userData*/ )
{
	const char *token = ini->getNextToken();
	Int value = scanInt(token);
	if (value < -32768 || value > 32767)
	{
		DEBUG_CRASH(("Bad value INI::parseShort"));
		throw ERROR_BUG;
	}
	*(Short *)store = (Short)value;
}

//-------------------------------------------------------------------------------------------------
/** Parse unsigned short from buffer and assign at location 'store' */
//-------------------------------------------------------------------------------------------------
void INI::parseUnsignedShort( INI* ini, void * /*instance*/, void *store, const void* /*userData*/ )
{
	const char *token = ini->getNextToken();
	Int value = scanInt(token);
	if (value < 0 || value > 65535)
	{
		DEBUG_CRASH(("Bad value INI::parseUnsignedShort"));
		throw ERROR_BUG;
	}
	*(UnsignedShort *)store = (UnsignedShort)value;
}

//-------------------------------------------------------------------------------------------------
/** Parse integer from buffer and assign at location 'store' */
//-------------------------------------------------------------------------------------------------
void INI::parseInt( INI* ini, void * /*instance*/, void *store, const void* /*userData*/ )
{
	const char *token = ini->getNextToken();
	*(Int *)store = scanInt(token);

}

//-------------------------------------------------------------------------------------------------
/** Parse unsigned integer from buffer and assign at location 'store' */
//-------------------------------------------------------------------------------------------------
void INI::parseUnsignedInt( INI* ini, void * /*instance*/, void *store, const void* /*userData*/ )
{
	const char *token = ini->getNextToken();
	*(UnsignedInt *)store = scanUnsignedInt(token);

}

//-------------------------------------------------------------------------------------------------
/** Parse real from buffer and assign at location 'store' */
//-------------------------------------------------------------------------------------------------
void INI::parseReal( INI* ini, void * /*instance*/, void *store, const void* /*userData*/ )
{
	const char *token = ini->getNextToken();
	*(Real *)store = scanReal(token);

}

//-------------------------------------------------------------------------------------------------
/** Parse real from buffer and assign at location 'store' */
//-------------------------------------------------------------------------------------------------
void INI::parsePositiveNonZeroReal( INI* ini, void * /*instance*/, void *store, const void* /*userData*/ )
{
	const char *token = ini->getNextToken();
	*(Real *)store = scanReal(token);
	if (*(Real *)store <= 0.0f)
	{
		DEBUG_CRASH(("invalid Real value %f -- expected > 0",*(Real*)store));
		throw INI_INVALID_DATA;
	}

}

//-------------------------------------------------------------------------------------------------
/** Parse a degree value (0 to 360) and store the radian value of that degree
	* in a Real */
//-------------------------------------------------------------------------------------------------
void INI::parseAngleReal( INI *ini, void * /*instance*/,
																			void *store, const void *userData )
{
	const char *token = ini->getNextToken();

	const Real RADS_PER_DEGREE = PI / 180.0f;
	*(Real *)store = scanReal( token ) * RADS_PER_DEGREE;

}

//-------------------------------------------------------------------------------------------------
/** Parse an angular velocity in degrees-per-sec and store the rads-per-frame value of that degree
	* in a Real */
//-------------------------------------------------------------------------------------------------
void INI::parseAngularVelocityReal( INI *ini, void * /*instance*/,
																			void *store, const void *userData )
{
	const char *token = ini->getNextToken();

	// scan the int and convert to radian and store as a real
	*(Real *)store = ConvertAngularVelocityInDegreesPerSecToRadsPerFrame(scanReal( token ));

}

//-------------------------------------------------------------------------------------------------
/** Parse Bool from buffer and assign at location 'store'.  The buffer token must
	* be in the form of a string "Yes" or "No" (case is ignored) */
//-------------------------------------------------------------------------------------------------
void INI::parseBool( INI* ini, void * /*instance*/, void *store, const void* /*userData*/ )
{
	*(Bool*)store = INI::scanBool(ini->getNextToken());
}

//-------------------------------------------------------------------------------------------------
/** Parse Bool from buffer; if true, or in MASK, otherwise and out MASK. The buffer token must
	* be in the form of a string "Yes" or "No" (case is ignored) */
//-------------------------------------------------------------------------------------------------
void INI::parseBitInInt32( INI *ini, void *instance, void *store, const void* userData )
{
	UnsignedInt* s = (UnsignedInt*)store;
	// GeneralsX @build BenderAI 12/02/2026 64-bit safe pointer cast
	UnsignedInt mask = static_cast<UnsignedInt>(reinterpret_cast<uintptr_t>(userData));

	if (INI::scanBool(ini->getNextToken()))
		*s |= mask;
	else
		*s &= ~mask;
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
/*static*/ Bool INI::scanBool(const char* token)
{
	// translate string yes/no into TRUE/FALSE
	if( stricmp( token, "yes" ) == 0 )
		return TRUE;
	else if( stricmp( token, "no" ) == 0 )
		return FALSE;
	else
	{
		DEBUG_CRASH(("invalid boolean token %s -- expected Yes or No",token));
		throw INI_INVALID_DATA;
		return false;	// keep compiler happy
	}

}

//-------------------------------------------------------------------------------------------------
/** Parse an *ASCII* string from buffer and assign at location 'store' */
//-------------------------------------------------------------------------------------------------
void INI::parseAsciiString( INI* ini, void * /*instance*/, void *store, const void* /*userData*/ )
{
	AsciiString* asciiString = (AsciiString *)store;
	*asciiString = ini->getNextAsciiString();
}

//-------------------------------------------------------------------------------------------------
/** Parse an *ASCII* string from buffer and assign at location 'store'. Has better support for quoted strings.
We don't really need this function, but parseString() is broken and we want to leave it broken to
maintain existing code.
 */
//-------------------------------------------------------------------------------------------------
void INI::parseQuotedAsciiString( INI* ini, void * /*instance*/, void *store, const void* /*userData*/ )
{
	AsciiString* asciiString = (AsciiString *)store;
	*asciiString = ini->getNextQuotedAsciiString();
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void INI::parseAsciiStringVector( INI* ini, void * /*instance*/, void *store, const void* /*userData*/ )
{
	std::vector<AsciiString>* asv = (std::vector<AsciiString>*)store;
	asv->clear();
	for (const char *token = ini->getNextTokenOrNull(); token; token = ini->getNextTokenOrNull())
	{
		asv->push_back(token);
	}
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void INI::parseAsciiStringVectorAppend( INI* ini, void * /*instance*/, void *store, const void* /*userData*/ )
{
	std::vector<AsciiString>* asv = (std::vector<AsciiString>*)store;
	// nope, don't clear. duh.
	// asv->clear();
	for (const char *token = ini->getNextTokenOrNull(); token; token = ini->getNextTokenOrNull())
	{
		asv->push_back(token);
	}
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
/* static */void INI::parseScienceVector( INI *ini, void * /*instance*/, void *store, const void *userData )
{
	ScienceVec* asv = (ScienceVec*)store;
	asv->clear();
	for (const char *token = ini->getNextTokenOrNull(); token; token = ini->getNextTokenOrNull())
	{
		if (stricmp(token, "None") == 0)
		{
			asv->clear();
			return;
		}
		asv->push_back(INI::scanScience( token ));
	}
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
AsciiString INI::getNextQuotedAsciiString()
{
	AsciiString result;
	char buff[INI_MAX_CHARS_PER_LINE];
	buff[0] = '\0';

	const char *token = getNextTokenOrNull();	// if null, just leave an empty string
	if (token != nullptr)
	{
		if (token[0] != '\"')
		{
			// if token is simply "
			result.set( token );	// Start following the "
		}
		else
		{	int strLen=0;
			Bool done=FALSE;
			if ((strLen=strlen(token)) > 1)
			{
				strlcpy(buff, &token[1], ARRAY_SIZE(buff));	//skip the starting quote
				//Check for end of quoted string.  Checking here fixes cases where quoted string on same line with other data.
				if (buff[strLen-2]=='"')	//skip ending quote if present
				{	buff[strLen-2]='\0';
					done=TRUE;
				}
			}

			if (!done)
			{
				token = getNextToken(getSepsQuote());

				if (strlen(token) > 1 && token[1] != '\t')
				{
					strlcat(buff, " ", ARRAY_SIZE(buff));
					strlcat(buff, token, ARRAY_SIZE(buff));
				}
				else
				{	Int buflen=strlen(buff);
					if (buff[buflen-1]=='\"')
						buff[buflen-1]='\0';
				}
			}
			result.set(buff);
		}
	}
	return result;
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
AsciiString INI::getNextAsciiString()
{
	AsciiString result;

	const char *token = getNextTokenOrNull();	// if null, just leave an empty string
	if (token != nullptr)
	{
		if (token[0] != '\"')
		{
			// if token is simply "
			result.set( token );	// Start following the "
		}
		else
		{
			static char buff[INI_MAX_CHARS_PER_LINE];
			buff[0] = 0;
			if (strlen(token) > 1)
			{
				strlcpy(buff, &token[1], ARRAY_SIZE(buff));
			}

			token = getNextTokenOrNull(getSepsQuote());
			if (token) {
				if (strlen(token) > 1 && token[1] != '\t')
				{
					strlcat(buff, " ", ARRAY_SIZE(buff));
				}
				strlcat(buff, token, ARRAY_SIZE(buff));
				result.set(buff);
			} else {
				Int len = strlen(buff);
				if (len && buff[len-1] == '"') { // strip off trailing quote jba. [2/12/2003]
					buff[len-1] = 0;
				}
				result.set(buff);
			}
		}
	}
	return result;
}

//-------------------------------------------------------------------------------------------------
/** Parse a string label, get the *translated* actual text from the label and store
	* into a *UNICODE* string. */
//-------------------------------------------------------------------------------------------------
void INI::parseAndTranslateLabel( INI* ini, void * /*instance*/, void *store, const void* /*userData*/ )
{
	const char *token = ini->getNextToken();

	// translate
	UnicodeString translated = TheGameText->fetch( token );
	if( translated.isEmpty() )
		throw INI_INVALID_DATA;

	// save the translated text
	UnicodeString *theString = (UnicodeString *)store;
	theString->set( translated.str() );

}

//-------------------------------------------------------------------------------------------------
/** Parse a string label assumed as an image as part of the image collection.  Translate
	* to an image pointer for storage */
//-------------------------------------------------------------------------------------------------
void INI::parseMappedImage( INI *ini, void * /*instance*/, void *store, const void *userData )
{
	const char *token = ini->getNextToken();

	if( TheMappedImageCollection )
	{
		typedef const Image* ConstImagePtr;
		*(ConstImagePtr*)store = TheMappedImageCollection->findImageByName( token );
	}

	//KM: If we are in the worldbuilder, we want to parse commandbuttons for informational purposes,
	//but we don't care about the images -- because we never access them. In RTS/GUIEdit, they always
	//exist -- and in those cases, it will never call this code anyways because it'll throw long before.
	//else
	//	throw INI_UNKNOWN_ERROR;

}

// ------------------------------------------------------------------------------------------------
/** Parse a string label assumed as a Anim2D template name.  Translate that name to an
	* actual template pointer for storage */
// ------------------------------------------------------------------------------------------------
/*static*/ void INI::parseAnim2DTemplate( INI *ini, void *instance, void *store, const void *userData )
{
	const char *token = ini->getNextToken();

	if( TheAnim2DCollection )
	{
		Anim2DTemplate **anim2DTemplate = (Anim2DTemplate **)store;
		*anim2DTemplate = TheAnim2DCollection->findTemplate( AsciiString( token ) );
	}
	else
	{

		DEBUG_CRASH(( "INI::parseAnim2DTemplate - TheAnim2DCollection is null" ));
		throw INI_UNKNOWN_ERROR;

	}

}

//-------------------------------------------------------------------------------------------------
/** Parse a percent in int or real form such as "23%" or "95.4%" and assign
	* to location 'store' as a number from 0.0 to 1.0 */
//-------------------------------------------------------------------------------------------------
void INI::parsePercentToReal( INI* ini, void * /*instance*/, void *store, const void* /*userData*/ )
{
	const char *token = ini->getNextToken(ini->getSepsPercent());
	Real *theReal = (Real *)store;
	*theReal = scanPercentToReal(token);

}

//-------------------------------------------------------------------------------------------------
/** 'store' points to an 32 bit unsigned integer.  We will zero that integer, parse each token
	* in the buffer, if the token is in the userData table of strings, we will set the
	* according bit flag for it */
//-------------------------------------------------------------------------------------------------
void INI::parseBitString8( INI* ini, void * /*instance*/, void *store, const void* userData )
{
	UnsignedInt tmp;
	INI::parseBitString32(ini, nullptr, &tmp, userData);
	if (tmp & 0xffffff00)
	{
		DEBUG_CRASH(("Bad bitstring list INI::parseBitString8"));
		throw ERROR_BUG;
	}
	*(Byte*)store = (Byte)tmp;
}

//-------------------------------------------------------------------------------------------------
/** 'store' points to an 32 bit unsigned integer.  We will zero that integer, parse each token
	* in the buffer, if the token is in the userData table of strings, we will set the
	* according bit flag for it */
//-------------------------------------------------------------------------------------------------
void INI::parseBitString32( INI* ini, void * /*instance*/, void *store, const void* userData )
{
	ConstCharPtrArray flagList = (ConstCharPtrArray)userData;
	UnsignedInt *bits = (UnsignedInt *)store;

	if( flagList == nullptr || flagList[ 0 ] == nullptr)
	{
		DEBUG_ASSERTCRASH( flagList, ("INTERNAL ERROR! parseBitString32: No flag list provided!") );
		throw INI_INVALID_NAME_LIST;
	}

	Bool foundNormal = false;
	Bool foundAddOrSub = false;

	// loop through all tokens
	for (const char *token = ini->getNextTokenOrNull(); token; token = ini->getNextTokenOrNull())
	{
		if (stricmp(token, "NONE") == 0)
		{
			if (foundNormal || foundAddOrSub)
			{
				DEBUG_CRASH(("you may not mix normal and +- ops in bitstring lists"));
				throw INI_INVALID_NAME_LIST;
			}
			*bits = 0;
			break;
		}

		if (token[0] == '+')
		{
			if (foundNormal)
			{
				DEBUG_CRASH(("you may not mix normal and +- ops in bitstring lists"));
				throw INI_INVALID_NAME_LIST;
			}
			Int bitIndex = INI::scanIndexList(token+1, flagList);	// this throws if the token is not found
			*bits |= (1 << bitIndex);
			foundAddOrSub = true;
		}
		else if (token[0] == '-')
		{
			if (foundNormal)
			{
				DEBUG_CRASH(("you may not mix normal and +- ops in bitstring lists"));
				throw INI_INVALID_NAME_LIST;
			}
			Int bitIndex = INI::scanIndexList(token+1, flagList);	// this throws if the token is not found
			*bits &= ~(1 << bitIndex);
			foundAddOrSub = true;
		}
		else
		{
			if (foundAddOrSub)
			{
				DEBUG_CRASH(("you may not mix normal and +- ops in bitstring lists"));
				throw INI_INVALID_NAME_LIST;
			}

			if (!foundNormal)
				*bits = 0;

			Int bitIndex = INI::scanIndexList(token, flagList);	// this throws if the token is not found
			*bits |= (1 << bitIndex);
			foundNormal = true;
		}
	}
}

//-------------------------------------------------------------------------------------------------
/** Parse a color in the form of
	*
	* RGB_COLOR = R:100 G:114 B:245
	* and store in "RGBColor" structure pointed to by 'store' */
//-------------------------------------------------------------------------------------------------
void INI::parseRGBColor( INI* ini, void * /*instance*/, void *store, const void* /*userData*/ )
{
	const char* names[3] = { "R", "G", "B" };
	Int colors[3];
	for( Int i = 0; i < 3; i++ )
	{
		colors[i] = scanInt(ini->getNextSubToken(names[i]));
		if( colors[ i ] < 0 )
			throw INI_INVALID_DATA;
		if( colors[ i ] > 255 )
			throw INI_INVALID_DATA;
	}

	// assign the color components to the "RGBColor" pointer at 'store'
	RGBColor *theColor = (RGBColor *)store;
	theColor->red		= (Real)colors[ 0 ] / 255.0f;
	theColor->green = (Real)colors[ 1 ] / 255.0f;
	theColor->blue	= (Real)colors[ 2 ] / 255.0f;

}

//-------------------------------------------------------------------------------------------------
/** Parse a color in the form of
	*
	* RGB_COLOR = R:100 G:114 B:245 [A:233]
	* and store in "RGBAColorInt" structure pointed to by 'store' */
//-------------------------------------------------------------------------------------------------
void INI::parseRGBAColorInt( INI* ini, void * /*instance*/, void *store, const void* /*userData*/ )
{
	const char* names[4] = { "R", "G", "B", "A" };
	Int colors[4];
	for( Int i = 0; i < 4; i++ )
	{
		const char* token = ini->getNextTokenOrNull(ini->getSepsColon());
		if (token == nullptr)
		{
			if (i < 3)
			{
				throw INI_INVALID_DATA;
			}
			else
			{
				// it's ok for A to be omitted.
				colors[i] = 255;
			}
		}
		else
		{
			// if present, the token must match.
			if (stricmp(token, names[i]) != 0)
			{
				throw INI_INVALID_DATA;
			}
			colors[i] = scanInt(ini->getNextToken(ini->getSepsColon()));
		}
		if( colors[ i ] < 0 )
			throw INI_INVALID_DATA;
		if( colors[ i ] > 255 )
			throw INI_INVALID_DATA;
	}

	//
	// assign the color components to the "RGBColorInt" pointer at 'store', keep
	// the numbers as between 0 and 255
	//
	RGBAColorInt *theColor = (RGBAColorInt *)store;
	theColor->red		= colors[ 0 ];
	theColor->green = colors[ 1 ];
	theColor->blue	= colors[ 2 ];
	theColor->alpha = colors[ 3 ];

}

//-------------------------------------------------------------------------------------------------
/** Parse a color in the form of
	*
	* RGB_COLOR = R:100 G:114 B:245 [A:233]
	* and store in "Color" structure pointed to by 'store' */
//-------------------------------------------------------------------------------------------------
void INI::parseColorInt( INI* ini, void * /*instance*/, void *store, const void* /*userData*/ )
{
	const char* names[4] = { "R", "G", "B", "A" };
	Int colors[4];
	for( Int i = 0; i < 4; i++ )
	{
		const char* token = ini->getNextTokenOrNull(ini->getSepsColon());
		if (token == nullptr)
		{
			if (i < 3)
			{
				throw INI_INVALID_DATA;
			}
			else
			{
				// it's ok for A to be omitted.
				colors[i] = 255;
			}
		}
		else
		{
			// if present, the token must match.
			if (stricmp(token, names[i]) != 0)
			{
				throw INI_INVALID_DATA;
			}
			colors[i] = scanInt(ini->getNextToken(ini->getSepsColon()));
		}
		if( colors[ i ] < 0 )
			throw INI_INVALID_DATA;
		if( colors[ i ] > 255 )
			throw INI_INVALID_DATA;
	}

	//
	// assign the color components to the "Color" pointer at 'store', keep
	// the numbers as between 0 and 255
	//
	Color *theColor = (Color *)store;
	*theColor = GameMakeColor(colors[0], colors[1], colors[2], colors[3]);

}

//-------------------------------------------------------------------------------------------------
/** Parse a 3D coordinate of reals in the form of:
	* FIELD_NAME = X:400 Y:-214.3 Z:8.6 */
//-------------------------------------------------------------------------------------------------
void INI::parseCoord3D( INI* ini, void * /*instance*/, void *store, const void* /*userData*/ )
{
	Coord3D *theCoord = (Coord3D *)store;

	theCoord->x = scanReal(ini->getNextSubToken("X"));
	theCoord->y = scanReal(ini->getNextSubToken("Y"));
	theCoord->z = scanReal(ini->getNextSubToken("Z"));

}

//-------------------------------------------------------------------------------------------------
/** Parse a 2D coordinate of reals in the form of:
	* FIELD_NAME = X:400 Y:-214.3 */
//-------------------------------------------------------------------------------------------------
void INI::parseCoord2D( INI* ini, void * /*instance*/, void *store, const void* /*userData*/ )
{
	Coord2D *theCoord = (Coord2D *)store;

	theCoord->x = scanReal(ini->getNextSubToken("X"));
	theCoord->y = scanReal(ini->getNextSubToken("Y"));

}

//-------------------------------------------------------------------------------------------------
/** Parse a 2D coordinate of Ints in the form of:
	* FIELD_NAME = X:400 Y:-214 */
//-------------------------------------------------------------------------------------------------
void INI::parseICoord2D( INI* ini, void * /*instance*/, void *store, const void* /*userData*/ )
{
	ICoord2D *theCoord = (ICoord2D *)store;

	theCoord->x = scanInt(ini->getNextSubToken("X"));
	theCoord->y = scanInt(ini->getNextSubToken("Y"));

}

//-------------------------------------------------------------------------------------------------
/** Parse an audio event and assign to the 'AudioEventRTS*' at store */
//-------------------------------------------------------------------------------------------------
void INI::parseDynamicAudioEventRTS( INI *ini, void * /*instance*/, void *store, const void* userData )
{
	const char *token = ini->getNextToken();
	DynamicAudioEventRTS** theSound = (DynamicAudioEventRTS**)store;

	// translate the string into a sound
	if (stricmp(token, "NoSound") == 0)
	{
		deleteInstance(*theSound);
		*theSound = nullptr;
	}
	else
	{
		if (*theSound == nullptr)
			*theSound = newInstance(DynamicAudioEventRTS);
		(*theSound)->m_event.setEventName(AsciiString(token));
	}

	if (*theSound)
		TheAudio->getInfoForAudioEvent(&(*theSound)->m_event);
}

//-------------------------------------------------------------------------------------------------
/** Parse an audio event and assign to the 'AudioEventRTS*' at store */
//-------------------------------------------------------------------------------------------------
void INI::parseAudioEventRTS( INI *ini, void * /*instance*/, void *store, const void* userData )
{
	const char *token = ini->getNextToken();

	AudioEventRTS *theSound = (AudioEventRTS*)store;

	// translate the string into a sound
	if (stricmp(token, "NoSound") != 0) {
		theSound->setEventName(AsciiString(token));
	}

	TheAudio->getInfoForAudioEvent(theSound);
}

//-------------------------------------------------------------------------------------------------
/** Parse an ThingTemplate and assign to the 'ThingTemplate *' at store */
//-------------------------------------------------------------------------------------------------
void INI::parseThingTemplate( INI* ini, void * /*instance*/, void *store, const void* /*userData*/ )
{
	const char *token = ini->getNextToken();

	if (!TheThingFactory)
	{
		DEBUG_CRASH(("TheThingFactory not inited yet"));
		throw ERROR_BUG;
	}

	typedef const ThingTemplate *ConstThingTemplatePtr;
	ConstThingTemplatePtr* theThingTemplate = (ConstThingTemplatePtr*)store;

	if (stricmp(token, "None") == 0)
	{
		*theThingTemplate = nullptr;
	}
	else
	{
		const ThingTemplate *tt = TheThingFactory->findTemplate(token);	// could be null!
		DEBUG_ASSERTCRASH(tt, ("ThingTemplate %s not found!",token));
		// assign it, even if null!
		*theThingTemplate = tt;
	}

}

//-------------------------------------------------------------------------------------------------
/** Parse an ArmorTemplate and assign to the 'ArmorTemplate *' at store */
//-------------------------------------------------------------------------------------------------
void INI::parseArmorTemplate( INI* ini, void * /*instance*/, void *store, const void* /*userData*/ )
{
	const char *token = ini->getNextToken();

	typedef const ArmorTemplate *ConstArmorTemplatePtr;
	ConstArmorTemplatePtr* theArmorTemplate = (ConstArmorTemplatePtr*)store;

	if (stricmp(token, "None") == 0)
	{
		*theArmorTemplate = nullptr;
	}
	else
	{
		const ArmorTemplate *tt = TheArmorStore->findArmorTemplate(token);	// could be null!
		DEBUG_ASSERTCRASH(tt, ("ArmorTemplate %s not found!",token));
		// assign it, even if null!
		*theArmorTemplate = tt;
	}

}

//-------------------------------------------------------------------------------------------------
/** Parse an WeaponTemplate and assign to the 'WeaponTemplate *' at store */
//-------------------------------------------------------------------------------------------------
void INI::parseWeaponTemplate( INI* ini, void * /*instance*/, void *store, const void* /*userData*/ )
{
	const char *token = ini->getNextToken();

	typedef const WeaponTemplate *ConstWeaponTemplatePtr;
	ConstWeaponTemplatePtr* theWeaponTemplate = (ConstWeaponTemplatePtr*)store;

	const WeaponTemplate *tt = TheWeaponStore->findWeaponTemplate(token);	// could be null!
	DEBUG_ASSERTCRASH(tt || stricmp(token, "None") == 0, ("WeaponTemplate %s not found!",token));
	// assign it, even if null!
	*theWeaponTemplate = tt;

}

//-------------------------------------------------------------------------------------------------
/** Parse an FXList and assign to the 'FXList *' at store */
//-------------------------------------------------------------------------------------------------
void INI::parseFXList( INI* ini, void * /*instance*/, void *store, const void* /*userData*/ )
{
	const char *token = ini->getNextToken();

	typedef const FXList *ConstFXListPtr;
	ConstFXListPtr* theFXList = (ConstFXListPtr*)store;

	const FXList *fxl = TheFXListStore->findFXList(token);	// could be null!
	DEBUG_ASSERTCRASH(fxl != nullptr || stricmp(token, "None") == 0, ("FXList %s not found!",token));
	// assign it, even if null!
	*theFXList = fxl;

}

//-------------------------------------------------------------------------------------------------
/** Parse a particle system and assign to 'ParticleSystemTemplate *' at store */
//-------------------------------------------------------------------------------------------------
void INI::parseParticleSystemTemplate( INI *ini, void * /*instance*/, void *store, const void *userData )
{
	const char *token = ini->getNextToken();

	const ParticleSystemTemplate *pSystemT = TheParticleSystemManager->findTemplate( AsciiString( token ) );
	DEBUG_ASSERTCRASH( pSystemT || stricmp( token, "None" ) == 0, ("ParticleSystem %s not found!",token) );

	typedef const ParticleSystemTemplate* ConstParticleSystemTemplatePtr;
	ConstParticleSystemTemplatePtr* theParticleSystemTemplate = (ConstParticleSystemTemplatePtr*)store;

	*theParticleSystemTemplate = pSystemT;

}

//-------------------------------------------------------------------------------------------------
/** Parse an DamageFX and assign to the 'DamageFX *' at store */
//-------------------------------------------------------------------------------------------------
void INI::parseDamageFX( INI* ini, void * /*instance*/, void *store, const void* /*userData*/ )
{
	const char *token = ini->getNextToken();

	typedef const DamageFX *ConstDamageFXPtr;
	ConstDamageFXPtr* theDamageFX = (ConstDamageFXPtr*)store;

	if (stricmp(token, "None") == 0)
	{
		*theDamageFX = nullptr;
	}
	else
	{
		const DamageFX *fxl = TheDamageFXStore->findDamageFX(token);	// could be null!
		DEBUG_ASSERTCRASH(fxl, ("DamageFX %s not found!",token));
		// assign it, even if null!
		*theDamageFX = fxl;
	}

}

//-------------------------------------------------------------------------------------------------
/** Parse an ObjectCreationList and assign to the 'ObjectCreationList *' at store */
//-------------------------------------------------------------------------------------------------
void INI::parseObjectCreationList( INI* ini, void * /*instance*/, void *store, const void* /*userData*/ )
{
	const char *token = ini->getNextToken();

	typedef const ObjectCreationList *ConstObjectCreationListPtr;
	ConstObjectCreationListPtr* theObjectCreationList = (ConstObjectCreationListPtr*)store;

	const ObjectCreationList *ocl = TheObjectCreationListStore->findObjectCreationList(token);	// could be null!
	DEBUG_ASSERTCRASH(ocl || stricmp(token, "None") == 0, ("ObjectCreationList %s not found!",token));
	// assign it, even if null!
	*theObjectCreationList = ocl;

}

//-------------------------------------------------------------------------------------------------
/** Parse a upgrade template string and store as template pointer */
//-------------------------------------------------------------------------------------------------
void INI::parseUpgradeTemplate( INI* ini, void * /*instance*/, void *store, const void* /*userData*/ )
{
	const char *token = ini->getNextToken();

	if (!TheUpgradeCenter)
	{
		DEBUG_CRASH(("TheUpgradeCenter not inited yet"));
		throw ERROR_BUG;
	}

	const UpgradeTemplate *uu = TheUpgradeCenter->findUpgrade( token );
	DEBUG_ASSERTCRASH( uu || stricmp( token, "None" ) == 0, ("Upgrade %s not found!",token) );

	typedef const UpgradeTemplate* ConstUpgradeTemplatePtr;
	ConstUpgradeTemplatePtr* theUpgradeTemplate = (ConstUpgradeTemplatePtr *)store;
	*theUpgradeTemplate = uu;
}

//-------------------------------------------------------------------------------------------------
/** Parse a special power template string and store as template pointer */
//-------------------------------------------------------------------------------------------------
void INI::parseSpecialPowerTemplate( INI* ini, void * /*instance*/, void *store, const void* /*userData*/ )
{
	const char *token = ini->getNextToken();

	if (!TheSpecialPowerStore)
	{
		DEBUG_CRASH(("TheSpecialPowerStore not inited yet"));
		throw ERROR_BUG;
	}

	const SpecialPowerTemplate *sPowerT = TheSpecialPowerStore->findSpecialPowerTemplate( AsciiString( token ) );
	if( !sPowerT && stricmp( token, "None" ) != 0 )
	{
		DEBUG_CRASH( ("[LINE: %d in '%s'] Specialpower %s not found!", ini->getLineNum(), ini->getFilename().str(), token) );
	}

	typedef const SpecialPowerTemplate* ConstSpecialPowerTemplatePtr;
	ConstSpecialPowerTemplatePtr* theSpecialPowerTemplate = (ConstSpecialPowerTemplatePtr *)store;
	*theSpecialPowerTemplate = sPowerT;
}

//-------------------------------------------------------------------------------------------------
/** Parse a science string and store as science type */
//-------------------------------------------------------------------------------------------------
/* static */void INI::parseScience( INI *ini, void * /*instance*/, void *store, const void *userData )
{
	const char *token = ini->getNextToken();

	if (!TheScienceStore)
	{
		DEBUG_CRASH(("TheScienceStore not inited yet"));
		throw ERROR_BUG;
	}

	*((ScienceType *)store) = INI::scanScience(token);

}

//-------------------------------------------------------------------------------------------------
/** Parse a single string token, check for that token in the index list
	* of names provided and store the index into that list.
	*
	* NOTE: Is is assumed that we are going to store the index into
	*				a 4 byte integer.  This works well for INT and ENUM definitions */
//-------------------------------------------------------------------------------------------------
void INI::parseIndexList( INI* ini, void * /*instance*/, void *store, const void* userData )
{
	ConstCharPtrArray nameList = (ConstCharPtrArray)userData;
	*(Int *)store = scanIndexList(ini->getNextToken(), nameList);
}

//-------------------------------------------------------------------------------------------------
/** Parse a single string token, check for that token in the index list
	* of names provided and store the index into that list.
	*
	* NOTE: Is is assumed that we are going to store the index into
	*				a 4 byte integer.  This works well for INT and ENUM definitions */
//-------------------------------------------------------------------------------------------------
void INI::parseByteSizedIndexList( INI* ini, void * /*instance*/, void *store, const void* userData )
{
	ConstCharPtrArray nameList = (ConstCharPtrArray)userData;
	Int value = scanIndexList(ini->getNextToken(), nameList);
	if (value < 0 || value > 255)
	{
		DEBUG_CRASH(("Bad index list INI::parseByteSizedIndexList"));
		throw ERROR_BUG;
	}
	*(Byte *)store = (Byte)value;
}

//-------------------------------------------------------------------------------------------------
/** Parse a single string token, check for that token in the index list
	* of names provided and store the associated value into that list.
	*
	* NOTE: Is is assumed that we are going to store the index into
	*				a 4 byte integer.  This works well for INT and ENUM definitions */
//-------------------------------------------------------------------------------------------------
void INI::parseLookupList( INI* ini, void * /*instance*/, void *store, const void* userData )
{
	ConstLookupListRecArray lookupList = (ConstLookupListRecArray)userData;
	*(Int *)store = scanLookupList(ini->getNextToken(), lookupList);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// PRIVATE FUNCTIONS //////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////


//-------------------------------------------------------------------------------------------------
void MultiIniFieldParse::add(const FieldParse* f, UnsignedInt e)
{
	if (m_count < MAX_MULTI_FIELDS)
	{
		m_fieldParse[m_count] = f;
		m_extraOffset[m_count] = e;
		++m_count;
	}
	else
	{
		DEBUG_CRASH(("too many multi-fields in INI::initFromINIMultiProc"));
		throw ERROR_BUG;
	}
}

//-------------------------------------------------------------------------------------------------
void INI::initFromINI( void *what, const FieldParse* parseTable )
{
	MultiIniFieldParse p;
	p.add(parseTable);
	initFromINIMulti(what, p);
}

//-------------------------------------------------------------------------------------------------
void INI::initFromINIMultiProc( void *what, BuildMultiIniFieldProc proc )
{
	MultiIniFieldParse p;
	(*proc)(p);
	initFromINIMulti(what, p);
}

//-------------------------------------------------------------------------------------------------
void INI::initFromINIMulti( void *what, const MultiIniFieldParse& parseTableList )
{
	Bool done = FALSE;

	if( what == nullptr )
	{
		DEBUG_CRASH( ("INI::initFromINI - Invalid parameters supplied!") );
		throw INI_INVALID_PARAMS;
	}

	// read each of the data fields
	while( !done )
	{

		// read next line
		readLine();

		// check for end token
		const char* field = strtok( m_buffer, INI::getSeps() );
		if( field )
		{

			if( stricmp( field, getEndToken() ) == 0 )
			{
				done = TRUE;
			}
			else
			{
				Bool found = false;
				for (int ptIdx = 0; ptIdx < parseTableList.getCount(); ++ptIdx)
				{
					int offset = 0;
					const void* userData = nullptr;
					INIFieldParseProc parse = findFieldParse(parseTableList.getNthFieldParse(ptIdx), field, offset, userData);
					if (parse)
					{
						// parse this block and check for parse errors
						try {

						(*parse)( this, what, (char *)what + offset + parseTableList.getNthExtraOffset(ptIdx), userData );

						} catch (...) {
							DEBUG_CRASH( ("[LINE: %d - FILE: '%s'] Error reading field '%s' of block '%s'",
																 INI::getLineNum(), INI::getFilename().str(), field, m_curBlockStart) );


							char buff[1024];
							snprintf(buff, ARRAY_SIZE(buff), "[LINE: %d - FILE: '%s'] Error reading field '%s'\n",
								INI::getLineNum(), INI::getFilename().str(), field);
							throw INIException(buff);
						}

						found = true;
						break;

					}
				}

				if (!found)
				{
					DEBUG_CRASH( ("[LINE: %d - FILE: '%s'] Unknown field '%s' in block '%s'",
														 INI::getLineNum(), INI::getFilename().str(), field, m_curBlockStart) );
				}

			}

		}

		// sanity check for reaching end of file with no closing end token
		if( done == FALSE && INI::isEOF() == TRUE )
		{

			done = TRUE;
			DEBUG_CRASH( ("Error parsing block '%s', in INI file '%s'.  Missing '%s' token",
												 m_curBlockStart, getFilename().str(), getEndToken()) );
			throw INI_MISSING_END_TOKEN;

		}

	}

}

//-------------------------------------------------------------------------------------------------
/*static*/ const char* INI::getNextToken(const char* seps)
{
	const char *token = ::strtok(nullptr, seps);
	if (!token)
		throw INI_INVALID_DATA;
	return token;
}

//-------------------------------------------------------------------------------------------------
/*static*/ const char* INI::getNextTokenOrNull(const char* seps)
{
	const char *token = ::strtok(nullptr, seps);
	return token;
}

//-------------------------------------------------------------------------------------------------
/*static*/ ScienceType INI::scanScience(const char* token)
{
	return TheScienceStore->friend_lookupScience( token );
}

#if USE_STD_FROM_CHARS_PARSING

template <typename Type>
Type scanType(std::string_view token)
{
        DEBUG_ASSERTCRASH(!token.empty(), ("token is not expected to be empty"));

        // Unlike sscanf, std::from_chars cannot parse "+".
        // Consume the plus symbol to accommodate custom ini files that have numbers prefixed with a plus.
        if (!token.empty() && token[0] == '+')
        {
                token.remove_prefix(1);
        }

        if constexpr (std::is_floating_point_v<Type>)
        {
                // GeneralsX @bugfix BenderAI 07/04/2026 Apple SDKs in our deployment target do not expose std::from_chars for floats.
                #if defined(__APPLE__)
                const std::string tokenString(token);
                char *end = nullptr;
                const double result = std::strtod(tokenString.c_str(), &end);

                if (end == tokenString.c_str())
                {
                        throw INI_INVALID_DATA;
                }

                return static_cast<Type>(result);
                #else
                Type result{};
                const auto [ptr, ec] = std::from_chars(token.data(), token.data() + token.size(), result);

                if (ec != std::errc{})
                {
                        throw INI_INVALID_DATA;
                }

                return result;
                #endif
        }

        // TheSuperHackers @info std::from_chars cannot parse "-1" as uint32 so the result needs to be int64 for integers.
	std::conditional_t<std::is_integral_v<Type>, Int64, Type> result{};
	const auto [ptr, ec] = std::from_chars(token.data(), token.data() + token.size(), result);

	if (ec != std::errc{})
	{
		throw INI_INVALID_DATA;
	}

	return static_cast<Type>(result);
}

#endif

//-------------------------------------------------------------------------------------------------
/*static*/ Int INI::scanInt(const char* token)
{
#if USE_STD_FROM_CHARS_PARSING == 1
	return scanType<Int>(token);
#else
	Int value;
	if (sscanf( token, "%d", &value ) != 1)
		throw INI_INVALID_DATA;

#if USE_STD_FROM_CHARS_PARSING == -1
	if (value != scanType<Int>(token))
		throw INI_INVALID_DATA;
#endif

	return value;
#endif
}

//-------------------------------------------------------------------------------------------------
/*static*/ UnsignedInt INI::scanUnsignedInt(const char* token)
{
#if USE_STD_FROM_CHARS_PARSING == 1
	return scanType<UnsignedInt>(token);
#else
	UnsignedInt value;
	if (sscanf( token, "%u", &value ) != 1)	// unsigned int is %u, not %d
		throw INI_INVALID_DATA;

#if USE_STD_FROM_CHARS_PARSING == -1
	if (value != scanType<UnsignedInt>(token))
		throw INI_INVALID_DATA;
#endif

	return value;
#endif
}

//-------------------------------------------------------------------------------------------------
/*static*/ Real INI::scanReal(const char* token)
{
#if USE_STD_FROM_CHARS_PARSING == 1
	return scanType<Real>(token);
#else
	Real value;
	if (sscanf( token, "%f", &value ) != 1)
		throw INI_INVALID_DATA;

#if USE_STD_FROM_CHARS_PARSING == -1
	if (value != scanType<Real>(token))
		throw INI_INVALID_DATA;
#endif

	return value;
#endif
}

//-------------------------------------------------------------------------------------------------
/*static*/ Real INI::scanPercentToReal(const char* token)
{
	return scanReal(token) / 100.0f;
}

//-------------------------------------------------------------------------------------------------
/*static*/ Int INI::scanIndexList(const char* token, ConstCharPtrArray nameList)
{
	if( nameList == nullptr || nameList[ 0 ] == nullptr )
	{

		DEBUG_CRASH( ("INTERNAL ERROR! scanIndexList, invalid name list") );
		throw INI_INVALID_NAME_LIST;

	}

	// search for matching name
	Int count = 0;
	for(ConstCharPtrArray name = nameList; *name; name++, count++ )
	{
		if( stricmp( *name, token ) == 0 )
		{
			return count;
		}
	}

	DEBUG_CRASH(("token %s is not a valid member of the index list",token));
	throw INI_INVALID_DATA;
	return 0;	// never executed, but keeps compiler happy

}
//-------------------------------------------------------------------------------------------------
/*static*/ Int INI::scanLookupList(const char* token, ConstLookupListRecArray lookupList)
{
	if( lookupList == nullptr || lookupList[ 0 ].name == nullptr )
	{
		DEBUG_CRASH( ("INTERNAL ERROR! scanLookupList, invalid name list") );
		throw INI_INVALID_NAME_LIST;
	}

	// search for matching name
	for( const LookupListRec* lookup = &lookupList[0]; lookup->name; lookup++ )
	{
		if( stricmp( lookup->name, token ) == 0 )
		{
			return lookup->value;
		}
	}

	DEBUG_CRASH(("token %s is not a valid member of the lookup list",token));
	throw INI_INVALID_DATA;
	return 0;	// never executed, but keeps compiler happy

}

//-------------------------------------------------------------------------------------------------
const char* INI::getNextSubToken(const char* expected)
{
	const char* token = getNextToken(getSepsColon());
	if (stricmp(token, expected) != 0)
		throw INI_INVALID_DATA;
	return getNextToken(getSepsColon());
}

//-------------------------------------------------------------------------------------------------
/**
 * Parse a "random variable".
 * The format is "FIELD = low high [distribution]".
 */
void INI::parseGameClientRandomVariable( INI* ini, void * /*instance*/, void *store, const void* /*userData*/ )
{
	GameClientRandomVariable *var = static_cast<GameClientRandomVariable *>(store);

	const char* token;

	token = ini->getNextToken();
	Real low = INI::scanReal(token);

	token = ini->getNextToken();
	Real high = INI::scanReal(token);

	// if omitted, assume uniform
	GameClientRandomVariable::DistributionType type = GameClientRandomVariable::UNIFORM;
	token = ini->getNextTokenOrNull();
	if (token)
		type = (GameClientRandomVariable::DistributionType)INI::scanIndexList(token, GameClientRandomVariable::DistributionTypeNames);

	// set the range of the random variable
	var->setRange( low, high, type );
}

//-------------------------------------------------------------------------------------------------
// parse a duration in msec and convert to duration in frames
void INI::parseDurationReal( INI *ini, void * /*instance*/, void *store, const void* /*userData*/ )
{
	Real val = scanReal(ini->getNextToken());
	*(Real *)store = ConvertDurationFromMsecsToFrames(val);
}

//-------------------------------------------------------------------------------------------------
// parse a duration in msec and convert to duration in integral number of frames, (unsignedint) rounding UP
void INI::parseDurationUnsignedInt( INI *ini, void * /*instance*/, void *store, const void* /*userData*/ )
{
	UnsignedInt val = scanUnsignedInt(ini->getNextToken());
	*(UnsignedInt *)store = (UnsignedInt)WWMath::Ceil(ConvertDurationFromMsecsToFrames((Real)val));
}

// ------------------------------------------------------------------------------------------------
// parse a duration in msec and convert to duration in integral number of frames, (unsignedshort) rounding UP
void INI::parseDurationUnsignedShort( INI *ini, void * /*instance*/, void *store, const void* /*userData*/ )
{
	UnsignedInt val = scanUnsignedInt(ini->getNextToken());
	*(UnsignedShort *)store = (UnsignedShort)WWMath::Ceil(ConvertDurationFromMsecsToFrames((Real)val));
}

//-------------------------------------------------------------------------------------------------
// parse acceleration in (dist/sec) and convert to (dist/frame)
void INI::parseVelocityReal( INI *ini, void * /*instance*/, void *store, const void* /*userData*/ )
{
	const char *token = ini->getNextToken();
	Real val = scanReal(token);
	*(Real *)store = ConvertVelocityInSecsToFrames(val);
}

//-------------------------------------------------------------------------------------------------
// parse acceleration in (dist/sec^2) and convert to (dist/frame^2)
void INI::parseAccelerationReal( INI *ini, void * /*instance*/, void *store, const void* /*userData*/ )
{
	const char *token = ini->getNextToken();
	Real val = scanReal(token);
	*(Real *)store = ConvertAccelerationInSecsToFrames(val);
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void INI::parseVeterancyLevelFlags(INI* ini, void* /*instance*/, void* store, const void* /*userData*/)
{
	VeterancyLevelFlags flags = VETERANCY_LEVEL_FLAGS_ALL;
	for (const char* token = ini->getNextToken(); token; token = ini->getNextTokenOrNull())
	{
		if (stricmp(token, "ALL") == 0)
		{
			flags = VETERANCY_LEVEL_FLAGS_ALL;
			continue;
		}
		else if (stricmp(token, "NONE") == 0)
		{
			flags = VETERANCY_LEVEL_FLAGS_NONE;
			continue;
		}
		else if (token[0] == '+')
		{
			VeterancyLevel dt = (VeterancyLevel)INI::scanIndexList(token+1, TheVeterancyNames);
			flags = setVeterancyLevelFlag(flags, dt);
			continue;
		}
		else if (token[0] == '-')
		{
			VeterancyLevel dt = (VeterancyLevel)INI::scanIndexList(token+1, TheVeterancyNames);
			flags = clearVeterancyLevelFlag(flags, dt);
			continue;
		}
		else
		{
			throw INI_UNKNOWN_TOKEN;
		}
	}
	*(VeterancyLevelFlags*)store = flags;
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void INI::parseSoundsList( INI* ini, void *instance, void *store, const void* /*userData*/ )
{
	std::vector<AsciiString> *vec = (std::vector<AsciiString>*) store;
	vec->clear();

	constexpr const char* Seps = " \t,=";
	for (const char* token = ini->getNextTokenOrNull(Seps); token; token = ini->getNextTokenOrNull(Seps))
	{
		vec->push_back(token);
	}
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void INI::parseDamageTypeFlags(INI* ini, void* /*instance*/, void* store, const void* /*userData*/)
{
	DamageTypeFlags flags = DAMAGE_TYPE_FLAGS_ALL;

	for (const char* token = ini->getNextToken(); token; token = ini->getNextTokenOrNull())
	{
		if (stricmp(token, "ALL") == 0)
		{
			flags = DAMAGE_TYPE_FLAGS_ALL;
			continue;
		}
		if (stricmp(token, "NONE") == 0)
		{
			flags = DAMAGE_TYPE_FLAGS_NONE;
			continue;
		}
		if (token[0] == '+')
		{
			DamageType dt = (DamageType)DamageTypeFlags::getSingleBitFromName(token+1);
			flags = setDamageTypeFlag(flags, dt);
			continue;
		}
		if (token[0] == '-')
		{
			DamageType dt = (DamageType)DamageTypeFlags::getSingleBitFromName(token+1);
			flags = clearDamageTypeFlag(flags, dt);
			continue;
		}
		throw INI_UNKNOWN_TOKEN;
	}
	*(DamageTypeFlags*)store = flags;
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void INI::parseDeathTypeFlags(INI* ini, void* /*instance*/, void* store, const void* /*userData*/)
{
	DeathTypeFlags flags = DEATH_TYPE_FLAGS_ALL;
	for (const char* token = ini->getNextToken(); token; token = ini->getNextTokenOrNull())
	{
		if (stricmp(token, "ALL") == 0)
		{
			flags = DEATH_TYPE_FLAGS_ALL;
			continue;
		}
		if (stricmp(token, "NONE") == 0)
		{
			flags = DEATH_TYPE_FLAGS_NONE;
			continue;
		}
		if (token[0] == '+')
		{
			DeathType dt = (DeathType)INI::scanIndexList(token+1, TheDeathNames);
			flags = setDeathTypeFlag(flags, dt);
			continue;
		}
		if (token[0] == '-')
		{
			DeathType dt = (DeathType)INI::scanIndexList(token+1, TheDeathNames);
			flags = clearDeathTypeFlag(flags, dt);
			continue;
		}
		throw INI_UNKNOWN_TOKEN;
	}
	*(DeathTypeFlags*)store = flags;
}

//-------------------------------------------------------------------------------------------------
// parse the line and return whether the given line is a Block declaration of the form
// [whitespace] blockType [whitespace] blockName [EOL]
// both blockType and blockName are case insensitive
Bool INI::isDeclarationOfType( AsciiString blockType, AsciiString blockName, char *bufferToCheck )
{
	if (!bufferToCheck || blockType.isEmpty() || blockName.isEmpty())
		return false;

	const char* tempBuff = bufferToCheck;

	while (isspace(*tempBuff))
		++tempBuff;

	const int blockTypeLength = blockType.getLength();
	if (strnicmp(tempBuff, blockType.str(), blockTypeLength) != 0)
		return false;

	tempBuff += blockTypeLength;

	if (!isspace(*tempBuff++))
		return false;

	while (isspace(*tempBuff))
		++tempBuff;

	const int blockNameLength = blockName.getLength();
	if (strnicmp(tempBuff, blockName.str(), blockNameLength) != 0)
		return false;

	tempBuff += blockNameLength;

	while (isspace(*tempBuff))
		++tempBuff;

	if (*tempBuff != '\0')
		return false;

	return true;
}

//-------------------------------------------------------------------------------------------------
// parse the line and return whether the given line is a Block declaration of the form
// [whitespace] end [EOL]
Bool INI::isEndOfBlock( char *bufferToCheck )
{
	Bool retVal = true;
	if (!bufferToCheck) {
		return false;
	}

	// DO NOT RETURN EARLY FROM THIS FUNCTION (beyond this point)
	// we have to restore the bufferToCheck to its previous state before returning, so
	// it is important to get through all the checks.

	static const char* endString = "End";
	int endStringLength = strlen(endString);
	char restoreChar;
	char *tempBuff = bufferToCheck;


	while (isspace(*tempBuff)) {
		++tempBuff;
	}

	if (strlen(tempBuff) > endStringLength) {
		restoreChar = tempBuff[endStringLength];
		tempBuff[endStringLength] = 0;

		if (stricmp(endString, tempBuff) != 0) {
			retVal = false;
		}

		tempBuff[endStringLength] = restoreChar;
		tempBuff = tempBuff + endStringLength;
	} else {
		retVal = false;
	}

	while (*tempBuff && retVal) {
		retVal = isspace(*tempBuff);
		++tempBuff;
	}

	return retVal;
}
