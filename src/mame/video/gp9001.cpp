// license:BSD-3-Clause
// copyright-holders:Quench, David Haywood
/* GP9001 Video Controller */

/***************************************************************************

  Functions to emulate the video hardware of some Toaplan games,
  which use one or more Toaplan L7A0498 GP9001 graphic controllers.

  The simpler hardware of these games use one GP9001 controller.

  Next we have games that use two GP9001 controllers, the mixing of
  the VDPs depends on a PAL on the motherboard.
  (mixing handled in toaplan2.c)

  Finally we have games using one GP9001 controller and an additional
  text tile layer, which has highest priority. This text tile layer
  appears to have line-scroll support. Some of these games copy the
  text tile gfx data to RAM from the main CPU ROM, which easily allows
  for effects to be added to the tiles, by manipulating the text tile
  gfx data. The tiles are then dynamically decoded from RAM before
  displaying them.


 To Do / Unknowns
    -  What do Scroll registers 0Eh and 0Fh really do ????
    -  Snow Bros 2 sets bit 6 of the sprite X info word during weather
        world map, and bits 4, 5 and 6 of the sprite X info word during
        the Rabbit boss screen - reasons are unknown.
    -  Fourth set of scroll registers have been used for Sprite scroll
        though it may not be correct. For most parts this looks right
        except for Snow Bros 2 when in the rabbit boss screen (all sprites
        jump when big green nasty (which is the foreground layer) comes
        in from the left)
    -  Teki Paki tests video RAM from address 0 past SpriteRAM to $37ff.
        This seems to be a bug in Teki Paki's vram test routine !




 GP9001 Tile RAM format (each tile takes up 32 bits)

  0         1         2         3
  ---- ---- ---- ---- xxxx xxxx xxxx xxxx = Tile number (0 - FFFFh)
  ---- ---- -xxx xxxx ---- ---- ---- ---- = Color (0 - 7Fh)
  ---- ---- ?--- ---- ---- ---- ---- ---- = unknown / unused
  ---- xxxx ---- ---- ---- ---- ---- ---- = Priority (0 - Fh)
  ???? ---- ---- ---- ---- ---- ---- ---- = unknown / unused / possible flips

Sprites are of varying sizes between 8x8 and 128x128 with any variation
in between, in multiples of 8 either way.

Here we draw the first 8x8 part of the sprite, then by using the sprite
dimensions, we draw the rest of the 8x8 parts to produce the complete
sprite.

There seems to be sprite buffering - double buffering actually.

 GP9001 Sprite RAM format (data for each sprite takes up 4 words)

  0
  ---- ----  ---- --xx = top 2 bits of Sprite number
  ---- ----  xxxx xx-- = Color (0 - 3Fh)
  ---- xxxx  ---- ---- = Priority (0 - Fh)
  ---x ----  ---- ---- = Flip X
  --x- ----  ---- ---- = Flip Y
  -x-- ----  ---- ---- = Multi-sprite
  x--- ----  ---- ---- = Show sprite ?

  1
  xxxx xxxx  xxxx xxxx = Sprite number (top two bits in word 0)

  2
  ---- ----  ---- xxxx = Sprite X size (add 1, then multiply by 8)
  ---- ----  -??? ---- = unknown - used in Snow Bros. 2
  xxxx xxxx  x--- ---- = X position

  3
  ---- ----  ---- xxxx = Sprite Y size (add 1, then multiply by 8)
  ---- ----  -??? ---- = unknown / unused
  xxxx xxxx  x--- ---- = Y position





 GP9001 Scroll Registers (hex) :

    00      Background scroll X (X flip off)
    01      Background scroll Y (Y flip off)
    02      Foreground scroll X (X flip off)
    03      Foreground scroll Y (Y flip off)
    04      Top (text) scroll X (X flip off)
    05      Top (text) scroll Y (Y flip off)
    06      Sprites    scroll X (X flip off) ???
    07      Sprites    scroll Y (Y flip off) ???
    0E      ??? Initialise Video controller at startup ???
    0F      Scroll update complete ??? (Not used in Ghox and V-Five)

    80      Background scroll X (X flip on)
    81      Background scroll Y (Y flip on)
    82      Foreground scroll X (X flip on)
    83      Foreground scroll Y (Y flip on)
    84      Top (text) scroll X (X flip on)
    85      Top (text) scroll Y (Y flip on)
    86      Sprites    scroll X (X flip on) ???
    87      Sprites    scroll Y (Y flip on) ???
    8F      Same as 0Fh except flip bit is active


Scroll Register 0E writes (Video controller inits ?) from different games:

Teki-Paki        | Ghox             | Knuckle Bash     | Truxton 2        |
0003, 0002, 4000 | ????, ????, ???? | 0202, 0203, 4200 | 0003, 0002, 4000 |

Dogyuun          | Batsugun         |
0202, 0203, 4200 | 0202, 0203, 4200 |
1202, 1203, 5200 | 1202, 1203, 5200 | <--- Second video controller

Pipi & Bibis     | Fix Eight        | V-Five           | Snow Bros. 2     |
0003, 0002, 4000 | 0202, 0203, 4200 | 0202, 0203, 4200 | 0202, 0203, 4200 |

***************************************************************************/


#include "emu.h"
#include "gp9001.h"

/*
 Single VDP mixing priority note:

 Initial thoughts were that 16 levels of priority exist for both sprites and tilemaps, ie GP9001_PRIMASK 0xf
 However the end of level scene rendered on the first VDP in Batsugun strongly suggests otherwise.

 Sprites  have 'priority' bits of 0x0600 (level 0x6) set
 Tilemaps have 'priority' bits of 0x7000 (level 0x7) set

 If a mask of 0xf is used then the tilemaps render above the sprites, which causes the V bonus items near the
 counters to be invisible (in addition to the English character quote text)

 using a mask of 0xe causes both priority levels to be equal, allowing the sprites to render above the tilemap.

 The alternative option of allowing sprites to render a priority level higher than tilemaps breaks at least the
 'Welcome to..' screen in Batrider after selecting your character.

 Batrider Gob-Robo boss however definitely requires SPRITES to still have 16 levels of priority against other
 sprites, see http://mametesters.org/view.php?id=5832

 It is unknown if the current solution breaks anything.  The majority of titles don't make extensive use of the
 priority system.

*/
#define GP9001_PRIMASK (0x000f)
#define GP9001_PRIMASK_TMAPS (0x000e)


const gfx_layout gp9001vdp_device::tilelayout =
{
	16,16,          /* 16x16 */
	RGN_FRAC(1,2),  /* Number of tiles */
	4,              /* 4 bits per pixel */
	{ RGN_FRAC(1,2)+8, RGN_FRAC(1,2), 8, 0 },
	{ 0, 1, 2, 3, 4, 5, 6, 7,
		8*16+0, 8*16+1, 8*16+2, 8*16+3, 8*16+4, 8*16+5, 8*16+6, 8*16+7 },
	{ 0*16, 1*16, 2*16, 3*16, 4*16, 5*16, 6*16, 7*16,
		16*16, 17*16, 18*16, 19*16, 20*16, 21*16, 22*16, 23*16 },
	8*4*16
};

