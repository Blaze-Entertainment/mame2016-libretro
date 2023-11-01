// license:BSD-3-Clause
// copyright-holders:Quench, Yochizo, David Haywood
/***************************************************************************

 Functions to emulate additional video hardware on several Toaplan2 games.
 The main video is handled by the GP9001 (see video/gp9001.c)

 Extra-text RAM format

 Truxton 2, Fixeight and Raizing games have an extra-text layer.

  Text RAM format      $0000-1FFF (actually its probably $0000-0FFF)
  ---- --xx xxxx xxxx = Tile number
  xxxx xx-- ---- ---- = Color (0 - 3Fh) + 40h

  Line select / flip   $0000-01EF (some games go to $01FF (excess?))
  ---x xxxx xxxx xxxx = Line select for each line
  x--- ---- ---- ---- = X flip for each line ???

  Line scroll          $0000-01EF (some games go to $01FF (excess?))
  ---- ---x xxxx xxxx = X scroll for each line


***************************************************************************/

#include "emu.h"
#include "includes/toaplan2.h"

#define RAIZING_TX_GFXRAM_SIZE  0x8000  /* GFX data decode RAM size */



/***************************************************************************

  Callbacks for the TileMap code

***************************************************************************/

TILE_GET_INFO_MEMBER(toaplan2_state::get_text_tile_info)
{
	int color, tile_number, attrib;

	attrib = m_tx_videoram[tile_index];
	tile_number = attrib & 0x3ff;
	color = attrib >> 10;
	SET_TILE_INFO_MEMBER(2,
			tile_number,
			color,
			0);
}

/***************************************************************************

  Start the video hardware emulation.

***************************************************************************/


void toaplan2_state::create_tx_tilemap(int dx, int dx_flipped)
{
	m_tx_tilemap = &machine().tilemap().create(m_gfxdecode, tilemap_get_info_delegate(FUNC(toaplan2_state::get_text_tile_info),this), TILEMAP_SCAN_ROWS, 8, 8, 64, 32);

	m_tx_tilemap->set_scroll_rows(8*32); /* line scrolling */
	m_tx_tilemap->set_scroll_cols(1);
	m_tx_tilemap->set_scrolldx(dx, dx_flipped);
	m_tx_tilemap->set_transparent_pen(0);
}

void toaplan2_state::truxton2_postload()
{
	m_gfxdecode->gfx(2)->mark_all_dirty();
}

VIDEO_START_MEMBER(toaplan2_state,toaplan2)
{
	vdp_start();
	second_vdp_start();
	/*
	for (int col = 0; col < 0x10000; col++)
	{
		rgb_t rgb = rgb_t(pal5bit(col & 0x1f), pal5bit(col >> 5), pal5bit(col >> 10));
		expanded_palette[col] = rgb;
	}
	*/

	m_secondary_render_bitmap.reset();

	/* our current VDP implementation needs this bitmap to work with */
	m_screen->register_screen_bitmap(m_secondary_render_bitmap);

}

VIDEO_START_MEMBER(toaplan2_state,truxton2)
{
	VIDEO_START_CALL_MEMBER( toaplan2 );

	/* Create the Text tilemap for this game */
	m_gfxdecode->gfx(2)->set_source(reinterpret_cast<UINT8 *>(m_tx_gfxram16.target()));
	machine().save().register_postload(save_prepost_delegate(FUNC(toaplan2_state::truxton2_postload), this));

	create_tx_tilemap(0x1d5, 0x16a);
}

VIDEO_START_MEMBER(toaplan2_state,fixeightbl)
{
	VIDEO_START_CALL_MEMBER( toaplan2 );

	/* Create the Text tilemap for this game */
	create_tx_tilemap();

	/* This bootleg has additional layer offsets on the VDP */
	bg.extra_xoffset.normal  = -0x1d6  -26;
	bg.extra_yoffset.normal  = -0x1ef  -15;

	fg.extra_xoffset.normal  = -0x1d8  -22;
	fg.extra_yoffset.normal  = -0x1ef  -15;

	top.extra_xoffset.normal = -0x1da  -18;
	top.extra_yoffset.normal = -0x1ef  -15;

	sp.extra_xoffset.normal  = 8;//-0x1cc  -64;
	sp.extra_yoffset.normal  = 8;//-0x1ef  -128;

	init_scroll_regs();
}

VIDEO_START_MEMBER(toaplan2_state,bgaregga)
{
	VIDEO_START_CALL_MEMBER( toaplan2 );

	/* Create the Text tilemap for this game */
	create_tx_tilemap(0x1d4, 0x16b);
}

VIDEO_START_MEMBER(toaplan2_state,bgareggabl)
{
	VIDEO_START_CALL_MEMBER( toaplan2 );

	/* Create the Text tilemap for this game */
	create_tx_tilemap(4, 4);
}

VIDEO_START_MEMBER(toaplan2_state,batrider)
{
	VIDEO_START_CALL_MEMBER( toaplan2 );

	sp.use_sprite_buffer = 0; // disable buffering on this game

	/* Create the Text tilemap for this game */
	m_tx_gfxram16.allocate(RAIZING_TX_GFXRAM_SIZE/2);
	m_gfxdecode->gfx(2)->set_source(reinterpret_cast<UINT8 *>(m_tx_gfxram16.target()));
	machine().save().register_postload(save_prepost_delegate(FUNC(toaplan2_state::truxton2_postload), this));

	create_tx_tilemap(0x1d4, 0x16b);

	/* Has special banking */
	gp9001_gfxrom_is_banked = 1;
}

WRITE16_MEMBER(toaplan2_state::toaplan2_tx_videoram_w)
{
	COMBINE_DATA(&m_tx_videoram[offset]);
	if (offset < 64*32)
		m_tx_tilemap->mark_tile_dirty(offset);
}

WRITE16_MEMBER(toaplan2_state::toaplan2_tx_linescroll_w)
{
	/*** Line-Scroll RAM for Text Layer ***/
	COMBINE_DATA(&m_tx_linescroll[offset]);

	m_tx_tilemap->set_scrollx(offset, m_tx_linescroll[offset]);
}

WRITE16_MEMBER(toaplan2_state::toaplan2_tx_gfxram16_w)
{
	/*** Dynamic GFX decoding for Truxton 2 / FixEight ***/

	UINT16 oldword = m_tx_gfxram16[offset];

	if (oldword != data)
	{
		COMBINE_DATA(&m_tx_gfxram16[offset]);
		m_gfxdecode->gfx(2)->mark_dirty(offset/32);
	}
}

WRITE16_MEMBER(toaplan2_state::batrider_textdata_dma_w)
{
	/*** Dynamic Text GFX decoding for Batrider ***/
	/*** Only done once during start-up ***/

	UINT16 *dest = m_tx_gfxram16;

	memcpy(dest, m_tx_videoram, m_tx_videoram.bytes());
	dest += (m_tx_videoram.bytes()/2);
	memcpy(dest, m_paletteram, m_paletteram.bytes());
	dest += (m_paletteram.bytes()/2);
	memcpy(dest, m_tx_lineselect, m_tx_lineselect.bytes());
	dest += (m_tx_lineselect.bytes()/2);
	memcpy(dest, m_tx_linescroll, m_tx_linescroll.bytes());
	dest += (m_tx_linescroll.bytes()/2);
	memcpy(dest, m_mainram16, m_mainram16.bytes());

	m_gfxdecode->gfx(2)->mark_all_dirty();
}

WRITE16_MEMBER(toaplan2_state::batrider_unknown_dma_w)
{
	// FIXME: In batrider and bbakraid, the text layer and palette RAM
	// are probably DMA'd from main RAM by writing here at every vblank,
	// rather than being directly accessible to the 68K like the other games
}

WRITE16_MEMBER(toaplan2_state::batrider_objectbank_w)
{
	if (ACCESSING_BITS_0_7)
	{
		data &= 0xf;
		if (gp9001_gfxrom_bank[offset] != data)
		{
			gp9001_gfxrom_bank[offset] = data;
			gp9001_gfxrom_bank_dirty = 1;
		}
	}
}


// Dogyuun doesn't appear to require fancy mixing?
UINT32 toaplan2_state::screen_update_dogyuun(screen_device &screen, bitmap_ind16 &bitmap, const rectangle &cliprect)
{
	//if (m_vdp1)
	{
		m_secondary_render_bitmap.fill(0, cliprect);
		second_gp9001_render_vdp(m_secondary_render_bitmap, cliprect);
	}
	//if (m_vdp0)
	//{
		bitmap.fill(0, cliprect);
		gp9001_render_vdp(bitmap, cliprect);
	//}

	{
		int width = screen.width();
		int height = screen.height();
		int y,x;
		UINT16* src_vdp0; // output buffer of vdp0
		UINT16* src_vdp1; // output buffer of vdp1

		for (y=0;y<height;y++)
		{
			src_vdp0 = &bitmap.pix16(y);
			src_vdp1 = &m_secondary_render_bitmap.pix16(y);

			for (x=0;x<width;x++)
			{
				UINT16 GPU0_LUTaddr = src_vdp0[x];
				UINT16 GPU1_LUTaddr = src_vdp1[x];

				if (GPU0_LUTaddr & 0x000f)
				{
					src_vdp0[x] = GPU0_LUTaddr;
				}
				else
				{
					src_vdp0[x] = GPU1_LUTaddr;
				}
			}
		}
	}

	return 0;
}


// renders to 2 bitmaps, and mixes output
UINT32 toaplan2_state::screen_update_batsugun(screen_device &screen, bitmap_ind16 &bitmap, const rectangle &cliprect)
{
//  bitmap.fill(0, cliprect);

	//if (m_vdp0)
	//{
		bitmap.fill(0, cliprect);
		gp9001_render_vdp(bitmap, cliprect);
	//}
	//if (m_vdp1)
	{
		m_secondary_render_bitmap.fill(0, cliprect);
		second_gp9001_render_vdp(m_secondary_render_bitmap, cliprect);
	}


	// key test places in batsugun
	// level 2 - the two layers of clouds (will appear under background, or over ships if wrong)
	// level 3 - the special effect 'layer' which should be under everything (will appear over background if wrong)
	// level 4(?) - the large clouds (will obscure player if wrong)
	// high score entry - letters will be missing if wrong
	// end credits - various issues if wrong, clouds like level 2
	//
	// when implemented based directly on the PAL equation it doesn't work, however, my own equations roughly based
	// on that do.
	//

#if 1
	//if (m_vdp1)
	{
		int width = screen.width();
		int height = screen.height();
		int y,x;
		UINT16* src_vdp0; // output buffer of vdp0
		UINT16* src_vdp1; // output buffer of vdp1

		for (y=0;y<height;y++)
		{
			src_vdp0 = &bitmap.pix16(y);
			src_vdp1 = &m_secondary_render_bitmap.pix16(y);

			for (x=0;x<width;x++)
			{
				UINT16 GPU0_LUTaddr = src_vdp0[x];
				UINT16 GPU1_LUTaddr = src_vdp1[x];

				// these equations is derived from the PAL, but doesn't seem to work?

				int COMPARISON = ((GPU0_LUTaddr & 0x0780) > (GPU1_LUTaddr & 0x0780));

				// note: GPU1_LUTaddr & 0x000f - transparency check for vdp1? (gfx are 4bpp, the low 4 bits of the lookup would be the pixel data value)
#if 0
				int result =
							((GPU0_LUTaddr & 0x0008) & !COMPARISON)
						| ((GPU0_LUTaddr & 0x0008) & !(GPU1_LUTaddr & 0x000f))
						| ((GPU0_LUTaddr & 0x0004) & !COMPARISON)
						| ((GPU0_LUTaddr & 0x0004) & !(GPU1_LUTaddr & 0x000f))
						| ((GPU0_LUTaddr & 0x0002) & !COMPARISON)
						| ((GPU0_LUTaddr & 0x0002) & !(GPU1_LUTaddr & 0x000f))
						| ((GPU0_LUTaddr & 0x0001) & !COMPARISON)
						| ((GPU0_LUTaddr & 0x0001) & !(GPU1_LUTaddr & 0x000f));

				if (result) src_vdp0[x] = GPU0_LUTaddr;
				else src_vdp0[x] = GPU1_LUTaddr;
#endif
				// this seems to work tho?
				if (!(GPU1_LUTaddr & 0x000f))
				{
					src_vdp0[x] = GPU0_LUTaddr;
				}
				else
				{
					if (!(GPU0_LUTaddr & 0x000f))
					{
						src_vdp0[x] = GPU1_LUTaddr; // bg pen
					}
					else
					{
						if (COMPARISON)
						{
							src_vdp0[x] = GPU1_LUTaddr;
						}
						else
						{
							src_vdp0[x] = GPU0_LUTaddr;
						}

					}
				}
			}
		}
	}
#endif
	return 0;
}


UINT32 toaplan2_state::screen_update_toaplan2(screen_device &screen, bitmap_ind16 &bitmap, const rectangle &cliprect)
{
	bitmap.fill(0, cliprect);
	gp9001_render_vdp(bitmap, cliprect);

	return 0;
}


/* fixeightbl and bgareggabl do not use the lineselect or linescroll tables */
UINT32 toaplan2_state::screen_update_bootleg(screen_device &screen, bitmap_ind16 &bitmap, const rectangle &cliprect)
{
#if 1
	screen_update_toaplan2(screen, bitmap, cliprect);
	m_tx_tilemap->draw(screen, bitmap, cliprect, 0);
#endif
	return 0;
}


