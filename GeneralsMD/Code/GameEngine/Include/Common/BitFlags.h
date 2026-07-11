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

// FILE: BitFlags.h /////////////////////////////////////////////////////////////////////////
// Author: Steven Johnson, March 2002
// Desc:
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

// INCLUDES ///////////////////////////////////////////////////////////////////////////////////////
#include "Common/STLTypedefs.h"

class INI;
class Xfer;
class AsciiString;

//-------------------------------------------------------------------------------------------------
/*
	BitFlags is a wrapper class that exists primarily because of a flaw in std::bitset<>.
	Although quite useful, it has horribly non-useful constructor, which (1) don't let
	us initialize stuff in useful ways, and (2) provide a default constructor that implicitly
	converts ints into bitsets in a "wrong" way (ie, it treats the int as a mask, not an index).
	So we wrap to correct this, but leave the bitset "exposed" so that we can use all the non-ctor
	functions on it directly (since it doesn't overload operator= to do the "wrong" thing, strangely enough)
*/
template <size_t NUMBITS>
class BitFlags
{
private:
	std::bitset<NUMBITS>				m_bits;

public:
	CPP_11(static constexpr size_t NumBits = NUMBITS);
	static const char* const s_bitNameList[];

	/*
		just a little syntactic sugar so that there is no "foo = 0" compatible constructor
	*/
	enum BogusInitType { kInit };
	enum InitSetAllType { kInitSetAll };

	BitFlags()
	{
	}

	// This constructor sets all bits to 1
	BitFlags(InitSetAllType)
	{
		m_bits.set();
	}

	BitFlags(UnsignedInt value)
		: m_bits(static_cast<unsigned long>(value))
	{
	}

	// TheSuperHackers @todo Replace with variadic template

	BitFlags(BogusInitType k, Int idx1)
	{
		m_bits.set(idx1);
	}

	BitFlags(BogusInitType k, Int idx1, Int idx2)
	{
		m_bits.set(idx1);
		m_bits.set(idx2);
	}

	BitFlags(BogusInitType k, Int idx1, Int idx2, Int idx3)
	{
		m_bits.set(idx1);
		m_bits.set(idx2);
		m_bits.set(idx3);
	}

	BitFlags(BogusInitType k, Int idx1, Int idx2, Int idx3, Int idx4)
	{
		m_bits.set(idx1);
		m_bits.set(idx2);
		m_bits.set(idx3);
		m_bits.set(idx4);
	}

	BitFlags(BogusInitType k, Int idx1, Int idx2, Int idx3, Int idx4, Int idx5)
	{
		m_bits.set(idx1);
		m_bits.set(idx2);
		m_bits.set(idx3);
		m_bits.set(idx4);
		m_bits.set(idx5);
	}

	// Set all given indices in the array.
	BitFlags(BogusInitType, const Int* idxs, Int count)
	{
		const Int* idx = idxs;
		const Int* end = idxs + count;
		for (; idx < end; ++idx)
		{
			m_bits.set(*idx);
		}
	}

	Bool operator==(const BitFlags& that) const
	{
		return m_bits == that.m_bits;
	}

	Bool operator!=(const BitFlags& that) const
	{
		return m_bits != that.m_bits;
	}

	void set(Int i, Int val = 1)
	{
		m_bits.set(i, val);
	}

	Bool test(Int i) const
	{
		return m_bits.test(i);
	}

	//Tests for any bits that are set in both.
	Bool testForAny( const BitFlags& that ) const
	{
		return (m_bits & that.m_bits).any();
	}

	//All argument bits must be set in our bits too in order to return TRUE
	Bool testForAll( const BitFlags& that ) const
	{
		return (m_bits & that.m_bits) == that.m_bits;
	}

	//None of the argument bits must be set in our bits in order to return TRUE
	Bool testForNone( const BitFlags& that ) const
	{
		return (m_bits & that.m_bits).none();
	}

	Int size() const
	{
		return m_bits.size();
	}

	Int count() const
	{
		return m_bits.count();
	}

	Bool any() const
	{
		return m_bits.any();
	}

	void flip()
	{
		m_bits.flip();
	}

	void clear()
	{
		m_bits.reset();
	}

	Int countIntersection(const BitFlags& that) const
	{
		return (m_bits & that.m_bits).count();
	}