const gfx_layout gp9001vdp_device::spritelayout =
{
	8,8,            /* 8x8 */
	RGN_FRAC(1,2),  /* Number of 8x8 sprites */
	4,              /* 4 bits per pixel */
	{ RGN_FRAC(1,2)+8, RGN_FRAC(1,2), 8, 0 },
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*16, 1*16, 2*16, 3*16, 4*16, 5*16, 6*16, 7*16 },
	8*16
};

GFXDECODE_MEMBER( gp9001vdp_device::gfxinfo )
	GFXDECODE_DEVICE( DEVICE_SELF, 0, tilelayout,   0, 0x1000 )
	GFXDECODE_DEVICE( DEVICE_SELF, 0, spritelayout, 0, 0x1000 )
GFXDECODE_END


const device_type GP9001_VDP = &device_creator<gp9001vdp_device>;

gp9001vdp_device::gp9001vdp_device(const machine_config &mconfig, const char *tag, device_t *owner, UINT32 clock)
	: device_t(mconfig, GP9001_VDP, "GP9001 VDP", tag, owner, clock, "gp9001vdp", __FILE__),
		device_gfx_interface(mconfig, *this, gfxinfo),
		device_video_interface(mconfig, *this)
{
}


TILE_GET_INFO_MEMBER(gp9001vdp_device::get_top0_tile_info)
{
	int color, tile_number, attrib;

	attrib = m_vram_top[2*tile_index];

	tile_number = m_vram_top[2*tile_index+1];

	if (gp9001_gfxrom_is_banked)
	{
		tile_number = ( gp9001_gfxrom_bank[(tile_number >> 13) & 7] << 13 ) | ( tile_number & 0x1fff );
	}

	color = attrib & 0x0fff; // 0x0f00 priority, 0x007f colour
	SET_TILE_INFO_MEMBER(0,
			tile_number,
			color,
			0);
	//tileinfo.category = (attrib & 0x0f00) >> 8;
}

TILE_GET_INFO_MEMBER(gp9001vdp_device::get_fg0_tile_info)
{
	int color, tile_number, attrib;

	attrib = m_vram_fg[2*tile_index];

	tile_number = m_vram_fg[2*tile_index+1];


	if (gp9001_gfxrom_is_banked)
	{
		tile_number = ( gp9001_gfxrom_bank[(tile_number >> 13) & 7] << 13 ) | ( tile_number & 0x1fff );
	}

	color = attrib & 0x0fff; // 0x0f00 priority, 0x007f colour
	SET_TILE_INFO_MEMBER(0,
			tile_number,
			color,
			0);
	//tileinfo.category = (attrib & 0x0f00) >> 8;
}

TILE_GET_INFO_MEMBER(gp9001vdp_device::get_bg0_tile_info)
{
	int color, tile_number, attrib;
	attrib = m_vram_bg[2*tile_index];

	tile_number = m_vram_bg[2*tile_index+1];

	if (gp9001_gfxrom_is_banked)
	{
		tile_number = ( gp9001_gfxrom_bank[(tile_number >> 13) & 7] << 13 ) | ( tile_number & 0x1fff );
	}

	color = attrib & 0x0fff; // 0x0f00 priority, 0x007f colour
	SET_TILE_INFO_MEMBER(0,
			tile_number,
			color,
			0);
	//tileinfo.category = (attrib & 0x0f00) >> 8;
}

void gp9001vdp_device::create_tilemaps()
{
	top.tmap = &machine().tilemap().create(*this, tilemap_get_info_delegate(FUNC(gp9001vdp_device::get_top0_tile_info),this),TILEMAP_SCAN_ROWS,16,16,32,32);
	fg.tmap = &machine().tilemap().create(*this, tilemap_get_info_delegate(FUNC(gp9001vdp_device::get_fg0_tile_info),this),TILEMAP_SCAN_ROWS,16,16,32,32);
	bg.tmap = &machine().tilemap().create(*this, tilemap_get_info_delegate(FUNC(gp9001vdp_device::get_bg0_tile_info),this),TILEMAP_SCAN_ROWS,16,16,32,32);

	top.tmap->set_transparent_pen(0);
	fg.tmap->set_transparent_pen(0);
	bg.tmap->set_transparent_pen(0);
}


void gp9001vdp_device::device_start()
{
	sp.vram16_buffer = make_unique_clear<UINT16[]>(GP9001_SPRITERAM_SIZE/2);

	create_tilemaps();

	save_item(NAME(m_vram_bg));
	save_item(NAME(m_vram_fg));
	save_item(NAME(m_vram_top));
	save_item(NAME(m_spriteram));
	save_item(NAME(m_unkram));

	save_pointer(NAME(sp.vram16_buffer.get()), GP9001_SPRITERAM_SIZE/2);

	save_item(NAME(gp9001_scroll_reg));
	save_item(NAME(gp9001_voffs));
	save_item(NAME(bg.scrollx));
	save_item(NAME(bg.scrolly));
	save_item(NAME(fg.scrollx));
	save_item(NAME(fg.scrolly));
	save_item(NAME(top.scrollx));
	save_item(NAME(top.scrolly));
	save_item(NAME(sp.scrollx));
	save_item(NAME(sp.scrolly));
	save_item(NAME(bg.flip));
	save_item(NAME(fg.flip));
	save_item(NAME(top.flip));
	save_item(NAME(sp.flip));

	gp9001_gfxrom_is_banked = 0;
	gp9001_gfxrom_bank_dirty = 0;
	save_item(NAME(gp9001_gfxrom_bank));

	// default layer offsets used by all original games
	bg.extra_xoffset.normal  = -0x1d6;
	bg.extra_xoffset.flipped = -0x229;
	bg.extra_yoffset.normal  = -0x1ef;
	bg.extra_yoffset.flipped = -0x210;

	fg.extra_xoffset.normal  = -0x1d8;
	fg.extra_xoffset.flipped = -0x227;
	fg.extra_yoffset.normal  = -0x1ef;
	fg.extra_yoffset.flipped = -0x210;

	top.extra_xoffset.normal = -0x1da;
	top.extra_xoffset.flipped= -0x225;
	top.extra_yoffset.normal = -0x1ef;
	top.extra_yoffset.flipped= -0x210;

	sp.extra_xoffset.normal  = -0x1cc;
	sp.extra_xoffset.flipped = -0x17b;
	sp.extra_yoffset.normal  = -0x1ef;
	sp.extra_yoffset.flipped = -0x108;

	sp.use_sprite_buffer = 1;
}

void gp9001vdp_device::device_reset()
{
	gp9001_voffs = 0;
	gp9001_scroll_reg = 0;
	bg.scrollx = bg.scrolly = 0;
	fg.scrollx = fg.scrolly = 0;
	top.scrollx = top.scrolly = 0;
	sp.scrollx = sp.scrolly = 0;

	bg.flip = 0;
	fg.flip = 0;
	top.flip = 0;
	sp.flip = 0;

	m_status = 0;

	init_scroll_regs();
}


