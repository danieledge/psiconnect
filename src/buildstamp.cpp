// buildstamp.cpp - when this firmware was built, as "19 Oct 14:05" (compiled
// again on every build by tools/buildstamp.py).
// MIT licence (see LICENSE at the top of the repository).
#include <stdio.h>
#include <string.h>
#include "settings.h"

namespace am {

void BuildStamp(char* aOut, size_t aMax)
	{
	const char* d = __DATE__;                 // "Oct 19 2026" (the day padded with a space)
	const char* t = __TIME__;                 // "14:05:33"
	int day = (d[4] == ' ' ? 0 : (d[4] - '0') * 10) + (d[5] - '0');
	snprintf(aOut, aMax, "%d %.3s %.5s", day, d, t);
	}

} // namespace am
