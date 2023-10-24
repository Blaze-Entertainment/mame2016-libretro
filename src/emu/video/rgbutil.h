// license:BSD-3-Clause
// copyright-holders:Aaron Giles
/***************************************************************************

    rgbutil.h

    Utility definitions for RGB manipulation. Allows RGB handling to be
    performed in an abstracted fashion and optimized with SIMD.

***************************************************************************/

#ifndef __RGBUTIL__
#define __RGBUTIL__

#if defined(__arm__)
#include <sse2neon.h>
#endif
#include "rgbsse.h"

#endif /* __RGBUTIL__ */