void gp9001vdp_device::gp9001_voffs_w(UINT16 data, UINT16 mem_mask)
{
	COMBINE_DATA(&gp9001_voffs);
}

int gp9001vdp_device::gp9001_videoram16_r()
{
	int offs = gp9001_voffs;
	gp9001_voffs++;
//	return space().read_word(offs*2);
	offs &= 0x1fff;

	if (offs < 0x1000 / 2)
	{
		return m_vram_bg[offs];
	}
	else if (offs < 0x2000 / 2)
	{
		offs -= 0x1000 / 2;
		return m_vram_fg[offs];
	}
	else if (offs < 0x3000 / 2)
	{
		offs -= 0x2000 / 2;
		return m_vram_top[offs];
	}
	else if (offs < 0x4000/2)
	{
		offs -= 0x3000 / 2;
		return m_spriteram[offs&0x3ff];
	}

	return 0;
}


void gp9001vdp_device::gp9001_videoram16_w(UINT16 data, UINT16 mem_mask)
{
	int offs = gp9001_voffs;
	gp9001_voffs++;

	offs &= 0x1fff;

	switch (offs & 0x1800)
	{
	case 0x1800:
	{
		offs &= 0x3ff;
		COMBINE_DATA(&m_spriteram[offs & 0x3ff]);
		break;
	}
	case 0x1000:
	{
		offs &= 0x7ff;
		uint16_t old = m_vram_top[offs];

		COMBINE_DATA(&m_vram_top[offs]);

		if (m_vram_top[offs] != old)
			top.tmap->mark_tile_dirty(offs / 2);

		break;
	}

	case 0x800:
	{
		offs &= 0x7ff;
		uint16_t old = m_vram_fg[offs];
		COMBINE_DATA(&m_vram_fg[offs]);

		if (m_vram_fg[offs] != old)
			fg.tmap->mark_tile_dirty(offs / 2);

		break;
	}
	case 0x000:
	{
		uint16_t old = m_vram_bg[offs];
		COMBINE_DATA(&m_vram_bg[offs]);

		if (m_vram_bg[offs] != old)
			bg.tmap->mark_tile_dirty(offs / 2);
		break;
	}
	}

}


UINT16 gp9001vdp_device::gp9001_vdpstatus_r()
{
	return m_status;
	//return ((m_screen->vpos() + 15) % 262) >= 245;
}

void gp9001vdp_device::gp9001_scroll_reg_select_w(UINT16 data, UINT16 mem_mask)
{
	if (ACCESSING_BITS_0_7)
	{
		gp9001_scroll_reg = data & 0x8f;
		if (data & 0x70)
			logerror("Hmmm, selecting unknown LSB video control register (%04x)\n",gp9001_scroll_reg);
	}
	else
	{
		logerror("Hmmm, selecting unknown MSB video control register (%04x)\n",gp9001_scroll_reg);
	}
}

static void gp9001_set_scrollx_and_flip_reg(gp9001tilemaplayer* layer, UINT16 data, UINT16 mem_mask, int flip)
{
	COMBINE_DATA(&layer->scrollx);

	if (flip)
	{
		layer->flip |= TILEMAP_FLIPX;
		layer->tmap->set_scrollx(0,-(layer->scrollx+layer->extra_xoffset.flipped));
	}
	else
	{
		layer->flip &= (~TILEMAP_FLIPX);
		layer->tmap->set_scrollx(0,layer->scrollx+layer->extra_xoffset.normal);
	}
	layer->tmap->set_flip(layer->flip);
}

static void gp9001_set_scrolly_and_flip_reg(gp9001tilemaplayer* layer, UINT16 data, UINT16 mem_mask, int flip)
{
	COMBINE_DATA(&layer->scrolly);

	if (flip)
	{
		layer->flip |= TILEMAP_FLIPY;
		layer->tmap->set_scrolly(0,-(layer->scrolly+layer->extra_yoffset.flipped));

	}
	else
	{
		layer->flip &= (~TILEMAP_FLIPY);
		layer->tmap->set_scrolly(0,layer->scrolly+layer->extra_yoffset.normal);
	}

	layer->tmap->set_flip(layer->flip);
}

static void gp9001_set_sprite_scrollx_and_flip_reg(gp9001spritelayer* layer, UINT16 data, UINT16 mem_mask, int flip)
{
	if (flip)
	{
		data += layer->extra_xoffset.flipped;
		COMBINE_DATA(&layer->scrollx);
		if (layer->scrollx & 0x8000) layer->scrollx |= 0xfe00;
		else layer->scrollx &= 0x1ff;
		layer->flip |= GP9001_SPRITE_FLIPX;
	}
	else
	{
		data += layer->extra_xoffset.normal;
		COMBINE_DATA(&layer->scrollx);

		if (layer->scrollx & 0x8000) layer->scrollx |= 0xfe00;
		else layer->scrollx &= 0x1ff;
		layer->flip &= (~GP9001_SPRITE_FLIPX);
	}
}

static void gp9001_set_sprite_scrolly_and_flip_reg(gp9001spritelayer* layer, UINT16 data, UINT16 mem_mask, int flip)
{
	if (flip)
	{
		data += layer->extra_yoffset.flipped;
		COMBINE_DATA(&layer->scrolly);
		if (layer->scrolly & 0x8000) layer->scrolly |= 0xfe00;
		else layer->scrolly &= 0x1ff;
		layer->flip |= GP9001_SPRITE_FLIPY;
	}
	else
	{
		data += layer->extra_yoffset.normal;
		COMBINE_DATA(&layer->scrolly);
		if (layer->scrolly & 0x8000) layer->scrolly |= 0xfe00;
		else layer->scrolly &= 0x1ff;
		layer->flip &= (~GP9001_SPRITE_FLIPY);
	}
}

void gp9001vdp_device::gp9001_scroll_reg_data_w(UINT16 data, UINT16 mem_mask)
{
	/************************************************************************/
	/***** layer X and Y flips can be set independently, so emulate it ******/
	/************************************************************************/

	// writes with 8x set turn on flip for the specified layer / axis
	int flip = gp9001_scroll_reg & 0x80;

	switch(gp9001_scroll_reg&0x7f)
	{
		case 0x00: gp9001_set_scrollx_and_flip_reg(&bg, data, mem_mask, flip); break;
		case 0x01: gp9001_set_scrolly_and_flip_reg(&bg, data, mem_mask, flip); break;

		case 0x02: gp9001_set_scrollx_and_flip_reg(&fg, data, mem_mask, flip); break;
		case 0x03: gp9001_set_scrolly_and_flip_reg(&fg, data, mem_mask, flip); break;

		case 0x04: gp9001_set_scrollx_and_flip_reg(&top,data, mem_mask, flip); break;
		case 0x05: gp9001_set_scrolly_and_flip_reg(&top,data, mem_mask, flip); break;

		case 0x06: gp9001_set_sprite_scrollx_and_flip_reg(&sp, data,mem_mask,flip); break;
		case 0x07: gp9001_set_sprite_scrolly_and_flip_reg(&sp, data,mem_mask,flip); break;


		case 0x0e:  /******* Initialise video controller register ? *******/

		case 0x0f:  break;


		default:    logerror("Hmmm, writing %08x to unknown video control register (%08x) !!!\n",data,gp9001_scroll_reg);
					break;
	}
}

