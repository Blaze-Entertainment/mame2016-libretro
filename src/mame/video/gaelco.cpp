// license:BSD-3-Clause
// copyright-holders:Manuel Abadia
/***************************************************************************

  Gaelco Type 1 Video Hardware

  Functions to emulate the video hardware of the machine

***************************************************************************/

#include "emu.h"
#include "includes/gaelco.h"

/***************************************************************************

    Callbacks for the TileMap code

***************************************************************************/

/*
    Tile format
    -----------

    Screen 0 & 1: (32*32, 16x16 tiles)

    Word | Bit(s)            | Description
    -----+-FEDCBA98-76543210-+--------------------------
      0  | -------- -------x | flip x
      0  | -------- ------x- | flip y
      0  | xxxxxxxx xxxxxx-- | code
      1  | -------- --xxxxxx | color
      1  | -------- xx------ | priority
      1  | xxxxxxxx -------- | not used
*/

int gaelco_state::squash_tilecode_remap(int code)
{
	if (
		(code == 0xc96) ||
		(code == 0xcbe) ||
		(code == 0xce3) ||
		(code == 0xce3) ||
		(code == 0xd07) ||
		(code == 0xd28) ||
		(code == 0xd4c) ||
		(code == 0xd4c) ||
		(code == 0xd6c) ||
		(code == 0xda9) ||
		(code == 0xdca) ||
		(code == 0xde9) ||
		(code == 0xe09)
		)
		code = 0x2b6;

	if (
		(code == 0xc97) ||
		(code == 0xcbf) ||
		(code == 0xce4) ||
		(code == 0xce4) ||
		(code == 0xd08) ||
		(code == 0xd29) ||
		(code == 0xd4d) ||
		(code == 0xd4d) ||
		(code == 0xd6d) ||
		(code == 0xdaa) ||
		(code == 0xdea)
		)
		code = 0x2b8;

	return code;
}

TILE_GET_INFO_MEMBER(gaelco_state::get_tile_info_gaelco_screen0)
{
	int data = m_videoram[tile_index << 1];
	int data2 = m_videoram[(tile_index << 1) + 1];
	int code = ((data & 0xfffc) >> 2);

	if (m_use_squash_sprite_disable)
		code = squash_tilecode_remap(code);

	SET_TILE_INFO_MEMBER(1, 0x4000 + code, data2 & 0xff, TILE_FLIPYX(data & 0x03));
}


TILE_GET_INFO_MEMBER(gaelco_state::get_tile_info_gaelco_screen1)
{
	int data = m_videoram[(0x1000 / 2) + (tile_index << 1)];
	int data2 = m_videoram[(0x1000 / 2) + (tile_index << 1) + 1];
	int code = ((data & 0xfffc) >> 2);

	if (m_use_squash_sprite_disable)
		code = squash_tilecode_remap(code);

	SET_TILE_INFO_MEMBER(1, 0x4000 + code, data2 & 0xff, TILE_FLIPYX(data & 0x03));
}

/***************************************************************************

    Memory Handlers

***************************************************************************/

WRITE16_MEMBER(gaelco_state::gaelco_vram_w)
{
	uint16_t olddata = m_videoram[offset];
	COMBINE_DATA(&m_videoram[offset]);

	if (m_videoram[offset] != olddata)
		m_tilemap[offset >> 11]->mark_tile_dirty(((offset << 1) & 0x0fff) >> 2);
}

/***************************************************************************

    Start/Stop the video hardware emulation.

***************************************************************************/

VIDEO_START_MEMBER(gaelco_state,bigkarnk)
{
	m_tilemap[0] = &machine().tilemap().create(m_gfxdecode, tilemap_get_info_delegate(FUNC(gaelco_state::get_tile_info_gaelco_screen0),this), TILEMAP_SCAN_ROWS, 16, 16, 32, 32);
	m_tilemap[1] = &machine().tilemap().create(m_gfxdecode, tilemap_get_info_delegate(FUNC(gaelco_state::get_tile_info_gaelco_screen1),this), TILEMAP_SCAN_ROWS, 16, 16, 32, 32);
}

VIDEO_START_MEMBER(gaelco_state,maniacsq)
{
	m_tilemap[0] = &machine().tilemap().create(m_gfxdecode, tilemap_get_info_delegate(FUNC(gaelco_state::get_tile_info_gaelco_screen0),this), TILEMAP_SCAN_ROWS, 16, 16, 32, 32);
	m_tilemap[1] = &machine().tilemap().create(m_gfxdecode, tilemap_get_info_delegate(FUNC(gaelco_state::get_tile_info_gaelco_screen1),this), TILEMAP_SCAN_ROWS, 16, 16, 32, 32);
}

VIDEO_START_MEMBER(gaelco_state,squash)
{
	m_tilemap[0] = &machine().tilemap().create(m_gfxdecode, tilemap_get_info_delegate(FUNC(gaelco_state::get_tile_info_gaelco_screen0),this), TILEMAP_SCAN_ROWS, 16, 16, 32, 32);
	m_tilemap[1] = &machine().tilemap().create(m_gfxdecode, tilemap_get_info_delegate(FUNC(gaelco_state::get_tile_info_gaelco_screen1),this), TILEMAP_SCAN_ROWS, 16, 16, 32, 32);

	m_sprite_palette_force_high = 0x3c;
	m_use_squash_sprite_disable = true;
}


/***************************************************************************

    Sprites

***************************************************************************/

/*
    Sprite Format
    -------------

    Word | Bit(s)            | Description
    -----+-FEDCBA98-76543210-+--------------------------
      0  | -------- xxxxxxxx | y position
      0  | -----xxx -------- | not used
      0  | ----x--- -------- | sprite size
      0  | --xx---- -------- | sprite priority
      0  | -x------ -------- | flipx
      0  | x------- -------- | flipy
      1  | xxxxxxxx xxxxxxxx | not used
      2  | -------x xxxxxxxx | x position
      2  | -xxxxxx- -------- | sprite color
      3  | -------- ------xx | sprite code (8x8 quadrant)
      3  | xxxxxxxx xxxxxx-- | sprite code
*/

void gaelco_state::draw_sprites( screen_device &screen, bitmap_ind16 &bitmap, const rectangle &cliprect )
{
	gfx_element* gfx;

	for (int i = 3; i < 0x800-4; i += 4)
	{
		int sx = m_spriteram[i + 2] & 0x01ff;
		int sy = (240 - (m_spriteram[i] & 0x00ff)) & 0x00ff;
		int number = m_spriteram[i + 3];
		int color = (m_spriteram[i + 2] & 0x7e00) >> 9;
		int attr = (m_spriteram[i] & 0xfe00) >> 9;
		int priority = (m_spriteram[i] & 0x3000) >> 12;

		int xflip = attr & 0x20;
		int yflip = attr & 0x40;

		/* Hide bad sprites that appear after Squash continue screen (unwanted
		   Insert Coin and Press 1P/2P start message in bad colouds)

		   This is a hack to work around a protection issue when the game
		   framerate isn't correct
		 */
		//if (m_use_squash_sprite_disable)
		//	if (m_spriteram[i] == 0x0800)
		//		continue;

		/* palettes 0x38-0x3f are used for high priority sprites in Big Karnak
		   the same logic in Squash causes player sprites to be drawn over the
		   pixels that form the glass play area.

		   Is this accurate, or just exposing a different flaw in the priority
		   handling? */
		if (color >= m_sprite_palette_force_high)
			priority = 4;

		priority += 1;

		color |= (priority << 6);
		
		if (attr & 0x04)
			gfx = m_gfxdecode->gfx(0);
		else
		{
			gfx = m_gfxdecode->gfx(1);
			number >>= 2;
		}

		gfx->transpen(bitmap,cliprect,number,
				color,xflip,yflip,
				sx-0x0f,sy,
				0);

	}
}

/***************************************************************************

    Display Refresh

***************************************************************************/

WRITE16_MEMBER(gaelco_state::gaelco_palette_w)
{
	uint16_t old = m_paletteram[offset];

	COMBINE_DATA(&m_paletteram[offset]);

	if (old != m_paletteram[offset])
	{
		const uint16_t color = m_paletteram[offset];

		/* extract RGB components */
		const rgb_t col = rgb_t(pal5bit(color & 0x1f), pal5bit(color >> 5), pal5bit(color >> 10));

		/* update game palette */
		for (int i = 0; i < 32; i++)
			m_palette->set_pen_color(0x400 * i + offset, col);
	}
}


inline void mix_normal(uint16_t* dstptr, uint16_t* pixdat, uint16_t* pixdat2)
{
	switch (*pixdat & 0xc00)
	{
	case 0x000:
	{
		switch (*pixdat2 & 0xc00)
		{

		case 0x000:
		{
			if (!(*pixdat & 0x8))
			{
				if (*pixdat & 0x7) { *dstptr = *pixdat; }
				else if (!(*pixdat2 & 0x8))
				{
					if (*pixdat2 & 0x7) { *dstptr = *pixdat2; }
					else if (*pixdat & 0x8) { *dstptr = *pixdat; }
				}
				else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; }
			}
			else if (!(*pixdat2 & 0x8))
			{
				if (*pixdat2 & 0x7) { *dstptr = *pixdat2; }
				else if (*pixdat & 0x8) { *dstptr = *pixdat; }
			}
			else if (*pixdat & 0x8) { *dstptr = *pixdat; }
			break;
		}

		case 0x400:
		{
			if (!(*pixdat & 0x8))
			{
				if (*pixdat & 0x7) { *dstptr = *pixdat; }
				else if (*pixdat & 0x8) { *dstptr = *pixdat; }
				else if (!(*pixdat2 & 0x8))
				{
					if (*pixdat2 & 0x7) { *dstptr = *pixdat2;; }
				}
				else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; }
			}
			else if (*pixdat & 0x8) { *dstptr = *pixdat; }
			break;
		}

		case 0x800:
		{
			if (!(*pixdat & 0x8))
			{
				if (*pixdat & 0x7) { *dstptr = *pixdat; }
				else if (!(*pixdat2 & 0x8))
				{
					if (*pixdat2 & 0x7) { *dstptr = *pixdat2;; }
				}
				else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; }
			}
			else if (*pixdat & 0x8) { *dstptr = *pixdat; }
			break;
		}

		case 0xc00:
		{
			if (!(*pixdat & 0x8))
			{
				if (*pixdat & 0x7) { *dstptr = *pixdat; }
				else if (!(*pixdat2 & 0x8))
				{
					if (*pixdat2 & 0x7) { *dstptr = *pixdat2;; }
				}
				else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; }
			}
			else if (*pixdat & 0x8) { *dstptr = *pixdat; }
			break;
		}
		}
		break;
	}

	case 0x400:
	{
		switch (*pixdat2 & 0xc00)
		{
		case 0x000:
		{
			if (!(*pixdat2 & 0x8))
			{
				if (*pixdat2 & 0x7) { *dstptr = *pixdat2; }
				else if (!(*pixdat & 0x8))
				{
					if (*pixdat & 0x7) { *dstptr = *pixdat; }
				}
				else if (*pixdat & 0x8) { *dstptr = *pixdat;; }
			}
			else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; }
			break;
		}

		case 0x400:
		{
			if (!(*pixdat & 0x8))
			{
				if (*pixdat & 0x7) { *dstptr = *pixdat; }
				else if (!(*pixdat2 & 0x8))
				{
					if (*pixdat2 & 0x7) { *dstptr = *pixdat2;; }
				}
				else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; }
			}
			else if (!(*pixdat2 & 0x8))
			{
				if (*pixdat2 & 0x7) { *dstptr = *pixdat2;; }
				else if (*pixdat & 0x8) { *dstptr = *pixdat;; }
			}
			else if (*pixdat & 0x8) { *dstptr = *pixdat;; }
			break;
		}

		case 0x800:
		{
			if (!(*pixdat & 0x8))
			{
				if (*pixdat & 0x7) { *dstptr = *pixdat; }
				else if (!(*pixdat2 & 0x8))
				{
					if (*pixdat2 & 0x7) { *dstptr = *pixdat2;; }
				}
				else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; }
			}
			else if (*pixdat & 0x8) { *dstptr = *pixdat;; }
			break;
		}

		case 0xc00:
		{
			if (!(*pixdat & 0x8))
			{
				if (*pixdat & 0x7) { *dstptr = *pixdat; }
				else if (!(*pixdat2 & 0x8))
				{
					if (*pixdat2 & 0x7) { *dstptr = *pixdat2;; }
				}
				else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; }
			}
			else if (*pixdat & 0x8) { *dstptr = *pixdat;; }
			break;
		}
		}
		break;
	}

	case 0x800:
	{
		switch (*pixdat2 & 0xc00)
		{

		case 0x000:
		{
			if (!(*pixdat2 & 0x8))
			{
				if (*pixdat2 & 0x7) { *dstptr = *pixdat2; }
				else if (!(*pixdat & 0x8))
				{
					if (*pixdat & 0x7) { *dstptr = *pixdat; }
				}
				else if (*pixdat & 0x8) { *dstptr = *pixdat;; }
			}
			else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; }
			break;
		}

		case 0x400:
		{
			if (!(*pixdat2 & 0x8))
			{
				if (*pixdat2 & 0x7) { *dstptr = *pixdat2;; }
				else if (!(*pixdat & 0x8))
				{
					if (*pixdat & 0x7) { *dstptr = *pixdat;; }
				}
				else if (*pixdat & 0x8) { *dstptr = *pixdat;; }
			}
			else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; }
			break;
		}

		case 0x800:
		{
			if (!(*pixdat & 0x8))
			{
				if (*pixdat & 0x7) { *dstptr = *pixdat;; }
				else if (!(*pixdat2 & 0x8))
				{
					if (*pixdat2 & 0x7) { *dstptr = *pixdat2;; }
				}
				else if (*pixdat & 0x8) { *dstptr = *pixdat;; }
				else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; }
			}
			else if (!(*pixdat2 & 0x8))
			{
				if (*pixdat2 & 0x7) { *dstptr = *pixdat2;; }
				else if (*pixdat & 0x8) { *dstptr = *pixdat;; }
			}
			else if (*pixdat & 0x8) { *dstptr = *pixdat;; }

			break;
		}

		case 0xc00:
		{

			if (!(*pixdat & 0x8))
			{
				if (*pixdat & 0x7) { *dstptr = *pixdat;; }
				else if (!(*pixdat2 & 0x8))
				{
					if (*pixdat2 & 0x7) { *dstptr = *pixdat2;; }
				}
				else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; }
			}
			else if (*pixdat & 0x8) { *dstptr = *pixdat;; }

			break;
		}
		}
		break;
	}

	case 0xc00:
	{
		switch (*pixdat2 & 0xc00)
		{

		case 0x000:
		{
			if (!(*pixdat2 & 0x8))
			{
				if (*pixdat2 & 0x7) { *dstptr = *pixdat2; }
				else if (!(*pixdat & 0x8))
				{
					if (*pixdat & 0x7) { *dstptr = *pixdat;; }
				}
				else if (*pixdat & 0x8) { *dstptr = *pixdat;; }
			}
			else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; }
			break;
		}

		case 0x400:
		{
			if (!(*pixdat2 & 0x8))
			{
				if (*pixdat2 & 0x7) { *dstptr = *pixdat2;; }
				else if (!(*pixdat & 0x8))
				{
					if (*pixdat & 0x7) { *dstptr = *pixdat;; }
				}
				else if (*pixdat & 0x8) { *dstptr = *pixdat;; }
			}
			else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; }
			break;
		}

		case 0x800:
		{
			if (!(*pixdat2 & 0x8))
			{
				if (*pixdat2 & 0x7) { *dstptr = *pixdat2;; }
				else if (!(*pixdat & 0x8))
				{
					if (*pixdat & 0x7) { *dstptr = *pixdat;; }
				}
				else if (*pixdat & 0x8) { *dstptr = *pixdat;; }
			}
			else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; }
			break;
		}

		case 0xc00:
		{
			if (!(*pixdat & 0x8))
			{
				if (*pixdat & 0x7) { *dstptr = *pixdat;; }
				else if (!(*pixdat2 & 0x8))
				{
					if (*pixdat2 & 0x7) { *dstptr = *pixdat2;; }
				}
				else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; }
			}
			else if (!(*pixdat2 & 0x8))
			{
				if (*pixdat2 & 0x7) { *dstptr = *pixdat2;; }
				else if (*pixdat & 0x8) { *dstptr = *pixdat;; }
			}
			else if (*pixdat & 0x8) { *dstptr = *pixdat;; }
			break;
		}
		}
		break;
	}
	}
}