UINT32 toaplan2_state::screen_update_truxton2(screen_device &screen, bitmap_ind16 &bitmap, const rectangle &cliprect)
{

	bitmap.fill(0, cliprect);

	gp9001_draw_a_tilemap_nopri(bitmap, cliprect, &bg, m_vram_bg);
	gp9001_draw_a_tilemap(bitmap, cliprect, &fg, m_vram_fg);
	gp9001_draw_a_tilemap(bitmap, cliprect, &top, m_vram_top);
	draw_sprites(bitmap,cliprect);

	rectangle clip = cliprect;

	/* it seems likely that flipx can be set per line! */
	/* however, none of the games does it, and emulating it in the */
	/* MAME tilemap system without being ultra slow would be tricky */
	m_tx_tilemap->set_flip(m_tx_lineselect[0] & 0x8000 ? 0 : TILEMAP_FLIPX);

	/* line select is used for 'for use in' and '8ing' screen on bbakraid, 'Raizing' logo on batrider */
	for (int y = cliprect.min_y; y <= cliprect.max_y; y++)
	{
		clip.min_y = clip.max_y = y;
		m_tx_tilemap->set_scrolly(0, m_tx_lineselect[y] - y);
		m_tx_tilemap->draw(screen, bitmap, clip, 0);
	}

	return 0;
}



void toaplan2_state::screen_eof_toaplan2(screen_device &screen, bool state)
{
	// rising edge
	if (state)
	{
		gp9001_screen_eof();
		second_gp9001_screen_eof();
	}
}


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
#define GP9001_PRIMASK_TMAPS (0x0e00)



#define GP9001_BG_VRAM_SIZE   0x1000    /* Background RAM size */
#define GP9001_FG_VRAM_SIZE   0x1000    /* Foreground RAM size */
#define GP9001_TOP_VRAM_SIZE  0x1000    /* Top Layer  RAM size */
#define GP9001_SPRITERAM_SIZE 0x800 /* Sprite     RAM size */
#define GP9001_SPRITE_FLIPX 0x1000  /* Sprite flip flags */
#define GP9001_SPRITE_FLIPY 0x2000




void toaplan2_state::create_tilemaps()
{
//	top.tmap = &machine().tilemap().create(m_gfxdecode, tilemap_get_info_delegate(FUNC(toaplan2_state::get_top0_tile_info),this),TILEMAP_SCAN_ROWS,16,16,32,32);
//	fg.tmap = &machine().tilemap().create(m_gfxdecode, tilemap_get_info_delegate(FUNC(toaplan2_state::get_fg0_tile_info),this),TILEMAP_SCAN_ROWS,16,16,32,32);
//	bg.tmap = &machine().tilemap().create(m_gfxdecode, tilemap_get_info_delegate(FUNC(toaplan2_state::get_bg0_tile_info),this),TILEMAP_SCAN_ROWS,16,16,32,32);

//	top.tmap->set_transparent_pen(0);
//	fg.tmap->set_transparent_pen(0);
//	bg.tmap->set_transparent_pen(0);
}


void toaplan2_state::vdp_start()
{
	sp.vram16_buffer = make_unique_clear<UINT16[]>(GP9001_SPRITERAM_SIZE/2);

	create_tilemaps();

	save_pointer(NAME(sp.vram16_buffer.get()), GP9001_SPRITERAM_SIZE/2);

	save_item(NAME(m_vram_bg));
	save_item(NAME(m_vram_fg));
	save_item(NAME(m_vram_top));
	save_item(NAME(m_spriteram));
	save_item(NAME(m_unkram));

	save_item(NAME(gp9001_scroll_reg));
	save_item(NAME(gp9001_voffs));
	save_item(NAME(bg.realscrollx));
	save_item(NAME(bg.realscrolly));
	save_item(NAME(bg.scrollx));
	save_item(NAME(bg.scrolly));
	save_item(NAME(fg.realscrollx));
	save_item(NAME(fg.realscrolly));
	save_item(NAME(fg.scrollx));
	save_item(NAME(fg.scrolly));
	save_item(NAME(top.realscrollx));
	save_item(NAME(top.realscrolly));
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

void toaplan2_state::vdp_reset()
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


void toaplan2_state::gp9001_voffs_w(UINT16 data, UINT16 mem_mask)
{
	COMBINE_DATA(&gp9001_voffs);
}

int toaplan2_state::gp9001_videoram16_r()
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


void toaplan2_state::gp9001_videoram16_w(UINT16 data, UINT16 mem_mask)
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
		COMBINE_DATA(&m_vram_top[offs]);
		break;
	}

	case 0x800:
	{
		offs &= 0x7ff;
		COMBINE_DATA(&m_vram_fg[offs]);
		break;
	}
	case 0x000:
	{
		COMBINE_DATA(&m_vram_bg[offs]);
		break;
	}
	}

}


UINT16 toaplan2_state::gp9001_vdpstatus_r()
{
	return m_status;
	//return ((m_screen->vpos() + 15) % 262) >= 245;
}

void toaplan2_state::gp9001_scroll_reg_select_w(UINT16 data, UINT16 mem_mask)
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

static void gp9001_set_scrollx_and_flip_reg(gp9001tilemaplayerx* layer, UINT16 data, UINT16 mem_mask, int flip)
{
	COMBINE_DATA(&layer->scrollx);

	if (flip)
	{
		layer->flip |= TILEMAP_FLIPX;
		layer->realscrollx = -(layer->scrollx+layer->extra_xoffset.flipped);
	}
	else
	{
		layer->flip &= (~TILEMAP_FLIPX);
		layer->realscrollx = layer->scrollx+layer->extra_xoffset.normal;
	}
	//layer->tmap->set_flip(layer->flip);
}

static void gp9001_set_scrolly_and_flip_reg(gp9001tilemaplayerx* layer, UINT16 data, UINT16 mem_mask, int flip)
{
	COMBINE_DATA(&layer->scrolly);

	if (flip)
	{
		layer->flip |= TILEMAP_FLIPY;
		layer->realscrolly = -(layer->scrolly+layer->extra_yoffset.flipped);

	}
	else
	{
		layer->flip &= (~TILEMAP_FLIPY);
		layer->realscrolly = layer->scrolly+layer->extra_yoffset.normal;
	}

	//layer->tmap->set_flip(layer->flip);
}

static void gp9001_set_sprite_scrollx_and_flip_reg(gp9001spritelayerx* layer, UINT16 data, UINT16 mem_mask, int flip)
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

static void gp9001_set_sprite_scrolly_and_flip_reg(gp9001spritelayerx* layer, UINT16 data, UINT16 mem_mask, int flip)
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

void toaplan2_state::gp9001_scroll_reg_data_w(UINT16 data, UINT16 mem_mask)
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

void toaplan2_state::init_scroll_regs()
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



READ16_MEMBER( toaplan2_state::gp9001_vdp_r )
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

WRITE16_MEMBER( toaplan2_state::gp9001_vdp_w )
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
READ16_MEMBER( toaplan2_state::gp9001_vdp_alt_r )
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

WRITE16_MEMBER( toaplan2_state::gp9001_vdp_alt_w )
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

WRITE16_MEMBER( toaplan2_state::pipibibi_bootleg_scroll_w )
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

READ16_MEMBER( toaplan2_state::pipibibi_bootleg_videoram16_r )
{
	gp9001_voffs_w(offset, 0xffff);
	return gp9001_videoram16_r();
}

WRITE16_MEMBER( toaplan2_state::pipibibi_bootleg_videoram16_w )
{
	gp9001_voffs_w(offset, 0xffff);
	gp9001_videoram16_w(data, mem_mask);
}

READ16_MEMBER( toaplan2_state::pipibibi_bootleg_spriteram16_r )
{
	gp9001_voffs_w((0x1800 + offset), 0);
	return gp9001_videoram16_r();
}

WRITE16_MEMBER( toaplan2_state::pipibibi_bootleg_spriteram16_w )
{
	gp9001_voffs_w((0x1800 + offset), mem_mask);
	gp9001_videoram16_w(data, mem_mask);
}

/***************************************************************************
    Sprite Handlers
***************************************************************************/



#define DO_PIX \
	if (priority >= (*dstptr & 0xf000)) \
	{ \
		const UINT8 pix = *srcdata++; \
		if (pix & 0xf) \
		{ \
			*dstptr++ = pix | color | priority; \
			/**dstpri++ = priority;*/ \
		} \
		else \
		{ \
			dstptr++; \
			/*dstpri++; */\
		} \
	} \
	else \
	{ \
		dstptr++; \
		/*dstpri++; */\
		srcdata++; \
	}


#define DO_PIX_REV \
	if (priority >= (*dstptr & 0xf000)) \
	{ \
		const UINT8 pix = *srcdata++; \
		if (pix & 0xf) \
		{ \
			*dstptr-- = pix | color | priority; \
			/**dstpri-- = priority;*/ \
		} \
		else \
		{ \
			dstptr--; \
			/*dstpri--;*/ \
		} \
	} \
	else \
	{ \
		dstptr--; \
		/*dstpri--;*/ \
		srcdata++; \
	}

#define DO_PIX_NOINC \
	if (priority >= (*dstptr & 0xf000)) \
	{ \
		UINT8 pix = *srcdata; \
		if (pix & 0xf) \
		{ \
			*dstptr = pix | color | priority; \
			/**dstpri = priority;*/	\
		} \
	}


#define HANDLE_FLIPX_FLIPY__NOCLIPX_NOCLIPY \
	for (int yy = 7; yy != -1; yy += -1) \
	{ \
		int drawyy = yy + sy; \
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
				DO_PIX_NOINC; \
			} \
			srcdata++; \
		} \
	}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////




#define OPAQUE_DOPIX \
	if (priority >= (*dstptr & 0xf000)) \
	{ \
		const UINT8 pix = *srcdata++; \
		*dstptr++ = pix | color | priority; \
		/**dstpri++ = priority;*/ \
	} \
	else \
	{ \
		srcdata++; \
		dstptr++; \
		/*dstpri++; */\
	}


#define OPAQUE_DOPIX_REV \
	if (priority >= (*dstptr & 0xf000)) \
	{ \
		const UINT8 pix = *srcdata++; \
		*dstptr-- = pix | color | priority; \
		/**dstpri-- = priority;*/ \
	} \
	else \
	{ \
		srcdata++; \
		dstptr--; \
		/*dstpri--;*/ \
	}

#define OPAQUE_DOPIX_NOINC \
	if (priority >= (*dstptr & 0xf000)) \
	{ \
		UINT8 pix = *srcdata; \
		if (pix & 0xf) \
		{ \
			*dstptr = pix | color | priority; \
			/**dstpri = priority;*/	\
		} \
	}


#define OPAQUE_HANDLEFLIPX_FLIPY__NOCLIPX_NOCLIPY \
	for (int yy = 7; yy != -1; yy += -1) \
	{ \
		int drawyy = yy + sy; \
		UINT16* dstptr = &bitmap.pix16(drawyy) + sx + 7; \
		OPAQUE_DOPIX_REV; \
		OPAQUE_DOPIX_REV; \
		OPAQUE_DOPIX_REV; \
		OPAQUE_DOPIX_REV; \
		OPAQUE_DOPIX_REV; \
		OPAQUE_DOPIX_REV; \
		OPAQUE_DOPIX_REV; \
		OPAQUE_DOPIX_REV; \
	}


#define OPAQUE_HANDLEFLIPX_FLIPY__NOCLIPX_CLIPY \
	for (int yy = 7; yy != -1; yy += -1) \
	{ \
		int drawyy = yy + sy; \
		if ((drawyy >= cliprect.min_y) && (drawyy <= cliprect.max_y)) \
		{ \
			UINT16* dstptr = &bitmap.pix16(drawyy) + sx + 7; \
			OPAQUE_DOPIX_REV; \
			OPAQUE_DOPIX_REV; \
			OPAQUE_DOPIX_REV; \
			OPAQUE_DOPIX_REV; \
			OPAQUE_DOPIX_REV; \
			OPAQUE_DOPIX_REV; \
			OPAQUE_DOPIX_REV; \
			OPAQUE_DOPIX_REV; \
		} \
		else \
		{ \
			srcdata += 8; \
		} \
	}

#define OPAQUE_HANDLEFLIPX_FLIPY_CLIPX_CLIPY \
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
					OPAQUE_DOPIX_NOINC; \
				} \
				srcdata++; \
			} \
		} \
		else \
		{ \
			srcdata += 8; \
		} \
	}

#define OPAQUE_HANDLENOFLIPX_FLIPY_NOCLIPX_NOCLIPY \
	for (int yy = 7; yy != -1; yy += -1) \
	{ \
		int drawyy = yy + sy; \
		UINT16* dstptr = &bitmap.pix16(drawyy) + sx; \
		OPAQUE_DOPIX; \
		OPAQUE_DOPIX; \
		OPAQUE_DOPIX; \
		OPAQUE_DOPIX; \
		OPAQUE_DOPIX; \
		OPAQUE_DOPIX; \
		OPAQUE_DOPIX; \
		OPAQUE_DOPIX; \
	}

#define OPAQUE_HANDLENOFLIPX_FLIPY_NOCLIPX_CLIPY \
	for (int yy = 7; yy != -1; yy += -1) \
	{ \
		int drawyy = yy + sy; \
		if ((drawyy >= cliprect.min_y) && (drawyy <= cliprect.max_y)) \
		{ \
			UINT16* dstptr = &bitmap.pix16(drawyy) + sx; \
			OPAQUE_DOPIX; \
			OPAQUE_DOPIX; \
			OPAQUE_DOPIX; \
			OPAQUE_DOPIX; \
			OPAQUE_DOPIX; \
			OPAQUE_DOPIX; \
			OPAQUE_DOPIX; \
			OPAQUE_DOPIX; \
		} \
		else \
		{ \
			srcdata += 8; \
		} \
	}

#define OPAQUE_HANDLENOFLIPX_FLIPY_CLIPX_CLIPY \
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
					OPAQUE_DOPIX_NOINC; \
				} \
				srcdata++; \
			} \
		} \
		else \
		{ \
			srcdata += 8; \
		} \
	}


#define OPAQUE_HANDLEFLIPX_NOFLIPY_NOCLIPX_NOCLIPY \
	for (int yy = 0; yy != 8; yy += 1) \
	{ \
		int drawyy = yy + sy; \
		UINT16* dstptr = &bitmap.pix16(drawyy) + sx + 7; \
		OPAQUE_DOPIX_REV; \
		OPAQUE_DOPIX_REV; \
		OPAQUE_DOPIX_REV; \
		OPAQUE_DOPIX_REV; \
		OPAQUE_DOPIX_REV; \
		OPAQUE_DOPIX_REV; \
		OPAQUE_DOPIX_REV; \
		OPAQUE_DOPIX_REV; \
	}