	Int countInverseIntersection(const BitFlags& that) const
	{
		return (~m_bits & that.m_bits).count();
	}

	Bool anyIntersectionWith(const BitFlags& that) const
	{
		return (m_bits & that.m_bits).any();
	}

	void clear(const BitFlags& that)
	{
		m_bits &= ~that.m_bits;
	}

	void set(const BitFlags& that)
	{
		m_bits |= that.m_bits;
	}

	void clearAndSet(const BitFlags& flagsToClear, const BitFlags& flagsToSet)
	{
		clear(flagsToClear);
		set(flagsToSet);
	}

	Bool testSetAndClear(const BitFlags& mustBeSet, const BitFlags& mustBeClear) const
	{
		return testForNone(mustBeClear) && testForAll(mustBeSet);
	}

	// TheSuperHackers @info Function for rare use cases where we must access the flags as an integer.
	// Not using to_ulong because that can throw. Truncates all bits above 32.
	UnsignedInt toUnsignedInt() const noexcept
	{
		UnsignedInt val = 0;
		const UnsignedInt count = min(m_bits.size(), sizeof(val) * 8);
		for (UnsignedInt i = 0; i < count; ++i)
			val |= m_bits.test(i) * (1u << i);
		return val;
	}

  static const char* const* getBitNames()
  {
    return s_bitNameList;
  }

  static const char* getNameFromSingleBit(Int i)
  {
    return (i >= 0 && i < NUMBITS) ? s_bitNameList[i] : nullptr;
  }

  static Int getSingleBitFromName(const char* token)
  {
    Int i = 0;
	  for(const char* const* name = s_bitNameList; *name; ++name, ++i )
	  {
		  if( stricmp( *name, token ) == 0 )
		  {
        return i;
		  }
	  }
		return -1;
  }

  const char* getBitNameIfSet(Int i) const
  {
    return test(i) ? s_bitNameList[i] : nullptr;
  }

  Bool setBitByName(const char* token)
  {
    Int i = getSingleBitFromName(token);
		if (i >= 0)
		{
      set(i);
			return true;
		}
		else
		{
	    return false;
		}
  }

	void parse(INI* ini, AsciiString* str);
	void parseSingleBit(INI* ini, AsciiString* str);
	void xfer(Xfer* xfer);
	static void parseFromINI(INI* ini, void* /*instance*/, void *store, const void* /*userData*/); ///< Returns a BitFlag
	static void parseSingleBitFromINI(INI* ini, void* /*instance*/, void *store, const void* /*userData*/); ///< Returns an int, the Index of the one bit

	void buildDescription( AsciiString* str ) const
	{
		if ( str == nullptr )
			return;//sanity

		for( Int i = 0; i < size(); ++i )
		{
			const char* bitName = getBitNameIfSet(i);

			if (bitName != nullptr)
			{
				str->concat( bitName );
				str->concat( ",\n");
			}
		}
	}

	// TheSuperHackers @feature Stubbjax 23/01/2026 Add function for outputting debug data.
	AsciiString toHexString() const
	{
		constexpr const int numChunks = (NUMBITS + 63) / 64;
		char chunkBuf[32]; // Enough for 16 hex digits + null terminator
		AsciiString result;
		bool printedAny = false;

		for (int chunk = numChunks - 1; chunk >= 0; --chunk)
		{
			UnsignedInt64 val = 0;
			for (int bit = 0; bit < 64 && (chunk * 64 + bit) < NUMBITS; ++bit)
			{
				if (m_bits.test(chunk * 64 + bit))
					val |= (UnsignedInt64)(1) << bit;
			}

			if (val != 0 || chunk == 0 || printedAny)
			{
#ifdef _MSC_VER
				if (printedAny)
					snprintf(chunkBuf, sizeof(chunkBuf), "%016I64X", val);
				else
					snprintf(chunkBuf, sizeof(chunkBuf), "%I64X", val);
#else
				if (printedAny)
					snprintf(chunkBuf, sizeof(chunkBuf), "%016llX", (unsigned long long)val);
				else
					snprintf(chunkBuf, sizeof(chunkBuf), "%llX", (unsigned long long)val);
#endif

				result.concat(chunkBuf);
				printedAny = true;
			}
		}

		if (!printedAny)
			result = "0";

		return result;
	}
};
