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

// FILE: Keyboard.cpp /////////////////////////////////////////////////////////////////////////////
// Created:    Colin Day, June 2001
// Desc:       Basic keyboard
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/Language.h"
#include "Common/GameEngine.h"
#include "Common/MessageStream.h"
#include "GameClient/Keyboard.h"
#include "GameClient/KeyDefs.h"


// PUBLIC DATA ////////////////////////////////////////////////////////////////////////////////////
Keyboard *TheKeyboard = nullptr;

///////////////////////////////////////////////////////////////////////////////////////////////////
// PRIVATE PROTOTYPES /////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

//-------------------------------------------------------------------------------------------------
/** Given the state of the device, create messages from the input and
	* place them on the message stream */
//-------------------------------------------------------------------------------------------------
void Keyboard::createStreamMessages()
{

	// sanity
	if( TheMessageStream == nullptr )
		return;

	KeyboardIO *key = getFirstKey();
	GameMessage *msg = nullptr;
	while( key->key != KEY_NONE )
	{

		// add message to stream
		if( BitIsSet( key->state, KEY_STATE_DOWN ) )
		{

			msg = TheMessageStream->appendMessage( GameMessage::MSG_RAW_KEY_DOWN );
			DEBUG_ASSERTCRASH( msg, ("Unable to append key down message to stream") );

		}
		else if( BitIsSet( key->state, KEY_STATE_UP ) )
		{

			msg = TheMessageStream->appendMessage( GameMessage::MSG_RAW_KEY_UP );
			DEBUG_ASSERTCRASH( msg, ("Unable to append key up message to stream") );

		}
		else
		{

			DEBUG_CRASH(( "Unknown key state when creating msg stream" ));

		}

		// fill out message arguments
		if( msg )
		{
			msg->appendIntegerArgument( key->key );
			msg->appendIntegerArgument( key->state );
		}

		// next key please
		key->setUsed();
		key++;

	}

}

//-------------------------------------------------------------------------------------------------
/** update all our key state data */
//-------------------------------------------------------------------------------------------------
void Keyboard::updateKeys()
{
	Int index = 0;

	// get latest keys
	do
	{
		do
		{

			// get a single key
			getKey( &m_keys[ index ] );

			//
			// if we get back that our device was lost, we haven't been paying
			// attention to the keyboard, lets start over now, we only need to
			// do this reset once in this loop though just to be efficient
			//
			if( m_keys[ index ].key == KEY_LOST)
			{

				resetKeys();
				index = 0;

			}

		}
		while( m_keys[ index ].key == KEY_LOST );
	}
	while( m_keys[ index++ ].key != KEY_NONE );

	// update keyboard status array
	index = 0;
	while( m_keys[ index ].key != KEY_NONE )
	{

		/** @todo -- if we don't have focus, we could destroy all the keys retrieved
		here so that we don't process anything */

		m_keyStatus[ m_keys[ index ].key ].state = m_keys[ index ].state;
		m_keyStatus[ m_keys[ index ].key ].status = m_keys[ index ].status;

		// Update key down time for new key presses
		if( BitIsSet( m_keys[ index ].state, KEY_STATE_DOWN ) )
		{
			m_keyStatus[ m_keys[ index ].key ].keyDownTimeMsec = m_keys[ index ].keyDownTimeMsec;
		}

		// prevent ALT-TAB from causing a TAB event
		if( m_keys[ index ].key == KEY_TAB )
		{
			if( BitIsSet( m_keyStatus[ KEY_LALT ].state, KEY_STATE_DOWN ) ||
					BitIsSet( m_keyStatus[ KEY_RALT ].state, KEY_STATE_DOWN ) )
			{
				m_keys[index].status = KeyboardIO::STATUS_USED;
			}
		}
		else if( m_keys[ index ].key == KEY_CAPS	 ||
						 m_keys[ index ].key == KEY_LCTRL  ||
						 m_keys[ index ].key == KEY_RCTRL	 ||
						 m_keys[ index ].key == KEY_LSHIFT ||
						 m_keys[ index ].key == KEY_RSHIFT ||
						 m_keys[ index ].key == KEY_LALT	 ||
						 m_keys[ index ].key == KEY_RALT )

		{

			//
			// this keeps our internal key state accurate event though we don't
			// use the returned translation ... kinda weird I think
			//
			translateKey( m_keys[ index ].key );

		}

		index++;

	}

	// check for key repeats
	checkKeyRepeat();

	if( m_modifiers )
	{
		index = 0;
		while( m_keys[ index ].key != KEY_NONE )
		{

			// set in the modifier data into the already existing up/down state
			BitSet( m_keys[ index ].state, m_modifiers );

			// next key
			index++;

		}

	}

}

//-------------------------------------------------------------------------------------------------
/** check key repeat timing, TRUE is returned if repeat is occurring */
//-------------------------------------------------------------------------------------------------
Bool Keyboard::checkKeyRepeat()
{
	Bool retVal = FALSE;
	Int index = 0;
	Int key;

	/** @todo we shouldn't think about repeating any keys while we
	don't have the focus */
//	if( currentFocus == FOCUS_OUT )
//		return FALSE;

	// Find end of real keys for this frame
	while( m_keys[ index ].key != KEY_NONE )
		index++;

	// Scan Keyboard status array for first key down
	// long enough to repeat
	for( key = 0; key < ARRAY_SIZE(m_keyStatus); key++ )
	{

		if( BitIsSet( m_keyStatus[ key ].state, KEY_STATE_DOWN ) )
		{

			const UnsignedInt now = timeGetTime();
			const UnsignedInt keyDownTime = m_keyStatus[ key ].keyDownTimeMsec;
			const UnsignedInt elapsedMsec = now - keyDownTime;

			if( elapsedMsec > Keyboard::KEY_REPEAT_DELAY_MSEC )
			{
				// Add key to this frame
				m_keys[ index ].key = (UnsignedByte)key;
				m_keys[ index ].state = KEY_STATE_DOWN | KEY_STATE_AUTOREPEAT;  // note: not a bitset; this is an assignment
				m_keys[ index ].status = KeyboardIO::STATUS_UNUSED;

				// Set End Flag
				m_keys[ ++index ].key = KEY_NONE;

				// Set all keys as new to prevent multiple keys repeating
				for( index = 0; index< NUM_KEYS; index++ )
					m_keyStatus[ index ].keyDownTimeMsec = now;

				// Set repeated key so it will repeat again after the interval
				m_keyStatus[ key ].keyDownTimeMsec = now - (Keyboard::KEY_REPEAT_DELAY_MSEC + Keyboard::KEY_REPEAT_INTERVAL_MSEC);

				retVal = TRUE;
				break;  // exit for key

			}

		}

	}

	return retVal;

}