inline void mix_above0(uint16_t* dstptr, uint16_t* pixdat, uint16_t* pixdat2)
{
	switch (*pixdat & 0xc00)
	{
	case 0x000:
	{
		switch (*pixdat2 & 0xc00)
		{

		case 0x000:
		{
			if (!(*pixdat & 0x8))
			{
				if (*pixdat & 0x7) { *dstptr = *pixdat; }
				else if (!(*pixdat2 & 0x8))
				{
					if (*pixdat2 & 0x7) { *dstptr = *pixdat2; }
					else if (*pixdat & 0x8) { *dstptr = *pixdat; }
				}
				else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; }
			}
			else if (!(*pixdat2 & 0x8))
			{
				if (*pixdat2 & 0x7) { *dstptr = *pixdat2; }
				else if (*pixdat & 0x8) { *dstptr = *pixdat; }
			}
			else if (*pixdat & 0x8) { *dstptr = *pixdat; }
			break;
		}

		case 0x400:
		{
			if (!(*pixdat & 0x8))
			{
				if (*pixdat & 0x7) { *dstptr = *pixdat; }
				else if (*pixdat & 0x8) { *dstptr = *pixdat; }
				else if (!(*pixdat2 & 0x8))
				{
					if (*pixdat2 & 0x7) { *dstptr = *pixdat2;; }
				}
				else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; }
			}
			else if (*pixdat & 0x8) { *dstptr = *pixdat; }
			break;
		}

		case 0x800:
		{
			if (!(*pixdat & 0x8))
			{
				if (*pixdat & 0x7) { *dstptr = *pixdat; }
				else if (!(*pixdat2 & 0x8))
				{
					if (*pixdat2 & 0x7) { *dstptr = *pixdat2;; }
				}
				else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; }
			}
			else if (*pixdat & 0x8) { *dstptr = *pixdat; }
			break;
		}

		case 0xc00:
		{
			if (!(*pixdat & 0x8))
			{
				if (*pixdat & 0x7) { *dstptr = *pixdat; }
				else if (!(*pixdat2 & 0x8))
				{
					if (*pixdat2 & 0x7) { *dstptr = *pixdat2;; }
				}
				else if (*pixdat2 & 0x8) { /* sprite has priority */; }
			}
			else if (*pixdat & 0x8) { *dstptr = *pixdat; }
			break;
		}
		}
		break;
	}

	case 0x400:
	{
		switch (*pixdat2 & 0xc00)
		{
		case 0x000:
		{
			if (!(*pixdat2 & 0x8))
			{
				if (*pixdat2 & 0x7) { *dstptr = *pixdat2; }
				else if (!(*pixdat & 0x8))
				{
					if (*pixdat & 0x7) { *dstptr = *pixdat; }
				}
				else if (*pixdat & 0x8) { *dstptr = *pixdat;; }
			}
			else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; }
			break;
		}

		case 0x400:
		{
			if (!(*pixdat & 0x8))
			{
				if (*pixdat & 0x7) { *dstptr = *pixdat; }
				else if (!(*pixdat2 & 0x8))
				{
					if (*pixdat2 & 0x7) { *dstptr = *pixdat2;; }
				}
				else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; }
			}
			else if (!(*pixdat2 & 0x8))
			{
				if (*pixdat2 & 0x7) { *dstptr = *pixdat2;; }
				else if (*pixdat & 0x8) { *dstptr = *pixdat;; }
			}
			else if (*pixdat & 0x8) { *dstptr = *pixdat;; }
			break;
		}

		case 0x800:
		{
			if (!(*pixdat & 0x8))
			{
				if (*pixdat & 0x7) { *dstptr = *pixdat; }
				else if (!(*pixdat2 & 0x8))
				{
					if (*pixdat2 & 0x7) { *dstptr = *pixdat2;; }
				}
				else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; }
			}
			else if (*pixdat & 0x8) { *dstptr = *pixdat;; }
			break;
		}

		case 0xc00:
		{
			if (!(*pixdat & 0x8))
			{
				if (*pixdat & 0x7) { *dstptr = *pixdat; }
				else if (!(*pixdat2 & 0x8))
				{
					if (*pixdat2 & 0x7) { *dstptr = *pixdat2;; }
				}
				else if (*pixdat2 & 0x8) { /* sprite has priority */; }
			}
			else if (*pixdat & 0x8) { *dstptr = *pixdat;; }
			break;
		}
		}
		break;
	}

	case 0x800:
	{
		switch (*pixdat2 & 0xc00)
		{

		case 0x000:
		{
			if (!(*pixdat2 & 0x8))
			{
				if (*pixdat2 & 0x7) { *dstptr = *pixdat2; }
				else if (!(*pixdat & 0x8))
				{
					if (*pixdat & 0x7) { *dstptr = *pixdat; }
				}
				else if (*pixdat & 0x8) { *dstptr = *pixdat;; }
			}
			else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; }
			break;
		}

		case 0x400:
		{
			if (!(*pixdat2 & 0x8))
			{
				if (*pixdat2 & 0x7) { *dstptr = *pixdat2;; }
				else if (!(*pixdat & 0x8))
				{
					if (*pixdat & 0x7) { *dstptr = *pixdat;; }
				}
				else if (*pixdat & 0x8) { *dstptr = *pixdat;; }
			}
			else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; }
			break;
		}

		case 0x800:
		{
			if (!(*pixdat & 0x8))
			{
				if (*pixdat & 0x7) { *dstptr = *pixdat;; }
				else if (!(*pixdat2 & 0x8))
				{
					if (*pixdat2 & 0x7) { *dstptr = *pixdat2;; }
				}
				else if (*pixdat & 0x8) { *dstptr = *pixdat;; }
				else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; }
			}
			else if (!(*pixdat2 & 0x8))
			{
				if (*pixdat2 & 0x7) { *dstptr = *pixdat2;; }
				else if (*pixdat & 0x8) { *dstptr = *pixdat;; }
			}
			else if (*pixdat & 0x8) { *dstptr = *pixdat;; }

			break;
		}

		case 0xc00:
		{

			if (!(*pixdat & 0x8))
			{
				if (*pixdat & 0x7) { *dstptr = *pixdat;; }
				else if (!(*pixdat2 & 0x8))
				{
					if (*pixdat2 & 0x7) { *dstptr = *pixdat2;; }
				}
				else if (*pixdat2 & 0x8) { /* sprite has priority */; }
			}
			else if (*pixdat & 0x8) { *dstptr = *pixdat;; }

			break;
		}
		}
		break;
	}

	case 0xc00:
	{
		switch (*pixdat2 & 0xc00)
		{

		case 0x000:
		{
			if (!(*pixdat2 & 0x8))
			{
				if (*pixdat2 & 0x7) { *dstptr = *pixdat2; }
				else if (!(*pixdat & 0x8))
				{
					if (*pixdat & 0x7) { *dstptr = *pixdat;; }
				}
				else if (*pixdat & 0x8) { /* sprite has priority */; }
			}
			else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; }
			break;
		}

		case 0x400:
		{
			if (!(*pixdat2 & 0x8))
			{
				if (*pixdat2 & 0x7) { *dstptr = *pixdat2;; }
				else if (!(*pixdat & 0x8))
				{
					if (*pixdat & 0x7) { *dstptr = *pixdat;; }
				}
				else if (*pixdat & 0x8) { /* sprite has priority */; }
			}
			else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; }
			break;
		}

		case 0x800:
		{
			if (!(*pixdat2 & 0x8))
			{
				if (*pixdat2 & 0x7) { *dstptr = *pixdat2;; }
				else if (!(*pixdat & 0x8))
				{
					if (*pixdat & 0x7) { *dstptr = *pixdat;; }
				}
				else if (*pixdat & 0x8) { /* sprite has priority */; }
			}
			else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; }
			break;
		}

		case 0xc00:
		{
			if (!(*pixdat & 0x8))
			{
				if (*pixdat & 0x7) { *dstptr = *pixdat;; }
				else if (!(*pixdat2 & 0x8))
				{
					if (*pixdat2 & 0x7) { *dstptr = *pixdat2;; }
				}
				else if (*pixdat2 & 0x8) { /* sprite has priority */; }
			}
			else if (!(*pixdat2 & 0x8))
			{
				if (*pixdat2 & 0x7) { *dstptr = *pixdat2;; }
				else if (*pixdat & 0x8) { /* sprite has priority */; }
			}
			else if (*pixdat & 0x8) { /* sprite has priority */; }
			break;
		}
		}
		break;
	}
	}
}



inline void mix_above0_1(uint16_t* dstptr, uint16_t* pixdat, uint16_t* pixdat2)
{
	switch (*pixdat & 0xc00)
	{
	case 0x000:
	{
		switch (*pixdat2 & 0xc00)
		{

		case 0x000:
		{
			if (!(*pixdat & 0x8))
			{
				if (*pixdat & 0x7) { *dstptr = *pixdat; }
				else if (!(*pixdat2 & 0x8))
				{
					if (*pixdat2 & 0x7) { *dstptr = *pixdat2; }
					else if (*pixdat & 0x8) { *dstptr = *pixdat; }
				}
				else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; }
			}
			else if (!(*pixdat2 & 0x8))
			{
				if (*pixdat2 & 0x7) { *dstptr = *pixdat2; }
				else if (*pixdat & 0x8) { *dstptr = *pixdat; }
			}
			else if (*pixdat & 0x8) { *dstptr = *pixdat; }
			break;
		}

		case 0x400:
		{
			if (!(*pixdat & 0x8))
			{
				if (*pixdat & 0x7) { *dstptr = *pixdat; }
				else if (*pixdat & 0x8) { *dstptr = *pixdat; }
				else if (!(*pixdat2 & 0x8))
				{
					if (*pixdat2 & 0x7) { *dstptr = *pixdat2;; }
				}
				else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; }
			}
			else if (*pixdat & 0x8) { *dstptr = *pixdat; }
			break;
		}

		case 0x800:
		{
			if (!(*pixdat & 0x8))
			{
				if (*pixdat & 0x7) { *dstptr = *pixdat; }
				else if (!(*pixdat2 & 0x8))
				{
					if (*pixdat2 & 0x7) { *dstptr = *pixdat2;; }
				}
				else if (*pixdat2 & 0x8) { /* sprite has priority */; }
			}
			else if (*pixdat & 0x8) { *dstptr = *pixdat; }
			break;
		}

		case 0xc00:
		{
			if (!(*pixdat & 0x8))
			{
				if (*pixdat & 0x7) { *dstptr = *pixdat; }
				else if (!(*pixdat2 & 0x8))
				{
					if (*pixdat2 & 0x7) { /* sprite has priority */; }
				}
				else if (*pixdat2 & 0x8) { /* sprite has priority */; }
			}
			else if (*pixdat & 0x8) { *dstptr = *pixdat; }
			break;
		}
		}
		break;
	}

	case 0x400:
	{
		switch (*pixdat2 & 0xc00)
		{
		case 0x000:
		{
			if (!(*pixdat2 & 0x8))
			{
				if (*pixdat2 & 0x7) { *dstptr = *pixdat2; }
				else if (!(*pixdat & 0x8))
				{
					if (*pixdat & 0x7) { *dstptr = *pixdat; }
				}
				else if (*pixdat & 0x8) { *dstptr = *pixdat;; }
			}
			else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; }
			break;
		}

		case 0x400:
		{
			if (!(*pixdat & 0x8))
			{
				if (*pixdat & 0x7) { *dstptr = *pixdat; }
				else if (!(*pixdat2 & 0x8))
				{
					if (*pixdat2 & 0x7) { *dstptr = *pixdat2;; }
				}
				else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; }
			}
			else if (!(*pixdat2 & 0x8))
			{
				if (*pixdat2 & 0x7) { *dstptr = *pixdat2;; }
				else if (*pixdat & 0x8) { *dstptr = *pixdat;; }
			}
			else if (*pixdat & 0x8) { *dstptr = *pixdat;; }
			break;
		}

		case 0x800:
		{
			if (!(*pixdat & 0x8))
			{
				if (*pixdat & 0x7) { *dstptr = *pixdat; }
				else if (!(*pixdat2 & 0x8))
				{
					if (*pixdat2 & 0x7) { *dstptr = *pixdat2;; }
				}
				else if (*pixdat2 & 0x8) { /* sprite has priority */; }
			}
			else if (*pixdat & 0x8) { *dstptr = *pixdat;; }
			break;
		}

		case 0xc00:
		{
			if (!(*pixdat & 0x8))
			{
				if (*pixdat & 0x7) { *dstptr = *pixdat; }
				else if (!(*pixdat2 & 0x8))
				{
					if (*pixdat2 & 0x7) { /* sprite has priority */; }
				}
				else if (*pixdat2 & 0x8) { /* sprite has priority */; }
			}
			else if (*pixdat & 0x8) { *dstptr = *pixdat;; }
			break;
		}
		}
		break;
	}

	case 0x800:
	{
		switch (*pixdat2 & 0xc00)
		{

		case 0x000:
		{
			if (!(*pixdat2 & 0x8))
			{
				if (*pixdat2 & 0x7) { *dstptr = *pixdat2; }
				else if (!(*pixdat & 0x8))
				{
					if (*pixdat & 0x7) { *dstptr = *pixdat; }
				}
				else if (*pixdat & 0x8) { /* sprite has priority */; }
			}
			else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; }
			break;
		}

		case 0x400:
		{
			if (!(*pixdat2 & 0x8))
			{
				if (*pixdat2 & 0x7) { *dstptr = *pixdat2;; }
				else if (!(*pixdat & 0x8))
				{
					if (*pixdat & 0x7) { *dstptr = *pixdat;; }
				}
				else if (*pixdat & 0x8) { /* sprite has priority */; }
			}
			else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; }
			break;
		}

		case 0x800:
		{
			if (!(*pixdat & 0x8))
			{
				if (*pixdat & 0x7) { *dstptr = *pixdat;; }
				else if (!(*pixdat2 & 0x8))
				{
					if (*pixdat2 & 0x7) { *dstptr = *pixdat2;; }
				}
				else if (*pixdat & 0x8) { /* sprite has priority */; }
				else if (*pixdat2 & 0x8) { /* sprite has priority */; }
			}
			else if (!(*pixdat2 & 0x8))
			{
				if (*pixdat2 & 0x7) { *dstptr = *pixdat2;; }
				else if (*pixdat & 0x8) { /* sprite has priority */; }
			}
			else if (*pixdat & 0x8) { /* sprite has priority */; }

			break;
		}

		case 0xc00:
		{

			if (!(*pixdat & 0x8))
			{
				if (*pixdat & 0x7) { *dstptr = *pixdat;; }
				else if (!(*pixdat2 & 0x8))
				{
					if (*pixdat2 & 0x7) { /* sprite has priority */; }
				}
				else if (*pixdat2 & 0x8) { /* sprite has priority */; }
			}
			else if (*pixdat & 0x8) { /* sprite has priority */; }

			break;
		}
		}
		break;
	}

	case 0xc00:
	{
		switch (*pixdat2 & 0xc00)
		{

		case 0x000:
		{
			if (!(*pixdat2 & 0x8))
			{
				if (*pixdat2 & 0x7) { *dstptr = *pixdat2; }
				else if (!(*pixdat & 0x8))
				{
					if (*pixdat & 0x7) { /* sprite has priority */; }
				}
				else if (*pixdat & 0x8) { /* sprite has priority */; }
			}
			else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; }
			break;
		}

		case 0x400:
		{
			if (!(*pixdat2 & 0x8))
			{
				if (*pixdat2 & 0x7) { *dstptr = *pixdat2;; }
				else if (!(*pixdat & 0x8))
				{
					if (*pixdat & 0x7) { /* sprite has priority */; }
				}
				else if (*pixdat & 0x8) { /* sprite has priority */; }
			}
			else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; }
			break;
		}

		case 0x800:
		{
			if (!(*pixdat2 & 0x8))
			{
				if (*pixdat2 & 0x7) { *dstptr = *pixdat2;; }
				else if (!(*pixdat & 0x8))
				{
					if (*pixdat & 0x7) { /* sprite has priority */; }
				}
				else if (*pixdat & 0x8) { /* sprite has priority */; }
			}
			else if (*pixdat2 & 0x8) { /* sprite has priority */; }
			break;
		}

		case 0xc00:
		{
			if (!(*pixdat & 0x8))
			{
				if (*pixdat & 0x7) { /* sprite has priority */; }
				else if (!(*pixdat2 & 0x8))
				{
					if (*pixdat2 & 0x7) { /* sprite has priority */; }
				}
				else if (*pixdat2 & 0x8) { /* sprite has priority */; }
			}
			else if (!(*pixdat2 & 0x8))
			{
				if (*pixdat2 & 0x7) { /* sprite has priority */; }
				else if (*pixdat & 0x8) { /* sprite has priority */; }
			}
			else if (*pixdat & 0x8) { /* sprite has priority */; }
			break;
		}
		}
		break;
	}
	}
}



