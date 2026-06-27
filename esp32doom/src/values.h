// Compatibility shim for the legacy SVR4/Linux <values.h> header, which does
// not exist in newlib (ESP32 toolchain). linuxdoom-1.10 includes <values.h>
// for the MAX*/MIN* limit macros; map them onto <limits.h>.
//
// This file is found via -I esp32doom/src (see platformio.ini).

#ifndef __DOOM_VALUES_SHIM_H__
#define __DOOM_VALUES_SHIM_H__

#include <limits.h>

#ifndef MAXINT
#define MAXINT   INT_MAX
#endif
#ifndef MININT
#define MININT   INT_MIN
#endif
#ifndef MAXSHORT
#define MAXSHORT SHRT_MAX
#endif
#ifndef MINSHORT
#define MINSHORT SHRT_MIN
#endif
#ifndef MAXLONG
#define MAXLONG  LONG_MAX
#endif
#ifndef MINLONG
#define MINLONG  LONG_MIN
#endif

#endif // __DOOM_VALUES_SHIM_H__