//-------------------------------------------------------------------------------------------------
/** Initialize the keyboard key-names array */
//-------------------------------------------------------------------------------------------------
void Keyboard::initKeyNames()
{
	Int i;

	#define _set_keyname_(k,s,s2,z) (m_keyNames[z].stdKey = k, m_keyNames[z].shifted = s, m_keyNames[z].shifted2 = s2)

	/*
	 * Initialize the keyboard key-names array.
	 */
	for( i = 0; i < ARRAY_SIZE(m_keyNames); i++ )
	{

		m_keyNames[ i ].stdKey =		L'\0';
		m_keyNames[ i ].shifted =		L'\0';
		m_keyNames[ i ].shifted2 =	L'\0';

	}

	m_shift2Key = KEY_NONE;

	// generic to all languages
	_set_keyname_(L'\0',	L'\0',	L'\0',	KEY_UP );
	_set_keyname_(L'\0',	L'\0',	L'\0',	KEY_DOWN );
	_set_keyname_(L'\0',	L'\0',	L'\0',	KEY_LEFT );
	_set_keyname_(L'\0',	L'\0',	L'\0',	KEY_RIGHT );

	_set_keyname_(L'\0',	L'\0',	L'\0',	KEY_HOME );
	_set_keyname_(L'\0',	L'\0',	L'\0',	KEY_END );
	_set_keyname_(L'\0',	L'\0',	L'\0',	KEY_PGUP );
	_set_keyname_(L'\0',	L'\0',	L'\0',	KEY_PGDN );
	_set_keyname_(L'\0',	L'\0',	L'\0',	KEY_INS );
	_set_keyname_(L'\b',	L'\b',	L'\0',	KEY_DEL );

	_set_keyname_(L'\b',	L'\b',	L'\0',	KEY_BACKSPACE  );
	_set_keyname_(L'\0',	L'\0',	L'\0',	KEY_ESC  );
	_set_keyname_(L'\t',	L'\t',	L'\0',	KEY_TAB  );
	_set_keyname_(L'\0',	L'\0',	L'\0',	KEY_CAPS  );
	_set_keyname_(L'\n',	L'\n',	L'\0',	KEY_ENTER  );

	_set_keyname_(L'\0',	L'\0',	L'\0',	KEY_RALT );
	_set_keyname_(L'\0',	L'\0',	L'\0',	KEY_RCTRL );
	_set_keyname_(L'\0',	L'\0',	L'\0',	KEY_RSHIFT  );

	_set_keyname_(L'\0',	L'\0',	L'\0',	KEY_LALT  );
	_set_keyname_(L'\0',	L'\0',	L'\0',	KEY_LCTRL  );
	_set_keyname_(L'\0',	L'\0',	L'\0',	KEY_LSHIFT  );

	_set_keyname_(L'\0',	L'\0',	L'\0',	KEY_NUM );
	_set_keyname_(L'\0',	L'\0',	L'\0',	KEY_SCROLL );
	_set_keyname_(L'\0',	L'\0',	L'\0',	KEY_SYSREQ );

	_set_keyname_(L'\0',	L'\0',	L'\0',	KEY_F1  );
	_set_keyname_(L'\0',	L'\0',	L'\0',	KEY_F2  );
	_set_keyname_(L'\0',	L'\0',	L'\0',	KEY_F3  );
	_set_keyname_(L'\0',	L'\0',	L'\0',	KEY_F4  );
	_set_keyname_(L'\0',	L'\0',	L'\0',	KEY_F5  );
	_set_keyname_(L'\0',	L'\0',	L'\0',	KEY_F6  );
	_set_keyname_(L'\0',	L'\0',	L'\0',	KEY_F7  );
	_set_keyname_(L'\0',	L'\0',	L'\0',	KEY_F8  );
	_set_keyname_(L'\0',	L'\0',	L'\0',	KEY_F9  );
	_set_keyname_(L'\0',	L'\0',	L'\0',	KEY_F10 );
	_set_keyname_(L'\0',	L'\0',	L'\0',	KEY_F11 );
	_set_keyname_(L'\0',	L'\0',	L'\0',	KEY_F12 );

	_set_keyname_(L'1',		L'1',		L'\0',	KEY_KP1 );
	_set_keyname_(L'2',		L'2',		L'\0',	KEY_KP2 );
	_set_keyname_(L'3',		L'3',		L'\0',	KEY_KP3 );
	_set_keyname_(L'4',		L'4',		L'\0',	KEY_KP4 );
	_set_keyname_(L'5',		L'5',		L'\0',	KEY_KP5 );
	_set_keyname_(L'6',		L'6',		L'\0',	KEY_KP6 );
	_set_keyname_(L'7',		L'7',		L'\0',	KEY_KP7 );
	_set_keyname_(L'8',		L'8',		L'\0',	KEY_KP8 );
	_set_keyname_(L'9',		L'9',		L'\0',	KEY_KP9 );
	_set_keyname_(L'0',		L'0',		L'\0',	KEY_KP0 );

	_set_keyname_(L' ',		L' ',		L'\0',	KEY_SPACE  );

	HKL kLayout = GetKeyboardLayout(0);

	Int low = (UnsignedInt)kLayout & 0xFFFF;
	LanguageID currentLanguage = OurLanguage;
	if(low == 0x040c
		 || low == 0x080c
		 || low == 0x0c0c
		 || low == 0x100c
		 || low == 0x140c)
		currentLanguage = LANGUAGE_ID_FRENCH;

	switch( currentLanguage )
	{
		case LANGUAGE_ID_US:
		case LANGUAGE_ID_JAPANESE:
		case LANGUAGE_ID_KOREAN:
		case LANGUAGE_ID_JABBER:
		case LANGUAGE_ID_UNKNOWN:
		case LANGUAGE_ID_SPANISH:		// not localized
			_set_keyname_(L'-',				L'-',				L'\0',	KEY_KPMINUS );
			_set_keyname_(L'+',				L'+',				L'\0',	KEY_KPPLUS );
			_set_keyname_(L'\n',			L'\n',			L'\0',	KEY_KPENTER );
			_set_keyname_(L'/',				L'/',				L'\0',	KEY_KPSLASH );
			_set_keyname_(L'.',				L'.',				L'\0',	KEY_KPDEL );
			_set_keyname_(L'*',				L'*',				L'\0',	KEY_KPSTAR );

			_set_keyname_(L'a',				L'A',				L'\0',	KEY_A  );
			_set_keyname_(L'b',				L'B',				L'\0',	KEY_B  );
			_set_keyname_(L'c',				L'C',				L'\0',	KEY_C  );
			_set_keyname_(L'd',				L'D',				L'\0',	KEY_D  );
			_set_keyname_(L'e',				L'E',				L'\0',	KEY_E  );
			_set_keyname_(L'f',				L'F',				L'\0',	KEY_F  );
			_set_keyname_(L'g',				L'G',				L'\0',	KEY_G  );
			_set_keyname_(L'h',				L'H',				L'\0',	KEY_H  );
			_set_keyname_(L'i',				L'I',				L'\0',	KEY_I  );
			_set_keyname_(L'j',				L'J',				L'\0',	KEY_J  );
			_set_keyname_(L'k',				L'K',				L'\0',	KEY_K  );
			_set_keyname_(L'l',				L'L',				L'\0',	KEY_L  );
			_set_keyname_(L'm',				L'M',				L'\0',	KEY_M  );
			_set_keyname_(L'n',				L'N',				L'\0',	KEY_N  );
			_set_keyname_(L'o',				L'O',				L'\0',	KEY_O  );
			_set_keyname_(L'p',				L'P',				L'\0',	KEY_P  );
			_set_keyname_(L'q',				L'Q',				L'\0',	KEY_Q  );
			_set_keyname_(L'r',				L'R',				L'\0',	KEY_R  );
			_set_keyname_(L's',				L'S',				L'\0',	KEY_S  );
			_set_keyname_(L't',				L'T',				L'\0',	KEY_T  );
			_set_keyname_(L'u',				L'U',				L'\0',	KEY_U  );
			_set_keyname_(L'v',				L'V',				L'\0',	KEY_V  );
			_set_keyname_(L'w',				L'W',				L'\0',	KEY_W  );
			_set_keyname_(L'x',				L'X',				L'\0',	KEY_X  );
			_set_keyname_(L'y',				L'Y',				L'\0',	KEY_Y  );
			_set_keyname_(L'z',				L'Z',				L'\0',	KEY_Z  );

			_set_keyname_(L'1',				L'!',				L'\0',	KEY_1  );
			_set_keyname_(L'2',				L'@',				L'\0',	KEY_2  );
			_set_keyname_(L'3',				L'#',				L'\0',	KEY_3  );
			_set_keyname_(L'4',				L'$',				L'\0',	KEY_4  );
			_set_keyname_(L'5',				L'%',				L'\0',	KEY_5  );
			_set_keyname_(L'6',				L'^',				L'\0',	KEY_6  );
			_set_keyname_(L'7',				L'&',				L'\0',	KEY_7  );
			_set_keyname_(L'8',				L'*',				L'\0',	KEY_8  );
			_set_keyname_(L'9',				L'(',				L'\0',	KEY_9  );
			_set_keyname_(L'0',				L')',				L'\0',	KEY_0  );

			_set_keyname_(L',',				L'<',				L'\0',	KEY_COMMA  );
			_set_keyname_(L'.',				L'>',				L'\0',	KEY_PERIOD  );
			_set_keyname_(L'/',				L'?',				L'\0',	KEY_SLASH  );

			_set_keyname_(L'[',				L'{',				L'\0',	KEY_LBRACKET  );
			_set_keyname_(L']',				L'}',				L'\0',	KEY_RBRACKET  );

			_set_keyname_(L';',				L':',				L'\0',	KEY_SEMICOLON  );
			_set_keyname_(L'\'',			L'\"',			L'\0',	KEY_APOSTROPHE  );
			_set_keyname_(L'`',				L'~',				L'\0',	KEY_TICK  );
			_set_keyname_(L'\\',			L'|',				L'\0',	KEY_BACKSLASH  );

			_set_keyname_(L'-',				L'_',				L'\0',	KEY_MINUS  );
			_set_keyname_(L'=',				L'+',				L'\0',	KEY_EQUAL  );

			break;

		case LANGUAGE_ID_UK:
			_set_keyname_(L'-',				L'-',				L'\0',	KEY_KPMINUS );
			_set_keyname_(L'+',				L'+',				L'\0',	KEY_KPPLUS );
			_set_keyname_(L'\n',			L'\n',			L'\0',	KEY_KPENTER );
			_set_keyname_(L'/',				L'/',				L'\0',	KEY_KPSLASH );
			_set_keyname_(L'.',				L'.',				L'\0',	KEY_KPDEL );
			_set_keyname_(L'*',				L'*',				L'\0',	KEY_KPSTAR );

			_set_keyname_(L'a',				L'A',				L'\0',	KEY_A  );
			_set_keyname_(L'b',				L'B',				L'\0',	KEY_B  );
			_set_keyname_(L'c',				L'C',				L'\0',	KEY_C  );
			_set_keyname_(L'd',				L'D',				L'\0',	KEY_D  );
			_set_keyname_(L'e',				L'E',				L'\0',	KEY_E  );
			_set_keyname_(L'f',				L'F',				L'\0',	KEY_F  );
			_set_keyname_(L'g',				L'G',				L'\0',	KEY_G  );
			_set_keyname_(L'h',				L'H',				L'\0',	KEY_H  );
			_set_keyname_(L'i',				L'I',				L'\0',	KEY_I  );
			_set_keyname_(L'j',				L'J',				L'\0',	KEY_J  );
			_set_keyname_(L'k',				L'K',				L'\0',	KEY_K  );
			_set_keyname_(L'l',				L'L',				L'\0',	KEY_L  );
			_set_keyname_(L'm',				L'M',				L'\0',	KEY_M  );
			_set_keyname_(L'n',				L'N',				L'\0',	KEY_N  );
			_set_keyname_(L'o',				L'O',				L'\0',	KEY_O  );
			_set_keyname_(L'p',				L'P',				L'\0',	KEY_P  );
			_set_keyname_(L'q',				L'Q',				L'\0',	KEY_Q  );
			_set_keyname_(L'r',				L'R',				L'\0',	KEY_R  );
			_set_keyname_(L's',				L'S',				L'\0',	KEY_S  );
			_set_keyname_(L't',				L'T',				L'\0',	KEY_T  );
			_set_keyname_(L'u',				L'U',				L'\0',	KEY_U  );
			_set_keyname_(L'v',				L'V',				L'\0',	KEY_V  );
			_set_keyname_(L'w',				L'W',				L'\0',	KEY_W  );
			_set_keyname_(L'x',				L'X',				L'\0',	KEY_X  );
			_set_keyname_(L'y',				L'Y',				L'\0',	KEY_Y  );
			_set_keyname_(L'z',				L'Z',				L'\0',	KEY_Z  );

			_set_keyname_(L'1',				L'!',				L'\0',	KEY_1  );
			_set_keyname_(L'2',				L'\"',			L'\0',	KEY_2  );
			_set_keyname_(L'3',				0x00A3,			L'\0',	KEY_3  );	//£ GBP SIGN
			_set_keyname_(L'4',				L'$',				L'\x20ac',		KEY_4  ); // € EUR SIGN
			_set_keyname_(L'5',				L'%',				L'\0',	KEY_5  );
			_set_keyname_(L'6',				L'^',				L'\0',	KEY_6  );
			_set_keyname_(L'7',				L'&',				L'\0',	KEY_7  );
			_set_keyname_(L'8',				L'*',				L'\0',	KEY_8  );
			_set_keyname_(L'9',				L'(',				L'\0',	KEY_9  );
			_set_keyname_(L'0',				L')',				L'\0',	KEY_0  );

			_set_keyname_(L',',				L'<',				L'\0',	KEY_COMMA  );
			_set_keyname_(L'.',				L'>',				L'\0',	KEY_PERIOD  );
			_set_keyname_(L'/',				L'?',				L'\0',	KEY_SLASH  );

			_set_keyname_(L'[',				L'{',				L'\0',	KEY_LBRACKET  );
			_set_keyname_(L']',				L'}',				L'\0',	KEY_RBRACKET  );

			_set_keyname_(L';',				L':',				L'\0',	KEY_SEMICOLON  );
			_set_keyname_(L'\'',			L'@',				L'\0',	KEY_APOSTROPHE  );
			_set_keyname_(L'`',				0x00AC,			0x00A6,	KEY_TICK  );	//¬¦ (not, broken bar)
			_set_keyname_(L'#',				L'~',				L'\0',	KEY_BACKSLASH  );

			_set_keyname_(L'-',				L'_',				L'\0',	KEY_MINUS  );
			_set_keyname_(L'=',				L'+',				L'\0',	KEY_EQUAL  );

			_set_keyname_(L'\\',			L'|',				L'\0',	KEY_102  );

			m_shift2Key = KEY_RALT;
			break;

		case LANGUAGE_ID_GERMAN:
			_set_keyname_(L'-',				L'-',				L'\0',	KEY_KPMINUS );
			_set_keyname_(L'+',				L'+',				L'\0',	KEY_KPPLUS );
			_set_keyname_(L'\n',			L'\n',			L'\0',	KEY_KPENTER );
			_set_keyname_(L'/',				L'/',				L'\0',	KEY_KPSLASH );
			_set_keyname_(L',',				L',',				L'\0',	KEY_KPDEL );
			_set_keyname_(L'*',				L'*',				L'\0',	KEY_KPSTAR );

			_set_keyname_(L'a',				L'A',				L'\0',	KEY_A  );
			_set_keyname_(L'b',				L'B',				L'\0',	KEY_B  );
			_set_keyname_(L'c',				L'C',				L'\0',	KEY_C  );
			_set_keyname_(L'd',				L'D',				L'\0',	KEY_D  );
			_set_keyname_(L'e',				L'E',				L'\0',	KEY_E  );
			_set_keyname_(L'f',				L'F',				L'\0',	KEY_F  );
			_set_keyname_(L'g',				L'G',				L'\0',	KEY_G  );
			_set_keyname_(L'h',				L'H',				L'\0',	KEY_H  );
			_set_keyname_(L'i',				L'I',				L'\0',	KEY_I  );
			_set_keyname_(L'j',				L'J',				L'\0',	KEY_J  );
			_set_keyname_(L'k',				L'K',				L'\0',	KEY_K  );
			_set_keyname_(L'l',				L'L',				L'\0',	KEY_L  );
			_set_keyname_(L'm',				L'M',				0x00B5,	KEY_M  );	//µ (micro)
			_set_keyname_(L'n',				L'N',				L'\0',	KEY_N  );
			_set_keyname_(L'o',				L'O',				L'\0',	KEY_O  );
			_set_keyname_(L'p',				L'P',				L'\0',	KEY_P  );
			_set_keyname_(L'q',				L'Q',				L'@',		KEY_Q  );
			_set_keyname_(L'r',				L'R',				L'\0',	KEY_R  );
			_set_keyname_(L's',				L'S',				L'\0',	KEY_S  );
			_set_keyname_(L't',				L'T',				L'\0',	KEY_T  );
			_set_keyname_(L'u',				L'U',				L'\0',	KEY_U  );
			_set_keyname_(L'v',				L'V',				L'\0',	KEY_V  );
			_set_keyname_(L'w',				L'W',				L'\0',	KEY_W  );
			_set_keyname_(L'x',				L'X',				L'\0',	KEY_X  );
			_set_keyname_(L'z',				L'Z',				L'\0',	KEY_Y  );
			_set_keyname_(L'y',				L'Y',				L'\0',	KEY_Z  );

			_set_keyname_(L'1',				L'!',				L'\0',	KEY_1  );
			_set_keyname_(L'2',				L'"',				0x00B2,	KEY_2  );	//² (superscript 2)
			_set_keyname_(L'3',				0x00A7,			0x00B3,	KEY_3  );	//§³ (section cubed)
			_set_keyname_(L'4',				L'$',				L'\0',	KEY_4  );
			_set_keyname_(L'5',				L'%',				L'\0',	KEY_5  );
			_set_keyname_(L'6',				L'&',				L'\0',	KEY_6  );
			_set_keyname_(L'7',				L'/',				L'{',		KEY_7  );
			_set_keyname_(L'8',				L'(',				L'[',		KEY_8  );
			_set_keyname_(L'9',				L')',				L']',		KEY_9  );
			_set_keyname_(L'0',				L'=',				L'}',		KEY_0  );

			_set_keyname_(L',',				L';',				L'\0',	KEY_COMMA  );
			_set_keyname_(L'.',				L':',				L'\0',	KEY_PERIOD  );
			_set_keyname_(L'-',				L'_',				L'\0',	KEY_SLASH  );

			_set_keyname_(0x00FC,			0x00DC,			L'\0',	KEY_LBRACKET  );		//üÜ (umlaut)
			_set_keyname_(L'+',				L'*',				L'~',		KEY_RBRACKET  );

			_set_keyname_(0x00F6,			0x00D6,			L'\0',	KEY_SEMICOLON  );		//öÖ (umlaut)
			_set_keyname_(0x00E4,			0x00C4,			L'\0',	KEY_APOSTROPHE  );	//äÄ (umlaut)
			_set_keyname_(L'^',				0x00B0,			L'\0',	KEY_TICK  );				//° (degree)
			_set_keyname_(L'#',				L'\'',			L'\0',	KEY_BACKSLASH  );

			_set_keyname_(0x00DF,			L'?',				L'\\',	KEY_MINUS  );				//ß (sharp s)
			_set_keyname_(0x00B4,			L'`',				L'\0',	KEY_EQUAL  );				//´ (acute)

			_set_keyname_(L'<',				L'>',				L'|',		KEY_102  );

			m_shift2Key = KEY_RALT;
			break;

		case LANGUAGE_ID_FRENCH:
			_set_keyname_(L'-',				L'-',				L'\0',	KEY_KPMINUS );
			_set_keyname_(L'+',				L'+',				L'\0',	KEY_KPPLUS );
			_set_keyname_(L'\n',			L'\n',			L'\0',	KEY_KPENTER );
			_set_keyname_(L'/',				L'/',				L'\0',	KEY_KPSLASH );
			_set_keyname_(L'.',				L'.',				L'\0',	KEY_KPDEL );
			_set_keyname_(L'*',				L'*',				L'\0',	KEY_KPSTAR );

			_set_keyname_(L'q',				L'Q',				L'\0',	KEY_A  );
			_set_keyname_(L'b',				L'B',				L'\0',	KEY_B  );
			_set_keyname_(L'c',				L'C',				L'\0',	KEY_C  );
			_set_keyname_(L'd',				L'D',				L'\0',	KEY_D  );
			_set_keyname_(L'e',				L'E',				L'\0',	KEY_E  );
			_set_keyname_(L'f',				L'F',				L'\0',	KEY_F  );
			_set_keyname_(L'g',				L'G',				L'\0',	KEY_G  );
			_set_keyname_(L'h',				L'H',				L'\0',	KEY_H  );
			_set_keyname_(L'i',				L'I',				L'\0',	KEY_I  );
			_set_keyname_(L'j',				L'J',				L'\0',	KEY_J  );
			_set_keyname_(L'k',				L'K',				L'\0',	KEY_K  );
			_set_keyname_(L'l',				L'L',				L'\0',	KEY_L  );
			_set_keyname_(L',',				L'?',				L'\0',	KEY_M  );
			_set_keyname_(L'n',				L'N',				L'\0',	KEY_N  );
			_set_keyname_(L'o',				L'O',				L'\0',	KEY_O  );
			_set_keyname_(L'p',				L'P',				L'\0',	KEY_P  );
			_set_keyname_(L'a',				L'A',				L'\0',	KEY_Q  );
			_set_keyname_(L'r',				L'R',				L'\0',	KEY_R  );
			_set_keyname_(L's',				L'S',				L'\0',	KEY_S  );
			_set_keyname_(L't',				L'T',				L'\0',	KEY_T  );
			_set_keyname_(L'u',				L'U',				L'\0',	KEY_U  );
			_set_keyname_(L'v',				L'V',				L'\0',	KEY_V  );
			_set_keyname_(L'z',				L'Z',				L'\0',	KEY_W  );
			_set_keyname_(L'x',				L'X',				L'\0',	KEY_X  );
			_set_keyname_(L'y',				L'Y',				L'\0',	KEY_Y  );
			_set_keyname_(L'w',				L'W',				L'\0',	KEY_Z  );

			_set_keyname_(L'&',				L'1',				L'\0',	KEY_1  );
			_set_keyname_(0x00E9,			L'2',				L'~',		KEY_2  );	//é (acute)
			_set_keyname_(L'"',				L'3',				L'#',		KEY_3  );
			_set_keyname_(L'\'',			L'4',				L'{',		KEY_4  );
			_set_keyname_(L'(',				L'5',				L'[',		KEY_5  );
			_set_keyname_(L'-',				L'6',				L'|',		KEY_6  );
			_set_keyname_(0x00E8,			L'7',				L'`',		KEY_7  );	//è (grave)
			_set_keyname_(L'_',				L'8',				L'\\',	KEY_8  );
			_set_keyname_(0x00E7,			L'9',				L'\0',	KEY_9  );	//ç (cedilla)
			_set_keyname_(0x00E0,			L'0',				L'@',		KEY_0  );	//à (grave)

			_set_keyname_(L';',				L'.',				L'\0',	KEY_COMMA  );
			_set_keyname_(L':',				L'/',				L'\0',	KEY_PERIOD  );
			_set_keyname_(L'!',				0x00A7,			L'\0',	KEY_SLASH  );				//§ (section)

			_set_keyname_(L'^',				0x00A8,			L'\0',	KEY_LBRACKET  );		//¨ (diaeresis)
			_set_keyname_(L'$',				0x00A3,			0x00A4,	KEY_RBRACKET  );		//£¤ (pound currency)

			_set_keyname_(L'm',				L'M',				L'\0',	KEY_SEMICOLON  );
			_set_keyname_(0x00F9,			L'%',				L'\0',	KEY_APOSTROPHE  );	//ù (grave)
			_set_keyname_(0x00B2,			L'\0',			L'\0',	KEY_TICK  );				//² (superscript 2)
			_set_keyname_(L'*',				0x00B5,			L'\0',	KEY_BACKSLASH  );		//µ (micro)

			_set_keyname_(L')',				0x00B0,			L']',		KEY_MINUS  );				//° (degree)
			_set_keyname_(L'=',				L'+',				L'}',		KEY_EQUAL  );

			_set_keyname_(L'<',				L'>',				L'\0',	KEY_102  );

			m_shift2Key = KEY_RALT;
			break;

		case LANGUAGE_ID_ITALIAN:
			_set_keyname_(L'-',				L'-',				L'\0',	KEY_KPMINUS );
			_set_keyname_(L'+',				L'+',				L'\0',	KEY_KPPLUS );
			_set_keyname_(L'\n',			L'\n',			L'\0',	KEY_KPENTER );
			_set_keyname_(L'/',				L'/',				L'\0',	KEY_KPSLASH );
			_set_keyname_(L'.',				L'.',				L'\0',	KEY_KPDEL );
			_set_keyname_(L'*',				L'*',				L'\0',	KEY_KPSTAR );

			_set_keyname_(L'a',				L'A',				L'\0',	KEY_A  );
			_set_keyname_(L'b',				L'B',				L'\0',	KEY_B  );
			_set_keyname_(L'c',				L'C',				L'\0',	KEY_C  );
			_set_keyname_(L'd',				L'D',				L'\0',	KEY_D  );
			_set_keyname_(L'e',				L'E',				L'\0',	KEY_E  );
			_set_keyname_(L'f',				L'F',				L'\0',	KEY_F  );
			_set_keyname_(L'g',				L'G',				L'\0',	KEY_G  );
			_set_keyname_(L'h',				L'H',				L'\0',	KEY_H  );
			_set_keyname_(L'i',				L'I',				L'\0',	KEY_I  );
			_set_keyname_(L'j',				L'J',				L'\0',	KEY_J  );
			_set_keyname_(L'k',				L'K',				L'\0',	KEY_K  );
			_set_keyname_(L'l',				L'L',				L'\0',	KEY_L  );
			_set_keyname_(L'm',				L'M',				L'\0',	KEY_M  );
			_set_keyname_(L'n',				L'N',				L'\0',	KEY_N  );
			_set_keyname_(L'o',				L'O',				L'\0',	KEY_O  );
			_set_keyname_(L'p',				L'P',				L'\0',	KEY_P  );
			_set_keyname_(L'q',				L'Q',				L'\0',	KEY_Q  );
			_set_keyname_(L'r',				L'R',				L'\0',	KEY_R  );
			_set_keyname_(L's',				L'S',				L'\0',	KEY_S  );
			_set_keyname_(L't',				L'T',				L'\0',	KEY_T  );
			_set_keyname_(L'u',				L'U',				L'\0',	KEY_U  );
			_set_keyname_(L'v',				L'V',				L'\0',	KEY_V  );
			_set_keyname_(L'w',				L'W',				L'\0',	KEY_W  );
			_set_keyname_(L'x',				L'X',				L'\0',	KEY_X  );
			_set_keyname_(L'y',				L'Y',				L'\0',	KEY_Y  );
			_set_keyname_(L'z',				L'Z',				L'\0',	KEY_Z  );

			_set_keyname_(L'1',				L'!',				L'\0',	KEY_1  );
			_set_keyname_(L'2',				L'"',				L'\0',	KEY_2  );
			_set_keyname_(L'3',				0x00A3,			L'\0',	KEY_3  );		//£ GBP SIGN
			_set_keyname_(L'4',				L'$',				L'\0',	KEY_4  );
			_set_keyname_(L'5',				L'%',				L'\0',	KEY_5  );
			_set_keyname_(L'6',				L'&',				L'\0',	KEY_6  );
			_set_keyname_(L'7',				L'/',				L'\0',	KEY_7  );
			_set_keyname_(L'8',				L'(',				L'\0',	KEY_8  );
			_set_keyname_(L'9',				L')',				L'\0',	KEY_9  );
			_set_keyname_(L'0',				L'=',				L'\0',	KEY_0  );

			_set_keyname_(L',',				L';',				L'\0',	KEY_COMMA  );
			_set_keyname_(L'.',				L':',				L'\0',	KEY_PERIOD  );
			_set_keyname_(L'-',				L'_',				L'\0',	KEY_SLASH  );

			_set_keyname_(0x00E8,			0x00E9,			L'[',		KEY_LBRACKET  );		//èé (grave acute)
			_set_keyname_(L'+',				L'*',				L']',		KEY_RBRACKET  );

			_set_keyname_(0x00F2,			0x00E7,			L'@',		KEY_SEMICOLON  );		//òç (grave cedilla)
			_set_keyname_(0x00E0,			0x00B0,			L'#',		KEY_APOSTROPHE  );	//à° (grave degree)
			_set_keyname_(L'\\',			L'|',				L'\0',	KEY_TICK  );
			_set_keyname_(0x00F9,			0x00A7,			L'\0',	KEY_BACKSLASH  );		//ù§ (grave section)

			_set_keyname_(L'\'',			L'?',				L'\0',	KEY_MINUS  );
			_set_keyname_(0x00EC,			L'^',				L'\0',	KEY_EQUAL  );				//ì (grave)

			_set_keyname_(L'<',				L'>',				L'\0',	KEY_102  );

			m_shift2Key = KEY_RALT;
			break;

	}

}