inline void mix_above0_1_2(uint16_t* dstptr, uint16_t* pixdat, uint16_t* pixdat2)
{
	switch (*pixdat & 0xc00)
	{
	case 0x000:
	{
		switch (*pixdat2 & 0xc00)
		{

		case 0x000:
		{
			if (!(*pixdat & 0x8))
			{
				if (*pixdat & 0x7) { *dstptr = *pixdat; }
				else if (!(*pixdat2 & 0x8))
				{
					if (*pixdat2 & 0x7) { *dstptr = *pixdat2; }
					else if (*pixdat & 0x8) { *dstptr = *pixdat; }
				}
				else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; }
			}
			else if (!(*pixdat2 & 0x8))
			{
				if (*pixdat2 & 0x7) { *dstptr = *pixdat2; }
				else if (*pixdat & 0x8) { *dstptr = *pixdat; }
			}
			else if (*pixdat & 0x8) { *dstptr = *pixdat; }
			break;
		}

		case 0x400:
		{
			if (!(*pixdat & 0x8))
			{
				if (*pixdat & 0x7) { *dstptr = *pixdat; }
				else if (*pixdat & 0x8) { *dstptr = *pixdat; }
				else if (!(*pixdat2 & 0x8))
				{
					if (*pixdat2 & 0x7) { *dstptr = *pixdat2;; }
				}
				else if (*pixdat2 & 0x8) { /* sprite has priority */; }
			}
			else if (*pixdat & 0x8) { *dstptr = *pixdat; }
			break;
		}

		case 0x800:
		{
			if (!(*pixdat & 0x8))
			{
				if (*pixdat & 0x7) { *dstptr = *pixdat; }
				else if (!(*pixdat2 & 0x8))
				{
					if (*pixdat2 & 0x7) { /* sprite has priority */; }
				}
				else if (*pixdat2 & 0x8) { /* sprite has priority */; }
			}
			else if (*pixdat & 0x8) { *dstptr = *pixdat; }
			break;
		}

		case 0xc00:
		{
			if (!(*pixdat & 0x8))
			{
				if (*pixdat & 0x7) { *dstptr = *pixdat; }
				else if (!(*pixdat2 & 0x8))
				{
					if (*pixdat2 & 0x7) { /* sprite has priority */; }
				}
				else if (*pixdat2 & 0x8) { /* sprite has priority */; }
			}
			else if (*pixdat & 0x8) { *dstptr = *pixdat; }
			break;
		}
		}
		break;
	}

	case 0x400:
	{
		switch (*pixdat2 & 0xc00)
		{
		case 0x000:
		{
			if (!(*pixdat2 & 0x8))
			{
				if (*pixdat2 & 0x7) { *dstptr = *pixdat2; }
				else if (!(*pixdat & 0x8))
				{
					if (*pixdat & 0x7) { *dstptr = *pixdat; }
				}
				else if (*pixdat & 0x8) { /* sprite has priority */; }
			}
			else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; }
			break;
		}

		case 0x400:
		{
			if (!(*pixdat & 0x8))
			{
				if (*pixdat & 0x7) { *dstptr = *pixdat; }
				else if (!(*pixdat2 & 0x8))
				{
					if (*pixdat2 & 0x7) { *dstptr = *pixdat2;; }
				}
				else if (*pixdat2 & 0x8) { /* sprite has priority */; }
			}
			else if (!(*pixdat2 & 0x8))
			{
				if (*pixdat2 & 0x7) { *dstptr = *pixdat2;; }
				else if (*pixdat & 0x8) { /* sprite has priority */; }
			}
			else if (*pixdat & 0x8) { /* sprite has priority */; }
			break;
		}

		case 0x800:
		{
			if (!(*pixdat & 0x8))
			{
				if (*pixdat & 0x7) { *dstptr = *pixdat; }
				else if (!(*pixdat2 & 0x8))
				{
					if (*pixdat2 & 0x7) { /* sprite has priority */; }
				}
				else if (*pixdat2 & 0x8) { /* sprite has priority */; }
			}
			else if (*pixdat & 0x8) { /* sprite has priority */; }
			break;
		}

		case 0xc00:
		{
			if (!(*pixdat & 0x8))
			{
				if (*pixdat & 0x7) { *dstptr = *pixdat; }
				else if (!(*pixdat2 & 0x8))
				{
					if (*pixdat2 & 0x7) { /* sprite has priority */; }
				}
				else if (*pixdat2 & 0x8) { /* sprite has priority */; }
			}
			else if (*pixdat & 0x8) { /* sprite has priority */; }
			break;
		}
		}
		break;
	}

	case 0x800:
	{
		switch (*pixdat2 & 0xc00)
		{

		case 0x000:
		{
			if (!(*pixdat2 & 0x8))
			{
				if (*pixdat2 & 0x7) { *dstptr = *pixdat2; }
				else if (!(*pixdat & 0x8))
				{
					if (*pixdat & 0x7) { /* sprite has priority */ }
				}
				else if (*pixdat & 0x8) { /* sprite has priority */; }
			}
			else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; }
			break;
		}

		case 0x400:
		{
			if (!(*pixdat2 & 0x8))
			{
				if (*pixdat2 & 0x7) { *dstptr = *pixdat2;; }
				else if (!(*pixdat & 0x8))
				{
					if (*pixdat & 0x7) { /* sprite has priority */; }
				}
				else if (*pixdat & 0x8) { /* sprite has priority */; }
			}
			else if (*pixdat2 & 0x8) { /* sprite has priority */; }
			break;
		}

		case 0x800:
		{
			if (!(*pixdat & 0x8))
			{
				if (*pixdat & 0x7) { /* sprite has priority */; }
				else if (!(*pixdat2 & 0x8))
				{
					if (*pixdat2 & 0x7) { /* sprite has priority */; }
				}
				else if (*pixdat & 0x8) { /* sprite has priority */; }
				else if (*pixdat2 & 0x8) { /* sprite has priority */; }
			}
			else if (!(*pixdat2 & 0x8))
			{
				if (*pixdat2 & 0x7) { /* sprite has priority */; }
				else if (*pixdat & 0x8) { /* sprite has priority */; }
			}
			else if (*pixdat & 0x8) { /* sprite has priority */; }

			break;
		}

		case 0xc00:
		{

			if (!(*pixdat & 0x8))
			{
				if (*pixdat & 0x7) { /* sprite has priority */; }
				else if (!(*pixdat2 & 0x8))
				{
					if (*pixdat2 & 0x7) { /* sprite has priority */; }
				}
				else if (*pixdat2 & 0x8) { /* sprite has priority */; }
			}
			else if (*pixdat & 0x8) { /* sprite has priority */; }

			break;
		}
		}
		break;
	}

	case 0xc00:
	{
		switch (*pixdat2 & 0xc00)
		{

		case 0x000:
		{
			if (!(*pixdat2 & 0x8))
			{
				if (*pixdat2 & 0x7) { *dstptr = *pixdat2; }
				else if (!(*pixdat & 0x8))
				{
					if (*pixdat & 0x7) { /* sprite has priority */; }
				}
				else if (*pixdat & 0x8) { /* sprite has priority */; }
			}
			else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; }
			break;
		}

		case 0x400:
		{
			if (!(*pixdat2 & 0x8))
			{
				if (*pixdat2 & 0x7) { *dstptr = *pixdat2;; }
				else if (!(*pixdat & 0x8))
				{
					if (*pixdat & 0x7) { /* sprite has priority */; }
				}
				else if (*pixdat & 0x8) { /* sprite has priority */; }
			}
			else if (*pixdat2 & 0x8) { /* sprite has priority */; }
			break;
		}

		case 0x800:
		{
			if (!(*pixdat2 & 0x8))
			{
				if (*pixdat2 & 0x7) { /* sprite has priority */; }
				else if (!(*pixdat & 0x8))
				{
					if (*pixdat & 0x7) { /* sprite has priority */; }
				}
				else if (*pixdat & 0x8) { /* sprite has priority */; }
			}
			else if (*pixdat2 & 0x8) { /* sprite has priority */; }
			break;
		}

		case 0xc00:
		{
			if (!(*pixdat & 0x8))
			{
				if (*pixdat & 0x7) { /* sprite has priority */; }
				else if (!(*pixdat2 & 0x8))
				{
					if (*pixdat2 & 0x7) { /* sprite has priority */; }
				}
				else if (*pixdat2 & 0x8) { /* sprite has priority */; }
			}
			else if (!(*pixdat2 & 0x8))
			{
				if (*pixdat2 & 0x7) { /* sprite has priority */; }
				else if (*pixdat & 0x8) { /* sprite has priority */; }
			}
			else if (*pixdat & 0x8) { /* sprite has priority */; }
			break;
		}
		}
		break;
	}
	}
}



