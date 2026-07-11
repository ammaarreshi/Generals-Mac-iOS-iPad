/*
**	Command & Conquer Generals(tm)
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

// Trig.cpp
// fast trig functions
// Author: Michael S. Booth, March 1994
// Converted to Generals by Matthew D. Campbell, February 2002

#include "PreRTS.h"

#include <math.h>
#include <limits.h>

#include "Lib/BaseType.h"
#include "Lib/trig.h"
#include "WWMath/wwmath.h"

#define TWOPI			6.28318530718f
#define DEG2RAD 	0.0174532925199f
#define TRIG_RES 4096

// the following are for fixed point ints with 12 fractional bits
#define INT_ONE								4096
#define INT_TWOPI							25736
#define INT_THREEPIOVERTWO 		19302
#define INT_PI								12868
#define INT_HALFPI 						6434

// GeneralsX @refactor fbraz 03/05/2026 Route legacy trig functions through WWMath wrappers.
// Upstream reference: Okladnoj, PR #2670
// https://github.com/TheSuperHackers/GeneralsGameCode/pull/2670

Real Sin(Real x)
{
	return WWMath::SinTrig(x);
}

Real Cos(Real x)
{
	return WWMath::CosTrig(x);
}

Real Tan(Real x)
{
	return WWMath::TanTrig(x);
}

Real ACos(Real x)
{
	return WWMath::ACosTrig(x);
}

Real ASin(Real x)
{
	return WWMath::ASinTrig(x);
}

double Sqrt(double x)
{
	return WWMath::SqrtOrigin(x);
}

#ifdef REGENERATE_TRIG_TABLES
void initTrig()
{
	static Byte inited = FALSE;
	Real angle, r;
	int i;

	if (inited)
		return;

	inited = TRUE;

	static int columns = 8;
	int column = 0;
	FILE *fp = fopen("trig.txt", "w");
	fprintf(fp, "static Int sinLookup[TRIG_RES] = {\n");
	for( i=0; i<TRIG_RES; i++ ) {
		angle = TWOPI * i / (Real)TRIG_RES;
		sinLookup[i] = (Int)(WWMath::SinTrig(angle) * INT_ONE);

		if (i == 0)
		{
			fprintf(fp, "\t0x%8.8X", sinLookup[i]);
		}
		else if (column == 0)
		{
		fprintf(fp, ",\n\t0x%8.8X", sinLookup[i]);
		}
		else
		{
		fprintf(fp, ", 0x%8.8X", sinLookup[i]);
		}
		column = (column + 1) % columns;
	}
	fprintf(fp, "\n};\n\n");

	column = 0;
	fprintf(fp, "static Int arcCosLookup[2 * INT_ONE] = {\n");
	for( i=0; i<2*INT_ONE; i++ ) {
		r = (Real)i / (Real)INT_ONE - 1.0f;

		arcCosLookup[i] = (Int)(WWMath::ACosTrig( (double)r ) * INT_TWOPI / TWOPI );

		if (i == 0)
		{
			fprintf(fp, "\t0x%8.8X", arcCosLookup[i]);
		}
		else if (column == 0)
		{
		fprintf(fp, ",\n\t0x%8.8X", arcCosLookup[i]);
		}
		else
		{
		fprintf(fp, ", 0x%8.8X", arcCosLookup[i]);
		}
		column = (column + 1) % columns;
	}
	fprintf(fp, "\n};\n\n");

	fclose(fp);
}

class TrigInit
{
public:
	TrigInit() { initTrig(); }
};
TrigInit trigInitializer;

#endif // REGENERATE_TRIG_TABLES