#define OPAQUE_HANDLEFLIPX_NOFLIPY_NOCLIPX_CLIPY \
	for (int yy = 0; yy != 8; yy += 1) \
	{ \
		int drawyy = yy + sy; \
		if ((drawyy >= cliprect.min_y) && (drawyy <= cliprect.max_y)) \
		{ \
			UINT16* dstptr = &bitmap.pix16(drawyy) + sx + 7; \
			OPAQUE_DOPIX_REV; \
			OPAQUE_DOPIX_REV; \
			OPAQUE_DOPIX_REV; \
			OPAQUE_DOPIX_REV; \
			OPAQUE_DOPIX_REV; \
			OPAQUE_DOPIX_REV; \
			OPAQUE_DOPIX_REV; \
			OPAQUE_DOPIX_REV; \
		} \
		else \
		{ \
			srcdata += 8; \
		} \
	}

#define OPAQUE_HANDLEFLIPX_NOFLIPY_CLIPX_CLIPY \
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
					OPAQUE_DOPIX_NOINC; \
				} \
				srcdata++; \
			} \
		} \
		else \
		{ \
			srcdata += 8; \
		} \
	}


#define OPAQUE_HANDLENOFLIPX_NOFLIPY_NOCLIPX_NOCLIPY \
	for (int yy = 0; yy != 8; yy += 1) \
	{ \
		int drawyy = yy + sy; \
		UINT16* dstptr = &bitmap.pix16(drawyy) + sx; \
		OPAQUE_DOPIX; \
		OPAQUE_DOPIX; \
		OPAQUE_DOPIX; \
		OPAQUE_DOPIX; \
		OPAQUE_DOPIX; \
		OPAQUE_DOPIX; \
		OPAQUE_DOPIX; \
		OPAQUE_DOPIX; \
	}

#define OPAQUE_HANDLENOFLIPX_NOFLIPY_NOCLIPX_CLIPY \
	for (int yy = 0; yy != 8; yy += 1) \
	{ \
		int drawyy = yy + sy; \
		if ((drawyy >= cliprect.min_y) && (drawyy <= cliprect.max_y)) \
		{ \
			UINT16* dstptr = &bitmap.pix16(drawyy) + sx; \
			OPAQUE_DOPIX; \
			OPAQUE_DOPIX; \
			OPAQUE_DOPIX; \
			OPAQUE_DOPIX; \
			OPAQUE_DOPIX; \
			OPAQUE_DOPIX; \
			OPAQUE_DOPIX; \
			OPAQUE_DOPIX; \
		} \
		else \
		{ \
			srcdata += 8; \
		} \
	}

#define OPAQUE_HANDLENOFLIPX_NOFLIPY_CLIPX_CLIPY \
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
					OPAQUE_DOPIX_NOINC; \
				} \
				srcdata++; \
			} \
		} \
		else \
		{ \
			srcdata += 8; \
		} \
	}

#define OPAQUE_HANDLENOFLIPX_NOFLIPY_CLIPX_NOCLIPY \
	for (int yy = 0; yy != 8; yy += 1) \
	{ \
		int drawyy = yy + sy; \
		for (int xx = 0; xx != 8; xx += 1) \
		{ \
			int drawxx = xx + sx; \
			if ((drawxx >= cliprect.min_x) && (drawxx <= cliprect.max_x)) \
			{ \
				UINT16* dstptr = &bitmap.pix16(drawyy, drawxx); \
				OPAQUE_DOPIX_NOINC; \
			} \
			srcdata++; \
		} \
	}

#define OPAQUE_HANDLEFLIPX_NOFLIPY_CLIPX_NOCLIPY \
	for (int yy = 0; yy != 8; yy += 1) \
	{ \
		int drawyy = yy + sy; \
		for (int xx = 7; xx != -1; xx += -1) \
		{ \
			int drawxx = xx + sx; \
			if ((drawxx >= cliprect.min_x) && (drawxx <= cliprect.max_x)) \
			{ \
				UINT16* dstptr = &bitmap.pix16(drawyy, drawxx); \
				OPAQUE_DOPIX_NOINC; \
			} \
			srcdata++; \
		} \
	}

#define OPAQUE_HANDLENOFLIPX_FLIPY_CLIPX_NOCLIPY \
	for (int yy = 7; yy != -1; yy += -1) \
	{ \
		int drawyy = yy + sy; \
		for (int xx = 0; xx != 8; xx += 1) \
		{ \
			int drawxx = xx + sx; \
			if ((drawxx >= cliprect.min_x) && (drawxx <= cliprect.max_x)) \
			{ \
				UINT16* dstptr = &bitmap.pix16(drawyy, drawxx); \
				OPAQUE_DOPIX_NOINC; \
			} \
			srcdata++; \
		} \
	}

#define OPAQUE_HANDLEFLIPX_FLIPY_CLIPX_NOCLIPY \
	for (int yy = 7; yy != -1; yy += -1) \
	{ \
		int drawyy = yy + sy; \
		for (int xx = 7; xx != -1; xx += -1) \
		{ \
			int drawxx = xx + sx; \
			if ((drawxx >= cliprect.min_x) && (drawxx <= cliprect.max_x)) \
			{ \
				UINT16* dstptr = &bitmap.pix16(drawyy, drawxx); \
				OPAQUE_DOPIX_NOINC; \
			} \
			srcdata++; \
		} \
	}


/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////

#define DO_TILEMAP_PIX \
	if (priority >= (*dstptr & 0xf000)) \
	{ \
		const UINT8 pix = *srcdata++; \
		if (pix & 0xf) \
		{ \
			*dstptr++ = pix | color | priority; \
			/**dstpri++ = priority;*/ \
		} \
		else \
		{ \
			dstptr++; \
			/*dstpri++; */\
		} \
	} \
	else \
	{ \
		dstptr++; \
		/*dstpri++; */\
		srcdata++; \
	}

#define DO_TILEMAP_PIX_NOINC \
	if (priority >= (*dstptr & 0xf000)) \
	{ \
		UINT8 pix = *srcdata; \
		if (pix & 0xf) \
		{ \
			*dstptr = pix | color | priority; \
			/**dstpri = priority;*/	\
		} \
	}

#define OPAQUE_DO_TILEMAP_PIX \
	if (priority >= (*dstptr & 0xf000)) \
	{ \
		const UINT8 pix = *srcdata++; \
		*dstptr++ = pix | color | priority; \
		/**dstpri++ = priority;*/ \
	} \
	else \
	{ \
		srcdata++; \
		dstptr++; \
		/*dstpri++; */\
	}


#define OPAQUE_DO_TILEMAP_PIX_NOINC \
	if (priority >= (*dstptr & 0xf000)) \
	{ \
		UINT8 pix = *srcdata; \
		if (pix & 0xf) \
		{ \
			*dstptr = pix | color | priority; \
			/**dstpri = priority;*/	\
		} \
	}

///////////////////////

#define DO_TILEMAP_PIX_NOPRI \
	{ \
	const UINT8 pix = *srcdata++; \
	if (pix & 0xf) \
	{ \
		*dstptr++ = pix | color | priority; \
		/**dstpri++ = priority;*/ \
	} \
	else \
	{ \
		dstptr++; \
		/*dstpri++; */\
	} \
	}


#define DO_TILEMAP_PIX_NOINC_NOPRI \
	{ \
	UINT8 pix = *srcdata; \
	if (pix & 0xf) \
	{ \
		*dstptr = pix | color | priority; \
		/**dstpri = priority;*/	\
	} \
	}


#define OPAQUE_DO_TILEMAP_PIX_NOPRI \
	{ \
	const UINT8 pix = *srcdata++; \
	*dstptr++ = pix | color | priority; \
	/**dstpri++ = priority;*/ \
	}

#define OPAQUE_DO_TILEMAP_PIX_NOINC_NOPRI \
	{ \
	UINT8 pix = *srcdata; \
	if (pix & 0xf) \
	{ \
		*dstptr = pix | color | priority; \
		/**dstpri = priority;*/	\
	} \
	}


///////////////

#define OPAQUE16_HANDLENOFLIPX_NOFLIPY_NOCLIPX_NOCLIPY \
	for (int yy = 0; yy != 16; yy += 1) \
	{ \
		int drawyy = yy + sy; \
		UINT16* dstptr = &bitmap.pix16(drawyy) + sx; \
		OPAQUE_DO_TILEMAP_PIX; \
		OPAQUE_DO_TILEMAP_PIX; \
		OPAQUE_DO_TILEMAP_PIX; \
		OPAQUE_DO_TILEMAP_PIX; \
		OPAQUE_DO_TILEMAP_PIX; \
		OPAQUE_DO_TILEMAP_PIX; \
		OPAQUE_DO_TILEMAP_PIX; \
		OPAQUE_DO_TILEMAP_PIX; \
		OPAQUE_DO_TILEMAP_PIX; \
		OPAQUE_DO_TILEMAP_PIX; \
		OPAQUE_DO_TILEMAP_PIX; \
		OPAQUE_DO_TILEMAP_PIX; \
		OPAQUE_DO_TILEMAP_PIX; \
		OPAQUE_DO_TILEMAP_PIX; \
		OPAQUE_DO_TILEMAP_PIX; \
		OPAQUE_DO_TILEMAP_PIX; \
	}

#define OPAQUE16_HANDLENOFLIPX_NOFLIPY_NOCLIPX_CLIPY \
	for (int yy = 0; yy != 16; yy += 1) \
	{ \
		int drawyy = yy + sy; \
		if ((drawyy >= cliprect.min_y) && (drawyy <= cliprect.max_y)) \
		{ \
			UINT16* dstptr = &bitmap.pix16(drawyy) + sx; \
			OPAQUE_DO_TILEMAP_PIX; \
			OPAQUE_DO_TILEMAP_PIX; \
			OPAQUE_DO_TILEMAP_PIX; \
			OPAQUE_DO_TILEMAP_PIX; \
			OPAQUE_DO_TILEMAP_PIX; \
			OPAQUE_DO_TILEMAP_PIX; \
			OPAQUE_DO_TILEMAP_PIX; \
			OPAQUE_DO_TILEMAP_PIX; \
			OPAQUE_DO_TILEMAP_PIX; \
			OPAQUE_DO_TILEMAP_PIX; \
			OPAQUE_DO_TILEMAP_PIX; \
			OPAQUE_DO_TILEMAP_PIX; \
			OPAQUE_DO_TILEMAP_PIX; \
			OPAQUE_DO_TILEMAP_PIX; \
			OPAQUE_DO_TILEMAP_PIX; \
			OPAQUE_DO_TILEMAP_PIX; \
		} \
		else \
		{ \
			srcdata += 16; \
		} \
	}

#define OPAQUE16_HANDLENOFLIPX_NOFLIPY_CLIPX_CLIPY \
	for (int yy = 0; yy != 16; yy += 1) \
	{ \
		int drawyy = yy + sy; \
		if ((drawyy >= cliprect.min_y) && (drawyy <= cliprect.max_y)) \
		{ \
			for (int xx = 0; xx != 16; xx += 1) \
			{ \
				int drawxx = xx + sx; \
				if ((drawxx >= cliprect.min_x) && (drawxx <= cliprect.max_x)) \
				{ \
					UINT16* dstptr = &bitmap.pix16(drawyy, drawxx); \
					OPAQUE_DO_TILEMAP_PIX_NOINC; \
				} \
				srcdata++; \
			} \
		} \
		else \
		{ \
			srcdata += 16; \
		} \
	}

#define OPAQUE16_HANDLENOFLIPX_NOFLIPY_CLIPX_NOCLIPY \
	for (int yy = 0; yy != 16; yy += 1) \
	{ \
		int drawyy = yy + sy; \
		for (int xx = 0; xx != 16; xx += 1) \
		{ \
			int drawxx = xx + sx; \
			if ((drawxx >= cliprect.min_x) && (drawxx <= cliprect.max_x)) \
			{ \
				UINT16* dstptr = &bitmap.pix16(drawyy, drawxx); \
				OPAQUE_DO_TILEMAP_PIX_NOINC; \
			} \
			srcdata++; \
		} \
	}



#define TRANS16_HANDLE_NOFLIPX_NOFLIPY_NOCLIPX_NOCLIPY \
	for (int yy = 0; yy != 16; yy += 1) \
	{ \
		int drawyy = yy + sy; \
		UINT16* dstptr = &bitmap.pix16(drawyy) + sx; \
		DO_TILEMAP_PIX; \
		DO_TILEMAP_PIX; \
		DO_TILEMAP_PIX; \
		DO_TILEMAP_PIX; \
		DO_TILEMAP_PIX; \
		DO_TILEMAP_PIX; \
		DO_TILEMAP_PIX; \
		DO_TILEMAP_PIX; \
		DO_TILEMAP_PIX; \
		DO_TILEMAP_PIX; \
		DO_TILEMAP_PIX; \
		DO_TILEMAP_PIX; \
		DO_TILEMAP_PIX; \
		DO_TILEMAP_PIX; \
		DO_TILEMAP_PIX; \
		DO_TILEMAP_PIX; \
	}

#define TRANS16_HANDLE_NOFLIPX_NOFLIPY_NOCLIPX_CLIPY \
	for (int yy = 0; yy != 16; yy += 1) \
	{ \
		int drawyy = yy + sy; \
		if ((drawyy >= cliprect.min_y) && (drawyy <= cliprect.max_y)) \
		{ \
			UINT16* dstptr = &bitmap.pix16(drawyy) + sx; \
			DO_TILEMAP_PIX; \
			DO_TILEMAP_PIX; \
			DO_TILEMAP_PIX; \
			DO_TILEMAP_PIX; \
			DO_TILEMAP_PIX; \
			DO_TILEMAP_PIX; \
			DO_TILEMAP_PIX; \
			DO_TILEMAP_PIX; \
			DO_TILEMAP_PIX; \
			DO_TILEMAP_PIX; \
			DO_TILEMAP_PIX; \
			DO_TILEMAP_PIX; \
			DO_TILEMAP_PIX; \
			DO_TILEMAP_PIX; \
			DO_TILEMAP_PIX; \
			DO_TILEMAP_PIX; \
		} \
		else \
		{ \
			srcdata += 16; \
		} \
	}

