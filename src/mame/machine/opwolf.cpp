// license:GPL-2.0+
// copyright-holders:David Graves, Jarek Burczynski,Stephane Humbert
/*************************************************************************

  Operation Wolf C-Chip Protection
  ================================

  The C-Chip (Taito TC0030CMD) is an unidentified mask programmed
  microcontroller of some sort with 64 pins.  It probably has
  about 2k of ROM and 8k of RAM.

  Interesting memory locations shared by cchip/68k:

    14 - dip switch A (written by 68k at start)
    15 - dip switch B (written by 68k at start)
    1b - Current level number (1-6)
    1c - Number of men remaining in level
    1e - Number of helicopters remaining in level
    1f - Number of tanks remaining in level
    20 - Number of boats remaining in level
    21 - Hostages in plane (last level)
    22 - Hostages remaining (last level)/Hostages saved (2nd last level)#
    27 - Set to 1 when final boss is destroyed
    32 - Set to 1 by cchip when level complete (no more enemies remaining)
    34 - Game state (0=attract mode, 1=intro, 2=in-game, 3=end-screen)
    51/52 - Used by cchip to signal change in credit level to 68k
    53 - Credit count
    75 - Set to 1 to trigger end of game boss
    7a - Used to trigger level data select command

  Notes on bootleg c-chip:
    Bootleg cchip forces english language mode
    Bootleg forces round 4 in attract mode
    Bootleg doesn't support service switch
    If you die after round 6 then the bootleg fails to reset the difficulty
    for the next game.

*************************************************************************/

#include "emu.h"
#include "includes/opwolf.h"


void opwolf_state::device_timer(emu_timer &timer, device_timer_id id, int param, void *ptr)
{
}


TIMER_CALLBACK_MEMBER(opwolf_state::opwolf_timer_callback)
{
}

void opwolf_state::updateDifficulty( int mode )
{
}

/*************************************
 *
 * Writes to C-Chip - Important Bits
 *
 *************************************/

WRITE16_MEMBER(opwolf_state::opwolf_cchip_status_w)
{
}

WRITE16_MEMBER(opwolf_state::opwolf_cchip_bank_w)
{
}

WRITE16_MEMBER(opwolf_state::opwolf_cchip_data_w)
{
}


/*************************************
 *
 * Reads from C-Chip
 *
 *************************************/

READ16_MEMBER(opwolf_state::opwolf_cchip_status_r)
{
    return 0x00;
}

READ16_MEMBER(opwolf_state::opwolf_cchip_data_r)
{
	return 0x00;
}

/*************************************
 *
 * C-Chip Tick
 *
 *************************************/

TIMER_CALLBACK_MEMBER(opwolf_state::cchip_timer)
{
}

/*************************************
 *
 * C-Chip State Saving
 *
 *************************************/

void opwolf_state::opwolf_cchip_init(  )
{
}