inline void mix_above0_1_2_4(uint16_t* dstptr, uint16_t* pixdat, uint16_t* pixdat2)
{
	switch (*pixdat & 0xc00)
	{
	case 0x000:
	{
		switch (*pixdat2 & 0xc00)
		{

		case 0x000:
		{
			if (!(*pixdat & 0x8))
			{
				if (*pixdat & 0x7) { *dstptr = *pixdat; }
				else if (!(*pixdat2 & 0x8))
				{
					if (*pixdat2 & 0x7) { *dstptr = *pixdat2; }
					else if (*pixdat & 0x8) { /* sprite has priority */ }
				}
				else if (*pixdat2 & 0x8) { /* sprite has priority */; }
			}
			else if (!(*pixdat2 & 0x8))
			{
				if (*pixdat2 & 0x7) { *dstptr = *pixdat2; }
				else if (*pixdat & 0x8) { /* sprite has priority */ }
			}
			else if (*pixdat & 0x8) { /* sprite has priority */ }
			break;
		}

		case 0x400:
		{
			if (!(*pixdat & 0x8))
			{
				if (*pixdat & 0x7) { *dstptr = *pixdat; }
				else if (*pixdat & 0x8) { /* sprite has priority */ }
				else if (!(*pixdat2 & 0x8))
				{
					if (*pixdat2 & 0x7) { /* sprite has priority */; }
				}
				else if (*pixdat2 & 0x8) { /* sprite has priority */; }
			}
			else if (*pixdat & 0x8) { /* sprite has priority */ }
			break;
		}

		case 0x800:
		{
			if (!(*pixdat & 0x8))
			{
				if (*pixdat & 0x7) { *dstptr = *pixdat; }
				else if (!(*pixdat2 & 0x8))
				{
					if (*pixdat2 & 0x7) { /* sprite has priority */; }
				}
				else if (*pixdat2 & 0x8) { /* sprite has priority */; }
			}
			else if (*pixdat & 0x8) { /* sprite has priority */ }
			break;
		}

		case 0xc00:
		{
			if (!(*pixdat & 0x8))
			{
				if (*pixdat & 0x7) { *dstptr = *pixdat; }
				else if (!(*pixdat2 & 0x8))
				{
					if (*pixdat2 & 0x7) { /* sprite has priority */; }
				}
				else if (*pixdat2 & 0x8) { /* sprite has priority */; }
			}
			else if (*pixdat & 0x8) { /* sprite has priority */ }
			break;
		}
		}
		break;
	}

	case 0x400:
	{
		switch (*pixdat2 & 0xc00)
		{
		case 0x000:
		{
			if (!(*pixdat2 & 0x8))
			{
				if (*pixdat2 & 0x7) { *dstptr = *pixdat2; }
				else if (!(*pixdat & 0x8))
				{
					if (*pixdat & 0x7) { /* sprite has priority */ }
				}
				else if (*pixdat & 0x8) { /* sprite has priority */; }
			}
			else if (*pixdat2 & 0x8) { /* sprite has priority */; }
			break;
		}

		case 0x400:
		{
			if (!(*pixdat & 0x8))
			{
				if (*pixdat & 0x7) { /* sprite has priority */ }
				else if (!(*pixdat2 & 0x8))
				{
					if (*pixdat2 & 0x7) { /* sprite has priority */; }
				}
				else if (*pixdat2 & 0x8) { /* sprite has priority */; }
			}
			else if (!(*pixdat2 & 0x8))
			{
				if (*pixdat2 & 0x7) { /* sprite has priority */; }
				else if (*pixdat & 0x8) { /* sprite has priority */; }
			}
			else if (*pixdat & 0x8) { /* sprite has priority */; }
			break;
		}

		case 0x800:
		{
			if (!(*pixdat & 0x8))
			{
				if (*pixdat & 0x7) { /* sprite has priority */ }
				else if (!(*pixdat2 & 0x8))
				{
					if (*pixdat2 & 0x7) { /* sprite has priority */; }
				}
				else if (*pixdat2 & 0x8) { /* sprite has priority */; }
			}
			else if (*pixdat & 0x8) { /* sprite has priority */; }
			break;
		}

		case 0xc00:
		{
			if (!(*pixdat & 0x8))
			{
				if (*pixdat & 0x7) { /* sprite has priority */ }
				else if (!(*pixdat2 & 0x8))
				{
					if (*pixdat2 & 0x7) { /* sprite has priority */; }
				}
				else if (*pixdat2 & 0x8) { /* sprite has priority */; }
			}
			else if (*pixdat & 0x8) { /* sprite has priority */; }
			break;
		}
		}
		break;
	}

	case 0x800:
	{
		switch (*pixdat2 & 0xc00)
		{

		case 0x000:
		{
			if (!(*pixdat2 & 0x8))
			{
				if (*pixdat2 & 0x7) { *dstptr = *pixdat2; }
				else if (!(*pixdat & 0x8))
				{
					if (*pixdat & 0x7) { /* sprite has priority */ }
				}
				else if (*pixdat & 0x8) { /* sprite has priority */; }
			}
			else if (*pixdat2 & 0x8) { /* sprite has priority */; }
			break;
		}

		case 0x400:
		{
			if (!(*pixdat2 & 0x8))
			{
				if (*pixdat2 & 0x7) { /* sprite has priority */; }
				else if (!(*pixdat & 0x8))
				{
					if (*pixdat & 0x7) { /* sprite has priority */; }
				}
				else if (*pixdat & 0x8) { /* sprite has priority */; }
			}
			else if (*pixdat2 & 0x8) { /* sprite has priority */; }
			break;
		}

		case 0x800:
		{
			if (!(*pixdat & 0x8))
			{
				if (*pixdat & 0x7) { /* sprite has priority */; }
				else if (!(*pixdat2 & 0x8))
				{
					if (*pixdat2 & 0x7) { /* sprite has priority */; }
				}
				else if (*pixdat & 0x8) { /* sprite has priority */; }
				else if (*pixdat2 & 0x8) { /* sprite has priority */; }
			}
			else if (!(*pixdat2 & 0x8))
			{
				if (*pixdat2 & 0x7) { /* sprite has priority */; }
				else if (*pixdat & 0x8) { /* sprite has priority */; }
			}
			else if (*pixdat & 0x8) { /* sprite has priority */; }

			break;
		}

		case 0xc00:
		{

			if (!(*pixdat & 0x8))
			{
				if (*pixdat & 0x7) { /* sprite has priority */; }
				else if (!(*pixdat2 & 0x8))
				{
					if (*pixdat2 & 0x7) { /* sprite has priority */; }
				}
				else if (*pixdat2 & 0x8) { /* sprite has priority */; }
			}
			else if (*pixdat & 0x8) { /* sprite has priority */; }

			break;
		}
		}
		break;
	}

	case 0xc00:
	{
		switch (*pixdat2 & 0xc00)
		{

		case 0x000:
		{
			if (!(*pixdat2 & 0x8))
			{
				if (*pixdat2 & 0x7) { *dstptr = *pixdat2; }
				else if (!(*pixdat & 0x8))
				{
					if (*pixdat & 0x7) { /* sprite has priority */; }
				}
				else if (*pixdat & 0x8) { /* sprite has priority */; }
			}
			else if (*pixdat2 & 0x8) { /* sprite has priority */; }
			break;
		}

		case 0x400:
		{
			if (!(*pixdat2 & 0x8))
			{
				if (*pixdat2 & 0x7) { /* sprite has priority */; }
				else if (!(*pixdat & 0x8))
				{
					if (*pixdat & 0x7) { /* sprite has priority */; }
				}
				else if (*pixdat & 0x8) { /* sprite has priority */; }
			}
			else if (*pixdat2 & 0x8) { /* sprite has priority */; }
			break;
		}

		case 0x800:
		{
			if (!(*pixdat2 & 0x8))
			{
				if (*pixdat2 & 0x7) { /* sprite has priority */; }
				else if (!(*pixdat & 0x8))
				{
					if (*pixdat & 0x7) { /* sprite has priority */; }
				}
				else if (*pixdat & 0x8) { /* sprite has priority */; }
			}
			else if (*pixdat2 & 0x8) { /* sprite has priority */; }
			break;
		}

		case 0xc00:
		{
			if (!(*pixdat & 0x8))
			{
				if (*pixdat & 0x7) { /* sprite has priority */; }
				else if (!(*pixdat2 & 0x8))
				{
					if (*pixdat2 & 0x7) { /* sprite has priority */; }
				}
				else if (*pixdat2 & 0x8) { /* sprite has priority */; }
			}
			else if (!(*pixdat2 & 0x8))
			{
				if (*pixdat2 & 0x7) { /* sprite has priority */; }
				else if (*pixdat & 0x8) { /* sprite has priority */; }
			}
			else if (*pixdat & 0x8) { /* sprite has priority */; }
			break;
		}
		}
		break;
	}
	}
}


UINT32 gaelco_state::screen_update_bigkarnk(screen_device& screen, bitmap_ind16& bitmap, const rectangle& cliprect)
{
	/* set scroll registers */
	m_tilemap[0]->set_scrolly(0, m_vregs[0]);
	m_tilemap[0]->set_scrollx(0, m_vregs[1] + 4);
	m_tilemap[1]->set_scrolly(0, m_vregs[2]);
	m_tilemap[1]->set_scrollx(0, m_vregs[3]);

	bitmap.fill(0, cliprect);

	draw_sprites(screen, bitmap, cliprect);

	bitmap_ind16& tmb = m_tilemap[0]->pixmap();
	bitmap_ind16& tmb2 = m_tilemap[1]->pixmap();

	UINT16* srcptr;
	UINT16* srcptr2;

	int scrollx = m_tilemap[0]->scrollx(0);
	int scrolly = m_tilemap[0]->scrolly(0);
	int scrollx2 = m_tilemap[1]->scrollx(0);
	int scrolly2 = m_tilemap[1]->scrolly(0);

	for (int y = cliprect.min_y; y <= cliprect.max_y; y++)
	{
		int realy = (y + scrolly) & 0x1ff;
		int realy2 = (y + scrolly2) & 0x1ff;

		srcptr = &tmb.pix16(realy);
		srcptr2 = &tmb2.pix16(realy2);

		UINT16* dstptrx = &bitmap.pix16(y);
		int realx = (scrollx) & 0x1ff;
		int realx2 = (scrollx2) & 0x1ff;

		UINT16* dstptr = &dstptrx[cliprect.min_x];
		UINT16* dstend = &dstptrx[cliprect.max_x + 1];

		while (dstptr != dstend)
		{
			uint16_t* pixdat = srcptr + ((realx++) & 0x1ff);
			uint16_t* pixdat2 = srcptr2 + ((realx2++) & 0x1ff);

			//  0 =  no sprites drawn here
			//  400    priority = 0  above 0
			//  800    priority = 1  above 0,1
			//  c00    priority = 2  above 0,1,2
			//  1000   priority = 3  above 0,1,2,4
			//  1400   priority = 4  above 0,1,2,4,8

			switch (*dstptr & 0x1c00)
			{
			default:
			{
				mix_normal(dstptr, pixdat, pixdat2);  // sprites are above nothing
			}
			break;

			case 0x400:
			{
				mix_above0_1_2_4(dstptr, pixdat, pixdat2);  // sprites are above 0,1,2
			}
			break;

			case 0x800:
			{
				mix_above0_1_2(dstptr, pixdat, pixdat2);  // sprites are above 0,1,2
			}
			break;

			case 0xc00:
			{
				mix_above0_1(dstptr, pixdat, pixdat2);  // sprites are above 0,1,2
			}
			break;

			case 0x1000:
			{
				mix_above0(dstptr, pixdat, pixdat2);  // sprites are above 0,1,2,4
			}
			break;

			case 0x1400: // Highest priority '4' sprites, no possibility of BG rendering
			{
				// sprites are above all
			}
			break;

			}

			//realx++;
			//realx2++;
			dstptr++;
		}
	}

	return 0;
}


inline void mix_normal_thoop(uint16_t* dstptr, uint16_t* pixdat, uint16_t* pixdat2)
{
	switch (*pixdat2 & 0xc00)
	{
	case 0x000:
	{
		switch (*pixdat & 0xc00)
		{

		case 0x000:
		{
			if (!(*pixdat & 0x8)) { if (*pixdat & 0x7) { *dstptr = *pixdat; } else if (*pixdat & 0x8) { *dstptr = *pixdat; } /* 2nd */	else if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { *dstptr = *pixdat2;; } else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; }  /* 4th */ }  /* 3rd */	else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; }  /* 4th */ } /* 1st */
			else if (*pixdat & 0x8) { *dstptr = *pixdat; } /* 2nd */	else if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { *dstptr = *pixdat2;; } else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; }  /* 4th */ }  /* 3rd */	else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; }  /* 4th */

			break;
		}

		case 0x400:
		{
			if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { *dstptr = *pixdat2;; } else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; } /* 2nd */	else if (!(*pixdat & 0x8)) { if (*pixdat & 0x7) { *dstptr = *pixdat;; } else if (*pixdat & 0x8) { *dstptr = *pixdat;; }  /* 4th */ }  /* 3rd */	else if (*pixdat & 0x8) { *dstptr = *pixdat;; }  /* 4th */ } /* 1st */
			else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; } /* 2nd */	else if (!(*pixdat & 0x8)) { if (*pixdat & 0x7) { *dstptr = *pixdat;; } else if (*pixdat & 0x8) { *dstptr = *pixdat;; }  /* 4th */ }  /* 3rd */	else if (*pixdat & 0x8) { *dstptr = *pixdat;; }  /* 4th */

			break;
		}

		case 0x800:
		{
			if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { *dstptr = *pixdat2;; } else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; } /* 2nd */	else if (!(*pixdat & 0x8)) { if (*pixdat & 0x7) { *dstptr = *pixdat;; } else if (*pixdat & 0x8) { *dstptr = *pixdat;; }  /* 4th */ }  /* 3rd */	else if (*pixdat & 0x8) { *dstptr = *pixdat;; }  /* 4th */ } /* 1st */
			else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; } /* 2nd */	else if (!(*pixdat & 0x8)) { if (*pixdat & 0x7) { *dstptr = *pixdat;; } else if (*pixdat & 0x8) { *dstptr = *pixdat;; }  /* 4th */ }  /* 3rd */	else if (*pixdat & 0x8) { *dstptr = *pixdat;; }  /* 4th */

			break;
		}

		case 0xc00:
		{
			if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { *dstptr = *pixdat2;; } else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; } /* 2nd */	else if (!(*pixdat & 0x8)) { if (*pixdat & 0x7) { *dstptr = *pixdat;; } else if (*pixdat & 0x8) { *dstptr = *pixdat;; }  /* 4th */ }  /* 3rd */	else if (*pixdat & 0x8) { *dstptr = *pixdat;; }  /* 4th */ } /* 1st */
			else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; } /* 2nd */	else if (!(*pixdat & 0x8)) { if (*pixdat & 0x7) { *dstptr = *pixdat;; } else if (*pixdat & 0x8) { *dstptr = *pixdat;; }  /* 4th */ }  /* 3rd */	else if (*pixdat & 0x8) { *dstptr = *pixdat;; }  /* 4th */

			break;
		}
		}
		break;
	}

	case 0x400:
	{
		switch (*pixdat & 0xc00)
		{
		case 0x000:
		{
			if (!(*pixdat & 0x8)) { if (*pixdat & 0x7) { *dstptr = *pixdat; } else if (*pixdat & 0x8) { *dstptr = *pixdat; } /* 2nd */	else if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { *dstptr = *pixdat2;; } else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; }  /* 4th */ }  /* 3rd */	else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; }  /* 4th */ } /* 1st */
			else if (*pixdat & 0x8) { *dstptr = *pixdat; } /* 2nd */	else if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { *dstptr = *pixdat2;; } else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; }  /* 4th */ }  /* 3rd */	else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; }  /* 4th */

			break;
		}

		case 0x400:
		{
			if (!(*pixdat & 0x8)) { if (*pixdat & 0x7) { *dstptr = *pixdat;; } else if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { *dstptr = *pixdat2;; } else if (*pixdat & 0x8) { *dstptr = *pixdat;; }  /* 3rd */	else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; }  /* 4th */ } /* 2nd */	else if (*pixdat & 0x8) { *dstptr = *pixdat;; }  /* 3rd */	else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; }  /* 4th */ } /* 1st */
			else if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { *dstptr = *pixdat2;; } else if (*pixdat & 0x8) { *dstptr = *pixdat;; }  /* 3rd */	else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; }  /* 4th */ } /* 2nd */	else if (*pixdat & 0x8) { *dstptr = *pixdat;; }  /* 3rd */	else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; }  /* 4th */

			break;
		}

		case 0x800:
		{
			if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { *dstptr = *pixdat2;; } else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; } /* 2nd */	else if (!(*pixdat & 0x8)) { if (*pixdat & 0x7) { *dstptr = *pixdat;; } else if (*pixdat & 0x8) { *dstptr = *pixdat;; }  /* 4th */ }  /* 3rd */	else if (*pixdat & 0x8) { *dstptr = *pixdat;; }  /* 4th */ } /* 1st */
			else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; } /* 2nd */	else if (!(*pixdat & 0x8)) { if (*pixdat & 0x7) { *dstptr = *pixdat;; } else if (*pixdat & 0x8) { *dstptr = *pixdat;; }  /* 4th */ }  /* 3rd */	else if (*pixdat & 0x8) { *dstptr = *pixdat;; }  /* 4th */

			break;
		}

		case 0xc00:
		{
			if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { *dstptr = *pixdat2;; } else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; } /* 2nd */	else if (!(*pixdat & 0x8)) { if (*pixdat & 0x7) { *dstptr = *pixdat;; } else if (*pixdat & 0x8) { *dstptr = *pixdat;; }  /* 4th */ }  /* 3rd */	else if (*pixdat & 0x8) { *dstptr = *pixdat;; }  /* 4th */ } /* 1st */
			else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; } /* 2nd */	else if (!(*pixdat & 0x8)) { if (*pixdat & 0x7) { *dstptr = *pixdat;; } else if (*pixdat & 0x8) { *dstptr = *pixdat;; }  /* 4th */ }  /* 3rd */	else if (*pixdat & 0x8) { *dstptr = *pixdat;; }  /* 4th */


			break;
		}
		}
		break;
	}

	case 0x800:
	{
		switch (*pixdat & 0xc00)
		{

		case 0x000:
		{
			if (!(*pixdat & 0x8)) { if (*pixdat & 0x7) { *dstptr = *pixdat; } else if (*pixdat & 0x8) { *dstptr = *pixdat; } /* 2nd */	else if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { *dstptr = *pixdat2;; } else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; }  /* 4th */ }  /* 3rd */	else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; }  /* 4th */ } /* 1st */
			else if (*pixdat & 0x8) { *dstptr = *pixdat; } /* 2nd */	else if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { *dstptr = *pixdat2;; } else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; }  /* 4th */ }  /* 3rd */	else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; }  /* 4th */


			break;
		}

		case 0x400:
		{
			if (!(*pixdat & 0x8)) { if (*pixdat & 0x7) { *dstptr = *pixdat;; } else if (*pixdat & 0x8) { *dstptr = *pixdat;; } /* 2nd */	else if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { *dstptr = *pixdat2;; } else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; }  /* 4th */ }  /* 3rd */	else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; }  /* 4th */ } /* 1st */
			else if (*pixdat & 0x8) { *dstptr = *pixdat;; } /* 2nd */	else if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { *dstptr = *pixdat2;; } else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; }  /* 4th */ }  /* 3rd */	else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; }  /* 4th */

			break;
		}

		case 0x800:
		{
			if (!(*pixdat & 0x8)) { if (*pixdat & 0x7) { *dstptr = *pixdat;; } else if (*pixdat & 0x8) { *dstptr = *pixdat;; } /* 2nd */	else if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { *dstptr = *pixdat2;; } else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; }  /* 4th */ }  /* 3rd */	else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; }  /* 4th */ } /* 1st */
			else if (*pixdat & 0x8) { *dstptr = *pixdat;; } /* 2nd */	else if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { *dstptr = *pixdat2;; } else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; }  /* 4th */ }  /* 3rd */	else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; }  /* 4th */


			break;
		}

		case 0xc00:
		{

			if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { *dstptr = *pixdat2;; } else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; } /* 2nd */	else if (!(*pixdat & 0x8)) { if (*pixdat & 0x7) { *dstptr = *pixdat;; } else if (*pixdat & 0x8) { *dstptr = *pixdat;; }  /* 4th */ }  /* 3rd */	else if (*pixdat & 0x8) { *dstptr = *pixdat;; }  /* 4th */ } /* 1st */
			else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; } /* 2nd */	else if (!(*pixdat & 0x8)) { if (*pixdat & 0x7) { *dstptr = *pixdat;; } else if (*pixdat & 0x8) { *dstptr = *pixdat;; }  /* 4th */ }  /* 3rd */	else if (*pixdat & 0x8) { *dstptr = *pixdat;; }  /* 4th */


			break;
		}
		}
		break;
	}

	case 0xc00:
	{
		switch (*pixdat & 0xc00)
		{

		case 0x000:
		{
			if (!(*pixdat & 0x8)) { if (*pixdat & 0x7) { *dstptr = *pixdat; } else if (*pixdat & 0x8) { *dstptr = *pixdat; } /* 2nd */	else if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { *dstptr = *pixdat2;; } else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; }  /* 4th */ }  /* 3rd */	else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; }  /* 4th */ } /* 1st */
			else if (*pixdat & 0x8) { *dstptr = *pixdat; } /* 2nd */	else if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { *dstptr = *pixdat2;; } else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; }  /* 4th */ }  /* 3rd */	else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; }  /* 4th */

			break;
		}

		case 0x400:
		{
			if (!(*pixdat & 0x8)) { if (*pixdat & 0x7) { *dstptr = *pixdat;; } else if (*pixdat & 0x8) { *dstptr = *pixdat;; } /* 2nd */	else if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { *dstptr = *pixdat2;; } else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; } }  /* 3rd */	else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; }  /* 4th */ } /* 1st */
			else if (*pixdat & 0x8) { *dstptr = *pixdat;; } /* 2nd */	else if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { *dstptr = *pixdat2;; } else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; } }  /* 3rd */	else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; }  /* 4th */

			break;
		}

		case 0x800:
		{
			if (!(*pixdat & 0x8)) { if (*pixdat & 0x7) { *dstptr = *pixdat;; } else if (*pixdat & 0x8) { *dstptr = *pixdat;; } /* 2nd */	else if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { *dstptr = *pixdat2;; } else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; } }  /* 3rd */	else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; }  /* 4th */ } /* 1st */
			else if (*pixdat & 0x8) { *dstptr = *pixdat;; } /* 2nd */	else if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { *dstptr = *pixdat2;; } else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; } }  /* 3rd */	else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; }  /* 4th */

			break;
		}

		case 0xc00:
		{
			if (!(*pixdat & 0x8)) { if (*pixdat & 0x7) { *dstptr = *pixdat;; } else if (*pixdat & 0x8) { *dstptr = *pixdat;; }  /* 2nd */	else if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { *dstptr = *pixdat2;; } else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; }  /* 4th */ }  /* 3rd */	else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; }  /* 4th */ }  /* 1st */
			else if (*pixdat & 0x8) { *dstptr = *pixdat;; }  /* 2nd */	else if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { *dstptr = *pixdat2;; } else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; }  /* 4th */ }  /* 3rd */	else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; }  /* 4th */

			break;
		}
		}
		break;
	}
	}
}