#define TRANS16_HANDLE_NOFLIPX_NOFLIPY_CLIPX_CLIPY \
	for (int yy = 0; yy != 16; yy += 1) \
	{ \
		int drawyy = yy + sy; \
		if ((drawyy >= cliprect.min_y) && (drawyy <= cliprect.max_y)) \
		{ \
			for (int xx = 0; xx != 16; xx += 1) \
			{ \
				int drawxx = xx + sx; \
				if ((drawxx >= cliprect.min_x) && (drawxx <= cliprect.max_x)) \
				{ \
					UINT16* dstptr = &bitmap.pix16(drawyy, drawxx); \
					DO_TILEMAP_PIX_NOINC; \
				} \
				srcdata++; \
			} \
		} \
		else \
		{ \
			srcdata += 16; \
		} \
	}

#define TRANS16_HANDLE_NOFLIPX_NOFLIPY_CLIPX_NOCLIPY \
	for (int yy = 0; yy != 16; yy += 1) \
	{ \
		int drawyy = yy + sy; \
		for (int xx = 0; xx != 16; xx += 1) \
		{ \
			int drawxx = xx + sx; \
			if ((drawxx >= cliprect.min_x) && (drawxx <= cliprect.max_x)) \
			{ \
				UINT16* dstptr = &bitmap.pix16(drawyy, drawxx); \
				DO_TILEMAP_PIX_NOINC; \
			} \
			srcdata++; \
		} \
	}


/////////////////////////


#define OPAQUE16_HANDLENOFLIPX_NOFLIPY_NOCLIPX_NOCLIPY_NOPRI \
	for (int yy = 0; yy != 16; yy += 1) \
	{ \
		int drawyy = yy + sy; \
		UINT16* dstptr = &bitmap.pix16(drawyy) + sx; \
		OPAQUE_DO_TILEMAP_PIX_NOPRI; \
		OPAQUE_DO_TILEMAP_PIX_NOPRI; \
		OPAQUE_DO_TILEMAP_PIX_NOPRI; \
		OPAQUE_DO_TILEMAP_PIX_NOPRI; \
		OPAQUE_DO_TILEMAP_PIX_NOPRI; \
		OPAQUE_DO_TILEMAP_PIX_NOPRI; \
		OPAQUE_DO_TILEMAP_PIX_NOPRI; \
		OPAQUE_DO_TILEMAP_PIX_NOPRI; \
		OPAQUE_DO_TILEMAP_PIX_NOPRI; \
		OPAQUE_DO_TILEMAP_PIX_NOPRI; \
		OPAQUE_DO_TILEMAP_PIX_NOPRI; \
		OPAQUE_DO_TILEMAP_PIX_NOPRI; \
		OPAQUE_DO_TILEMAP_PIX_NOPRI; \
		OPAQUE_DO_TILEMAP_PIX_NOPRI; \
		OPAQUE_DO_TILEMAP_PIX_NOPRI; \
		OPAQUE_DO_TILEMAP_PIX_NOPRI; \
	}

#define OPAQUE16_HANDLENOFLIPX_NOFLIPY_NOCLIPX_CLIPY_NOPRI \
	for (int yy = 0; yy != 16; yy += 1) \
	{ \
		int drawyy = yy + sy; \
		if ((drawyy >= cliprect.min_y) && (drawyy <= cliprect.max_y)) \
		{ \
			UINT16* dstptr = &bitmap.pix16(drawyy) + sx; \
			OPAQUE_DO_TILEMAP_PIX_NOPRI; \
			OPAQUE_DO_TILEMAP_PIX_NOPRI; \
			OPAQUE_DO_TILEMAP_PIX_NOPRI; \
			OPAQUE_DO_TILEMAP_PIX_NOPRI; \
			OPAQUE_DO_TILEMAP_PIX_NOPRI; \
			OPAQUE_DO_TILEMAP_PIX_NOPRI; \
			OPAQUE_DO_TILEMAP_PIX_NOPRI; \
			OPAQUE_DO_TILEMAP_PIX_NOPRI; \
			OPAQUE_DO_TILEMAP_PIX_NOPRI; \
			OPAQUE_DO_TILEMAP_PIX_NOPRI; \
			OPAQUE_DO_TILEMAP_PIX_NOPRI; \
			OPAQUE_DO_TILEMAP_PIX_NOPRI; \
			OPAQUE_DO_TILEMAP_PIX_NOPRI; \
			OPAQUE_DO_TILEMAP_PIX_NOPRI; \
			OPAQUE_DO_TILEMAP_PIX_NOPRI; \
			OPAQUE_DO_TILEMAP_PIX_NOPRI; \
		} \
		else \
		{ \
			srcdata += 16; \
		} \
	}

#define OPAQUE16_HANDLENOFLIPX_NOFLIPY_CLIPX_CLIPY_NOPRI \
	for (int yy = 0; yy != 16; yy += 1) \
	{ \
		int drawyy = yy + sy; \
		if ((drawyy >= cliprect.min_y) && (drawyy <= cliprect.max_y)) \
		{ \
			for (int xx = 0; xx != 16; xx += 1) \
			{ \
				int drawxx = xx + sx; \
				if ((drawxx >= cliprect.min_x) && (drawxx <= cliprect.max_x)) \
				{ \
					UINT16* dstptr = &bitmap.pix16(drawyy, drawxx); \
					OPAQUE_DO_TILEMAP_PIX_NOINC_NOPRI; \
				} \
				srcdata++; \
			} \
		} \
		else \
		{ \
			srcdata += 16; \
		} \
	}

#define OPAQUE16_HANDLENOFLIPX_NOFLIPY_CLIPX_NOCLIPY_NOPRI \
	for (int yy = 0; yy != 16; yy += 1) \
	{ \
		int drawyy = yy + sy; \
		for (int xx = 0; xx != 16; xx += 1) \
		{ \
			int drawxx = xx + sx; \
			if ((drawxx >= cliprect.min_x) && (drawxx <= cliprect.max_x)) \
			{ \
				UINT16* dstptr = &bitmap.pix16(drawyy, drawxx); \
				OPAQUE_DO_TILEMAP_PIX_NOINC_NOPRI; \
			} \
			srcdata++; \
		} \
	}



#define TRANS16_HANDLE_NOFLIPX_NOFLIPY_NOCLIPX_NOCLIPY_NOPRI \
	for (int yy = 0; yy != 16; yy += 1) \
	{ \
		int drawyy = yy + sy; \
		UINT16* dstptr = &bitmap.pix16(drawyy) + sx; \
		DO_TILEMAP_PIX_NOPRI; \
		DO_TILEMAP_PIX_NOPRI; \
		DO_TILEMAP_PIX_NOPRI; \
		DO_TILEMAP_PIX_NOPRI; \
		DO_TILEMAP_PIX_NOPRI; \
		DO_TILEMAP_PIX_NOPRI; \
		DO_TILEMAP_PIX_NOPRI; \
		DO_TILEMAP_PIX_NOPRI; \
		DO_TILEMAP_PIX_NOPRI; \
		DO_TILEMAP_PIX_NOPRI; \
		DO_TILEMAP_PIX_NOPRI; \
		DO_TILEMAP_PIX_NOPRI; \
		DO_TILEMAP_PIX_NOPRI; \
		DO_TILEMAP_PIX_NOPRI; \
		DO_TILEMAP_PIX_NOPRI; \
		DO_TILEMAP_PIX_NOPRI; \
	}

#define TRANS16_HANDLE_NOFLIPX_NOFLIPY_NOCLIPX_CLIPY_NOPRI \
	for (int yy = 0; yy != 16; yy += 1) \
	{ \
		int drawyy = yy + sy; \
		if ((drawyy >= cliprect.min_y) && (drawyy <= cliprect.max_y)) \
		{ \
			UINT16* dstptr = &bitmap.pix16(drawyy) + sx; \
			DO_TILEMAP_PIX_NOPRI; \
			DO_TILEMAP_PIX_NOPRI; \
			DO_TILEMAP_PIX_NOPRI; \
			DO_TILEMAP_PIX_NOPRI; \
			DO_TILEMAP_PIX_NOPRI; \
			DO_TILEMAP_PIX_NOPRI; \
			DO_TILEMAP_PIX_NOPRI; \
			DO_TILEMAP_PIX_NOPRI; \
			DO_TILEMAP_PIX_NOPRI; \
			DO_TILEMAP_PIX_NOPRI; \
			DO_TILEMAP_PIX_NOPRI; \
			DO_TILEMAP_PIX_NOPRI; \
			DO_TILEMAP_PIX_NOPRI; \
			DO_TILEMAP_PIX_NOPRI; \
			DO_TILEMAP_PIX_NOPRI; \
			DO_TILEMAP_PIX_NOPRI; \
		} \
		else \
		{ \
			srcdata += 16; \
		} \
	}

#define TRANS16_HANDLE_NOFLIPX_NOFLIPY_CLIPX_CLIPY_NOPRI \
	for (int yy = 0; yy != 16; yy += 1) \
	{ \
		int drawyy = yy + sy; \
		if ((drawyy >= cliprect.min_y) && (drawyy <= cliprect.max_y)) \
		{ \
			for (int xx = 0; xx != 16; xx += 1) \
			{ \
				int drawxx = xx + sx; \
				if ((drawxx >= cliprect.min_x) && (drawxx <= cliprect.max_x)) \
				{ \
					UINT16* dstptr = &bitmap.pix16(drawyy, drawxx); \
					DO_TILEMAP_PIX_NOINC_NOPRI; \
				} \
				srcdata++; \
			} \
		} \
		else \
		{ \
			srcdata += 16; \
		} \
	}

#define TRANS16_HANDLE_NOFLIPX_NOFLIPY_CLIPX_NOCLIPY_NOPRI \
	for (int yy = 0; yy != 16; yy += 1) \
	{ \
		int drawyy = yy + sy; \
		for (int xx = 0; xx != 16; xx += 1) \
		{ \
			int drawxx = xx + sx; \
			if ((drawxx >= cliprect.min_x) && (drawxx <= cliprect.max_x)) \
			{ \
				UINT16* dstptr = &bitmap.pix16(drawyy, drawxx); \
				DO_TILEMAP_PIX_NOINC_NOPRI; \
			} \
			srcdata++; \
		} \
	}



///////////////////////


