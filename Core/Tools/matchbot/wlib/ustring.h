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

#include <stdlib.h>
#include <stdio.h>
#include <Utility/iostream_adapter.h>
#include <string>

// Windows headers have a tendency to redefine IN
#ifdef IN
#undef IN
#endif
#define IN const

#define MAX_BYTES_PER_CHAR 1

template <class charT>
class UstringT : public basic_string<charT, string_char_traits<charT>/**/>
{
 public:
		explicit UstringT(int max_charlength) {
			set_max_bytelength(max_charlength*MAX_BYTES_PER_CHAR);
		}

		UstringT() { max_bytelength=4000; }

      size_t   get_max_bytelength(void) { return(max_bytelength); }
      void     set_max_bytelength(size_t max) { max_bytelength=max; }

      bool     operator==(const UstringT<charT> &other)
      {
        const basic_string<charT, string_char_traits<charT>/**/> *other_basic=&other;
        const basic_string<charT, string_char_traits<charT>/**/> *this_basic=this;
        return((*other_basic)==(*this_basic));
      }

 private:
		size_t   max_bytelength;
};

typedef UstringT<char> Ustring;
