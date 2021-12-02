// license:BSD-3-Clause
// copyright-holders:Manuel Abadia, Peter Ferrie
/***************************************************************************

  Gaelco Type 1 Video Hardware Rev B

  The video hardware it's nearly identical to the previous
  revision but it can handle more tiles and more sprites

  Functions to emulate the video hardware of the machine

***************************************************************************/

#include "emu.h"
#include "includes/thoop2.h"


/***************************************************************************

    Callbacks for the TileMap code

***************************************************************************/

/*
    Tile format
    -----------

    Screen 0 & 1: (32*32, 16x16 tiles)

    Word | Bit(s)            | Description
    -----+-FEDCBA98-76543210-+--------------------------
      0  | -------- ------xx | code (high bits)
      0  | xxxxxxxx xxxxxx-- | code (low bits)
      1  | -------- --xxxxxx | color
      1  | -------- xx------ | priority
      1  | --xxxxxx -------- | not used
      1  | -x------ -------- | flip x
      1  | x------- -------- | flip y
*/

TILE_GET_INFO_MEMBER(thoop2_state::get_tile_info_screen0)
{
	int data = m_videoram[tile_index << 1];
	int data2 = m_videoram[(tile_index << 1) + 1];
	int code = ((data & 0xfffc) >> 2) | ((data & 0x0003) << 14);

	tileinfo.category = (data2 >> 6) & 0x03;

	SET_TILE_INFO_MEMBER(1, code, data2 & 0x3f, TILE_FLIPYX((data2 >> 14) & 0x03));
}


TILE_GET_INFO_MEMBER(thoop2_state::get_tile_info_screen1)
{
	int data = m_videoram[(0x1000/2) + (tile_index << 1)];
	int data2 = m_videoram[(0x1000/2) + (tile_index << 1) + 1];
	int code = ((data & 0xfffc) >> 2) | ((data & 0x0003) << 14);

	tileinfo.category = (data2 >> 6) & 0x03;

	SET_TILE_INFO_MEMBER(1, code, data2 & 0x3f, TILE_FLIPYX((data2 >> 14) & 0x03));
}

/***************************************************************************

    Memory Handlers

***************************************************************************/

WRITE16_MEMBER(thoop2_state::vram_w)
{
	uint16_t old = m_videoram[offset];
	COMBINE_DATA(&m_videoram[offset]);

	if (m_videoram[offset]!=old)
		m_pant[offset >> 11]->mark_tile_dirty(((offset << 1) & 0x0fff) >> 2);
}

/***************************************************************************

    Start/Stop the video hardware emulation.

***************************************************************************/