void toaplan2_state::draw_sprites( bitmap_ind16 &bitmap, const rectangle &cliprect )
{
	const UINT16 primask = (GP9001_PRIMASK << 8);

	UINT16 *source;

	if (sp.use_sprite_buffer) source = sp.vram16_buffer.get();
	else source = &m_spriteram[0];
	int total_elements = m_gfxdecode->gfx(1)->elements();

	int old_x = (-(sp.scrollx)) & 0x1ff;
	int old_y = (-(sp.scrolly)) & 0x1ff;

	for (int offs = 0; offs < (GP9001_SPRITERAM_SIZE/2); offs += 4)
	{
		const int attrib = source[offs];
		const UINT16 priority = ((attrib & primask) >> 8) << 12;// << 3;
			
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

					if (m_gfxdecode->gfx(1)->has_pen_usage())
					{
						// fully transparent; do nothing
						UINT32 usage = m_gfxdecode->gfx(1)->pen_usage(sprite);
						if ((usage & ~(1 << 0)) == 0)
						{
							sprite++;
							continue;
						}

						if ((usage & (1 << 0)) == 0) // full opaque
						{
							const UINT8* srcdata = m_gfxdecode->gfx(1)->get_data(sprite);

							if (flipy) // FLIPY 
							{
								if (flipx) // FLIPY AND FLIP X
								{
									// --------------------------
									if ((sx >= cliprect.min_x) && (sx + 7 <= cliprect.max_x))
									{ // x unclipped
										if ((sy >= cliprect.min_y) && (sy + 7 <= cliprect.max_y))
										{  // fully unclipped
											OPAQUE_HANDLEFLIPX_FLIPY__NOCLIPX_NOCLIPY;
										}
										else
										{ // x unclipped, y clipped
											OPAQUE_HANDLEFLIPX_FLIPY__NOCLIPX_CLIPY;
										}
									}
									else
									{  // FLIPY AND FLIP X fully clipped
										if ((sy >= cliprect.min_y) && (sy + 7 <= cliprect.max_y))
										{
											OPAQUE_HANDLEFLIPX_FLIPY_CLIPX_NOCLIPY;
										}
										else
										{
											OPAQUE_HANDLEFLIPX_FLIPY_CLIPX_CLIPY;
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
											OPAQUE_HANDLENOFLIPX_FLIPY_NOCLIPX_NOCLIPY;
#endif
										}
										else
										{  // y needs clipping
#if 1
											OPAQUE_HANDLENOFLIPX_FLIPY_NOCLIPX_CLIPY;
#endif
										}

									}
									else // with clipping
									{
										if ((sy >= cliprect.min_y) && (sy + 7 <= cliprect.max_y))
										{
											OPAQUE_HANDLENOFLIPX_FLIPY_CLIPX_NOCLIPY;
										}
										else
										{
											OPAQUE_HANDLENOFLIPX_FLIPY_CLIPX_CLIPY;
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
											OPAQUE_HANDLEFLIPX_NOFLIPY_NOCLIPX_NOCLIPY;
#endif
										}
										else // x not clipped, y is
										{
#if 1
											OPAQUE_HANDLEFLIPX_NOFLIPY_NOCLIPX_CLIPY;
#endif
										}
									}
									else // clipping required
									{
										if ((sy >= cliprect.min_y) && (sy + 7 <= cliprect.max_y))
										{
											OPAQUE_HANDLEFLIPX_NOFLIPY_CLIPX_NOCLIPY;
										}
										else
										{
											OPAQUE_HANDLEFLIPX_NOFLIPY_CLIPX_CLIPY;
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
											OPAQUE_HANDLENOFLIPX_NOFLIPY_NOCLIPX_NOCLIPY;
#endif
										}
										else
										{ // y needs clipping
#if 1
											OPAQUE_HANDLENOFLIPX_NOFLIPY_NOCLIPX_CLIPY;
#endif
										}

									}
									else // with clipping
									{
										if ((sy >= cliprect.min_y) && (sy + 7 <= cliprect.max_y))
										{
											OPAQUE_HANDLENOFLIPX_NOFLIPY_CLIPX_NOCLIPY;
										}
										else
										{
											OPAQUE_HANDLENOFLIPX_NOFLIPY_CLIPX_CLIPY;
										}
									}

									// -------------------
								}
							}
						}
						else
						{ // has transparent bits
							const UINT8* srcdata = m_gfxdecode->gfx(1)->get_data(sprite);

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

void toaplan2_state::draw_tmap_tile(bitmap_ind16& bitmap, const rectangle& cliprect, int sx, int sy, int sprite, int priority, int color)
{
	sprite &= m_gfxdecode->gfx(0)->elements() - 1;

	if (m_gfxdecode->gfx(0)->has_pen_usage())
	{
		//fully transparent; do nothing
		UINT32 usage = m_gfxdecode->gfx(0)->pen_usage(sprite);
		if ((usage & ~(1 << 0)) == 0)
			return;

		if ((usage & (1 << 0)) == 0) // full opaque
		{
			const UINT8* srcdata = m_gfxdecode->gfx(0)->get_data(sprite);

			if ((sx >= cliprect.min_x) && (sx + 15 <= cliprect.max_x))
			{ // x doesn't need clipping
				if ((sy >= cliprect.min_y) && (sy + 15 <= cliprect.max_y))
				{  // fully unclipped
					OPAQUE16_HANDLENOFLIPX_NOFLIPY_NOCLIPX_NOCLIPY;
				}
				else
				{ // y needs clipping
					OPAQUE16_HANDLENOFLIPX_NOFLIPY_NOCLIPX_CLIPY;
				}
			}
			else // with clipping
			{
				if ((sy >= cliprect.min_y) && (sy + 15 <= cliprect.max_y))
				{
					OPAQUE16_HANDLENOFLIPX_NOFLIPY_CLIPX_NOCLIPY;
				}
				else
				{
					OPAQUE16_HANDLENOFLIPX_NOFLIPY_CLIPX_CLIPY;
				}
			}
		}
		else
		{ // has transparent bits
			const UINT8* srcdata = m_gfxdecode->gfx(0)->get_data(sprite);

			if ((sx >= cliprect.min_x) && (sx + 15 <= cliprect.max_x))
			{ // x doesn't need clipping
				if ((sy >= cliprect.min_y) && (sy + 15 <= cliprect.max_y))
				{  // fully unclipped
					TRANS16_HANDLE_NOFLIPX_NOFLIPY_NOCLIPX_NOCLIPY;
				}
				else
				{ // y needs clipping
					TRANS16_HANDLE_NOFLIPX_NOFLIPY_NOCLIPX_CLIPY;
				}
			}
			else // with clipping
			{
				if ((sy >= cliprect.min_y) && (sy + 15 <= cliprect.max_y))
				{
					TRANS16_HANDLE_NOFLIPX_NOFLIPY_CLIPX_NOCLIPY;
				}
				else
				{
					TRANS16_HANDLE_NOFLIPX_NOFLIPY_CLIPX_CLIPY;
				}
			}
		}
	}
}


void toaplan2_state::draw_tmap_tile_nopri(bitmap_ind16& bitmap, const rectangle& cliprect, int sx, int sy, int sprite, int priority, int color)
{
	sprite &= m_gfxdecode->gfx(0)->elements() - 1;

	if (m_gfxdecode->gfx(0)->has_pen_usage())
	{
		//fully transparent; do nothing
		UINT32 usage = m_gfxdecode->gfx(0)->pen_usage(sprite);
		if ((usage & ~(1 << 0)) == 0)
			return;

		if ((usage & (1 << 0)) == 0) // full opaque
		{
			const UINT8* srcdata = m_gfxdecode->gfx(0)->get_data(sprite);

			if ((sx >= cliprect.min_x) && (sx + 15 <= cliprect.max_x))
			{ // x doesn't need clipping
				if ((sy >= cliprect.min_y) && (sy + 15 <= cliprect.max_y))
				{  // fully unclipped
					OPAQUE16_HANDLENOFLIPX_NOFLIPY_NOCLIPX_NOCLIPY_NOPRI;
				}
				else
				{ // y needs clipping
					OPAQUE16_HANDLENOFLIPX_NOFLIPY_NOCLIPX_CLIPY_NOPRI;
				}
			}
			else // with clipping
			{
				if ((sy >= cliprect.min_y) && (sy + 15 <= cliprect.max_y))
				{
					OPAQUE16_HANDLENOFLIPX_NOFLIPY_CLIPX_NOCLIPY_NOPRI;
				}
				else
				{
					OPAQUE16_HANDLENOFLIPX_NOFLIPY_CLIPX_CLIPY_NOPRI;
				}
			}
		}
		else
		{ // has transparent bits
			const UINT8* srcdata = m_gfxdecode->gfx(0)->get_data(sprite);

			if ((sx >= cliprect.min_x) && (sx + 15 <= cliprect.max_x))
			{ // x doesn't need clipping
				if ((sy >= cliprect.min_y) && (sy + 15 <= cliprect.max_y))
				{  // fully unclipped
					TRANS16_HANDLE_NOFLIPX_NOFLIPY_NOCLIPX_NOCLIPY_NOPRI;
				}
				else
				{ // y needs clipping
					TRANS16_HANDLE_NOFLIPX_NOFLIPY_NOCLIPX_CLIPY_NOPRI;
				}
			}
			else // with clipping
			{
				if ((sy >= cliprect.min_y) && (sy + 15 <= cliprect.max_y))
				{
					TRANS16_HANDLE_NOFLIPX_NOFLIPY_CLIPX_NOCLIPY_NOPRI;
				}
				else
				{
					TRANS16_HANDLE_NOFLIPX_NOFLIPY_CLIPX_CLIPY_NOPRI;
				}
			}
		}
	}
}


void toaplan2_state::gp9001_draw_a_tilemap(bitmap_ind16& bitmap, const rectangle& cliprect, gp9001tilemaplayerx *tmap, UINT16* vram)
{
	int scrollxfg = tmap->realscrollx;
	int scrollyfg = tmap->realscrolly;

	int ydraw = -(scrollyfg & 0xf);
	int xsource_base = (scrollxfg >> 4);
	int xdrawbase = -(scrollxfg & 0xf);

	int ysourcetile= (scrollyfg >> 4);

	for (int y = 0; y < 16; y++, ydraw += 16, ysourcetile++)
	{
		ysourcetile &= 0x1f;

		int xdraw = xdrawbase;
		int xsourcetile  = xsource_base;

		for (int x = 0; x < 21; x++, xdraw += 16, xsourcetile++)
		{
			xsourcetile &= 0x1f;

			int tile_number, attrib;
			int tile_index = ((ysourcetile * 32) + xsourcetile);
				
			attrib = vram[2*tile_index];

			tile_number = vram[2*tile_index+1];

			if (gp9001_gfxrom_is_banked)
			{
				tile_number = ( gp9001_gfxrom_bank[(tile_number >> 13) & 7] << 13 ) | ( tile_number & 0x1fff );
			}

			int color = attrib & 0x007f; // 0x0f00 priority, 0x007f colour
			int priority = (attrib & GP9001_PRIMASK_TMAPS) << 4;

			draw_tmap_tile(bitmap, cliprect, xdraw, ydraw, tile_number, priority, color << 4);
		}
	}

}

void toaplan2_state::gp9001_draw_a_tilemap_nopri(bitmap_ind16& bitmap, const rectangle& cliprect, gp9001tilemaplayerx *tmap, UINT16* vram)
{
	int scrollxfg = tmap->realscrollx;
	int scrollyfg = tmap->realscrolly;

	int ydraw = -(scrollyfg & 0xf);
	int xsource_base = (scrollxfg >> 4);
	int xdrawbase = -(scrollxfg & 0xf);

	int ysourcetile= (scrollyfg >> 4);

	for (int y = 0; y < 16; y++, ydraw += 16, ysourcetile++)
	{
		ysourcetile &= 0x1f;

		int xdraw = xdrawbase;
		int xsourcetile  = xsource_base;

		for (int x = 0; x < 21; x++, xdraw += 16, xsourcetile++)
		{
			xsourcetile &= 0x1f;

			int tile_number, attrib;
			int tile_index = ((ysourcetile * 32) + xsourcetile);
				
			attrib = vram[2*tile_index];

			tile_number = vram[2*tile_index+1];

			if (gp9001_gfxrom_is_banked)
			{
				tile_number = ( gp9001_gfxrom_bank[(tile_number >> 13) & 7] << 13 ) | ( tile_number & 0x1fff );
			}

			int color = attrib & 0x007f; // 0x0f00 priority, 0x007f colour
			int priority = (attrib & GP9001_PRIMASK_TMAPS) << 4;

			draw_tmap_tile_nopri(bitmap, cliprect, xdraw, ydraw, tile_number, priority, color << 4);
		}
	}

}





void toaplan2_state::gp9001_render_vdp(bitmap_ind16 &bitmap, const rectangle &cliprect)
{
	if (gp9001_gfxrom_is_banked && gp9001_gfxrom_bank_dirty)
	{
		gp9001_gfxrom_bank_dirty = 0;
	}

	gp9001_draw_a_tilemap_nopri(bitmap, cliprect, &bg, m_vram_bg);
	gp9001_draw_a_tilemap(bitmap, cliprect, &fg, m_vram_fg);
	gp9001_draw_a_tilemap(bitmap, cliprect, &top, m_vram_top);
	draw_sprites(bitmap,cliprect);

}


void toaplan2_state::gp9001_screen_eof(void)
{
	/** Shift sprite RAM buffers  ***  Used to fix sprite lag **/
	if (sp.use_sprite_buffer) memcpy(sp.vram16_buffer.get(),m_spriteram,GP9001_SPRITERAM_SIZE);
}

/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////

// 2nd VDP copy!

/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////





void toaplan2_state::second_create_tilemaps()
{
}


void toaplan2_state::second_vdp_start()
{
	second_sp.vram16_buffer = make_unique_clear<UINT16[]>(GP9001_SPRITERAM_SIZE/2);

	second_create_tilemaps();

	save_pointer(NAME(second_sp.vram16_buffer.get()), GP9001_SPRITERAM_SIZE/2);

	save_item(NAME(second_m_vram_bg));
	save_item(NAME(second_m_vram_fg));
	save_item(NAME(second_m_vram_top));
	save_item(NAME(second_m_spriteram));
	save_item(NAME(second_m_unkram));

	save_item(NAME(second_gp9001_scroll_reg));
	save_item(NAME(second_gp9001_voffs));
	save_item(NAME(second_bg.realscrollx));
	save_item(NAME(second_bg.realscrolly));
	save_item(NAME(second_bg.scrollx));
	save_item(NAME(second_bg.scrolly));
	save_item(NAME(second_fg.realscrollx));
	save_item(NAME(second_fg.realscrolly));
	save_item(NAME(second_fg.scrollx));
	save_item(NAME(second_fg.scrolly));
	save_item(NAME(second_top.realscrollx));
	save_item(NAME(second_top.realscrolly));
	save_item(NAME(second_top.scrollx));
	save_item(NAME(second_top.scrolly));
	save_item(NAME(second_sp.scrollx));
	save_item(NAME(second_sp.scrolly));
	save_item(NAME(second_bg.flip));
	save_item(NAME(second_fg.flip));
	save_item(NAME(second_top.flip));
	save_item(NAME(second_sp.flip));

	//second_gp9001_gfxrom_is_banked = 0;
	//second_gp9001_gfxrom_bank_dirty = 0;
	//save_item(NAME(second_gp9001_gfxrom_bank));

	// default layer offsets used by all original games
	second_bg.extra_xoffset.normal  = -0x1d6;
	second_bg.extra_xoffset.flipped = -0x229;
	second_bg.extra_yoffset.normal  = -0x1ef;
	second_bg.extra_yoffset.flipped = -0x210;

	second_fg.extra_xoffset.normal  = -0x1d8;
	second_fg.extra_xoffset.flipped = -0x227;
	second_fg.extra_yoffset.normal  = -0x1ef;
	second_fg.extra_yoffset.flipped = -0x210;

	second_top.extra_xoffset.normal = -0x1da;
	second_top.extra_xoffset.flipped= -0x225;
	second_top.extra_yoffset.normal = -0x1ef;
	second_top.extra_yoffset.flipped= -0x210;

	second_sp.extra_xoffset.normal  = -0x1cc;
	second_sp.extra_xoffset.flipped = -0x17b;
	second_sp.extra_yoffset.normal  = -0x1ef;
	second_sp.extra_yoffset.flipped = -0x108;

	second_sp.use_sprite_buffer = 1;
}

void toaplan2_state::second_vdp_reset()
{
	second_gp9001_voffs = 0;
	second_gp9001_scroll_reg = 0;
	second_bg.scrollx = bg.scrolly = 0;
	second_fg.scrollx = fg.scrolly = 0;
	second_top.scrollx = top.scrolly = 0;
	second_sp.scrollx = sp.scrolly = 0;

	second_bg.flip = 0;
	second_fg.flip = 0;
	second_top.flip = 0;
	second_sp.flip = 0;

	second_m_status = 0;

	second_init_scroll_regs();
}


void toaplan2_state::second_gp9001_voffs_w(UINT16 data, UINT16 mem_mask)
{
	COMBINE_DATA(&second_gp9001_voffs);
}

int toaplan2_state::second_gp9001_videoram16_r()
{
	int offs = second_gp9001_voffs;
	second_gp9001_voffs++;
//	return space().read_word(offs*2);
	offs &= 0x1fff;

	if (offs < 0x1000 / 2)
	{
		return second_m_vram_bg[offs];
	}
	else if (offs < 0x2000 / 2)
	{
		offs -= 0x1000 / 2;
		return second_m_vram_fg[offs];
	}
	else if (offs < 0x3000 / 2)
	{
		offs -= 0x2000 / 2;
		return second_m_vram_top[offs];
	}
	else if (offs < 0x4000/2)
	{
		offs -= 0x3000 / 2;
		return second_m_spriteram[offs&0x3ff];
	}

	return 0;
}


void toaplan2_state::second_gp9001_videoram16_w(UINT16 data, UINT16 mem_mask)
{
	int offs = second_gp9001_voffs;
	second_gp9001_voffs++;

	offs &= 0x1fff;

	switch (offs & 0x1800)
	{
	case 0x1800:
	{
		offs &= 0x3ff;
		COMBINE_DATA(&second_m_spriteram[offs & 0x3ff]);
		break;
	}
	case 0x1000:
	{
		offs &= 0x7ff;
		COMBINE_DATA(&second_m_vram_top[offs]);
		break;
	}

	case 0x800:
	{
		offs &= 0x7ff;
		COMBINE_DATA(&second_m_vram_fg[offs]);
		break;
	}
	case 0x000:
	{
		COMBINE_DATA(&second_m_vram_bg[offs]);
		break;
	}
	}

}


UINT16 toaplan2_state::second_gp9001_vdpstatus_r()
{
	return second_m_status;
	//return ((m_screen->vpos() + 15) % 262) >= 245;
}

void toaplan2_state::second_gp9001_scroll_reg_select_w(UINT16 data, UINT16 mem_mask)
{
	if (ACCESSING_BITS_0_7)
	{
		second_gp9001_scroll_reg = data & 0x8f;
		if (data & 0x70)
			logerror("Hmmm, selecting unknown LSB video control register (%04x)\n",second_gp9001_scroll_reg);
	}
	else
	{
		logerror("Hmmm, selecting unknown MSB video control register (%04x)\n",second_gp9001_scroll_reg);
	}
}

static void second_gp9001_set_scrollx_and_flip_reg(gp9001tilemaplayerx* layer, UINT16 data, UINT16 mem_mask, int flip)
{
	COMBINE_DATA(&layer->scrollx);

	if (flip)
	{
		layer->flip |= TILEMAP_FLIPX;
		layer->realscrollx = -(layer->scrollx+layer->extra_xoffset.flipped);
	}
	else
	{
		layer->flip &= (~TILEMAP_FLIPX);
		layer->realscrollx = layer->scrollx+layer->extra_xoffset.normal;
	}
	//layer->tmap->set_flip(layer->flip);
}

static void second_gp9001_set_scrolly_and_flip_reg(gp9001tilemaplayerx* layer, UINT16 data, UINT16 mem_mask, int flip)
{
	COMBINE_DATA(&layer->scrolly);

	if (flip)
	{
		layer->flip |= TILEMAP_FLIPY;
		layer->realscrolly = -(layer->scrolly+layer->extra_yoffset.flipped);

	}
	else
	{
		layer->flip &= (~TILEMAP_FLIPY);
		layer->realscrolly = layer->scrolly+layer->extra_yoffset.normal;
	}

	//layer->tmap->set_flip(layer->flip);
}

static void second_gp9001_set_sprite_scrollx_and_flip_reg(gp9001spritelayerx* layer, UINT16 data, UINT16 mem_mask, int flip)
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

static void second_gp9001_set_sprite_scrolly_and_flip_reg(gp9001spritelayerx* layer, UINT16 data, UINT16 mem_mask, int flip)
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

void toaplan2_state::second_gp9001_scroll_reg_data_w(UINT16 data, UINT16 mem_mask)
{
	/************************************************************************/
	/***** layer X and Y flips can be set independently, so emulate it ******/
	/************************************************************************/

	// writes with 8x set turn on flip for the specified layer / axis
	int flip = second_gp9001_scroll_reg & 0x80;

	switch(second_gp9001_scroll_reg&0x7f)
	{
		case 0x00: second_gp9001_set_scrollx_and_flip_reg(&second_bg, data, mem_mask, flip); break;
		case 0x01: second_gp9001_set_scrolly_and_flip_reg(&second_bg, data, mem_mask, flip); break;

		case 0x02: second_gp9001_set_scrollx_and_flip_reg(&second_fg, data, mem_mask, flip); break;
		case 0x03: second_gp9001_set_scrolly_and_flip_reg(&second_fg, data, mem_mask, flip); break;

		case 0x04: second_gp9001_set_scrollx_and_flip_reg(&second_top,data, mem_mask, flip); break;
		case 0x05: second_gp9001_set_scrolly_and_flip_reg(&second_top,data, mem_mask, flip); break;

		case 0x06: second_gp9001_set_sprite_scrollx_and_flip_reg(&second_sp, data,mem_mask,flip); break;
		case 0x07: second_gp9001_set_sprite_scrolly_and_flip_reg(&second_sp, data,mem_mask,flip); break;


		case 0x0e:  /******* Initialise video controller register ? *******/

		case 0x0f:  break;


		default:    logerror("second_ Hmmm, writing %08x to unknown video control register (%08x) !!!\n",data,gp9001_scroll_reg);
					break;
	}
}

void toaplan2_state::second_init_scroll_regs()
{
	second_gp9001_set_scrollx_and_flip_reg(&second_bg, 0, 0xffff, 0);
	second_gp9001_set_scrolly_and_flip_reg(&second_bg, 0, 0xffff, 0);
	second_gp9001_set_scrollx_and_flip_reg(&second_fg, 0, 0xffff, 0);
	second_gp9001_set_scrolly_and_flip_reg(&second_fg, 0, 0xffff, 0);
	second_gp9001_set_scrollx_and_flip_reg(&second_top,0, 0xffff, 0);
	second_gp9001_set_scrolly_and_flip_reg(&second_top,0, 0xffff, 0);
	second_gp9001_set_sprite_scrollx_and_flip_reg(&second_sp, 0,0xffff,0);
	second_gp9001_set_sprite_scrolly_and_flip_reg(&second_sp, 0,0xffff,0);
}



READ16_MEMBER( toaplan2_state::second_gp9001_vdp_r )
{
	switch (offset & (0xc/2))
	{
		case 0x04/2:
			return second_gp9001_videoram16_r();

		case 0x0c/2:
			return second_gp9001_vdpstatus_r();

		default:
			logerror("second_gp9001_vdp_r: read from unhandled offset %04x\n",offset*2);
	}

	return 0xffff;
}

WRITE16_MEMBER( toaplan2_state::second_gp9001_vdp_w )
{
	switch (offset & (0xc/2))
	{
		case 0x00/2:
			second_gp9001_voffs_w(data, mem_mask);
			break;

		case 0x04/2:
			second_gp9001_videoram16_w(data, mem_mask);
			break;

		case 0x08/2:
			second_gp9001_scroll_reg_select_w(data, mem_mask);
			break;

		case 0x0c/2:
			second_gp9001_scroll_reg_data_w(data, mem_mask);
			break;
	}

}

/* batrider and bbakraid invert the register select lines */
READ16_MEMBER( toaplan2_state::second_gp9001_vdp_alt_r )
{
	switch (offset & (0xc/2))
	{
		case 0x0/2:
			return second_gp9001_vdpstatus_r();

		case 0x8/2:
			return second_gp9001_videoram16_r();

		default:
			logerror("second_gp9001_vdp_alt_r: read from unhandled offset %04x\n",offset*2);
	}

	return 0xffff;
}

WRITE16_MEMBER( toaplan2_state::second_gp9001_vdp_alt_w )
{
	switch (offset & (0xc/2))
	{
		case 0x0/2:
			second_gp9001_scroll_reg_data_w(data, mem_mask);
			break;

		case 0x4/2:
			second_gp9001_scroll_reg_select_w(data, mem_mask);
			break;

		case 0x8/2:
			second_gp9001_videoram16_w(data, mem_mask);
			break;

		case 0xc/2:
			second_gp9001_voffs_w(data, mem_mask);
			break;
	}
}



/***************************************************************************/
/**************** PIPIBIBI bootleg interface into this video driver ********/

WRITE16_MEMBER( toaplan2_state::second_pipibibi_bootleg_scroll_w )
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
			default:    logerror("second_PIPIBIBI writing %04x to unknown scroll register %04x",data, offset);
		}

		second_gp9001_scroll_reg = offset;
		second_gp9001_scroll_reg_data_w(data, mem_mask);
	}
}

READ16_MEMBER( toaplan2_state::second_pipibibi_bootleg_videoram16_r )
{
	second_gp9001_voffs_w(offset, 0xffff);
	return second_gp9001_videoram16_r();
}

WRITE16_MEMBER( toaplan2_state::second_pipibibi_bootleg_videoram16_w )
{
	second_gp9001_voffs_w(offset, 0xffff);
	second_gp9001_videoram16_w(data, mem_mask);
}

READ16_MEMBER( toaplan2_state::second_pipibibi_bootleg_spriteram16_r )
{
	second_gp9001_voffs_w((0x1800 + offset), 0);
	return second_gp9001_videoram16_r();
}

WRITE16_MEMBER( toaplan2_state::second_pipibibi_bootleg_spriteram16_w )
{
	second_gp9001_voffs_w((0x1800 + offset), mem_mask);
	second_gp9001_videoram16_w(data, mem_mask);
}

/***************************************************************************
    Sprite Handlers
***************************************************************************/


#define second_HANDLE_FLIPX_FLIPY__NOCLIPX_NOCLIPY \
	for (int yy = 7; yy != -1; yy += -1) \
	{ \
		int drawyy = yy + sy; \
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


#define second_HANDLE_FLIPX_FLIPY__NOCLIPX_CLIPY \
	for (int yy = 7; yy != -1; yy += -1) \
	{ \
		int drawyy = yy + sy; \
		if ((drawyy >= cliprect.min_y) && (drawyy <= cliprect.max_y)) \
		{ \
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

#define second_HANDLE_FLIPX_FLIPY_CLIPX_CLIPY \
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

#define second_HANDLE_NOFLIPX_FLIPY_NOCLIPX_NOCLIPY \
	for (int yy = 7; yy != -1; yy += -1) \
	{ \
		int drawyy = yy + sy; \
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

#define second_HANDLE_NOFLIPX_FLIPY_NOCLIPX_CLIPY \
	for (int yy = 7; yy != -1; yy += -1) \
	{ \
		int drawyy = yy + sy; \
		if ((drawyy >= cliprect.min_y) && (drawyy <= cliprect.max_y)) \
		{ \
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

#define second_HANDLE_NOFLIPX_FLIPY_CLIPX_CLIPY \
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


#define second_HANDLE_FLIPX_NOFLIPY_NOCLIPX_NOCLIPY \
	for (int yy = 0; yy != 8; yy += 1) \
	{ \
		int drawyy = yy + sy; \
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

#define second_HANDLE_FLIPX_NOFLIPY_NOCLIPX_CLIPY \
	for (int yy = 0; yy != 8; yy += 1) \
	{ \
		int drawyy = yy + sy; \
		if ((drawyy >= cliprect.min_y) && (drawyy <= cliprect.max_y)) \
		{ \
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

#define second_HANDLE_FLIPX_NOFLIPY_CLIPX_CLIPY \
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


#define second_HANDLE_NOFLIPX_NOFLIPY_NOCLIPX_NOCLIPY \
	for (int yy = 0; yy != 8; yy += 1) \
	{ \
		int drawyy = yy + sy; \
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

#define second_HANDLE_NOFLIPX_NOFLIPY_NOCLIPX_CLIPY \
	for (int yy = 0; yy != 8; yy += 1) \
	{ \
		int drawyy = yy + sy; \
		if ((drawyy >= cliprect.min_y) && (drawyy <= cliprect.max_y)) \
		{ \
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

#define second_HANDLE_NOFLIPX_NOFLIPY_CLIPX_CLIPY \
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

#define second_HANDLE_NOFLIPX_NOFLIPY_CLIPX_NOCLIPY \
	for (int yy = 0; yy != 8; yy += 1) \
	{ \
		int drawyy = yy + sy; \
		for (int xx = 0; xx != 8; xx += 1) \
		{ \
			int drawxx = xx + sx; \
			if ((drawxx >= cliprect.min_x) && (drawxx <= cliprect.max_x)) \
			{ \
				UINT16* dstptr = &bitmap.pix16(drawyy, drawxx); \
				DO_PIX_NOINC; \
			} \
			srcdata++; \
		} \
	}

#define second_HANDLE_FLIPX_NOFLIPY_CLIPX_NOCLIPY \
	for (int yy = 0; yy != 8; yy += 1) \
	{ \
		int drawyy = yy + sy; \
		for (int xx = 7; xx != -1; xx += -1) \
		{ \
			int drawxx = xx + sx; \
			if ((drawxx >= cliprect.min_x) && (drawxx <= cliprect.max_x)) \
			{ \
				UINT16* dstptr = &bitmap.pix16(drawyy, drawxx); \
				DO_PIX_NOINC; \
			} \
			srcdata++; \
		} \
	}

#define second_HANDLE_NOFLIPX_FLIPY_CLIPX_NOCLIPY \
	for (int yy = 7; yy != -1; yy += -1) \
	{ \
		int drawyy = yy + sy; \
		for (int xx = 0; xx != 8; xx += 1) \
		{ \
			int drawxx = xx + sx; \
			if ((drawxx >= cliprect.min_x) && (drawxx <= cliprect.max_x)) \
			{ \
				UINT16* dstptr = &bitmap.pix16(drawyy, drawxx); \
				DO_PIX_NOINC; \
			} \
			srcdata++; \
		} \
	}

#define second_HANDLE_FLIPX_FLIPY_CLIPX_NOCLIPY \
	for (int yy = 7; yy != -1; yy += -1) \
	{ \
		int drawyy = yy + sy; \
		for (int xx = 7; xx != -1; xx += -1) \
		{ \
			int drawxx = xx + sx; \
			if ((drawxx >= cliprect.min_x) && (drawxx <= cliprect.max_x)) \
			{ \
				UINT16* dstptr = &bitmap.pix16(drawyy, drawxx); \
				DO_PIX_NOINC; \
			} \
			srcdata++; \
		} \
	}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////




#define second_OPAQUE_HANDLEFLIPX_FLIPY__NOCLIPX_NOCLIPY \
	for (int yy = 7; yy != -1; yy += -1) \
	{ \
		int drawyy = yy + sy; \
		UINT16* dstptr = &bitmap.pix16(drawyy) + sx + 7; \
		OPAQUE_DOPIX_REV; \
		OPAQUE_DOPIX_REV; \
		OPAQUE_DOPIX_REV; \
		OPAQUE_DOPIX_REV; \
		OPAQUE_DOPIX_REV; \
		OPAQUE_DOPIX_REV; \
		OPAQUE_DOPIX_REV; \
		OPAQUE_DOPIX_REV; \
	}


#define second_OPAQUE_HANDLEFLIPX_FLIPY__NOCLIPX_CLIPY \
	for (int yy = 7; yy != -1; yy += -1) \
	{ \
		int drawyy = yy + sy; \
		if ((drawyy >= cliprect.min_y) && (drawyy <= cliprect.max_y)) \
		{ \
			UINT16* dstptr = &bitmap.pix16(drawyy) + sx + 7; \
			OPAQUE_DOPIX_REV; \
			OPAQUE_DOPIX_REV; \
			OPAQUE_DOPIX_REV; \
			OPAQUE_DOPIX_REV; \
			OPAQUE_DOPIX_REV; \
			OPAQUE_DOPIX_REV; \
			OPAQUE_DOPIX_REV; \
			OPAQUE_DOPIX_REV; \
		} \
		else \
		{ \
			srcdata += 8; \
		} \
	}

#define second_OPAQUE_HANDLEFLIPX_FLIPY_CLIPX_CLIPY \
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
					OPAQUE_DOPIX_NOINC; \
				} \
				srcdata++; \
			} \
		} \
		else \
		{ \
			srcdata += 8; \
		} \
	}

#define second_OPAQUE_HANDLENOFLIPX_FLIPY_NOCLIPX_NOCLIPY \
	for (int yy = 7; yy != -1; yy += -1) \
	{ \
		int drawyy = yy + sy; \
		UINT16* dstptr = &bitmap.pix16(drawyy) + sx; \
		OPAQUE_DOPIX; \
		OPAQUE_DOPIX; \
		OPAQUE_DOPIX; \
		OPAQUE_DOPIX; \
		OPAQUE_DOPIX; \
		OPAQUE_DOPIX; \
		OPAQUE_DOPIX; \
		OPAQUE_DOPIX; \
	}

#define second_OPAQUE_HANDLENOFLIPX_FLIPY_NOCLIPX_CLIPY \
	for (int yy = 7; yy != -1; yy += -1) \
	{ \
		int drawyy = yy + sy; \
		if ((drawyy >= cliprect.min_y) && (drawyy <= cliprect.max_y)) \
		{ \
			UINT16* dstptr = &bitmap.pix16(drawyy) + sx; \
			OPAQUE_DOPIX; \
			OPAQUE_DOPIX; \
			OPAQUE_DOPIX; \
			OPAQUE_DOPIX; \
			OPAQUE_DOPIX; \
			OPAQUE_DOPIX; \
			OPAQUE_DOPIX; \
			OPAQUE_DOPIX; \
		} \
		else \
		{ \
			srcdata += 8; \
		} \
	}

#define second_OPAQUE_HANDLENOFLIPX_FLIPY_CLIPX_CLIPY \
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
					OPAQUE_DOPIX_NOINC; \
				} \
				srcdata++; \
			} \
		} \
		else \
		{ \
			srcdata += 8; \
		} \
	}


#define second_OPAQUE_HANDLEFLIPX_NOFLIPY_NOCLIPX_NOCLIPY \
	for (int yy = 0; yy != 8; yy += 1) \
	{ \
		int drawyy = yy + sy; \
		UINT16* dstptr = &bitmap.pix16(drawyy) + sx + 7; \
		OPAQUE_DOPIX_REV; \
		OPAQUE_DOPIX_REV; \
		OPAQUE_DOPIX_REV; \
		OPAQUE_DOPIX_REV; \
		OPAQUE_DOPIX_REV; \
		OPAQUE_DOPIX_REV; \
		OPAQUE_DOPIX_REV; \
		OPAQUE_DOPIX_REV; \
	}

#define second_OPAQUE_HANDLEFLIPX_NOFLIPY_NOCLIPX_CLIPY \
	for (int yy = 0; yy != 8; yy += 1) \
	{ \
		int drawyy = yy + sy; \
		if ((drawyy >= cliprect.min_y) && (drawyy <= cliprect.max_y)) \
		{ \
			UINT16* dstptr = &bitmap.pix16(drawyy) + sx + 7; \
			OPAQUE_DOPIX_REV; \
			OPAQUE_DOPIX_REV; \
			OPAQUE_DOPIX_REV; \
			OPAQUE_DOPIX_REV; \
			OPAQUE_DOPIX_REV; \
			OPAQUE_DOPIX_REV; \
			OPAQUE_DOPIX_REV; \
			OPAQUE_DOPIX_REV; \
		} \
		else \
		{ \
			srcdata += 8; \
		} \
	}

#define second_OPAQUE_HANDLEFLIPX_NOFLIPY_CLIPX_CLIPY \
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
					OPAQUE_DOPIX_NOINC; \
				} \
				srcdata++; \
			} \
		} \
		else \
		{ \
			srcdata += 8; \
		} \
	}


#define second_OPAQUE_HANDLENOFLIPX_NOFLIPY_NOCLIPX_NOCLIPY \
	for (int yy = 0; yy != 8; yy += 1) \
	{ \
		int drawyy = yy + sy; \
		UINT16* dstptr = &bitmap.pix16(drawyy) + sx; \
		OPAQUE_DOPIX; \
		OPAQUE_DOPIX; \
		OPAQUE_DOPIX; \
		OPAQUE_DOPIX; \
		OPAQUE_DOPIX; \
		OPAQUE_DOPIX; \
		OPAQUE_DOPIX; \
		OPAQUE_DOPIX; \
	}

#define second_OPAQUE_HANDLENOFLIPX_NOFLIPY_NOCLIPX_CLIPY \
	for (int yy = 0; yy != 8; yy += 1) \
	{ \
		int drawyy = yy + sy; \
		if ((drawyy >= cliprect.min_y) && (drawyy <= cliprect.max_y)) \
		{ \
			UINT16* dstptr = &bitmap.pix16(drawyy) + sx; \
			OPAQUE_DOPIX; \
			OPAQUE_DOPIX; \
			OPAQUE_DOPIX; \
			OPAQUE_DOPIX; \
			OPAQUE_DOPIX; \
			OPAQUE_DOPIX; \
			OPAQUE_DOPIX; \
			OPAQUE_DOPIX; \
		} \
		else \
		{ \
			srcdata += 8; \
		} \
	}

#define second_OPAQUE_HANDLENOFLIPX_NOFLIPY_CLIPX_CLIPY \
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
					OPAQUE_DOPIX_NOINC; \
				} \
				srcdata++; \
			} \
		} \
		else \
		{ \
			srcdata += 8; \
		} \
	}