void gp9001vdp_device::init_scroll_regs()
{
	gp9001_set_scrollx_and_flip_reg(&bg, 0, 0xffff, 0);
	gp9001_set_scrolly_and_flip_reg(&bg, 0, 0xffff, 0);
	gp9001_set_scrollx_and_flip_reg(&fg, 0, 0xffff, 0);
	gp9001_set_scrolly_and_flip_reg(&fg, 0, 0xffff, 0);
	gp9001_set_scrollx_and_flip_reg(&top,0, 0xffff, 0);
	gp9001_set_scrolly_and_flip_reg(&top,0, 0xffff, 0);
	gp9001_set_sprite_scrollx_and_flip_reg(&sp, 0,0xffff,0);
	gp9001_set_sprite_scrolly_and_flip_reg(&sp, 0,0xffff,0);
}



READ16_MEMBER( gp9001vdp_device::gp9001_vdp_r )
{
	switch (offset & (0xc/2))
	{
		case 0x04/2:
			return gp9001_videoram16_r();

		case 0x0c/2:
			return gp9001_vdpstatus_r();

		default:
			logerror("gp9001_vdp_r: read from unhandled offset %04x\n",offset*2);
	}

	return 0xffff;
}

WRITE16_MEMBER( gp9001vdp_device::gp9001_vdp_w )
{
	switch (offset & (0xc/2))
	{
		case 0x00/2:
			gp9001_voffs_w(data, mem_mask);
			break;

		case 0x04/2:
			gp9001_videoram16_w(data, mem_mask);
			break;

		case 0x08/2:
			gp9001_scroll_reg_select_w(data, mem_mask);
			break;

		case 0x0c/2:
			gp9001_scroll_reg_data_w(data, mem_mask);
			break;
	}

}

/* batrider and bbakraid invert the register select lines */
READ16_MEMBER( gp9001vdp_device::gp9001_vdp_alt_r )
{
	switch (offset & (0xc/2))
	{
		case 0x0/2:
			return gp9001_vdpstatus_r();

		case 0x8/2:
			return gp9001_videoram16_r();

		default:
			logerror("gp9001_vdp_alt_r: read from unhandled offset %04x\n",offset*2);
	}

	return 0xffff;
}

WRITE16_MEMBER( gp9001vdp_device::gp9001_vdp_alt_w )
{
	switch (offset & (0xc/2))
	{
		case 0x0/2:
			gp9001_scroll_reg_data_w(data, mem_mask);
			break;

		case 0x4/2:
			gp9001_scroll_reg_select_w(data, mem_mask);
			break;

		case 0x8/2:
			gp9001_videoram16_w(data, mem_mask);
			break;

		case 0xc/2:
			gp9001_voffs_w(data, mem_mask);
			break;
	}
}



/***************************************************************************/
/**************** PIPIBIBI bootleg interface into this video driver ********/

WRITE16_MEMBER( gp9001vdp_device::pipibibi_bootleg_scroll_w )
{
	if (ACCESSING_BITS_8_15 && ACCESSING_BITS_0_7)
	{
		switch(offset)
		{
			case 0x00:  data -= 0x01f; break;
			case 0x01:  data += 0x1ef; break;
			case 0x02:  data -= 0x01d; break;
			case 0x03:  data += 0x1ef; break;
			case 0x04:  data -= 0x01b; break;
			case 0x05:  data += 0x1ef; break;
			case 0x06:  data += 0x1d4; break;
			case 0x07:  data += 0x1f7; break;
			default:    logerror("PIPIBIBI writing %04x to unknown scroll register %04x",data, offset);
		}

		gp9001_scroll_reg = offset;
		gp9001_scroll_reg_data_w(data, mem_mask);
	}
}

READ16_MEMBER( gp9001vdp_device::pipibibi_bootleg_videoram16_r )
{
	gp9001_voffs_w(offset, 0xffff);
	return gp9001_videoram16_r();
}

WRITE16_MEMBER( gp9001vdp_device::pipibibi_bootleg_videoram16_w )
{
	gp9001_voffs_w(offset, 0xffff);
	gp9001_videoram16_w(data, mem_mask);
}

READ16_MEMBER( gp9001vdp_device::pipibibi_bootleg_spriteram16_r )
{
	gp9001_voffs_w((0x1800 + offset), 0);
	return gp9001_videoram16_r();
}

WRITE16_MEMBER( gp9001vdp_device::pipibibi_bootleg_spriteram16_w )
{
	gp9001_voffs_w((0x1800 + offset), mem_mask);
	gp9001_videoram16_w(data, mem_mask);
}

/***************************************************************************
    Sprite Handlers
***************************************************************************/



#define DO_PIX \
	if (priority >= *dstpri) \
	{ \
		const UINT8 pix = *srcdata++; \
		if (pix & 0xf) \
		{ \
			*dstptr++ = pix | color; \
			*dstpri++ = priority; \
		} \
		else \
		{ \
			dstptr++; \
			dstpri++; \
		} \
	} \
	else \
	{ \
		dstptr++; \
		dstpri++; \
		srcdata++; \
	}


#define DO_PIX_REV \
	if (priority >= *dstpri) \
	{ \
		const UINT8 pix = *srcdata++; \
		if (pix & 0xf) \
		{ \
			*dstptr-- = pix | color; \
			*dstpri-- = priority; \
		} \
		else \
		{ \
			dstptr--; \
			dstpri--; \
		} \
	} \
	else \
	{ \
		dstptr--; \
		dstpri--; \
		srcdata++; \
	}

#define DO_PIX_NOINC \
	if (priority >= *dstpri) \
	{ \
		UINT8 pix = *srcdata; \
		if (pix & 0xf) \
		{ \
			*dstptr = pix | color; \
			*dstpri = priority;	\
		} \
	}


#define HANDLE_FLIPX_FLIPY__NOCLIPX_NOCLIPY \
	for (int yy = 7; yy != -1; yy += -1) \
	{ \
		int drawyy = yy + sy; \
		INT16* dstpri = (INT16*)&this->custom_priority_bitmap->pix16(drawyy) + sx + 7; \
		UINT16* dstptr = &bitmap.pix16(drawyy) + sx + 7; \
		DO_PIX_REV; \
		DO_PIX_REV; \
		DO_PIX_REV; \
		DO_PIX_REV; \
		DO_PIX_REV; \
		DO_PIX_REV; \
		DO_PIX_REV; \
		DO_PIX_REV; \
	}


#define HANDLE_FLIPX_FLIPY__NOCLIPX_CLIPY \
	for (int yy = 7; yy != -1; yy += -1) \
	{ \
		int drawyy = yy + sy; \
		if ((drawyy >= cliprect.min_y) && (drawyy <= cliprect.max_y)) \
		{ \
			INT16* dstpri = (INT16*)&this->custom_priority_bitmap->pix16(drawyy) + sx + 7; \
			UINT16* dstptr = &bitmap.pix16(drawyy) + sx + 7; \
			DO_PIX_REV; \
			DO_PIX_REV; \
			DO_PIX_REV; \
			DO_PIX_REV; \
			DO_PIX_REV; \
			DO_PIX_REV; \
			DO_PIX_REV; \
			DO_PIX_REV; \
		} \
		else \
		{ \
			srcdata += 8; \
		} \
	}