void thoop2_state::video_start()
{
	m_pant[0] = &machine().tilemap().create(m_gfxdecode, tilemap_get_info_delegate(FUNC(thoop2_state::get_tile_info_screen0),this),TILEMAP_SCAN_ROWS,16,16,32,32);
	m_pant[1] = &machine().tilemap().create(m_gfxdecode, tilemap_get_info_delegate(FUNC(thoop2_state::get_tile_info_screen1),this),TILEMAP_SCAN_ROWS,16,16,32,32);

	m_pant[0]->set_transmask(0,0xff01,0x00ff); /* pens 1-7 opaque, pens 0, 8-15 transparent */
	m_pant[1]->set_transmask(0,0xff01,0x00ff); /* pens 1-7 opaque, pens 0, 8-15 transparent */
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
      3  | -------- ------xx | sprite code (high bits)
      3  | xxxxxxxx xxxxxx-- | sprite code (low bits)
*/

void thoop2_state::draw_sprites(screen_device &screen, bitmap_ind16 &bitmap, const rectangle &cliprect)
{
	int i, x, y, ex, ey;
	gfx_element *gfx = m_gfxdecode->gfx(0);

	static const int x_offset[2] = {0x0,0x2};
	static const int y_offset[2] = {0x0,0x1};

	for (i = 0x800 - 4 - 1; i >= 3; i -= 4)
	{
		int sx = m_spriteram[i+2] & 0x01ff;
		int sy = (240 - (m_spriteram[i] & 0x00ff)) & 0x00ff;
		int number = m_spriteram[i+3];
		int color = (m_spriteram[i+2] & 0x7e00) >> 9;
		int attr = (m_spriteram[i] & 0xfe00) >> 9;
		int priority = (m_spriteram[i] & 0x3000) >> 12;

		int xflip = attr & 0x20;
		int yflip = attr & 0x40;
		int spr_size, pri_mask;

		/* palettes 0x38-0x3f are used for high priority sprites */
		if (color >= 0x38)
			priority = 4;

		switch (priority)
		{
			case 0: pri_mask = 0xff00; break;
			case 1: pri_mask = 0xff00 | 0xf0f0; break;
			case 2: pri_mask = 0xff00 | 0xf0f0 | 0xcccc; break;
			case 3: pri_mask = 0xff00 | 0xf0f0 | 0xcccc | 0xaaaa; break;
			default:
			case 4: pri_mask = 0; break;
		}

		number |= ((number & 0x03) << 16);

		if (attr & 0x04)
		{
			spr_size = 1;
		}
		else
		{
			spr_size = 2;
			number &= (~3);
		}

		for (y = 0; y < spr_size; y++)
		{
			for (x = 0; x < spr_size; x++)
			{
				ex = xflip ? (spr_size-1-x) : x;
				ey = yflip ? (spr_size-1-y) : y;

				gfx->prio_transpen(bitmap,cliprect,number + x_offset[ex] + y_offset[ey],
						color,xflip,yflip,
						sx-0x0f+x*8,sy+y*8,
						screen.priority(),pri_mask,0);
			}
		}
	}
}

/***************************************************************************

    Display Refresh

***************************************************************************/

uint32_t thoop2_state::screen_update(screen_device &screen, bitmap_ind16 &bitmap, const rectangle &cliprect)
{
	/* set scroll registers */
	/* set scroll registers */
	m_pant[0]->set_scrolly(0, m_vregs[0]);
	m_pant[0]->set_scrollx(0, m_vregs[1] + 4);
	m_pant[1]->set_scrolly(0, m_vregs[2]);
	m_pant[1]->set_scrollx(0, m_vregs[3]);

	screen.priority().fill(0, cliprect);
	bitmap.fill(0, cliprect);

	bitmap_ind16 &tmb = m_pant[0]->pixmap();
	bitmap_ind16 &tmb2 = m_pant[1]->pixmap();

	bitmap_ind8 &flags = m_pant[0]->flagsmap();
	bitmap_ind8 &flags2 = m_pant[1]->flagsmap();

	UINT16* srcptr;
	UINT16* srcptr2;
	UINT8* flagsptr;
	UINT8* flagsptr2;

//	UINT16* dstptr;
//	UINT8* dstpriptr;

	int scrollx = m_pant[0]->scrollx(0);
	int scrolly = m_pant[0]->scrolly(0);
	int scrollx2 = m_pant[1]->scrollx(0);
	int scrolly2 = m_pant[1]->scrolly(0);

	for (int y = cliprect.min_y; y <= cliprect.max_y; y++)
	{
		int realy = (y + scrolly) & 0x1ff;
		int realy2 = (y + scrolly2) & 0x1ff;

		srcptr = &tmb.pix16(realy);
		srcptr2 = &tmb2.pix16(realy2);

		flagsptr = &flags.pix8(realy);
		flagsptr2 = &flags2.pix8(realy2);


		UINT16* dstptrx = &bitmap.pix16(y);
		UINT8* dstpriptrx = &screen.priority().pix8(y);
		int realx = (scrollx)&0x1ff;
		int realx2 = (scrollx2)&0x1ff;

		UINT16* dstptr = &dstptrx[cliprect.min_x];
		UINT16* dstend = &dstptrx[cliprect.max_x+1];

		UINT8* dstpriptr = &dstpriptrx[cliprect.min_x];

		while (dstptr != dstend)
		{
			
			UINT8 flagsdat = flagsptr[realx&0x1ff];
			UINT8 flagsdat2 = flagsptr2[realx2&0x1ff];

			UINT16 pixdat = srcptr[(realx++)&0x1ff];
			UINT16 pixdat2 = srcptr2[(realx2++)&0x1ff];

			switch (flagsdat & 0x03)
			{
			case 0x00:
			{
				switch (flagsdat2 & 0x03)
				{

				case 0x00:
				{
					if (!(pixdat & 0x8)) 
					{
						if (pixdat & 0x7) {	*dstptr = pixdat & 0x3ff;	*dstpriptr= 8; }
						else if (!(pixdat2 & 0x8)) 
						{
							if (pixdat2 & 0x7) { *dstptr = pixdat2 & 0x3ff; *dstpriptr= 8; }
							else if (pixdat & 0x8) { *dstptr = pixdat & 0x3ff; *dstpriptr= 4; }
							//else if (pixdat2 & 0x8) { *dstptr = pixdat2 & 0x3ff; *dstpriptr= 4; }
						}
						//else if (pixdat & 0x8) { *dstptr = pixdat & 0x3ff; *dstpriptr= 4; }
						else if (pixdat2 & 0x8) { *dstptr = pixdat2 & 0x3ff; *dstpriptr= 4; }
					}
					else if (!(pixdat2 & 0x8)) 
					{
						if (pixdat2 & 0x7) { *dstptr = pixdat2 & 0x3ff; *dstpriptr= 8; }
						else if (pixdat & 0x8) { *dstptr = pixdat & 0x3ff; *dstpriptr= 4; }
						//else if (pixdat2 & 0x8) { *dstptr = pixdat2 & 0x3ff; *dstpriptr= 4; }
					}
					else if (pixdat & 0x8) { *dstptr = pixdat & 0x3ff; *dstpriptr= 4; }
					//else if (pixdat2 & 0x8) { *dstptr = pixdat2 & 0x3ff; *dstpriptr= 4; }
					break;
				}

				case 0x01:
				{
					if (!(pixdat & 0x8)) 
					{
						if (pixdat & 0x7) { *dstptr = pixdat & 0x3ff; *dstpriptr= 8; }
						else if (pixdat & 0x8) { *dstptr = pixdat & 0x3ff; *dstpriptr= 4; }
						else if (!(pixdat2 & 0x8))
						{
							if (pixdat2 & 0x7) { *dstptr = pixdat2 & 0x3ff; *dstpriptr= 4; }
							//else if (pixdat2 & 0x8) { *dstptr = pixdat2 & 0x3ff; *dstpriptr= 2; }
						}
						else if (pixdat2 & 0x8) { *dstptr = pixdat2 & 0x3ff; *dstpriptr= 2; }
					}
					else if (pixdat & 0x8) { *dstptr = pixdat & 0x3ff; *dstpriptr= 4; }
					//else if (!(pixdat2 & 0x8))
					//{
					//	if (pixdat2 & 0x7) { *dstptr = pixdat2 & 0x3ff; *dstpriptr= 4; }
					//	else if (pixdat2 & 0x8) { *dstptr = pixdat2 & 0x3ff; *dstpriptr= 2; }
					//}
					//else if (pixdat2 & 0x8) { *dstptr = pixdat2 & 0x3ff; *dstpriptr= 2; }
					break;
				}

				case 0x02:
				{
					if (!(pixdat & 0x8)) 
					{
						if (pixdat & 0x7) {	*dstptr = pixdat & 0x3ff;	*dstpriptr= 8; }
						//else if (pixdat & 0x8) { *dstptr = pixdat & 0x3ff; *dstpriptr= 4; }
						else if (!(pixdat2 & 0x8))
						{
							if (pixdat2 & 0x7) { *dstptr = pixdat2 & 0x3ff; *dstpriptr= 2; }
							//else if (pixdat2 & 0x8) { *dstptr = pixdat2 & 0x3ff; *dstpriptr= 1; }
						}
						else if (pixdat2 & 0x8)	{ *dstptr = pixdat2 & 0x3ff; *dstpriptr= 1; }
					}
					else if (pixdat & 0x8) { *dstptr = pixdat & 0x3ff; *dstpriptr= 4; }
					//else if (!(pixdat2 & 0x8)) 
					//{
					//	if (pixdat2 & 0x7) { *dstptr = pixdat2 & 0x3ff; *dstpriptr= 2; }
					//	else if (pixdat2 & 0x8) { *dstptr = pixdat2 & 0x3ff; *dstpriptr= 1; }
					//}
					//else if (pixdat2 & 0x8) { *dstptr = pixdat2 & 0x3ff; *dstpriptr= 1; }
					break;
				}

				case 0x03:
				{
					if (!(pixdat & 0x8)) 
					{
						if (pixdat & 0x7) { *dstptr = pixdat & 0x3ff; *dstpriptr= 8; }
						//else if (pixdat & 0x8) { *dstptr = pixdat & 0x3ff; *dstpriptr= 4; }
						else if (!(pixdat2 & 0x8))  
						{
							if (pixdat2 & 0x7) { *dstptr = pixdat2 & 0x3ff; *dstpriptr= 1; }
							//else if (pixdat2 & 0x8) { *dstptr = pixdat2 & 0x3ff; *dstpriptr= 0; }
						}
						else if (pixdat2 & 0x8) { *dstptr = pixdat2 & 0x3ff; *dstpriptr= 0; }
					}
					else if (pixdat & 0x8) { *dstptr = pixdat & 0x3ff; *dstpriptr= 4; }
					//else if (!(pixdat2 & 0x8))
					//{	
					//	if (pixdat2 & 0x7) { *dstptr = pixdat2 & 0x3ff; *dstpriptr= 1; }
					//	else if (pixdat2 & 0x8) { *dstptr = pixdat2 & 0x3ff; *dstpriptr= 0; }
					//}
					//else if (pixdat2 & 0x8) { *dstptr = pixdat2 & 0x3ff; *dstpriptr= 0; }
					break;
				}
				}
				break;
			}

			case 0x01:
			{
				switch (flagsdat2 & 0x03)
				{
				case 0x00:
				{
					if (!(pixdat2 & 0x8)) 
					{
						if (pixdat2 & 0x7) { *dstptr = pixdat2 & 0x3ff; *dstpriptr= 8; }
						//else if (pixdat2 & 0x8) { *dstptr = pixdat2 & 0x3ff; *dstpriptr= 4; }
						else if (!(pixdat & 0x8))  
						{
							if (pixdat & 0x7) { *dstptr = pixdat & 0x3ff;	*dstpriptr= 4; }
							//else if (pixdat & 0x8) { *dstptr = pixdat & 0x3ff; *dstpriptr= 2; }
						}
						else if (pixdat & 0x8) { *dstptr = pixdat & 0x3ff; *dstpriptr= 2; }
					}
					else if (pixdat2 & 0x8) { *dstptr = pixdat2 & 0x3ff; *dstpriptr= 4; }
					//else if (!(pixdat & 0x8))  
					//{
					//	if (pixdat & 0x7) { *dstptr = pixdat & 0x3ff; *dstpriptr= 4; }
					//	else if (pixdat & 0x8) { *dstptr = pixdat & 0x3ff; *dstpriptr= 2; }
					//}
					//else if (pixdat & 0x8) { *dstptr = pixdat & 0x3ff; *dstpriptr= 2; }
					break;
				}

				case 0x01:
				{
					if (!(pixdat & 0x8)) 
					{
						if (pixdat & 0x7) { *dstptr = pixdat & 0x3ff; *dstpriptr= 4; }
						else if (!(pixdat2 & 0x8))
						{
							if (pixdat2 & 0x7) { *dstptr = pixdat2 & 0x3ff; *dstpriptr= 4; }
							//else if (pixdat & 0x8) { *dstptr = pixdat & 0x3ff; *dstpriptr= 2; }
							//else if (pixdat2 & 0x8) { *dstptr = pixdat2 & 0x3ff; *dstpriptr= 2; }

						}
						//else if (pixdat & 0x8) { *dstptr = pixdat & 0x3ff; *dstpriptr= 2; }
						else if (pixdat2 & 0x8) { *dstptr = pixdat2 & 0x3ff; *dstpriptr= 2; }
					}
					else if (!(pixdat2 & 0x8))  
					{
						if (pixdat2 & 0x7) { *dstptr = pixdat2 & 0x3ff; *dstpriptr= 4; }
						else if (pixdat & 0x8) { *dstptr = pixdat & 0x3ff; *dstpriptr= 2; }
						//else if (pixdat2 & 0x8) { *dstptr = pixdat2 & 0x3ff; *dstpriptr= 2; }
					}
					else if (pixdat & 0x8) { *dstptr = pixdat & 0x3ff; *dstpriptr= 2; }
					//else if (pixdat2 & 0x8) { *dstptr = pixdat2 & 0x3ff; *dstpriptr= 2; }
					break;
				}

				case 0x02:
				{
					if (!(pixdat & 0x8)) 
					{
						if (pixdat & 0x7) { *dstptr = pixdat & 0x3ff; *dstpriptr= 4; }
						//else if (pixdat & 0x8) { *dstptr = pixdat & 0x3ff; *dstpriptr= 2; }
						else if (!(pixdat2 & 0x8))  
						{
							if (pixdat2 & 0x7) { *dstptr = pixdat2 & 0x3ff; *dstpriptr= 2; }
							//else if (pixdat2 & 0x8) { *dstptr = pixdat2 & 0x3ff; *dstpriptr= 1; }
						}
						else if (pixdat2 & 0x8) { *dstptr = pixdat2 & 0x3ff; *dstpriptr= 1; }
					}
					else if (pixdat & 0x8) { *dstptr = pixdat & 0x3ff; *dstpriptr= 2; }
					//else if (!(pixdat2 & 0x8))  
					//{
					//	if (pixdat2 & 0x7) { *dstptr = pixdat2 & 0x3ff; *dstpriptr= 2; }
					//	else if (pixdat2 & 0x8)	{ *dstptr = pixdat2 & 0x3ff; *dstpriptr= 1; }
					//}
					//else if (pixdat2 & 0x8)	{ *dstptr = pixdat2 & 0x3ff; *dstpriptr= 1; }
					break;
				}

				case 0x03:
				{
					if (!(pixdat & 0x8)) 
					{
						if (pixdat & 0x7) { *dstptr = pixdat & 0x3ff; *dstpriptr= 4; }
						//else if (pixdat & 0x8) { *dstptr = pixdat & 0x3ff; *dstpriptr= 2; }
						else if (!(pixdat2 & 0x8))
						{
							if (pixdat2 & 0x7) { *dstptr = pixdat2 & 0x3ff; *dstpriptr= 1; }
							//else if (pixdat2 & 0x8) { *dstptr = pixdat2 & 0x3ff; *dstpriptr= 0; }
						}
						else if (pixdat2 & 0x8) { *dstptr = pixdat2 & 0x3ff; *dstpriptr= 0; }
					}
					else if (pixdat & 0x8) { *dstptr = pixdat & 0x3ff; *dstpriptr= 2; }
					//else if (!(pixdat2 & 0x8))  
					//{
					//	if (pixdat2 & 0x7) { *dstptr = pixdat2 & 0x3ff; *dstpriptr= 1; }
					//	else if (pixdat2 & 0x8) { *dstptr = pixdat2 & 0x3ff; *dstpriptr= 0; }
					//}
					//else if (pixdat2 & 0x8) { *dstptr = pixdat2 & 0x3ff; *dstpriptr= 0; }
				
					break;
				}
				}
				break;
			}

			case 0x02:
			{
				switch (flagsdat2 & 0x03)
				{

				case 0x00:
				{
					if (!(pixdat2 & 0x8)) 
					{
						if (pixdat2 & 0x7) { *dstptr = pixdat2 & 0x3ff; *dstpriptr= 8; }
						//else if (pixdat2 & 0x8) { *dstptr = pixdat2 & 0x3ff; *dstpriptr= 4; }
						else if (!(pixdat & 0x8)) 
						{
							if (pixdat & 0x7) {	*dstptr = pixdat & 0x3ff;	*dstpriptr= 2; }
							//else if (pixdat & 0x8) { *dstptr = pixdat & 0x3ff; *dstpriptr= 1; }
						}
						else if (pixdat & 0x8) { *dstptr = pixdat & 0x3ff; *dstpriptr= 1; }
					}
					else if (pixdat2 & 0x8) { *dstptr = pixdat2 & 0x3ff; *dstpriptr= 4; }
					//else if (!(pixdat & 0x8))
					//{
					//	if (pixdat & 0x7) {	*dstptr = pixdat & 0x3ff;	*dstpriptr= 2; }
					//	else if (pixdat & 0x8) { *dstptr = pixdat & 0x3ff; *dstpriptr= 1; }
					//}
					//else if (pixdat & 0x8) { *dstptr = pixdat & 0x3ff; *dstpriptr= 1; }
			
					break;
				}

				case 0x01:
				{
					if (!(pixdat2 & 0x8)) 
					{
						if (pixdat2 & 0x7) { *dstptr = pixdat2 & 0x3ff; *dstpriptr= 4; }
						//else if (pixdat2 & 0x8) { *dstptr = pixdat2 & 0x3ff; *dstpriptr= 2; }
						else if (!(pixdat & 0x8))
						{
							if (pixdat & 0x7) { *dstptr = pixdat & 0x3ff; *dstpriptr= 2; }
							//else if (pixdat & 0x8) { *dstptr = pixdat & 0x3ff; *dstpriptr= 1; }
						}
						else if (pixdat & 0x8) { *dstptr = pixdat & 0x3ff; *dstpriptr= 1; }
					}
					else if (pixdat2 & 0x8) { *dstptr = pixdat2 & 0x3ff; *dstpriptr= 2; }
					//else if (!(pixdat & 0x8))  
					//{
					//	if (pixdat & 0x7) { *dstptr = pixdat & 0x3ff; *dstpriptr= 2; }
					//	else if (pixdat & 0x8) { *dstptr = pixdat & 0x3ff; *dstpriptr= 1; }
					//}
					//else if (pixdat & 0x8) { *dstptr = pixdat & 0x3ff; *dstpriptr= 1; }
					break;
				}

				case 0x02:
				{
					if (!(pixdat & 0x8)) 
					{
						if (pixdat & 0x7) { *dstptr = pixdat & 0x3ff; *dstpriptr= 2; }
						else if (!(pixdat2 & 0x8))
						{
							if (pixdat2 & 0x7) { *dstptr = pixdat2 & 0x3ff; *dstpriptr= 2; }
							//else if (pixdat & 0x8) { *dstptr = pixdat & 0x3ff; *dstpriptr= 1; }
							//else if (pixdat2 & 0x8) { *dstptr = pixdat2 & 0x3ff; *dstpriptr= 1; }
						}
						else if (pixdat & 0x8) { *dstptr = pixdat & 0x3ff; *dstpriptr= 1; }
						else if (pixdat2 & 0x8) { *dstptr = pixdat2 & 0x3ff; *dstpriptr= 1; }
					}
					else if (!(pixdat2 & 0x8))  
					{
						if (pixdat2 & 0x7) { *dstptr = pixdat2 & 0x3ff; *dstpriptr= 2; }
						else if (pixdat & 0x8) { *dstptr = pixdat & 0x3ff; *dstpriptr= 1; }
						//else if (pixdat2 & 0x8) { *dstptr = pixdat2 & 0x3ff; *dstpriptr= 1; }
					}
					else if (pixdat & 0x8) { *dstptr = pixdat & 0x3ff; *dstpriptr= 1; }
					//else if (pixdat2 & 0x8) { *dstptr = pixdat2 & 0x3ff; *dstpriptr= 1; }

					break;
				}

				case 0x03:
				{

					if (!(pixdat & 0x8)) 
					{
						if (pixdat & 0x7) { *dstptr = pixdat & 0x3ff; *dstpriptr= 2; }
						//else if (pixdat & 0x8) { *dstptr = pixdat & 0x3ff; *dstpriptr= 1; }
						else if (!(pixdat2 & 0x8))
						{
							if (pixdat2 & 0x7) { *dstptr = pixdat2 & 0x3ff; *dstpriptr= 1; }
							//else if (pixdat2 & 0x8) { *dstptr = pixdat2 & 0x3ff; *dstpriptr= 0; }
						}
						else if (pixdat2 & 0x8) { *dstptr = pixdat2 & 0x3ff; *dstpriptr= 0; }
					}
					else if (pixdat & 0x8) { *dstptr = pixdat & 0x3ff; *dstpriptr= 1; }
					//else if (!(pixdat2 & 0x8))  
					//{
					//	if (pixdat2 & 0x7) { *dstptr = pixdat2 & 0x3ff; *dstpriptr= 1; }
					//	else if (pixdat2 & 0x8) { *dstptr = pixdat2 & 0x3ff; *dstpriptr= 0; }
					//}
					//else if (pixdat2 & 0x8) { *dstptr = pixdat2 & 0x3ff; *dstpriptr= 0; }
				
					break;
				}
				}
				break;
			}

			case 0x03:
			{
				switch (flagsdat2 & 0x03)
				{

				case 0x00:
				{
					if (!(pixdat2 & 0x8)) 
					{
						if (pixdat2 & 0x7) { *dstptr = pixdat2 & 0x3ff; *dstpriptr= 8; }
						//else if (pixdat2 & 0x8) { *dstptr = pixdat2 & 0x3ff; *dstpriptr= 4; }
						else if (!(pixdat & 0x8))  
						{
							if (pixdat & 0x7) { *dstptr = pixdat & 0x3ff; *dstpriptr= 1; }
							//else if (pixdat & 0x8) { *dstptr = pixdat & 0x3ff; *dstpriptr= 0; }
						}
						else if (pixdat & 0x8) { *dstptr = pixdat & 0x3ff; *dstpriptr= 0; }
					}
					else if (pixdat2 & 0x8) { *dstptr = pixdat2 & 0x3ff; *dstpriptr= 4; }
					//else if (!(pixdat & 0x8))  
					//{
					//	if (pixdat & 0x7) { *dstptr = pixdat & 0x3ff; *dstpriptr= 1; }
					//	else if (pixdat & 0x8) { *dstptr = pixdat & 0x3ff; *dstpriptr= 0; }
					//}
					//else if (pixdat & 0x8) { *dstptr = pixdat & 0x3ff; *dstpriptr= 0; }
					break;
				}

				case 0x01:
				{
					if (!(pixdat2 & 0x8)) 
					{
						if (pixdat2 & 0x7) { *dstptr = pixdat2 & 0x3ff; *dstpriptr= 4; }
						//else if (pixdat2 & 0x8) { *dstptr = pixdat2 & 0x3ff; *dstpriptr= 2; }
						else if (!(pixdat & 0x8))  
						{
							if (pixdat & 0x7) { *dstptr = pixdat & 0x3ff; *dstpriptr= 1; }
							//else if (pixdat & 0x8) { *dstptr = pixdat & 0x3ff; *dstpriptr= 0; }
						}
						else if (pixdat & 0x8) { *dstptr = pixdat & 0x3ff; *dstpriptr= 0; }
					}
					else if (pixdat2 & 0x8) { *dstptr = pixdat2 & 0x3ff; *dstpriptr= 2; }
					//else if (!(pixdat & 0x8))
					//{
					//	if (pixdat & 0x7) { *dstptr = pixdat & 0x3ff; *dstpriptr= 1; }
					//	else if (pixdat & 0x8) { *dstptr = pixdat & 0x3ff; *dstpriptr= 0; }
					//}
					//else if (pixdat & 0x8) { *dstptr = pixdat & 0x3ff; *dstpriptr= 0; }
					break;
				}

				case 0x02:
				{
					if (!(pixdat2 & 0x8)) 
					{
						if (pixdat2 & 0x7) { *dstptr = pixdat2 & 0x3ff; *dstpriptr= 2; }
						//else if (pixdat2 & 0x8) { *dstptr = pixdat2 & 0x3ff; *dstpriptr= 1; }
						else if (!(pixdat & 0x8))  
						{
							if (pixdat & 0x7) { *dstptr = pixdat & 0x3ff; *dstpriptr= 1; }
							//else if (pixdat & 0x8) { *dstptr = pixdat & 0x3ff; *dstpriptr= 0; }
						}
						else if (pixdat & 0x8) { *dstptr = pixdat & 0x3ff; *dstpriptr= 0; }
					}
					else if (pixdat2 & 0x8) { *dstptr = pixdat2 & 0x3ff; *dstpriptr= 1; }
					//else if (!(pixdat & 0x8))
					//{
					//	if (pixdat & 0x7) { *dstptr = pixdat & 0x3ff; *dstpriptr= 1; }
					//	else if (pixdat & 0x8) { *dstptr = pixdat & 0x3ff; *dstpriptr= 0; }
					//}
					//else if (pixdat & 0x8) { *dstptr = pixdat & 0x3ff; *dstpriptr= 0; }
					break;
				}

				case 0x03:
				{
					if (!(pixdat & 0x8)) 
					{
						if (pixdat & 0x7) { *dstptr = pixdat & 0x3ff; *dstpriptr= 1; }
						else if (!(pixdat2 & 0x8))
						{
							if (pixdat2 & 0x7) { *dstptr = pixdat2 & 0x3ff; *dstpriptr= 1; }
							//else if (pixdat & 0x8) { *dstptr = pixdat & 0x3ff; *dstpriptr= 0; }
							//else if (pixdat2 & 0x8) { *dstptr = pixdat2 & 0x3ff; *dstpriptr= 0; }
						}
						//else if (pixdat & 0x8) { *dstptr = pixdat & 0x3ff; *dstpriptr= 0; }
						else if (pixdat2 & 0x8) { *dstptr = pixdat2 & 0x3ff; *dstpriptr= 0; }
					}
					else if (!(pixdat2 & 0x8))  
					{
						if (pixdat2 & 0x7) { *dstptr = pixdat2 & 0x3ff; *dstpriptr= 1; }
						else if (pixdat & 0x8) { *dstptr = pixdat & 0x3ff; *dstpriptr= 0; }
						//else if (pixdat2 & 0x8)	{ *dstptr = pixdat2 & 0x3ff; *dstpriptr= 0; }
					}
					else if (pixdat & 0x8) { *dstptr = pixdat & 0x3ff; *dstpriptr= 0; }
					//else if (pixdat2 & 0x8) { *dstptr = pixdat2 & 0x3ff; *dstpriptr= 0; }
					break;
				}
				}
				break;
			}
			}

			//realx++;
			//realx2++;
			dstptr++;
			dstpriptr++;
		}
	}

	draw_sprites(screen, bitmap, cliprect);
	return 0;
}