///////////////////////////////////////////////////////////////////////////////////////////////////
// PUBLIC PROTOTYPES //////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
Keyboard::Keyboard()
{

	memset( m_keys, 0, sizeof( m_keys ) );
	memset( m_keyStatus, 0, sizeof( m_keyStatus ) );
	m_modifiers = KEY_STATE_NONE;
	m_shift2Key = KEY_NONE;

	memset( m_keyNames, 0, sizeof( m_keyNames ) );

}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
Keyboard::~Keyboard()
{

}

//-------------------------------------------------------------------------------------------------
/** Initialize the keyboard */
//-------------------------------------------------------------------------------------------------
void Keyboard::init()
{

	// initialize the key names
	initKeyNames();

}

//-------------------------------------------------------------------------------------------------
/** Reset keyboard system */
//-------------------------------------------------------------------------------------------------
void Keyboard::reset()
{

}

//-------------------------------------------------------------------------------------------------
/** Called once per frame to gather key data input */
//-------------------------------------------------------------------------------------------------
void Keyboard::update()
{

	// update the key data
	updateKeys();

}

//-------------------------------------------------------------------------------------------------
/** Reset the state data for the keys, we likely want to do this when
	* we once again gain focus to our app from something like an alt tab */
//-------------------------------------------------------------------------------------------------
void Keyboard::resetKeys()
{

	// TheSuperHackers @fix Caball009 13/12/2025 Fix bug where game remains in waypoint mode
	// because the key up state for the alt key is not detected after alt tab.
	refreshAltKeys();

	memset( m_keys, 0, sizeof( m_keys ) );
	memset( m_keyStatus, 0, sizeof( m_keyStatus ) );
	m_modifiers = KEY_STATE_NONE;
	if( getCapsState() )
	{
		m_modifiers |= KEY_STATE_CAPSLOCK;
	}

}