#define HANDLE_FLIPX_FLIPY_CLIPX_CLIPY \
	for (int yy = 7; yy != -1; yy += -1) \
	{ \
		int drawyy = yy + sy; \
		if ((drawyy >= cliprect.min_y) && (drawyy <= cliprect.max_y)) \
		{ \
			for (int xx = 7; xx != -1; xx += -1) \
			{ \
				int drawxx = xx + sx; \
				if ((drawxx >= cliprect.min_x) && (drawxx <= cliprect.max_x)) \
				{ \
					UINT16* dstptr = &bitmap.pix16(drawyy, drawxx); \
					INT16* dstpri = (INT16*)&this->custom_priority_bitmap->pix16(drawyy, drawxx); \
					DO_PIX_NOINC; \
				} \
				srcdata++; \
			} \
		} \
		else \
		{ \
			srcdata += 8; \
		} \
	}

#define HANDLE_NOFLIPX_FLIPY_NOCLIPX_NOCLIPY \
	for (int yy = 7; yy != -1; yy += -1) \
	{ \
		int drawyy = yy + sy; \
		INT16* dstpri = (INT16*)&this->custom_priority_bitmap->pix16(drawyy) + sx; \
		UINT16* dstptr = &bitmap.pix16(drawyy) + sx; \
		DO_PIX; \
		DO_PIX; \
		DO_PIX; \
		DO_PIX; \
		DO_PIX; \
		DO_PIX; \
		DO_PIX; \
		DO_PIX; \
	}

#define HANDLE_NOFLIPX_FLIPY_NOCLIPX_CLIPY \
	for (int yy = 7; yy != -1; yy += -1) \
	{ \
		int drawyy = yy + sy; \
		if ((drawyy >= cliprect.min_y) && (drawyy <= cliprect.max_y)) \
		{ \
			INT16* dstpri = (INT16*)&this->custom_priority_bitmap->pix16(drawyy) + sx; \
			UINT16* dstptr = &bitmap.pix16(drawyy) + sx; \
			DO_PIX; \
			DO_PIX; \
			DO_PIX; \
			DO_PIX; \
			DO_PIX; \
			DO_PIX; \
			DO_PIX; \
			DO_PIX; \
		} \
		else \
		{ \
			srcdata += 8; \
		} \
	}

#define HANDLE_NOFLIPX_FLIPY_CLIPX_CLIPY \
	for (int yy = 7; yy != -1; yy += -1) \
	{ \
		int drawyy = yy + sy; \
		if ((drawyy >= cliprect.min_y) && (drawyy <= cliprect.max_y)) \
		{ \
			for (int xx = 0; xx != 8; xx += 1) \
			{ \
				int drawxx = xx + sx; \
				if ((drawxx >= cliprect.min_x) && (drawxx <= cliprect.max_x)) \
				{ \
					UINT16* dstptr = &bitmap.pix16(drawyy, drawxx); \
					INT16* dstpri = (INT16*)&this->custom_priority_bitmap->pix16(drawyy, drawxx); \
					DO_PIX_NOINC; \
				} \
				srcdata++; \
			} \
		} \
		else \
		{ \
			srcdata += 8; \
		} \
	}


#define HANDLE_FLIPX_NOFLIPY_NOCLIPX_NOCLIPY \
	for (int yy = 0; yy != 8; yy += 1) \
	{ \
		int drawyy = yy + sy; \
		INT16* dstpri = (INT16*)&this->custom_priority_bitmap->pix16(drawyy) + sx + 7; \
		UINT16* dstptr = &bitmap.pix16(drawyy) + sx + 7; \
		DO_PIX_REV; \
		DO_PIX_REV; \
		DO_PIX_REV; \
		DO_PIX_REV; \
		DO_PIX_REV; \
		DO_PIX_REV; \
		DO_PIX_REV; \
		DO_PIX_REV; \
	}

#define HANDLE_FLIPX_NOFLIPY_NOCLIPX_CLIPY \
	for (int yy = 0; yy != 8; yy += 1) \
	{ \
		int drawyy = yy + sy; \
		if ((drawyy >= cliprect.min_y) && (drawyy <= cliprect.max_y)) \
		{ \
			INT16* dstpri = (INT16*)&this->custom_priority_bitmap->pix16(drawyy) + sx + 7; \
			UINT16* dstptr = &bitmap.pix16(drawyy) + sx + 7; \
			DO_PIX_REV; \
			DO_PIX_REV; \
			DO_PIX_REV; \
			DO_PIX_REV; \
			DO_PIX_REV; \
			DO_PIX_REV; \
			DO_PIX_REV; \
			DO_PIX_REV; \
		} \
		else \
		{ \
			srcdata += 8; \
		} \
	}

#define HANDLE_FLIPX_NOFLIPY_CLIPX_CLIPY \
	for (int yy = 0; yy != 8; yy += 1) \
	{ \
		int drawyy = yy + sy; \
		if ((drawyy >= cliprect.min_y) && (drawyy <= cliprect.max_y)) \
		{ \
			for (int xx = 7; xx != -1; xx += -1) \
			{ \
				int drawxx = xx + sx; \
				if ((drawxx >= cliprect.min_x) && (drawxx <= cliprect.max_x)) \
				{ \
					UINT16* dstptr = &bitmap.pix16(drawyy, drawxx); \
					INT16* dstpri = (INT16*)&this->custom_priority_bitmap->pix16(drawyy, drawxx); \
					DO_PIX_NOINC; \
				} \
				srcdata++; \
			} \
		} \
		else \
		{ \
			srcdata += 8; \
		} \
	}


#define HANDLE_NOFLIPX_NOFLIPY_NOCLIPX_NOCLIPY \
	for (int yy = 0; yy != 8; yy += 1) \
	{ \
		int drawyy = yy + sy; \
		INT16* dstpri = (INT16*)&this->custom_priority_bitmap->pix16(drawyy) + sx; \
		UINT16* dstptr = &bitmap.pix16(drawyy) + sx; \
		DO_PIX; \
		DO_PIX; \
		DO_PIX; \
		DO_PIX; \
		DO_PIX; \
		DO_PIX; \
		DO_PIX; \
		DO_PIX; \
	}

#define HANDLE_NOFLIPX_NOFLIPY_NOCLIPX_CLIPY \
	for (int yy = 0; yy != 8; yy += 1) \
	{ \
		int drawyy = yy + sy; \
		if ((drawyy >= cliprect.min_y) && (drawyy <= cliprect.max_y)) \
		{ \
			INT16* dstpri = (INT16*)&this->custom_priority_bitmap->pix16(drawyy) + sx; \
			UINT16* dstptr = &bitmap.pix16(drawyy) + sx; \
			DO_PIX; \
			DO_PIX; \
			DO_PIX; \
			DO_PIX; \
			DO_PIX; \
			DO_PIX; \
			DO_PIX; \
			DO_PIX; \
		} \
		else \
		{ \
			srcdata += 8; \
		} \
	}