inline void mix_above0_thoop(uint16_t* dstptr, uint16_t* pixdat, uint16_t* pixdat2)
{
	switch (*pixdat2 & 0xc00)
	{
	case 0x000:
	{
		switch (*pixdat & 0xc00)
		{

		case 0x000:
		{
			if (!(*pixdat & 0x8)) { if (*pixdat & 0x7) { *dstptr = *pixdat; } else if (*pixdat & 0x8) { *dstptr = *pixdat; } /* 2nd */	else if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { *dstptr = *pixdat2;; } else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; }  /* 4th */ }  /* 3rd */	else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; }  /* 4th */ } /* 1st */
			else if (*pixdat & 0x8) { *dstptr = *pixdat; } /* 2nd */	else if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { *dstptr = *pixdat2;; } else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; }  /* 4th */ }  /* 3rd */	else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; }  /* 4th */

			break;
		}

		case 0x400:
		{
			if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { *dstptr = *pixdat2;; } else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; } /* 2nd */	else if (!(*pixdat & 0x8)) { if (*pixdat & 0x7) { *dstptr = *pixdat;; } else if (*pixdat & 0x8) { *dstptr = *pixdat;; }  /* 4th */ }  /* 3rd */	else if (*pixdat & 0x8) { *dstptr = *pixdat;; }  /* 4th */ } /* 1st */
			else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; } /* 2nd */	else if (!(*pixdat & 0x8)) { if (*pixdat & 0x7) { *dstptr = *pixdat;; } else if (*pixdat & 0x8) { *dstptr = *pixdat;; }  /* 4th */ }  /* 3rd */	else if (*pixdat & 0x8) { *dstptr = *pixdat;; }  /* 4th */

			break;
		}

		case 0x800:
		{
			if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { *dstptr = *pixdat2;; } else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; } /* 2nd */	else if (!(*pixdat & 0x8)) { if (*pixdat & 0x7) { *dstptr = *pixdat;; } else if (*pixdat & 0x8) { *dstptr = *pixdat;; }  /* 4th */ }  /* 3rd */	else if (*pixdat & 0x8) { *dstptr = *pixdat;; }  /* 4th */ } /* 1st */
			else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; } /* 2nd */	else if (!(*pixdat & 0x8)) { if (*pixdat & 0x7) { *dstptr = *pixdat;; } else if (*pixdat & 0x8) { *dstptr = *pixdat;; }  /* 4th */ }  /* 3rd */	else if (*pixdat & 0x8) { *dstptr = *pixdat;; }  /* 4th */

			break;
		}

		case 0xc00:
		{
			if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { *dstptr = *pixdat2;; } else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; } /* 2nd */	else if (!(*pixdat & 0x8)) { if (*pixdat & 0x7) { /* sprite has priority */; } else if (*pixdat & 0x8) { /* sprite has priority */; }  /* 4th */ }  /* 3rd */	else if (*pixdat & 0x8) { /* sprite has priority */; }  /* 4th */ } /* 1st */
			else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; } /* 2nd */	else if (!(*pixdat & 0x8)) { if (*pixdat & 0x7) { /* sprite has priority */; } else if (*pixdat & 0x8) { /* sprite has priority */; }  /* 4th */ }  /* 3rd */	else if (*pixdat & 0x8) { /* sprite has priority */; }  /* 4th */

			break;
		}
		}
		break;
	}

	case 0x400:
	{
		switch (*pixdat & 0xc00)
		{
		case 0x000:
		{
			if (!(*pixdat & 0x8)) { if (*pixdat & 0x7) { *dstptr = *pixdat; } else if (*pixdat & 0x8) { *dstptr = *pixdat; } /* 2nd */	else if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { *dstptr = *pixdat2;; } else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; }  /* 4th */ }  /* 3rd */	else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; }  /* 4th */ } /* 1st */
			else if (*pixdat & 0x8) { *dstptr = *pixdat; } /* 2nd */	else if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { *dstptr = *pixdat2;; } else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; }  /* 4th */ }  /* 3rd */	else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; }  /* 4th */

			break;
		}

		case 0x400:
		{
			if (!(*pixdat & 0x8)) { if (*pixdat & 0x7) { *dstptr = *pixdat;; } else if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { *dstptr = *pixdat2;; } else if (*pixdat & 0x8) { *dstptr = *pixdat;; }  /* 3rd */	else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; }  /* 4th */ } /* 2nd */	else if (*pixdat & 0x8) { *dstptr = *pixdat;; }  /* 3rd */	else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; }  /* 4th */ } /* 1st */
			else if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { *dstptr = *pixdat2;; } else if (*pixdat & 0x8) { *dstptr = *pixdat;; }  /* 3rd */	else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; }  /* 4th */ } /* 2nd */	else if (*pixdat & 0x8) { *dstptr = *pixdat;; }  /* 3rd */	else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; }  /* 4th */

			break;
		}

		case 0x800:
		{
			if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { *dstptr = *pixdat2;; } else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; } /* 2nd */	else if (!(*pixdat & 0x8)) { if (*pixdat & 0x7) { *dstptr = *pixdat;; } else if (*pixdat & 0x8) { *dstptr = *pixdat;; }  /* 4th */ }  /* 3rd */	else if (*pixdat & 0x8) { *dstptr = *pixdat;; }  /* 4th */ } /* 1st */
			else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; } /* 2nd */	else if (!(*pixdat & 0x8)) { if (*pixdat & 0x7) { *dstptr = *pixdat;; } else if (*pixdat & 0x8) { *dstptr = *pixdat;; }  /* 4th */ }  /* 3rd */	else if (*pixdat & 0x8) { *dstptr = *pixdat;; }  /* 4th */

			break;
		}

		case 0xc00:
		{
			if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { *dstptr = *pixdat2;; } else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; } /* 2nd */	else if (!(*pixdat & 0x8)) { if (*pixdat & 0x7) { /* sprite has priority */; } else if (*pixdat & 0x8) { /* sprite has priority */; }  /* 4th */ }  /* 3rd */	else if (*pixdat & 0x8) { /* sprite has priority */; }  /* 4th */ } /* 1st */
			else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; } /* 2nd */	else if (!(*pixdat & 0x8)) { if (*pixdat & 0x7) { /* sprite has priority */; } else if (*pixdat & 0x8) { /* sprite has priority */; }  /* 4th */ }  /* 3rd */	else if (*pixdat & 0x8) { /* sprite has priority */; }  /* 4th */


			break;
		}
		}
		break;
	}

	case 0x800:
	{
		switch (*pixdat & 0xc00)
		{

		case 0x000:
		{
			if (!(*pixdat & 0x8)) { if (*pixdat & 0x7) { *dstptr = *pixdat; } else if (*pixdat & 0x8) { *dstptr = *pixdat; } /* 2nd */	else if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { *dstptr = *pixdat2;; } else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; }  /* 4th */ }  /* 3rd */	else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; }  /* 4th */ } /* 1st */
			else if (*pixdat & 0x8) { *dstptr = *pixdat; } /* 2nd */	else if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { *dstptr = *pixdat2;; } else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; }  /* 4th */ }  /* 3rd */	else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; }  /* 4th */


			break;
		}

		case 0x400:
		{
			if (!(*pixdat & 0x8)) { if (*pixdat & 0x7) { *dstptr = *pixdat;; } else if (*pixdat & 0x8) { *dstptr = *pixdat;; } /* 2nd */	else if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { *dstptr = *pixdat2;; } else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; }  /* 4th */ }  /* 3rd */	else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; }  /* 4th */ } /* 1st */
			else if (*pixdat & 0x8) { *dstptr = *pixdat;; } /* 2nd */	else if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { *dstptr = *pixdat2;; } else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; }  /* 4th */ }  /* 3rd */	else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; }  /* 4th */

			break;
		}

		case 0x800:
		{
			if (!(*pixdat & 0x8)) { if (*pixdat & 0x7) { *dstptr = *pixdat;; } else if (*pixdat & 0x8) { *dstptr = *pixdat;; } /* 2nd */	else if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { *dstptr = *pixdat2;; } else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; }  /* 4th */ }  /* 3rd */	else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; }  /* 4th */ } /* 1st */
			else if (*pixdat & 0x8) { *dstptr = *pixdat;; } /* 2nd */	else if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { *dstptr = *pixdat2;; } else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; }  /* 4th */ }  /* 3rd */	else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; }  /* 4th */


			break;
		}

		case 0xc00:
		{

			if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { *dstptr = *pixdat2;; } else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; } /* 2nd */	else if (!(*pixdat & 0x8)) { if (*pixdat & 0x7) { /* sprite has priority */; } else if (*pixdat & 0x8) { /* sprite has priority */; }  /* 4th */ }  /* 3rd */	else if (*pixdat & 0x8) { /* sprite has priority */; }  /* 4th */ } /* 1st */
			else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; } /* 2nd */	else if (!(*pixdat & 0x8)) { if (*pixdat & 0x7) { /* sprite has priority */; } else if (*pixdat & 0x8) { /* sprite has priority */; }  /* 4th */ }  /* 3rd */	else if (*pixdat & 0x8) { /* sprite has priority */; }  /* 4th */


			break;
		}
		}
		break;
	}

	case 0xc00:
	{
		switch (*pixdat & 0xc00)
		{

		case 0x000:
		{
			if (!(*pixdat & 0x8)) { if (*pixdat & 0x7) { *dstptr = *pixdat; } else if (*pixdat & 0x8) { *dstptr = *pixdat; } /* 2nd */	else if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { /* sprite has priority */; } else if (*pixdat2 & 0x8) { /* sprite has priority */; }  /* 4th */ }  /* 3rd */	else if (*pixdat2 & 0x8) { /* sprite has priority */; }  /* 4th */ } /* 1st */
			else if (*pixdat & 0x8) { *dstptr = *pixdat; } /* 2nd */	else if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { /* sprite has priority */; } else if (*pixdat2 & 0x8) { /* sprite has priority */; }  /* 4th */ }  /* 3rd */	else if (*pixdat2 & 0x8) { /* sprite has priority */; }  /* 4th */

			break;
		}

		case 0x400:
		{
			if (!(*pixdat & 0x8)) { if (*pixdat & 0x7) { *dstptr = *pixdat;; } else if (*pixdat & 0x8) { *dstptr = *pixdat;; } /* 2nd */	else if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { /* sprite has priority */; } else if (*pixdat2 & 0x8) { /* sprite has priority */; } }  /* 3rd */	else if (*pixdat2 & 0x8) { /* sprite has priority */; }  /* 4th */ } /* 1st */
			else if (*pixdat & 0x8) { *dstptr = *pixdat;; } /* 2nd */	else if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { /* sprite has priority */; } else if (*pixdat2 & 0x8) { /* sprite has priority */; } }  /* 3rd */	else if (*pixdat2 & 0x8) { /* sprite has priority */; }  /* 4th */

			break;
		}

		case 0x800:
		{
			if (!(*pixdat & 0x8)) { if (*pixdat & 0x7) { *dstptr = *pixdat;; } else if (*pixdat & 0x8) { *dstptr = *pixdat;; } /* 2nd */	else if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { /* sprite has priority */; } else if (*pixdat2 & 0x8) { /* sprite has priority */; } }  /* 3rd */	else if (*pixdat2 & 0x8) { /* sprite has priority */; }  /* 4th */ } /* 1st */
			else if (*pixdat & 0x8) { *dstptr = *pixdat;; } /* 2nd */	else if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { /* sprite has priority */; } else if (*pixdat2 & 0x8) { /* sprite has priority */; } }  /* 3rd */	else if (*pixdat2 & 0x8) { /* sprite has priority */; }  /* 4th */

			break;
		}

		case 0xc00:
		{
			if (!(*pixdat & 0x8)) { if (*pixdat & 0x7) { /* sprite has priority */; } else if (*pixdat & 0x8) { /* sprite has priority */; }  /* 2nd */	else if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { /* sprite has priority */; } else if (*pixdat2 & 0x8) { /* sprite has priority */; }  /* 4th */ }  /* 3rd */	else if (*pixdat2 & 0x8) { /* sprite has priority */; }  /* 4th */ }  /* 1st */
			else if (*pixdat & 0x8) { /* sprite has priority */; }  /* 2nd */	else if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { /* sprite has priority */; } else if (*pixdat2 & 0x8) { /* sprite has priority */; }  /* 4th */ }  /* 3rd */	else if (*pixdat2 & 0x8) { /* sprite has priority */; }  /* 4th */

			break;
		}
		}
		break;
	}
	}
}