//-------------------------------------------------------------------------------------------------
// Refresh the state of the alt keys, necessary after alt tab
//-------------------------------------------------------------------------------------------------
void Keyboard::refreshAltKeys() const
{
	// GeneralsX @bugfix 10/07/2026 Guard against focus events arriving before the message stream exists.
	if (TheMessageStream == nullptr)
		return;

	if (BitIsSet(m_keyStatus[KEY_LALT].state, KEY_STATE_DOWN))
	{
		GameMessage* msg = TheMessageStream->appendMessage(GameMessage::MSG_RAW_KEY_UP);
		msg->appendIntegerArgument(KEY_LALT);
		msg->appendIntegerArgument(KEY_STATE_UP);
	}
	if (BitIsSet(m_keyStatus[KEY_RALT].state, KEY_STATE_DOWN))
	{
		GameMessage* msg = TheMessageStream->appendMessage(GameMessage::MSG_RAW_KEY_UP);
		msg->appendIntegerArgument(KEY_RALT);
		msg->appendIntegerArgument(KEY_STATE_UP);
	}
}

//-------------------------------------------------------------------------------------------------
/** get the first key in our current state of the keyboard */
//-------------------------------------------------------------------------------------------------
KeyboardIO *Keyboard::getFirstKey()
{
	return &m_keys[ 0 ];
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
KeyboardIO *Keyboard::findKey( KeyDefType key, KeyboardIO::StatusType status )
{
	for (KeyboardIO *io = getFirstKey(); io->key != KEY_NONE; ++io)
	{
		if (io->key == key && io->status == status)
		{
			return io;
		}
	}
	return nullptr;
}

//-------------------------------------------------------------------------------------------------
/** return the key status for the specified key */
//-------------------------------------------------------------------------------------------------
UnsignedByte Keyboard::getKeyStatusData( KeyDefType key )
{
	return m_keyStatus[ key ].status;
}

//-------------------------------------------------------------------------------------------------
/** Get the key state data as a Bool for the specified key */
//-------------------------------------------------------------------------------------------------
Bool Keyboard::getKeyStateBit( KeyDefType key, Int bit )
{
	return (m_keyStatus[ key ].state & bit) ? 1 : 0;
}

//-------------------------------------------------------------------------------------------------
/** set the key status data */
//-------------------------------------------------------------------------------------------------
void Keyboard::setKeyStatusData( KeyDefType key, KeyboardIO::StatusType data )
{
	m_keyStatus[ key ].status = data;
}

//-------------------------------------------------------------------------------------------------
/** set the key state data */
//-------------------------------------------------------------------------------------------------
void Keyboard::setKeyStateData( KeyDefType key, UnsignedByte data )
{
	m_keyStatus[ key ].state = data;
}

//-------------------------------------------------------------------------------------------------
/** This routine must be called with every character to
	* properly monitor the shift state.  Takes a keycode as
	* input, and returns the UNICODE char representing the
	* character. */
//-------------------------------------------------------------------------------------------------
WideChar Keyboard::translateKey( WideChar keyCode )
{

	if( keyCode > 0x00FF )
		return keyCode;

	UnsignedByte ubKeyCode = (UnsignedByte)keyCode;
	switch( ubKeyCode )
	{

		case KEY_CAPS:
			if( getKeyStatusData( KEY_CAPS ) == KeyboardIO::STATUS_UNUSED )
			{
				if( getKeyStateBit( KEY_CAPS, KEY_STATE_DOWN ) )
				{
					if( m_modifiers & KEY_STATE_CAPSLOCK )
					{
						// Toggle caplocks off
						m_modifiers &= ~KEY_STATE_CAPSLOCK;
					}
					else
					{
						// Toggle caplocks on
						m_modifiers |= KEY_STATE_CAPSLOCK;
					}
				}

				setKeyStatusData( KEY_CAPS, KeyboardIO::STATUS_USED );
			}
			return 0;

		case KEY_LSHIFT:
			if( getKeyStateBit( KEY_LSHIFT, KEY_STATE_DOWN ) )
			{
				m_modifiers |= KEY_STATE_LSHIFT;
			}
			else
			{
				m_modifiers &= ~KEY_STATE_LSHIFT;
			}
			return 0;

		case KEY_RSHIFT:
			if( getKeyStateBit( KEY_RSHIFT, KEY_STATE_DOWN ) )
			{
				m_modifiers |= KEY_STATE_RSHIFT;
			}
			else
			{
				m_modifiers &= ~KEY_STATE_RSHIFT;
			}
			return 0;

		case KEY_LCTRL:
			if( getKeyStateBit( KEY_LCTRL, KEY_STATE_DOWN ) )
			{
				m_modifiers |= KEY_STATE_LCONTROL;
			}
			else
			{
				m_modifiers &= ~KEY_STATE_LCONTROL;
			}
			return 0;

		case KEY_RCTRL:
			if( getKeyStateBit( KEY_RCTRL, KEY_STATE_DOWN ) )
			{
				m_modifiers |= KEY_STATE_RCONTROL;
			}
			else
			{
				m_modifiers &= ~KEY_STATE_RCONTROL;
			}
			return 0;

		case KEY_LALT:
			if( getKeyStateBit( KEY_LALT, KEY_STATE_DOWN ) )
			{
				m_modifiers |= KEY_STATE_LALT;
			}
			else
			{
				m_modifiers &= ~KEY_STATE_LALT;
			}
			return 0;

		case KEY_RALT:
			if( getKeyStateBit( KEY_RALT, KEY_STATE_DOWN ) )
			{
				m_modifiers |= KEY_STATE_RALT;
			}
			else
			{
				m_modifiers &= ~KEY_STATE_RALT;
			}
			return 0;

		default:
			if( ubKeyCode == m_shift2Key )
			{
				if( getKeyStateBit( m_shift2Key, KEY_STATE_DOWN ) )
				{
					m_modifiers |= KEY_STATE_SHIFT2;
				}
				else
				{
					m_modifiers &= ~KEY_STATE_SHIFT2;
				}
				return 0;
			}

			if( m_modifiers & KEY_STATE_SHIFT2 )
			{
				return( m_keyNames[ ubKeyCode ].shifted2 );
			}

			if( isShift() || ( getCapsState() && iswalpha( m_keyNames[ ubKeyCode ].stdKey ) ) )
			{
				return( m_keyNames[ ubKeyCode ].shifted );
			}

			return( m_keyNames[ubKeyCode ].stdKey );
	}
}

//-------------------------------------------------------------------------------------------------
/** returns true if any shift state is pressed */
//-------------------------------------------------------------------------------------------------
Bool Keyboard::isShift()
{
		if( m_modifiers & KEY_STATE_LSHIFT || m_modifiers & KEY_STATE_RSHIFT || m_modifiers & KEY_STATE_SHIFT2 )
		{
			return TRUE;
		}
		return FALSE;
}

//-------------------------------------------------------------------------------------------------
/** returns true if any control state is pressed */
//-------------------------------------------------------------------------------------------------
Bool Keyboard::isCtrl()
{
		if( m_modifiers & KEY_STATE_LCONTROL || m_modifiers & KEY_STATE_RCONTROL )
		{
			return TRUE;
		}
		return FALSE;
}

//-------------------------------------------------------------------------------------------------
/** returns true if any shift state is pressed */
//-------------------------------------------------------------------------------------------------
Bool Keyboard::isAlt()
{
	if( m_modifiers & KEY_STATE_LALT || m_modifiers & KEY_STATE_RALT )
	{
		return TRUE;
	}
	return FALSE;
}


WideChar Keyboard::getPrintableKey( KeyDefType key, Int state )
{
	if((key < 0 || key >= ARRAY_SIZE(m_keyNames)) || ( state < 0 || state >= MAX_KEY_STATES))
		return L'\0';
	if(state == 0)
		return m_keyNames[key].stdKey;
	else if(state == 1)
		return m_keyNames[key].shifted;
	else
		return m_keyNames[key].shifted2;
}