#define HANDLE_NOFLIPX_NOFLIPY_CLIPX_CLIPY \
	for (int yy = 0; yy != 8; yy += 1) \
	{ \
		int drawyy = yy + sy; \
		if ((drawyy >= cliprect.min_y) && (drawyy <= cliprect.max_y)) \
		{ \
			for (int xx = 0; xx != 8; xx += 1) \
			{ \
				int drawxx = xx + sx; \
				if ((drawxx >= cliprect.min_x) && (drawxx <= cliprect.max_x)) \
				{ \
					UINT16* dstptr = &bitmap.pix16(drawyy, drawxx); \
					INT16* dstpri = (INT16*)&this->custom_priority_bitmap->pix16(drawyy, drawxx); \
					DO_PIX_NOINC; \
				} \
				srcdata++; \
			} \
		} \
		else \
		{ \
			srcdata += 8; \
		} \
	}

#define HANDLE_NOFLIPX_NOFLIPY_CLIPX_NOCLIPY \
	for (int yy = 0; yy != 8; yy += 1) \
	{ \
		int drawyy = yy + sy; \
		for (int xx = 0; xx != 8; xx += 1) \
		{ \
			int drawxx = xx + sx; \
			if ((drawxx >= cliprect.min_x) && (drawxx <= cliprect.max_x)) \
			{ \
				UINT16* dstptr = &bitmap.pix16(drawyy, drawxx); \
				INT16* dstpri = (INT16*)&this->custom_priority_bitmap->pix16(drawyy, drawxx); \
				DO_PIX_NOINC; \
			} \
			srcdata++; \
		} \
	}

#define HANDLE_FLIPX_NOFLIPY_CLIPX_NOCLIPY \
	for (int yy = 0; yy != 8; yy += 1) \
	{ \
		int drawyy = yy + sy; \
		for (int xx = 7; xx != -1; xx += -1) \
		{ \
			int drawxx = xx + sx; \
			if ((drawxx >= cliprect.min_x) && (drawxx <= cliprect.max_x)) \
			{ \
				UINT16* dstptr = &bitmap.pix16(drawyy, drawxx); \
				INT16* dstpri = (INT16*)&this->custom_priority_bitmap->pix16(drawyy, drawxx); \
				DO_PIX_NOINC; \
			} \
			srcdata++; \
		} \
	}

#define HANDLE_NOFLIPX_FLIPY_CLIPX_NOCLIPY \
	for (int yy = 7; yy != -1; yy += -1) \
	{ \
		int drawyy = yy + sy; \
		for (int xx = 0; xx != 8; xx += 1) \
		{ \
			int drawxx = xx + sx; \
			if ((drawxx >= cliprect.min_x) && (drawxx <= cliprect.max_x)) \
			{ \
				UINT16* dstptr = &bitmap.pix16(drawyy, drawxx); \
				INT16* dstpri = (INT16*)&this->custom_priority_bitmap->pix16(drawyy, drawxx); \
				DO_PIX_NOINC; \
			} \
			srcdata++; \
		} \
	}

#define HANDLE_FLIPX_FLIPY_CLIPX_NOCLIPY \
	for (int yy = 7; yy != -1; yy += -1) \
	{ \
		int drawyy = yy + sy; \
		for (int xx = 7; xx != -1; xx += -1) \
		{ \
			int drawxx = xx + sx; \
			if ((drawxx >= cliprect.min_x) && (drawxx <= cliprect.max_x)) \
			{ \
				UINT16* dstptr = &bitmap.pix16(drawyy, drawxx); \
				INT16* dstpri = (INT16*)&this->custom_priority_bitmap->pix16(drawyy, drawxx); \
				DO_PIX_NOINC; \
			} \
			srcdata++; \
		} \
	}