inline void mix_above0_1_thoop(uint16_t* dstptr, uint16_t* pixdat, uint16_t* pixdat2)
{
	switch (*pixdat2 & 0xc00)
	{
	case 0x000:
	{
		switch (*pixdat & 0xc00)
		{

		case 0x000:
		{
			if (!(*pixdat & 0x8)) { if (*pixdat & 0x7) { *dstptr = *pixdat; } else if (*pixdat & 0x8) { *dstptr = *pixdat; } /* 2nd */	else if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { *dstptr = *pixdat2;; } else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; }  /* 4th */ }  /* 3rd */	else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; }  /* 4th */ } /* 1st */
			else if (*pixdat & 0x8) { *dstptr = *pixdat; } /* 2nd */	else if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { *dstptr = *pixdat2;; } else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; }  /* 4th */ }  /* 3rd */	else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; }  /* 4th */

			break;
		}

		case 0x400:
		{
			if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { *dstptr = *pixdat2;; } else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; } /* 2nd */	else if (!(*pixdat & 0x8)) { if (*pixdat & 0x7) { *dstptr = *pixdat;; } else if (*pixdat & 0x8) { *dstptr = *pixdat;; }  /* 4th */ }  /* 3rd */	else if (*pixdat & 0x8) { *dstptr = *pixdat;; }  /* 4th */ } /* 1st */
			else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; } /* 2nd */	else if (!(*pixdat & 0x8)) { if (*pixdat & 0x7) { *dstptr = *pixdat;; } else if (*pixdat & 0x8) { *dstptr = *pixdat;; }  /* 4th */ }  /* 3rd */	else if (*pixdat & 0x8) { *dstptr = *pixdat;; }  /* 4th */

			break;
		}

		case 0x800:
		{
			if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { *dstptr = *pixdat2;; } else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; } /* 2nd */	else if (!(*pixdat & 0x8)) { if (*pixdat & 0x7) { /* sprite has priority */; } else if (*pixdat & 0x8) { /* sprite has priority */; }  /* 4th */ }  /* 3rd */	else if (*pixdat & 0x8) { /* sprite has priority */; }  /* 4th */ } /* 1st */
			else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; } /* 2nd */	else if (!(*pixdat & 0x8)) { if (*pixdat & 0x7) { /* sprite has priority */; } else if (*pixdat & 0x8) { /* sprite has priority */; }  /* 4th */ }  /* 3rd */	else if (*pixdat & 0x8) { /* sprite has priority */; }  /* 4th */

			break;
		}

		case 0xc00:
		{
			if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { *dstptr = *pixdat2;; } else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; } /* 2nd */	else if (!(*pixdat & 0x8)) { if (*pixdat & 0x7) { /* sprite has priority */; } else if (*pixdat & 0x8) { /* sprite has priority */; }  /* 4th */ }  /* 3rd */	else if (*pixdat & 0x8) { /* sprite has priority */; }  /* 4th */ } /* 1st */
			else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; } /* 2nd */	else if (!(*pixdat & 0x8)) { if (*pixdat & 0x7) { /* sprite has priority */; } else if (*pixdat & 0x8) { /* sprite has priority */; }  /* 4th */ }  /* 3rd */	else if (*pixdat & 0x8) { /* sprite has priority */; }  /* 4th */

			break;
		}
		}
		break;
	}

	case 0x400:
	{
		switch (*pixdat & 0xc00)
		{
		case 0x000:
		{
			if (!(*pixdat & 0x8)) { if (*pixdat & 0x7) { *dstptr = *pixdat; } else if (*pixdat & 0x8) { *dstptr = *pixdat; } /* 2nd */	else if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { *dstptr = *pixdat2;; } else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; }  /* 4th */ }  /* 3rd */	else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; }  /* 4th */ } /* 1st */
			else if (*pixdat & 0x8) { *dstptr = *pixdat; } /* 2nd */	else if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { *dstptr = *pixdat2;; } else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; }  /* 4th */ }  /* 3rd */	else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; }  /* 4th */

			break;
		}

		case 0x400:
		{
			if (!(*pixdat & 0x8)) { if (*pixdat & 0x7) { *dstptr = *pixdat;; } else if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { *dstptr = *pixdat2;; } else if (*pixdat & 0x8) { *dstptr = *pixdat;; }  /* 3rd */	else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; }  /* 4th */ } /* 2nd */	else if (*pixdat & 0x8) { *dstptr = *pixdat;; }  /* 3rd */	else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; }  /* 4th */ } /* 1st */
			else if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { *dstptr = *pixdat2;; } else if (*pixdat & 0x8) { *dstptr = *pixdat;; }  /* 3rd */	else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; }  /* 4th */ } /* 2nd */	else if (*pixdat & 0x8) { *dstptr = *pixdat;; }  /* 3rd */	else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; }  /* 4th */

			break;
		}

		case 0x800:
		{
			if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { *dstptr = *pixdat2;; } else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; } /* 2nd */	else if (!(*pixdat & 0x8)) { if (*pixdat & 0x7) { /* sprite has priority */; } else if (*pixdat & 0x8) { /* sprite has priority */; }  /* 4th */ }  /* 3rd */	else if (*pixdat & 0x8) { /* sprite has priority */; }  /* 4th */ } /* 1st */
			else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; } /* 2nd */	else if (!(*pixdat & 0x8)) { if (*pixdat & 0x7) { /* sprite has priority */; } else if (*pixdat & 0x8) { /* sprite has priority */; }  /* 4th */ }  /* 3rd */	else if (*pixdat & 0x8) { /* sprite has priority */; }  /* 4th */

			break;
		}

		case 0xc00:
		{
			if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { *dstptr = *pixdat2;; } else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; } /* 2nd */	else if (!(*pixdat & 0x8)) { if (*pixdat & 0x7) { /* sprite has priority */; } else if (*pixdat & 0x8) { /* sprite has priority */; }  /* 4th */ }  /* 3rd */	else if (*pixdat & 0x8) { /* sprite has priority */; }  /* 4th */ } /* 1st */
			else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; } /* 2nd */	else if (!(*pixdat & 0x8)) { if (*pixdat & 0x7) { /* sprite has priority */; } else if (*pixdat & 0x8) { /* sprite has priority */; }  /* 4th */ }  /* 3rd */	else if (*pixdat & 0x8) { /* sprite has priority */; }  /* 4th */


			break;
		}
		}
		break;
	}

	case 0x800:
	{
		switch (*pixdat & 0xc00)
		{

		case 0x000:
		{
			if (!(*pixdat & 0x8)) { if (*pixdat & 0x7) { *dstptr = *pixdat; } else if (*pixdat & 0x8) { *dstptr = *pixdat; } /* 2nd */	else if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { /* sprite has priority */; } else if (*pixdat2 & 0x8) { /* sprite has priority */; }  /* 4th */ }  /* 3rd */	else if (*pixdat2 & 0x8) { /* sprite has priority */; }  /* 4th */ } /* 1st */
			else if (*pixdat & 0x8) { *dstptr = *pixdat; } /* 2nd */	else if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { /* sprite has priority */; } else if (*pixdat2 & 0x8) { /* sprite has priority */; }  /* 4th */ }  /* 3rd */	else if (*pixdat2 & 0x8) { /* sprite has priority */; }  /* 4th */


			break;
		}

		case 0x400:
		{
			if (!(*pixdat & 0x8)) { if (*pixdat & 0x7) { *dstptr = *pixdat;; } else if (*pixdat & 0x8) { *dstptr = *pixdat;; } /* 2nd */	else if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { /* sprite has priority */; } else if (*pixdat2 & 0x8) { /* sprite has priority */; }  /* 4th */ }  /* 3rd */	else if (*pixdat2 & 0x8) { /* sprite has priority */; }  /* 4th */ } /* 1st */
			else if (*pixdat & 0x8) { *dstptr = *pixdat;; } /* 2nd */	else if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { /* sprite has priority */; } else if (*pixdat2 & 0x8) { /* sprite has priority */; }  /* 4th */ }  /* 3rd */	else if (*pixdat2 & 0x8) { /* sprite has priority */; }  /* 4th */

			break;
		}

		case 0x800:
		{
			if (!(*pixdat & 0x8)) { if (*pixdat & 0x7) { /* sprite has priority */; } else if (*pixdat & 0x8) { /* sprite has priority */; } /* 2nd */	else if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { /* sprite has priority */; } else if (*pixdat2 & 0x8) { /* sprite has priority */; }  /* 4th */ }  /* 3rd */	else if (*pixdat2 & 0x8) { /* sprite has priority */; }  /* 4th */ } /* 1st */
			else if (*pixdat & 0x8) { /* sprite has priority */; } /* 2nd */	else if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { /* sprite has priority */; } else if (*pixdat2 & 0x8) { /* sprite has priority */; }  /* 4th */ }  /* 3rd */	else if (*pixdat2 & 0x8) { /* sprite has priority */; }  /* 4th */


			break;
		}

		case 0xc00:
		{

			if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { /* sprite has priority */; } else if (*pixdat2 & 0x8) { /* sprite has priority */; } /* 2nd */	else if (!(*pixdat & 0x8)) { if (*pixdat & 0x7) { /* sprite has priority */; } else if (*pixdat & 0x8) { /* sprite has priority */; }  /* 4th */ }  /* 3rd */	else if (*pixdat & 0x8) { /* sprite has priority */; }  /* 4th */ } /* 1st */
			else if (*pixdat2 & 0x8) { /* sprite has priority */; } /* 2nd */	else if (!(*pixdat & 0x8)) { if (*pixdat & 0x7) { /* sprite has priority */; } else if (*pixdat & 0x8) { /* sprite has priority */; }  /* 4th */ }  /* 3rd */	else if (*pixdat & 0x8) { /* sprite has priority */; }  /* 4th */


			break;
		}
		}
		break;
	}

	case 0xc00:
	{
		switch (*pixdat & 0xc00)
		{

		case 0x000:
		{
			if (!(*pixdat & 0x8)) { if (*pixdat & 0x7) { *dstptr = *pixdat; } else if (*pixdat & 0x8) { *dstptr = *pixdat; } /* 2nd */	else if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { /* sprite has priority */; } else if (*pixdat2 & 0x8) { /* sprite has priority */; }  /* 4th */ }  /* 3rd */	else if (*pixdat2 & 0x8) { /* sprite has priority */; }  /* 4th */ } /* 1st */
			else if (*pixdat & 0x8) { *dstptr = *pixdat; } /* 2nd */	else if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { /* sprite has priority */; } else if (*pixdat2 & 0x8) { /* sprite has priority */; }  /* 4th */ }  /* 3rd */	else if (*pixdat2 & 0x8) { /* sprite has priority */; }  /* 4th */

			break;
		}

		case 0x400:
		{
			if (!(*pixdat & 0x8)) { if (*pixdat & 0x7) { *dstptr = *pixdat;; } else if (*pixdat & 0x8) { *dstptr = *pixdat;; } /* 2nd */	else if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { /* sprite has priority */; } else if (*pixdat2 & 0x8) { /* sprite has priority */; } }  /* 3rd */	else if (*pixdat2 & 0x8) { /* sprite has priority */; }  /* 4th */ } /* 1st */
			else if (*pixdat & 0x8) { *dstptr = *pixdat;; } /* 2nd */	else if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { /* sprite has priority */; } else if (*pixdat2 & 0x8) { /* sprite has priority */; } }  /* 3rd */	else if (*pixdat2 & 0x8) { /* sprite has priority */; }  /* 4th */

			break;
		}

		case 0x800:
		{
			if (!(*pixdat & 0x8)) { if (*pixdat & 0x7) { /* sprite has priority */; } else if (*pixdat & 0x8) { /* sprite has priority */; } /* 2nd */	else if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { /* sprite has priority */; } else if (*pixdat2 & 0x8) { /* sprite has priority */; } }  /* 3rd */	else if (*pixdat2 & 0x8) { /* sprite has priority */; }  /* 4th */ } /* 1st */
			else if (*pixdat & 0x8) { /* sprite has priority */; } /* 2nd */	else if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { /* sprite has priority */; } else if (*pixdat2 & 0x8) { /* sprite has priority */; } }  /* 3rd */	else if (*pixdat2 & 0x8) { /* sprite has priority */; }  /* 4th */

			break;
		}

		case 0xc00:
		{
			if (!(*pixdat & 0x8)) { if (*pixdat & 0x7) { /* sprite has priority */; } else if (*pixdat & 0x8) { /* sprite has priority */; }  /* 2nd */	else if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { /* sprite has priority */; } else if (*pixdat2 & 0x8) { /* sprite has priority */; }  /* 4th */ }  /* 3rd */	else if (*pixdat2 & 0x8) { /* sprite has priority */; }  /* 4th */ }  /* 1st */
			else if (*pixdat & 0x8) { /* sprite has priority */; }  /* 2nd */	else if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { /* sprite has priority */; } else if (*pixdat2 & 0x8) { /* sprite has priority */; }  /* 4th */ }  /* 3rd */	else if (*pixdat2 & 0x8) { /* sprite has priority */; }  /* 4th */

			break;
		}
		}
		break;
	}
	}
}