#define second_OPAQUE_HANDLENOFLIPX_NOFLIPY_CLIPX_NOCLIPY \
	for (int yy = 0; yy != 8; yy += 1) \
	{ \
		int drawyy = yy + sy; \
		for (int xx = 0; xx != 8; xx += 1) \
		{ \
			int drawxx = xx + sx; \
			if ((drawxx >= cliprect.min_x) && (drawxx <= cliprect.max_x)) \
			{ \
				UINT16* dstptr = &bitmap.pix16(drawyy, drawxx); \
				OPAQUE_DOPIX_NOINC; \
			} \
			srcdata++; \
		} \
	}

#define second_OPAQUE_HANDLEFLIPX_NOFLIPY_CLIPX_NOCLIPY \
	for (int yy = 0; yy != 8; yy += 1) \
	{ \
		int drawyy = yy + sy; \
		for (int xx = 7; xx != -1; xx += -1) \
		{ \
			int drawxx = xx + sx; \
			if ((drawxx >= cliprect.min_x) && (drawxx <= cliprect.max_x)) \
			{ \
				UINT16* dstptr = &bitmap.pix16(drawyy, drawxx); \
				OPAQUE_DOPIX_NOINC; \
			} \
			srcdata++; \
		} \
	}

#define second_OPAQUE_HANDLENOFLIPX_FLIPY_CLIPX_NOCLIPY \
	for (int yy = 7; yy != -1; yy += -1) \
	{ \
		int drawyy = yy + sy; \
		for (int xx = 0; xx != 8; xx += 1) \
		{ \
			int drawxx = xx + sx; \
			if ((drawxx >= cliprect.min_x) && (drawxx <= cliprect.max_x)) \
			{ \
				UINT16* dstptr = &bitmap.pix16(drawyy, drawxx); \
				OPAQUE_DOPIX_NOINC; \
			} \
			srcdata++; \
		} \
	}