void gp9001vdp_device::draw_sprites( bitmap_ind16 &bitmap, const rectangle &cliprect )
{
	const UINT16 primask = (GP9001_PRIMASK << 8);

	UINT16 *source;

	if (sp.use_sprite_buffer) source = sp.vram16_buffer.get();
	else source = &m_spriteram[0];
	int total_elements = gfx(1)->elements();

	int old_x = (-(sp.scrollx)) & 0x1ff;
	int old_y = (-(sp.scrolly)) & 0x1ff;

	for (int offs = 0; offs < (GP9001_SPRITERAM_SIZE/2); offs += 4)
	{
		const int attrib = source[offs];
		const int priority = ((attrib & primask) >> 8) << 12;// << 3;
			
	//	priority+=1;

		if ((attrib & 0x8000))
		{
			int sprite;
			if (!gp9001_gfxrom_is_banked)   /* No Sprite select bank switching needed */
			{
				sprite = ((attrib & 3) << 16) | source[offs + 1];   /* 18 bit */
			}
			else        /* Batrider Sprite select bank switching required */
			{
				const int sprite_num = source[offs + 1] & 0x7fff;
				const int bank = ((attrib & 3) << 1) | (source[offs + 1] >> 15);
				sprite = (gp9001_gfxrom_bank[bank] << 15 ) | sprite_num;
			}
			const int color = ((attrib >> 2) & 0x3f) << 4;

			/***** find out sprite size *****/
			const int sprite_sizex = ((source[offs + 2] & 0x0f) + 1) * 8;
			const int sprite_sizey = ((source[offs + 3] & 0x0f) + 1) * 8;

			/***** find position to display sprite *****/
			int sx_base, sy_base;
			if (!(attrib & 0x4000))
			{
				sx_base = ((source[offs + 2] >> 7) - (sp.scrollx)) & 0x1ff;
				sy_base = ((source[offs + 3] >> 7) - (sp.scrolly)) & 0x1ff;

			} else {
				sx_base = (old_x + (source[offs + 2] >> 7)) & 0x1ff;
				sy_base = (old_y + (source[offs + 3] >> 7)) & 0x1ff;
			}

			old_x = sx_base;
			old_y = sy_base;

			int flipx = attrib & GP9001_SPRITE_FLIPX;
			int flipy = attrib & GP9001_SPRITE_FLIPY;

			if (flipx)
			{
				/***** Wrap sprite position around *****/
				sx_base -= 7;
				if (sx_base >= 0x1c0) sx_base -= 0x200;
			}
			else
			{
				if (sx_base >= 0x180) sx_base -= 0x200;
			}

			if (flipy)
			{
				sy_base -= 7;
				if (sy_base >= 0x1c0) sy_base -= 0x200;
			}
			else
			{
				if (sy_base >= 0x180) sy_base -= 0x200;
			}

			/***** Flip the sprite layer in any active X or Y flip *****/
			if (sp.flip)
			{
				if (sp.flip & GP9001_SPRITE_FLIPX)
					sx_base = 320 - sx_base;
				if (sp.flip & GP9001_SPRITE_FLIPY)
					sy_base = 240 - sy_base;
			}

			/***** Cancel flip, if it, and sprite layer flip are active *****/
			flipx = (flipx ^ (sp.flip & GP9001_SPRITE_FLIPX));
			flipy = (flipy ^ (sp.flip & GP9001_SPRITE_FLIPY));

			/***** Draw the complete sprites using the dimension info *****/
			for (int dim_y = 0; dim_y < sprite_sizey; dim_y += 8)
			{
				int sy;
				if (flipy) sy = sy_base - dim_y;
				else       sy = sy_base + dim_y;

				if ((sy > cliprect.max_y) || (sy + 7 < cliprect.min_y))
				{
					sprite+=sprite_sizex >> 3;
					continue;
				}

				for (int dim_x = 0; dim_x < sprite_sizex; dim_x += 8)
				{
					int sx;
					if (flipx) sx = sx_base - dim_x;
					else       sx = sx_base + dim_x;

					if ((sx > cliprect.max_x) || (sx + 7 < cliprect.min_x))
					{
						sprite++;
						continue;
					}

					sprite &= total_elements-1;
					//color %= total_colors;
					//const pen_t *paldata = &palette().pen(color * 16);
					{
						const UINT8* srcdata = gfx(1)->get_data(sprite);

						if (flipy) // FLIPY 
						{
							if (flipx) // FLIPY AND FLIP X
							{
								// --------------------------
								if ((sx >= cliprect.min_x) && (sx + 7 <= cliprect.max_x))
								{ // x unclipped
									if ((sy >= cliprect.min_y) && (sy + 7 <= cliprect.max_y))
									{  // fully unclipped
										HANDLE_FLIPX_FLIPY__NOCLIPX_NOCLIPY;
									}
									else
									{ // x unclipped, y clipped
										HANDLE_FLIPX_FLIPY__NOCLIPX_CLIPY;
									}
								}
								else
								{  // FLIPY AND FLIP X fully clipped
									if ((sy >= cliprect.min_y) && (sy + 7 <= cliprect.max_y))
									{
										HANDLE_FLIPX_FLIPY_CLIPX_NOCLIPY;
									}
									else
									{
										HANDLE_FLIPX_FLIPY_CLIPX_CLIPY;
									}
								}

								// --------------------------
							}
							else // FLIPY AND NO FLIPX
							{
								// --------------------------

								if ((sx >= cliprect.min_x) && (sx + 7 <= cliprect.max_x))
								{ // x doesn't need clipping
									if ((sy >= cliprect.min_y) && (sy + 7 <= cliprect.max_y))
									{  // fully unclipped
#if 1
										HANDLE_NOFLIPX_FLIPY_NOCLIPX_NOCLIPY;
#endif
									}
									else
									{  // y needs clipping
#if 1
										HANDLE_NOFLIPX_FLIPY_NOCLIPX_CLIPY;
#endif
									}

								}
								else // with clipping
								{
									if ((sy >= cliprect.min_y) && (sy + 7 <= cliprect.max_y))
									{
										HANDLE_NOFLIPX_FLIPY_CLIPX_NOCLIPY;
									}
									else
									{
										HANDLE_NOFLIPX_FLIPY_CLIPX_CLIPY;
									}
								}

								// --------------------------
							}
						}
						else  // NO FLIP Y
						{
							if (flipx)  // NO FLIP Y, FLIP X
							{
								// -------------------------
								if ((sx >= cliprect.min_x) && (sx + 7 <= cliprect.max_x))
								{ // x doesn't need clipping
									if ((sy >= cliprect.min_y) && (sy + 7 <= cliprect.max_y))
									{  // fully unclipped
#if 1
										HANDLE_FLIPX_NOFLIPY_NOCLIPX_NOCLIPY;
#endif
									}
									else // x not clipped, y is
									{
#if 1
										HANDLE_FLIPX_NOFLIPY_NOCLIPX_CLIPY;
#endif
									}
								}
								else // clipping required
								{
									if ((sy >= cliprect.min_y) && (sy + 7 <= cliprect.max_y))
									{
										HANDLE_FLIPX_NOFLIPY_CLIPX_NOCLIPY;
									}
									else
									{
										HANDLE_FLIPX_NOFLIPY_CLIPX_CLIPY;
									}
								}
								// -------------------
							}
							else  // NO FLIP Y, NO FLIP X
							{
								// -------------------------
			
								if ((sx >= cliprect.min_x) && (sx + 7 <= cliprect.max_x))
								{ // x doesn't need clipping
									if ((sy >= cliprect.min_y) && (sy + 7 <= cliprect.max_y))
									{  // fully unclipped
#if 1
										HANDLE_NOFLIPX_NOFLIPY_NOCLIPX_NOCLIPY;
#endif
									}
									else
									{ // y needs clipping
#if 1
										HANDLE_NOFLIPX_NOFLIPY_NOCLIPX_CLIPY;
#endif
									}

								}
								else // with clipping
								{
									if ((sy >= cliprect.min_y) && (sy + 7 <= cliprect.max_y))
									{
										HANDLE_NOFLIPX_NOFLIPY_CLIPX_NOCLIPY;
									}
									else
									{
										HANDLE_NOFLIPX_NOFLIPY_CLIPX_CLIPY;
									}
								}

								// -------------------
							}
						}
					}

					sprite++;
				}
			}
		}
	}
}


/***************************************************************************
    Draw the game screen in the given bitmap_ind16.
***************************************************************************/