inline void mix_above0_1_2_thoop(uint16_t* dstptr, uint16_t* pixdat, uint16_t* pixdat2)
{
	switch (*pixdat2 & 0xc00)
	{
	case 0x000:
	{
		switch (*pixdat & 0xc00)
		{

		case 0x000:
		{
			if (!(*pixdat & 0x8)) { if (*pixdat & 0x7) { *dstptr = *pixdat; } else if (*pixdat & 0x8) { *dstptr = *pixdat; } /* 2nd */	else if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { *dstptr = *pixdat2;; } else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; }  /* 4th */ }  /* 3rd */	else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; }  /* 4th */ } /* 1st */
			else if (*pixdat & 0x8) { *dstptr = *pixdat; } /* 2nd */	else if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { *dstptr = *pixdat2;; } else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; }  /* 4th */ }  /* 3rd */	else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; }  /* 4th */

			break;
		}

		case 0x400:
		{
			if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { *dstptr = *pixdat2;; } else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; } /* 2nd */	else if (!(*pixdat & 0x8)) { if (*pixdat & 0x7) { *dstptr = *pixdat;; } else if (*pixdat & 0x8) { /* *dstptr = *pixdat;*/; }  /* 4th */ }  /* 3rd */	else if (*pixdat & 0x8) { /* *dstptr = *pixdat;*/; }  /* 4th */ } /* 1st */
			else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; } /* 2nd */	else if (!(*pixdat & 0x8)) { if (*pixdat & 0x7) { *dstptr = *pixdat;; } else if (*pixdat & 0x8) { /* *dstptr = *pixdat;*/; }  /* 4th */ }  /* 3rd */	else if (*pixdat & 0x8) { /* *dstptr = *pixdat;*/; }  /* 4th */

			break;
		}

		case 0x800:
		{
			if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { *dstptr = *pixdat2;; } else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; } /* 2nd */	else if (!(*pixdat & 0x8)) { if (*pixdat & 0x7) { /* sprite has priority */; } else if (*pixdat & 0x8) { /* sprite has priority */; }  /* 4th */ }  /* 3rd */	else if (*pixdat & 0x8) { /* sprite has priority */; }  /* 4th */ } /* 1st */
			else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; } /* 2nd */	else if (!(*pixdat & 0x8)) { if (*pixdat & 0x7) { /* sprite has priority */; } else if (*pixdat & 0x8) { /* sprite has priority */; }  /* 4th */ }  /* 3rd */	else if (*pixdat & 0x8) { /* sprite has priority */; }  /* 4th */

			break;
		}

		case 0xc00:
		{
			if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { *dstptr = *pixdat2;; } else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; } /* 2nd */	else if (!(*pixdat & 0x8)) { if (*pixdat & 0x7) { /* sprite has priority */; } else if (*pixdat & 0x8) { /* sprite has priority */; }  /* 4th */ }  /* 3rd */	else if (*pixdat & 0x8) { /* sprite has priority */; }  /* 4th */ } /* 1st */
			else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; } /* 2nd */	else if (!(*pixdat & 0x8)) { if (*pixdat & 0x7) { /* sprite has priority */; } else if (*pixdat & 0x8) { /* sprite has priority */; }  /* 4th */ }  /* 3rd */	else if (*pixdat & 0x8) { /* sprite has priority */; }  /* 4th */

			break;
		}
		}
		break;
	}

	case 0x400:
	{
		switch (*pixdat & 0xc00)
		{
		case 0x000:
		{
			if (!(*pixdat & 0x8)) { if (*pixdat & 0x7) { *dstptr = *pixdat; } else if (*pixdat & 0x8) { *dstptr = *pixdat; } /* 2nd */	else if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { *dstptr = *pixdat2;; } else if (*pixdat2 & 0x8) { /* *dstptr = *pixdat2;*/; }  /* 4th */ }  /* 3rd */	else if (*pixdat2 & 0x8) { /* *dstptr = *pixdat2;*/; }  /* 4th */ } /* 1st */
			else if (*pixdat & 0x8) { *dstptr = *pixdat; } /* 2nd */	else if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { *dstptr = *pixdat2;; } else if (*pixdat2 & 0x8) { /* *dstptr = *pixdat2;*/; }  /* 4th */ }  /* 3rd */	else if (*pixdat2 & 0x8) { /* *dstptr = *pixdat2;*/; }  /* 4th */

			break;
		}

		case 0x400:
		{
			if (!(*pixdat & 0x8)) { if (*pixdat & 0x7) { *dstptr = *pixdat;; } else if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { *dstptr = *pixdat2;; } else if (*pixdat & 0x8) { /* *dstptr = *pixdat;*/; }  /* 3rd */	else if (*pixdat2 & 0x8) { /* *dstptr = *pixdat2;*/; }  /* 4th */ } /* 2nd */	else if (*pixdat & 0x8) { /* *dstptr = *pixdat;*/; }  /* 3rd */	else if (*pixdat2 & 0x8) { /* *dstptr = *pixdat2;*/; }  /* 4th */ } /* 1st */
			else if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { *dstptr = *pixdat2;; } else if (*pixdat & 0x8) { /* *dstptr = *pixdat;*/; }  /* 3rd */	else if (*pixdat2 & 0x8) { /* *dstptr = *pixdat2;*/; }  /* 4th */ } /* 2nd */	else if (*pixdat & 0x8) { /* *dstptr = *pixdat;*/; }  /* 3rd */	else if (*pixdat2 & 0x8) { /* *dstptr = *pixdat2;*/; }  /* 4th */

			break;
		}

		case 0x800:
		{
			if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { *dstptr = *pixdat2;; } else if (*pixdat2 & 0x8) { /* *dstptr = *pixdat2;*/; } /* 2nd */	else if (!(*pixdat & 0x8)) { if (*pixdat & 0x7) { /* sprite has priority */; } else if (*pixdat & 0x8) { /* sprite has priority */; }  /* 4th */ }  /* 3rd */	else if (*pixdat & 0x8) { /* sprite has priority */; }  /* 4th */ } /* 1st */
			else if (*pixdat2 & 0x8) { /* *dstptr = *pixdat2;*/; } /* 2nd */	else if (!(*pixdat & 0x8)) { if (*pixdat & 0x7) { /* sprite has priority */; } else if (*pixdat & 0x8) { /* sprite has priority */; }  /* 4th */ }  /* 3rd */	else if (*pixdat & 0x8) { /* sprite has priority */; }  /* 4th */

			break;
		}

		case 0xc00:
		{
			if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { *dstptr = *pixdat2;; } else if (*pixdat2 & 0x8) { /* *dstptr = *pixdat2;*/; } /* 2nd */	else if (!(*pixdat & 0x8)) { if (*pixdat & 0x7) { /* sprite has priority */; } else if (*pixdat & 0x8) { /* sprite has priority */; }  /* 4th */ }  /* 3rd */	else if (*pixdat & 0x8) { /* sprite has priority */; }  /* 4th */ } /* 1st */
			else if (*pixdat2 & 0x8) { /* *dstptr = *pixdat2;*/; } /* 2nd */	else if (!(*pixdat & 0x8)) { if (*pixdat & 0x7) { /* sprite has priority */; } else if (*pixdat & 0x8) { /* sprite has priority */; }  /* 4th */ }  /* 3rd */	else if (*pixdat & 0x8) { /* sprite has priority */; }  /* 4th */


			break;
		}
		}
		break;
	}

	case 0x800:
	{
		switch (*pixdat & 0xc00)
		{

		case 0x000:
		{
			if (!(*pixdat & 0x8)) { if (*pixdat & 0x7) { *dstptr = *pixdat; } else if (*pixdat & 0x8) { *dstptr = *pixdat; } /* 2nd */	else if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { /* sprite has priority */; } else if (*pixdat2 & 0x8) { /* sprite has priority */; }  /* 4th */ }  /* 3rd */	else if (*pixdat2 & 0x8) { /* sprite has priority */; }  /* 4th */ } /* 1st */
			else if (*pixdat & 0x8) { *dstptr = *pixdat; } /* 2nd */	else if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { /* sprite has priority */; } else if (*pixdat2 & 0x8) { /* sprite has priority */; }  /* 4th */ }  /* 3rd */	else if (*pixdat2 & 0x8) { /* sprite has priority */; }  /* 4th */


			break;
		}

		case 0x400:
		{
			if (!(*pixdat & 0x8)) { if (*pixdat & 0x7) { *dstptr = *pixdat;; } else if (*pixdat & 0x8) { /* *dstptr = *pixdat;*/; } /* 2nd */	else if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { /* sprite has priority */; } else if (*pixdat2 & 0x8) { /* sprite has priority */; }  /* 4th */ }  /* 3rd */	else if (*pixdat2 & 0x8) { /* sprite has priority */; }  /* 4th */ } /* 1st */
			else if (*pixdat & 0x8) { /* *dstptr = *pixdat;*/; } /* 2nd */	else if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { /* sprite has priority */; } else if (*pixdat2 & 0x8) { /* sprite has priority */; }  /* 4th */ }  /* 3rd */	else if (*pixdat2 & 0x8) { /* sprite has priority */; }  /* 4th */

			break;
		}

		case 0x800:
		{
			if (!(*pixdat & 0x8)) { if (*pixdat & 0x7) { /* sprite has priority */; } else if (*pixdat & 0x8) { /* sprite has priority */; } /* 2nd */	else if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { /* sprite has priority */; } else if (*pixdat2 & 0x8) { /* sprite has priority */; }  /* 4th */ }  /* 3rd */	else if (*pixdat2 & 0x8) { /* sprite has priority */; }  /* 4th */ } /* 1st */
			else if (*pixdat & 0x8) { /* sprite has priority */; } /* 2nd */	else if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { /* sprite has priority */; } else if (*pixdat2 & 0x8) { /* sprite has priority */; }  /* 4th */ }  /* 3rd */	else if (*pixdat2 & 0x8) { /* sprite has priority */; }  /* 4th */


			break;
		}

		case 0xc00:
		{

			if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { /* sprite has priority */; } else if (*pixdat2 & 0x8) { /* sprite has priority */; } /* 2nd */	else if (!(*pixdat & 0x8)) { if (*pixdat & 0x7) { /* sprite has priority */; } else if (*pixdat & 0x8) { /* sprite has priority */; }  /* 4th */ }  /* 3rd */	else if (*pixdat & 0x8) { /* sprite has priority */; }  /* 4th */ } /* 1st */
			else if (*pixdat2 & 0x8) { /* sprite has priority */; } /* 2nd */	else if (!(*pixdat & 0x8)) { if (*pixdat & 0x7) { /* sprite has priority */; } else if (*pixdat & 0x8) { /* sprite has priority */; }  /* 4th */ }  /* 3rd */	else if (*pixdat & 0x8) { /* sprite has priority */; }  /* 4th */


			break;
		}
		}
		break;
	}

	case 0xc00:
	{
		switch (*pixdat & 0xc00)
		{

		case 0x000:
		{
			if (!(*pixdat & 0x8)) { if (*pixdat & 0x7) { *dstptr = *pixdat; } else if (*pixdat & 0x8) { *dstptr = *pixdat; } /* 2nd */	else if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { /* sprite has priority */; } else if (*pixdat2 & 0x8) { /* sprite has priority */; }  /* 4th */ }  /* 3rd */	else if (*pixdat2 & 0x8) { /* sprite has priority */; }  /* 4th */ } /* 1st */
			else if (*pixdat & 0x8) { *dstptr = *pixdat; } /* 2nd */	else if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { /* sprite has priority */; } else if (*pixdat2 & 0x8) { /* sprite has priority */; }  /* 4th */ }  /* 3rd */	else if (*pixdat2 & 0x8) { /* sprite has priority */; }  /* 4th */

			break;
		}

		case 0x400:
		{
			if (!(*pixdat & 0x8)) { if (*pixdat & 0x7) { *dstptr = *pixdat;; } else if (*pixdat & 0x8) { /* *dstptr = *pixdat;*/; } /* 2nd */	else if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { /* sprite has priority */; } else if (*pixdat2 & 0x8) { /* sprite has priority */; } }  /* 3rd */	else if (*pixdat2 & 0x8) { /* sprite has priority */; }  /* 4th */ } /* 1st */
			else if (*pixdat & 0x8) { /* *dstptr = *pixdat;*/; } /* 2nd */	else if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { /* sprite has priority */; } else if (*pixdat2 & 0x8) { /* sprite has priority */; } }  /* 3rd */	else if (*pixdat2 & 0x8) { /* sprite has priority */; }  /* 4th */

			break;
		}

		case 0x800:
		{
			if (!(*pixdat & 0x8)) { if (*pixdat & 0x7) { /* sprite has priority */; } else if (*pixdat & 0x8) { /* sprite has priority */; } /* 2nd */	else if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { /* sprite has priority */; } else if (*pixdat2 & 0x8) { /* sprite has priority */; } }  /* 3rd */	else if (*pixdat2 & 0x8) { /* sprite has priority */; }  /* 4th */ } /* 1st */
			else if (*pixdat & 0x8) { /* sprite has priority */; } /* 2nd */	else if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { /* sprite has priority */; } else if (*pixdat2 & 0x8) { /* sprite has priority */; } }  /* 3rd */	else if (*pixdat2 & 0x8) { /* sprite has priority */; }  /* 4th */

			break;
		}

		case 0xc00:
		{
			if (!(*pixdat & 0x8)) { if (*pixdat & 0x7) { /* sprite has priority */; } else if (*pixdat & 0x8) { /* sprite has priority */; }  /* 2nd */	else if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { /* sprite has priority */; } else if (*pixdat2 & 0x8) { /* sprite has priority */; }  /* 4th */ }  /* 3rd */	else if (*pixdat2 & 0x8) { /* sprite has priority */; }  /* 4th */ }  /* 1st */
			else if (*pixdat & 0x8) { /* sprite has priority */; }  /* 2nd */	else if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { /* sprite has priority */; } else if (*pixdat2 & 0x8) { /* sprite has priority */; }  /* 4th */ }  /* 3rd */	else if (*pixdat2 & 0x8) { /* sprite has priority */; }  /* 4th */

			break;
		}
		}
		break;
	}
	}
}