#define second_OPAQUE_HANDLEFLIPX_FLIPY_CLIPX_NOCLIPY \
	for (int yy = 7; yy != -1; yy += -1) \
	{ \
		int drawyy = yy + sy; \
		for (int xx = 7; xx != -1; xx += -1) \
		{ \
			int drawxx = xx + sx; \
			if ((drawxx >= cliprect.min_x) && (drawxx <= cliprect.max_x)) \
			{ \
				UINT16* dstptr = &bitmap.pix16(drawyy, drawxx); \
				OPAQUE_DOPIX_NOINC; \
			} \
			srcdata++; \
		} \
	}


/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////



void toaplan2_state::second_draw_sprites( bitmap_ind16 &bitmap, const rectangle &cliprect )
{
	const UINT16 primask = (GP9001_PRIMASK << 8);

	UINT16 *source;

	if (second_sp.use_sprite_buffer) source = second_sp.vram16_buffer.get();
	else source = &second_m_spriteram[0];
	int total_elements = m_gfxdecode->gfx(3)->elements();

	int old_x = (-(sp.scrollx)) & 0x1ff;
	int old_y = (-(sp.scrolly)) & 0x1ff;

	for (int offs = 0; offs < (GP9001_SPRITERAM_SIZE/2); offs += 4)
	{
		const int attrib = source[offs];
		const UINT16 priority = ((attrib & primask) >> 8) << 12;// << 3;
			
	//	priority+=1;

		if ((attrib & 0x8000))
		{
			int sprite;
			//if (!second_gp9001_gfxrom_is_banked)   /* No Sprite select bank switching needed */
			{
				sprite = ((attrib & 3) << 16) | source[offs + 1];   /* 18 bit */
			}
			//else        /* Batrider Sprite select bank switching required */
			//{
			//	const int sprite_num = source[offs + 1] & 0x7fff;
			//	const int bank = ((attrib & 3) << 1) | (source[offs + 1] >> 15);
			//	sprite = (second_gp9001_gfxrom_bank[bank] << 15 ) | sprite_num;
			//}
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
			if (second_sp.flip)
			{
				if (sp.flip & GP9001_SPRITE_FLIPX)
					sx_base = 320 - sx_base;
				if (sp.flip & GP9001_SPRITE_FLIPY)
					sy_base = 240 - sy_base;
			}

			/***** Cancel flip, if it, and sprite layer flip are active *****/
			flipx = (flipx ^ (second_sp.flip & GP9001_SPRITE_FLIPX));
			flipy = (flipy ^ (second_sp.flip & GP9001_SPRITE_FLIPY));

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

					if (m_gfxdecode->gfx(3)->has_pen_usage())
					{
						// fully transparent; do nothing
						UINT32 usage = m_gfxdecode->gfx(3)->pen_usage(sprite);
						if ((usage & ~(1 << 0)) == 0)
						{
							sprite++;
							continue;
						}

						if ((usage & (1 << 0)) == 0) // full opaque
						{
							const UINT8* srcdata = m_gfxdecode->gfx(3)->get_data(sprite);

							if (flipy) // FLIPY 
							{
								if (flipx) // FLIPY AND FLIP X
								{
									// --------------------------
									if ((sx >= cliprect.min_x) && (sx + 7 <= cliprect.max_x))
									{ // x unclipped
										if ((sy >= cliprect.min_y) && (sy + 7 <= cliprect.max_y))
										{  // fully unclipped
											second_OPAQUE_HANDLEFLIPX_FLIPY__NOCLIPX_NOCLIPY;
										}
										else
										{ // x unclipped, y clipped
											second_OPAQUE_HANDLEFLIPX_FLIPY__NOCLIPX_CLIPY;
										}
									}
									else
									{  // FLIPY AND FLIP X fully clipped
										if ((sy >= cliprect.min_y) && (sy + 7 <= cliprect.max_y))
										{
											second_OPAQUE_HANDLEFLIPX_FLIPY_CLIPX_NOCLIPY;
										}
										else
										{
											second_OPAQUE_HANDLEFLIPX_FLIPY_CLIPX_CLIPY;
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
											second_OPAQUE_HANDLENOFLIPX_FLIPY_NOCLIPX_NOCLIPY;
#endif
										}
										else
										{  // y needs clipping
#if 1
											second_OPAQUE_HANDLENOFLIPX_FLIPY_NOCLIPX_CLIPY;
#endif
										}

									}
									else // with clipping
									{
										if ((sy >= cliprect.min_y) && (sy + 7 <= cliprect.max_y))
										{
											second_OPAQUE_HANDLENOFLIPX_FLIPY_CLIPX_NOCLIPY;
										}
										else
										{
											second_OPAQUE_HANDLENOFLIPX_FLIPY_CLIPX_CLIPY;
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
											second_OPAQUE_HANDLEFLIPX_NOFLIPY_NOCLIPX_NOCLIPY;
#endif
										}
										else // x not clipped, y is
										{
#if 1
											second_OPAQUE_HANDLEFLIPX_NOFLIPY_NOCLIPX_CLIPY;
#endif
										}
									}
									else // clipping required
									{
										if ((sy >= cliprect.min_y) && (sy + 7 <= cliprect.max_y))
										{
											second_OPAQUE_HANDLEFLIPX_NOFLIPY_CLIPX_NOCLIPY;
										}
										else
										{
											second_OPAQUE_HANDLEFLIPX_NOFLIPY_CLIPX_CLIPY;
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
											second_OPAQUE_HANDLENOFLIPX_NOFLIPY_NOCLIPX_NOCLIPY;
#endif
										}
										else
										{ // y needs clipping
#if 1
											second_OPAQUE_HANDLENOFLIPX_NOFLIPY_NOCLIPX_CLIPY;
#endif
										}

									}
									else // with clipping
									{
										if ((sy >= cliprect.min_y) && (sy + 7 <= cliprect.max_y))
										{
											second_OPAQUE_HANDLENOFLIPX_NOFLIPY_CLIPX_NOCLIPY;
										}
										else
										{
											second_OPAQUE_HANDLENOFLIPX_NOFLIPY_CLIPX_CLIPY;
										}
									}

									// -------------------
								}
							}
						}
						else
						{ // has transparent bits
							const UINT8* srcdata = m_gfxdecode->gfx(3)->get_data(sprite);

							if (flipy) // FLIPY 
							{
								if (flipx) // FLIPY AND FLIP X
								{
									// --------------------------
									if ((sx >= cliprect.min_x) && (sx + 7 <= cliprect.max_x))
									{ // x unclipped
										if ((sy >= cliprect.min_y) && (sy + 7 <= cliprect.max_y))
										{  // fully unclipped
											second_HANDLE_FLIPX_FLIPY__NOCLIPX_NOCLIPY;
										}
										else
										{ // x unclipped, y clipped
											second_HANDLE_FLIPX_FLIPY__NOCLIPX_CLIPY;
										}
									}
									else
									{  // FLIPY AND FLIP X fully clipped
										if ((sy >= cliprect.min_y) && (sy + 7 <= cliprect.max_y))
										{
											second_HANDLE_FLIPX_FLIPY_CLIPX_NOCLIPY;
										}
										else
										{
											second_HANDLE_FLIPX_FLIPY_CLIPX_CLIPY;
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
											second_HANDLE_NOFLIPX_FLIPY_NOCLIPX_NOCLIPY;
#endif
										}
										else
										{  // y needs clipping
#if 1
											second_HANDLE_NOFLIPX_FLIPY_NOCLIPX_CLIPY;
#endif
										}

									}
									else // with clipping
									{
										if ((sy >= cliprect.min_y) && (sy + 7 <= cliprect.max_y))
										{
											second_HANDLE_NOFLIPX_FLIPY_CLIPX_NOCLIPY;
										}
										else
										{
											second_HANDLE_NOFLIPX_FLIPY_CLIPX_CLIPY;
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
											second_HANDLE_FLIPX_NOFLIPY_NOCLIPX_NOCLIPY;
#endif
										}
										else // x not clipped, y is
										{
#if 1
											second_HANDLE_FLIPX_NOFLIPY_NOCLIPX_CLIPY;
#endif
										}
									}
									else // clipping required
									{
										if ((sy >= cliprect.min_y) && (sy + 7 <= cliprect.max_y))
										{
											second_HANDLE_FLIPX_NOFLIPY_CLIPX_NOCLIPY;
										}
										else
										{
											second_HANDLE_FLIPX_NOFLIPY_CLIPX_CLIPY;
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
											second_HANDLE_NOFLIPX_NOFLIPY_NOCLIPX_NOCLIPY;
#endif
										}
										else
										{ // y needs clipping
#if 1
											second_HANDLE_NOFLIPX_NOFLIPY_NOCLIPX_CLIPY;
#endif
										}

									}
									else // with clipping
									{
										if ((sy >= cliprect.min_y) && (sy + 7 <= cliprect.max_y))
										{
											second_HANDLE_NOFLIPX_NOFLIPY_CLIPX_NOCLIPY;
										}
										else
										{
											second_HANDLE_NOFLIPX_NOFLIPY_CLIPX_CLIPY;
										}
									}

									// -------------------
								}
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

void toaplan2_state::second_draw_tmap_tile(bitmap_ind16& bitmap, const rectangle& cliprect, int sx, int sy, int sprite, int priority, int color)
{
	sprite &= m_gfxdecode->gfx(2)->elements() - 1;

	if (m_gfxdecode->gfx(2)->has_pen_usage())
	{
		//fully transparent; do nothing
		UINT32 usage = m_gfxdecode->gfx(2)->pen_usage(sprite);
		if ((usage & ~(1 << 0)) == 0)
			return;

		if ((usage & (1 << 0)) == 0) // full opaque
		{
			const UINT8* srcdata = m_gfxdecode->gfx(2)->get_data(sprite);

			if ((sx >= cliprect.min_x) && (sx + 15 <= cliprect.max_x))
			{ // x doesn't need clipping
				if ((sy >= cliprect.min_y) && (sy + 15 <= cliprect.max_y))
				{  // fully unclipped
					OPAQUE16_HANDLENOFLIPX_NOFLIPY_NOCLIPX_NOCLIPY;
				}
				else
				{ // y needs clipping
					OPAQUE16_HANDLENOFLIPX_NOFLIPY_NOCLIPX_CLIPY;
				}
			}
			else // with clipping
			{
				if ((sy >= cliprect.min_y) && (sy + 15 <= cliprect.max_y))
				{
					OPAQUE16_HANDLENOFLIPX_NOFLIPY_CLIPX_NOCLIPY;
				}
				else
				{
					OPAQUE16_HANDLENOFLIPX_NOFLIPY_CLIPX_CLIPY;
				}
			}
		}
		else
		{ // has transparent bits
			const UINT8* srcdata = m_gfxdecode->gfx(2)->get_data(sprite);

			if ((sx >= cliprect.min_x) && (sx + 15 <= cliprect.max_x))
			{ // x doesn't need clipping
				if ((sy >= cliprect.min_y) && (sy + 15 <= cliprect.max_y))
				{  // fully unclipped
					TRANS16_HANDLE_NOFLIPX_NOFLIPY_NOCLIPX_NOCLIPY;
				}
				else
				{ // y needs clipping
					TRANS16_HANDLE_NOFLIPX_NOFLIPY_NOCLIPX_CLIPY;
				}
			}
			else // with clipping
			{
				if ((sy >= cliprect.min_y) && (sy + 15 <= cliprect.max_y))
				{
					TRANS16_HANDLE_NOFLIPX_NOFLIPY_CLIPX_NOCLIPY;
				}
				else
				{
					TRANS16_HANDLE_NOFLIPX_NOFLIPY_CLIPX_CLIPY;
				}
			}
		}
	}
}


void toaplan2_state::second_draw_tmap_tile_nopri(bitmap_ind16& bitmap, const rectangle& cliprect, int sx, int sy, int sprite, int priority, int color)
{
	sprite &= m_gfxdecode->gfx(2)->elements() - 1;

	if (m_gfxdecode->gfx(2)->has_pen_usage())
	{
		//fully transparent; do nothing
		UINT32 usage = m_gfxdecode->gfx(2)->pen_usage(sprite);
		if ((usage & ~(1 << 0)) == 0)
			return;

		if ((usage & (1 << 0)) == 0) // full opaque
		{
			const UINT8* srcdata = m_gfxdecode->gfx(2)->get_data(sprite);

			if ((sx >= cliprect.min_x) && (sx + 15 <= cliprect.max_x))
			{ // x doesn't need clipping
				if ((sy >= cliprect.min_y) && (sy + 15 <= cliprect.max_y))
				{  // fully unclipped
					OPAQUE16_HANDLENOFLIPX_NOFLIPY_NOCLIPX_NOCLIPY_NOPRI;
				}
				else
				{ // y needs clipping
					OPAQUE16_HANDLENOFLIPX_NOFLIPY_NOCLIPX_CLIPY_NOPRI;
				}
			}
			else // with clipping
			{
				if ((sy >= cliprect.min_y) && (sy + 15 <= cliprect.max_y))
				{
					OPAQUE16_HANDLENOFLIPX_NOFLIPY_CLIPX_NOCLIPY_NOPRI;
				}
				else
				{
					OPAQUE16_HANDLENOFLIPX_NOFLIPY_CLIPX_CLIPY_NOPRI;
				}
			}
		}
		else
		{ // has transparent bits
			const UINT8* srcdata = m_gfxdecode->gfx(2)->get_data(sprite);

			if ((sx >= cliprect.min_x) && (sx + 15 <= cliprect.max_x))
			{ // x doesn't need clipping
				if ((sy >= cliprect.min_y) && (sy + 15 <= cliprect.max_y))
				{  // fully unclipped
					TRANS16_HANDLE_NOFLIPX_NOFLIPY_NOCLIPX_NOCLIPY_NOPRI;
				}
				else
				{ // y needs clipping
					TRANS16_HANDLE_NOFLIPX_NOFLIPY_NOCLIPX_CLIPY_NOPRI;
				}
			}
			else // with clipping
			{
				if ((sy >= cliprect.min_y) && (sy + 15 <= cliprect.max_y))
				{
					TRANS16_HANDLE_NOFLIPX_NOFLIPY_CLIPX_NOCLIPY_NOPRI;
				}
				else
				{
					TRANS16_HANDLE_NOFLIPX_NOFLIPY_CLIPX_CLIPY_NOPRI;
				}
			}
		}
	}
}


void toaplan2_state::second_gp9001_draw_a_tilemap(bitmap_ind16& bitmap, const rectangle& cliprect, gp9001tilemaplayerx *tmap, UINT16* vram)
{
	int scrollxfg = tmap->realscrollx;
	int scrollyfg = tmap->realscrolly;

	int ydraw = -(scrollyfg & 0xf);
	int xsource_base = (scrollxfg >> 4);
	int xdrawbase = -(scrollxfg & 0xf);

	int ysourcetile= (scrollyfg >> 4);

	for (int y = 0; y < 16; y++, ydraw += 16, ysourcetile++)
	{
		ysourcetile &= 0x1f;

		int xdraw = xdrawbase;
		int xsourcetile  = xsource_base;

		for (int x = 0; x < 21; x++, xdraw += 16, xsourcetile++)
		{
			xsourcetile &= 0x1f;

			int tile_number, attrib;
			int tile_index = ((ysourcetile * 32) + xsourcetile);
				
			attrib = vram[2*tile_index];

			tile_number = vram[2*tile_index+1];

			//if (second_gp9001_gfxrom_is_banked)
			//{
			//	tile_number = ( second_gp9001_gfxrom_bank[(tile_number >> 13) & 7] << 13 ) | ( tile_number & 0x1fff );
			//}

			int color = attrib & 0x007f; // 0x0f00 priority, 0x007f colour
			int priority = (attrib & GP9001_PRIMASK_TMAPS) << 4;

			second_draw_tmap_tile(bitmap, cliprect, xdraw, ydraw, tile_number, priority, color << 4);
		}
	}

}

void toaplan2_state::second_gp9001_draw_a_tilemap_nopri(bitmap_ind16& bitmap, const rectangle& cliprect, gp9001tilemaplayerx *tmap, UINT16* vram)
{
	int scrollxfg = tmap->realscrollx;
	int scrollyfg = tmap->realscrolly;

	int ydraw = -(scrollyfg & 0xf);
	int xsource_base = (scrollxfg >> 4);
	int xdrawbase = -(scrollxfg & 0xf);

	int ysourcetile= (scrollyfg >> 4);

	for (int y = 0; y < 16; y++, ydraw += 16, ysourcetile++)
	{
		ysourcetile &= 0x1f;

		int xdraw = xdrawbase;
		int xsourcetile  = xsource_base;

		for (int x = 0; x < 21; x++, xdraw += 16, xsourcetile++)
		{
			xsourcetile &= 0x1f;

			int tile_number, attrib;
			int tile_index = ((ysourcetile * 32) + xsourcetile);
				
			attrib = vram[2*tile_index];

			tile_number = vram[2*tile_index+1];

			//if (second_gp9001_gfxrom_is_banked)
			//{
			//	tile_number = ( second_gp9001_gfxrom_bank[(tile_number >> 13) & 7] << 13 ) | ( tile_number & 0x1fff );
			//}

			int color = attrib & 0x007f; // 0x0f00 priority, 0x007f colour
			int priority = (attrib & GP9001_PRIMASK_TMAPS) << 4;

			second_draw_tmap_tile_nopri(bitmap, cliprect, xdraw, ydraw, tile_number, priority, color << 4);
		}
	}

}



void toaplan2_state::second_gp9001_render_vdp(bitmap_ind16 &bitmap, const rectangle &cliprect)
{
	//if (second_gp9001_gfxrom_is_banked && second_gp9001_gfxrom_bank_dirty)
	//{
	//	second_gp9001_gfxrom_bank_dirty = 0;
	//}

	second_gp9001_draw_a_tilemap_nopri(bitmap, cliprect, &second_bg, second_m_vram_bg);
	second_gp9001_draw_a_tilemap(bitmap, cliprect, &second_fg, second_m_vram_fg);
	second_gp9001_draw_a_tilemap(bitmap, cliprect, &second_top, second_m_vram_top);
	second_draw_sprites(bitmap,cliprect);

}


void toaplan2_state::second_gp9001_screen_eof(void)
{
	/** Shift sprite RAM buffers  ***  Used to fix sprite lag **/
	if (second_sp.use_sprite_buffer) memcpy(second_sp.vram16_buffer.get(),second_m_spriteram,GP9001_SPRITERAM_SIZE);
}