void gp9001vdp_device::gp9001_draw_mixed_tilemap(bitmap_ind16& bitmap, const rectangle &cliprect)
{
	bitmap_ind16& tmbbg = bg.tmap->pixmap();
	bitmap_ind16& tmbfg = fg.tmap->pixmap();
	bitmap_ind16& tmbtop = top.tmap->pixmap();

	UINT16* srcptrbg;
	UINT16* srcptrfg;
	UINT16* srcptrtop;


	UINT16* dstptr;
	INT16* dstpriptr;

	int scrollxbg = bg.tmap->scrollx(0);
	int scrollybg = bg.tmap->scrolly(0);
	int scrollxfg = fg.tmap->scrollx(0);
	int scrollyfg = fg.tmap->scrolly(0);
	int scrollxtop = top.tmap->scrollx(0);
	int scrollytop = top.tmap->scrolly(0);

	for (int y = cliprect.min_y; y <= cliprect.max_y; y++)
	{
		int realybg = (y + scrollybg) & 0x1ff;
		int realyfg = (y + scrollyfg) & 0x1ff;
		int realytop = (y + scrollytop) & 0x1ff;

		srcptrbg = &tmbbg.pix16(realybg);
		srcptrfg = &tmbfg.pix16(realyfg);
		srcptrtop = &tmbtop.pix16(realytop);

		dstptr = &bitmap.pix16(y);
		dstpriptr = (INT16*)&this->custom_priority_bitmap->pix16(y);

		for (int x = cliprect.min_x; x <= cliprect.max_x; x++)
		{
			int realxbg = (x + scrollxbg) & 0x1ff;
			int realxfg = (x + scrollxfg) & 0x1ff;
			int realxtop = (x + scrollxtop) & 0x1ff;

			UINT16 pixdatbg = srcptrbg[realxbg];
			UINT16 pixdatfg = srcptrfg[realxfg];
			UINT16 pixdattop = srcptrtop[realxtop];

			if (pixdattop & 0xf) // if top pixel is visible (and mid + bottom potentially are)
			{
				if (pixdatfg & 0xf) // if top pixel is visible, mid pixel is visible, and bottom potentially is
				{
					if (pixdatbg & 0xf) // if top pixel is visible, mid pixel is visible and bottom pixel is visible
					{
						UINT16 pixpritop = (pixdattop & 0xe000);
						UINT16 pixprifg = (pixdatfg & 0xe000);
						UINT16 pixpribg = (pixdatbg & 0xe000);

						if (pixpribg > pixprifg) // bg is greater than fg, so fg will never be drawn
						{
							if (pixpribg > pixpritop) // bg is greater than top, so bg is drawn
							{
								//pixpribg >>= 12;
								if (pixpribg > *dstpriptr)
								{
									*dstptr++ = pixdatbg &0x07ff;
									*dstpriptr++ = pixpribg;
								}
								else
								{
									// sprite wins
									dstptr++;
									dstpriptr++;
								}
							}
							else
							{
								//pixpritop >>= 12;
								if (pixpritop > *dstpriptr)
								{
									*dstptr++ = pixdattop &0x07ff;
									*dstpriptr++ = pixpritop;
								}
								else
								{
									// sprite wins
									dstptr++;
									dstpriptr++;
								}
							}

						}
						else // fg is greater than bg, so bg will never be drawn
						{
							if (pixprifg > pixpritop) // fg is greater than top, so top never drawn
							{
								//pixprifg >>= 12;
								if (pixprifg > *dstpriptr)
								{
									*dstptr++ = pixdatfg &0x07ff;
									*dstpriptr++ = pixprifg;
								}
								else
								{
									// sprite wins
									dstptr++;
									dstpriptr++;
								}
							}
							else // fg is lower than top, so fg never drawn
							{
								//pixpritop >>= 12;
								if (pixpritop > *dstpriptr)
								{
									*dstptr++ = pixdattop &0x07ff;
									*dstpriptr++ = pixpritop;
								}
								else
								{
									// sprite wins
									dstptr++;
									dstpriptr++;
								}
							}
						}
					}
					else // if top pixel is visible, mid pixel is visible and bottom is not
					{
						UINT16 pixpritop = (pixdattop & 0xe000);
						UINT16 pixprifg = (pixdatfg & 0xe000);

						if (pixprifg > pixpritop)
						{
							//pixprifg >>= 12;
							if (pixprifg > *dstpriptr)
							{
								*dstptr++ = pixdatfg &0x07ff;
								*dstpriptr++ = pixprifg;
							}
							else
							{
								// sprite wins
								dstptr++;
								dstpriptr++;
							}
						}
						else
						{
							//pixpritop >>= 12;
							if (pixpritop > *dstpriptr)
							{
								*dstptr++ = pixdattop &0x07ff;
								*dstpriptr++ = pixpritop;
							}
							else
							{
								// sprite wins
								dstptr++;
								dstpriptr++;
							}
						}
					}
				}
				else // if top pixel is visible, mid pixel is NOT, but bottom potentially is
				{
					if (pixdatbg & 0xf) // if top pixel is visible, mid pixel is NOT, but bottom IS
					{
						UINT16 pixpritop = (pixdattop & 0xe000);
						UINT16 pixpribg = (pixdatbg & 0xe000);

						if (pixpribg > pixpritop)
						{
							//pixpribg >>= 12;
							if (pixpribg > *dstpriptr)
							{
								*dstptr++ = pixdatbg &0x07ff;
								*dstpriptr++ = pixpribg;
							}
							else
							{
								// sprite wins
								dstptr++;
								dstpriptr++;
							}
						}
						else
						{
							//pixpritop >>= 12;
							if (pixpritop > *dstpriptr)
							{
								*dstptr++ = pixdattop &0x07ff;
								*dstpriptr++ = pixpritop;
							}
							else
							{
								// sprite wins
								dstptr++;
								dstpriptr++;
							}
						}
					}
					else  // if top pixel is visible mid is not, bottom is not
					{
						UINT16 pixpritop = (pixdattop & 0xe000);
						if (pixpritop > *dstpriptr)
						{
							*dstptr++ = pixdattop &0x07ff;
							*dstpriptr++ = pixpritop;
						}
						else
						{
							// sprite wins
							dstptr++;
							dstpriptr++;
						}
					}
				}
			}
			else if (pixdatfg & 0xf)  // if top pixel isn't visible but mid pixel is and bottom potentially is
			{
				if (pixdatbg & 0xf) // if top pixel isn't visible, but mid and bottom are
				{

					UINT16 pixprifg = (pixdatfg & 0xe000);
					UINT16 pixpribg = (pixdatbg & 0xe000);

					if (pixpribg > pixprifg)
					{
						//pixpribg >>= 12;
						if (pixpribg > *dstpriptr)
						{
							*dstptr++ = pixdatbg &0x07ff;
							*dstpriptr++ = pixpribg;
						}
						else
						{
							// sprite wins
							dstptr++;
							dstpriptr++;
						}
					}
					else
					{
						//pixprifg >>= 12;
						if (pixprifg > *dstpriptr)
						{
							*dstptr++ = pixdatfg &0x07ff;
							*dstpriptr++ = pixprifg;
						}
						else
						{
							// sprite wins
							dstptr++;
							dstpriptr++;
						}
					}
				}
				else // if top pixel isn't visible, but mid is, and bottom isn't
				{
					UINT16 pixprifg = (pixdatfg & 0xe000);
					if (pixprifg > *dstpriptr)
					{
						*dstptr++ = pixdatfg &0x07ff;
						*dstpriptr++ = pixprifg;
					}
					else
					{
						// sprite wins
						dstptr++;
						dstpriptr++;
					}
				}
			}
			else if (pixdatbg & 0xf)  // if top pixel and mid pixels aren't visible, but bottom is
			{
				UINT16 pixpribg = (pixdatbg & 0xe000);
				if (pixpribg > *dstpriptr)
				{
					*dstptr++ = pixdatbg &0x07ff;
					*dstpriptr++ = pixpribg;
				}
				else
				{
					// sprite wins
					dstptr++;
					dstpriptr++;
				}

			}
			else
			{
				// sprite wins
				dstptr++;
				dstpriptr++;
			}
		}
	}
}




void gp9001vdp_device::gp9001_render_vdp(bitmap_ind16 &bitmap, const rectangle &cliprect)
{
	if (gp9001_gfxrom_is_banked && gp9001_gfxrom_bank_dirty)
	{
		bg.tmap->mark_all_dirty();
		fg.tmap->mark_all_dirty();
		gp9001_gfxrom_bank_dirty = 0;
	}

	draw_sprites(bitmap,cliprect);
	gp9001_draw_mixed_tilemap(bitmap, cliprect);
}


void gp9001vdp_device::gp9001_screen_eof(void)
{
	/** Shift sprite RAM buffers  ***  Used to fix sprite lag **/
	if (sp.use_sprite_buffer) memcpy(sp.vram16_buffer.get(),m_spriteram,GP9001_SPRITERAM_SIZE);
}