inline void mix_above0_1_2_4_thoop(uint16_t* dstptr, uint16_t* pixdat, uint16_t* pixdat2)
{
	switch (*pixdat2 & 0xc00)
	{
	case 0x000:
	{
		switch (*pixdat & 0xc00)
		{

		case 0x000:
		{
			if (!(*pixdat & 0x8)) { if (*pixdat & 0x7) { *dstptr = *pixdat; } else if (*pixdat & 0x8) { *dstptr = *pixdat; } /* 2nd */	else if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { *dstptr = *pixdat2;; } else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; }  /* 4th */ }  /* 3rd */	else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; }  /* 4th */ } /* 1st */
			else if (*pixdat & 0x8) { *dstptr = *pixdat; } /* 2nd */	else if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { *dstptr = *pixdat2;; } else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; }  /* 4th */ }  /* 3rd */	else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; }  /* 4th */

			break;
		}

		case 0x400:
		{
			if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { *dstptr = *pixdat2;; } else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; } /* 2nd */	else if (!(*pixdat & 0x8)) { if (*pixdat & 0x7) { /* *dstptr = *pixdat;*/; } else if (*pixdat & 0x8) { /* *dstptr = *pixdat;*/; }  /* 4th */ }  /* 3rd */	else if (*pixdat & 0x8) { /* *dstptr = *pixdat;*/; }  /* 4th */ } /* 1st */
			else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; } /* 2nd */	else if (!(*pixdat & 0x8)) { if (*pixdat & 0x7) { /* *dstptr = *pixdat;*/; } else if (*pixdat & 0x8) { /* *dstptr = *pixdat;*/; }  /* 4th */ }  /* 3rd */	else if (*pixdat & 0x8) { /* *dstptr = *pixdat;*/; }  /* 4th */

			break;
		}

		case 0x800:
		{
			if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { *dstptr = *pixdat2;; } else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; } /* 2nd */	else if (!(*pixdat & 0x8)) { if (*pixdat & 0x7) { /* sprite has priority */; } else if (*pixdat & 0x8) { /* sprite has priority */; }  /* 4th */ }  /* 3rd */	else if (*pixdat & 0x8) { /* sprite has priority */; }  /* 4th */ } /* 1st */
			else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; } /* 2nd */	else if (!(*pixdat & 0x8)) { if (*pixdat & 0x7) { /* sprite has priority */; } else if (*pixdat & 0x8) { /* sprite has priority */; }  /* 4th */ }  /* 3rd */	else if (*pixdat & 0x8) { /* sprite has priority */; }  /* 4th */

			break;
		}

		case 0xc00:
		{
			if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { *dstptr = *pixdat2;; } else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; } /* 2nd */	else if (!(*pixdat & 0x8)) { if (*pixdat & 0x7) { /* sprite has priority */; } else if (*pixdat & 0x8) { /* sprite has priority */; }  /* 4th */ }  /* 3rd */	else if (*pixdat & 0x8) { /* sprite has priority */; }  /* 4th */ } /* 1st */
			else if (*pixdat2 & 0x8) { *dstptr = *pixdat2;; } /* 2nd */	else if (!(*pixdat & 0x8)) { if (*pixdat & 0x7) { /* sprite has priority */; } else if (*pixdat & 0x8) { /* sprite has priority */; }  /* 4th */ }  /* 3rd */	else if (*pixdat & 0x8) { /* sprite has priority */; }  /* 4th */

			break;
		}
		}
		break;
	}

	case 0x400:
	{
		switch (*pixdat & 0xc00)
		{
		case 0x000:
		{
			if (!(*pixdat & 0x8)) { if (*pixdat & 0x7) { *dstptr = *pixdat; } else if (*pixdat & 0x8) { *dstptr = *pixdat; } /* 2nd */	else if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { /* *dstptr = *pixdat2;*/; } else if (*pixdat2 & 0x8) { /* *dstptr = *pixdat2;*/; }  /* 4th */ }  /* 3rd */	else if (*pixdat2 & 0x8) { /* *dstptr = *pixdat2;*/; }  /* 4th */ } /* 1st */
			else if (*pixdat & 0x8) { *dstptr = *pixdat; } /* 2nd */	else if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { /* *dstptr = *pixdat2;*/; } else if (*pixdat2 & 0x8) { /* *dstptr = *pixdat2;*/; }  /* 4th */ }  /* 3rd */	else if (*pixdat2 & 0x8) { /* *dstptr = *pixdat2;*/; }  /* 4th */

			break;
		}

		case 0x400:
		{
			if (!(*pixdat & 0x8)) { if (*pixdat & 0x7) { /* *dstptr = *pixdat;*/; } else if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { /* *dstptr = *pixdat2;*/; } else if (*pixdat & 0x8) { /* *dstptr = *pixdat;*/; }  /* 3rd */	else if (*pixdat2 & 0x8) { /* *dstptr = *pixdat2;*/; }  /* 4th */ } /* 2nd */	else if (*pixdat & 0x8) { /* *dstptr = *pixdat;*/; }  /* 3rd */	else if (*pixdat2 & 0x8) { /* *dstptr = *pixdat2;*/; }  /* 4th */ } /* 1st */
			else if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { /* *dstptr = *pixdat2;*/; } else if (*pixdat & 0x8) { /* *dstptr = *pixdat;*/; }  /* 3rd */	else if (*pixdat2 & 0x8) { /* *dstptr = *pixdat2;*/; }  /* 4th */ } /* 2nd */	else if (*pixdat & 0x8) { /* *dstptr = *pixdat;*/; }  /* 3rd */	else if (*pixdat2 & 0x8) { /* *dstptr = *pixdat2;*/; }  /* 4th */

			break;
		}

		case 0x800:
		{
			if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { /* *dstptr = *pixdat2;*/; } else if (*pixdat2 & 0x8) { /* *dstptr = *pixdat2;*/; } /* 2nd */	else if (!(*pixdat & 0x8)) { if (*pixdat & 0x7) { /* sprite has priority */; } else if (*pixdat & 0x8) { /* sprite has priority */; }  /* 4th */ }  /* 3rd */	else if (*pixdat & 0x8) { /* sprite has priority */; }  /* 4th */ } /* 1st */
			else if (*pixdat2 & 0x8) { /* *dstptr = *pixdat2;*/; } /* 2nd */	else if (!(*pixdat & 0x8)) { if (*pixdat & 0x7) { /* sprite has priority */; } else if (*pixdat & 0x8) { /* sprite has priority */; }  /* 4th */ }  /* 3rd */	else if (*pixdat & 0x8) { /* sprite has priority */; }  /* 4th */

			break;
		}

		case 0xc00:
		{
			if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { /* *dstptr = *pixdat2;*/; } else if (*pixdat2 & 0x8) { /* *dstptr = *pixdat2;*/; } /* 2nd */	else if (!(*pixdat & 0x8)) { if (*pixdat & 0x7) { /* sprite has priority */; } else if (*pixdat & 0x8) { /* sprite has priority */; }  /* 4th */ }  /* 3rd */	else if (*pixdat & 0x8) { /* sprite has priority */; }  /* 4th */ } /* 1st */
			else if (*pixdat2 & 0x8) { /* *dstptr = *pixdat2;*/; } /* 2nd */	else if (!(*pixdat & 0x8)) { if (*pixdat & 0x7) { /* sprite has priority */; } else if (*pixdat & 0x8) { /* sprite has priority */; }  /* 4th */ }  /* 3rd */	else if (*pixdat & 0x8) { /* sprite has priority */; }  /* 4th */


			break;
		}
		}
		break;
	}

	case 0x800:
	{
		switch (*pixdat & 0xc00)
		{

		case 0x000:
		{
			if (!(*pixdat & 0x8)) { if (*pixdat & 0x7) { *dstptr = *pixdat; } else if (*pixdat & 0x8) { *dstptr = *pixdat; } /* 2nd */	else if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { /* sprite has priority */; } else if (*pixdat2 & 0x8) { /* sprite has priority */; }  /* 4th */ }  /* 3rd */	else if (*pixdat2 & 0x8) { /* sprite has priority */; }  /* 4th */ } /* 1st */
			else if (*pixdat & 0x8) { *dstptr = *pixdat; } /* 2nd */	else if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { /* sprite has priority */; } else if (*pixdat2 & 0x8) { /* sprite has priority */; }  /* 4th */ }  /* 3rd */	else if (*pixdat2 & 0x8) { /* sprite has priority */; }  /* 4th */


			break;
		}

		case 0x400:
		{
			if (!(*pixdat & 0x8)) { if (*pixdat & 0x7) { /* *dstptr = *pixdat;*/; } else if (*pixdat & 0x8) { /* *dstptr = *pixdat;*/; } /* 2nd */	else if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { /* sprite has priority */; } else if (*pixdat2 & 0x8) { /* sprite has priority */; }  /* 4th */ }  /* 3rd */	else if (*pixdat2 & 0x8) { /* sprite has priority */; }  /* 4th */ } /* 1st */
			else if (*pixdat & 0x8) { /* *dstptr = *pixdat;*/; } /* 2nd */	else if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { /* sprite has priority */; } else if (*pixdat2 & 0x8) { /* sprite has priority */; }  /* 4th */ }  /* 3rd */	else if (*pixdat2 & 0x8) { /* sprite has priority */; }  /* 4th */

			break;
		}

		case 0x800:
		{
			if (!(*pixdat & 0x8)) { if (*pixdat & 0x7) { /* sprite has priority */; } else if (*pixdat & 0x8) { /* sprite has priority */; } /* 2nd */	else if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { /* sprite has priority */; } else if (*pixdat2 & 0x8) { /* sprite has priority */; }  /* 4th */ }  /* 3rd */	else if (*pixdat2 & 0x8) { /* sprite has priority */; }  /* 4th */ } /* 1st */
			else if (*pixdat & 0x8) { /* sprite has priority */; } /* 2nd */	else if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { /* sprite has priority */; } else if (*pixdat2 & 0x8) { /* sprite has priority */; }  /* 4th */ }  /* 3rd */	else if (*pixdat2 & 0x8) { /* sprite has priority */; }  /* 4th */


			break;
		}

		case 0xc00:
		{

			if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { /* sprite has priority */; } else if (*pixdat2 & 0x8) { /* sprite has priority */; } /* 2nd */	else if (!(*pixdat & 0x8)) { if (*pixdat & 0x7) { /* sprite has priority */; } else if (*pixdat & 0x8) { /* sprite has priority */; }  /* 4th */ }  /* 3rd */	else if (*pixdat & 0x8) { /* sprite has priority */; }  /* 4th */ } /* 1st */
			else if (*pixdat2 & 0x8) { /* sprite has priority */; } /* 2nd */	else if (!(*pixdat & 0x8)) { if (*pixdat & 0x7) { /* sprite has priority */; } else if (*pixdat & 0x8) { /* sprite has priority */; }  /* 4th */ }  /* 3rd */	else if (*pixdat & 0x8) { /* sprite has priority */; }  /* 4th */


			break;
		}
		}
		break;
	}

	case 0xc00:
	{
		switch (*pixdat & 0xc00)
		{

		case 0x000:
		{
			if (!(*pixdat & 0x8)) { if (*pixdat & 0x7) { *dstptr = *pixdat; } else if (*pixdat & 0x8) { *dstptr = *pixdat; } /* 2nd */	else if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { /* sprite has priority */; } else if (*pixdat2 & 0x8) { /* sprite has priority */; }  /* 4th */ }  /* 3rd */	else if (*pixdat2 & 0x8) { /* sprite has priority */; }  /* 4th */ } /* 1st */
			else if (*pixdat & 0x8) { *dstptr = *pixdat; } /* 2nd */	else if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { /* sprite has priority */; } else if (*pixdat2 & 0x8) { /* sprite has priority */; }  /* 4th */ }  /* 3rd */	else if (*pixdat2 & 0x8) { /* sprite has priority */; }  /* 4th */

			break;
		}

		case 0x400:
		{
			if (!(*pixdat & 0x8)) { if (*pixdat & 0x7) { /* *dstptr = *pixdat;*/; } else if (*pixdat & 0x8) { /* *dstptr = *pixdat;*/; } /* 2nd */	else if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { /* sprite has priority */; } else if (*pixdat2 & 0x8) { /* sprite has priority */; } }  /* 3rd */	else if (*pixdat2 & 0x8) { /* sprite has priority */; }  /* 4th */ } /* 1st */
			else if (*pixdat & 0x8) { /* *dstptr = *pixdat;*/; } /* 2nd */	else if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { /* sprite has priority */; } else if (*pixdat2 & 0x8) { /* sprite has priority */; } }  /* 3rd */	else if (*pixdat2 & 0x8) { /* sprite has priority */; }  /* 4th */

			break;
		}

		case 0x800:
		{
			if (!(*pixdat & 0x8)) { if (*pixdat & 0x7) { /* sprite has priority */; } else if (*pixdat & 0x8) { /* sprite has priority */; } /* 2nd */	else if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { /* sprite has priority */; } else if (*pixdat2 & 0x8) { /* sprite has priority */; } }  /* 3rd */	else if (*pixdat2 & 0x8) { /* sprite has priority */; }  /* 4th */ } /* 1st */
			else if (*pixdat & 0x8) { /* sprite has priority */; } /* 2nd */	else if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { /* sprite has priority */; } else if (*pixdat2 & 0x8) { /* sprite has priority */; } }  /* 3rd */	else if (*pixdat2 & 0x8) { /* sprite has priority */; }  /* 4th */

			break;
		}

		case 0xc00:
		{
			if (!(*pixdat & 0x8)) { if (*pixdat & 0x7) { /* sprite has priority */; } else if (*pixdat & 0x8) { /* sprite has priority */; }  /* 2nd */	else if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { /* sprite has priority */; } else if (*pixdat2 & 0x8) { /* sprite has priority */; }  /* 4th */ }  /* 3rd */	else if (*pixdat2 & 0x8) { /* sprite has priority */; }  /* 4th */ }  /* 1st */
			else if (*pixdat & 0x8) { /* sprite has priority */; }  /* 2nd */	else if (!(*pixdat2 & 0x8)) { if (*pixdat2 & 0x7) { /* sprite has priority */; } else if (*pixdat2 & 0x8) { /* sprite has priority */; }  /* 4th */ }  /* 3rd */	else if (*pixdat2 & 0x8) { /* sprite has priority */; }  /* 4th */

			break;
		}
		}
		break;
	}
	}
}


UINT32 gaelco_state::screen_update_thoop(screen_device& screen, bitmap_ind16& bitmap, const rectangle& cliprect)
{
	/* set scroll registers */
	m_tilemap[0]->set_scrolly(0, m_vregs[0]);
	m_tilemap[0]->set_scrollx(0, m_vregs[1] + 4);
	m_tilemap[1]->set_scrolly(0, m_vregs[2]);
	m_tilemap[1]->set_scrollx(0, m_vregs[3]);

	bitmap.fill(0, cliprect);

	draw_sprites(screen, bitmap, cliprect);

	bitmap_ind16 &tmb = m_tilemap[0]->pixmap();
	bitmap_ind16 &tmb2 = m_tilemap[1]->pixmap();

	UINT16* srcptr;
	UINT16* srcptr2;

	int scrollx = m_tilemap[0]->scrollx(0);
	int scrolly = m_tilemap[0]->scrolly(0);
	int scrollx2 = m_tilemap[1]->scrollx(0);
	int scrolly2 = m_tilemap[1]->scrolly(0);

	for (int y = cliprect.min_y; y <= cliprect.max_y; y++)
	{
		int realy = (y + scrolly) & 0x1ff;
		int realy2 = (y + scrolly2) & 0x1ff;

		srcptr = &tmb.pix16(realy);
		srcptr2 = &tmb2.pix16(realy2);

		UINT16* dstptrx = &bitmap.pix16(y);
		int realx = (scrollx)&0x1ff;
		int realx2 = (scrollx2)&0x1ff;

		UINT16* dstptr = &dstptrx[cliprect.min_x];
		UINT16* dstend = &dstptrx[cliprect.max_x+1];


		while (dstptr != dstend)
		{
			uint16_t* pixdat = srcptr + ((realx++) & 0x1ff);
			uint16_t* pixdat2 = srcptr2 + ((realx2++) & 0x1ff);

			//  0 =  no sprites drawn here
			//  400    priority = 0  above 0
			//  800    priority = 1  above 0,1
			//  c00    priority = 2  above 0,1,2
			//  1000   priority = 3  above 0,1,2,4
			//  1400   priority = 4  above 0,1,2,4,8

			switch (*dstptr & 0x1c00)
			{
			default:
			{
				mix_normal_thoop(dstptr, pixdat, pixdat2);  // sprites are above nothing
			}
			break;

			case 0x400:
			{
				mix_above0_1_2_4_thoop(dstptr, pixdat, pixdat2);  // sprites are above 0,1,2
			}
			break;

			case 0x800:
			{
				mix_above0_1_2_thoop(dstptr, pixdat, pixdat2);  // sprites are above 0,1,2
			}
			break;

			case 0xc00:
			{
				mix_above0_1_thoop(dstptr, pixdat, pixdat2);  // sprites are above 0,1,2
			}
			break;

			case 0x1000:
			{
				mix_above0_thoop(dstptr, pixdat, pixdat2);  // sprites are above 0,1,2,4
			}
			break;

			case 0x1400: // Highest priority '4' sprites, no possibility of BG rendering
			{
				// sprites are above all
			}
			break;

			}

			//realx++;
			//realx2++;
			dstptr++;
		}
	}

	return 0;
}




