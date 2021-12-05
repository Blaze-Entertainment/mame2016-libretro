// license:BSD-3-Clause
// copyright-holders:Manuel Abadia
/***************************************************************************

    Gaelco game hardware from 1991-1996

***************************************************************************/

#include "machine/gaelcrpt.h"

class gaelco_state : public driver_device
{
public:
	gaelco_state(const machine_config &mconfig, device_type type, const char *tag)
		: driver_device(mconfig, type, tag),
		m_maincpu(*this, "maincpu"),
		m_gfxdecode(*this, "gfxdecode"),
		m_palette(*this, "palette"),
		m_paletteram(*this, "paletteram"),
		m_audiocpu(*this, "audiocpu"),
		m_vramcrypt(*this, "vramcrypt"),
		m_videoram(*this, "videoram"),
		m_vregs(*this, "vregs"),
		m_spriteram(*this, "spriteram"),
		m_screenram(*this, "screenram"),
		m_mainram(*this, "mainram"),
		m_mainrom(*this, "maincpu"),
		m_okibank(*this, "okibank"),
		m_sprite_palette_force_high(0x38),
		m_use_squash_sprite_disable(false)
	{ }

	/* devices */
	required_device<cpu_device> m_maincpu;
	required_device<gfxdecode_device> m_gfxdecode;
	required_device<palette_device> m_palette;
	required_shared_ptr<uint16_t> m_paletteram;
	optional_device<cpu_device> m_audiocpu;
	optional_device<gaelco_crypt_device> m_vramcrypt;

	/* memory pointers */
	required_shared_ptr<UINT16> m_videoram;
	required_shared_ptr<UINT16> m_vregs;
	required_shared_ptr<UINT16> m_spriteram;
	optional_shared_ptr<UINT16> m_screenram;
	required_shared_ptr<UINT16> m_mainram;
	
	required_region_ptr<UINT16> m_mainrom;
	optional_memory_bank m_okibank;

	/* video-related */
	tilemap_t      *m_tilemap[2];

	DECLARE_WRITE16_MEMBER(bigkarnk_sound_command_w);
	DECLARE_WRITE16_MEMBER(bigkarnk_coin_w);
	DECLARE_WRITE16_MEMBER(OKIM6295_bankswitch_w);
	DECLARE_WRITE16_MEMBER(gaelco_vram_encrypted_w);
	DECLARE_WRITE16_MEMBER(gaelco_encrypted_w);
	DECLARE_WRITE16_MEMBER(thoop_vram_encrypted_w);
	DECLARE_WRITE16_MEMBER(thoop_encrypted_w);
	DECLARE_WRITE16_MEMBER(gaelco_vram_w);
	DECLARE_WRITE16_MEMBER(gaelco_irq6_line_clear);
	DECLARE_WRITE16_MEMBER(gaelco_palette_w);

	TILE_GET_INFO_MEMBER(get_tile_info_gaelco_screen0);
	TILE_GET_INFO_MEMBER(get_tile_info_gaelco_screen1);

	virtual void machine_start() override;
	DECLARE_VIDEO_START(bigkarnk);
	DECLARE_VIDEO_START(maniacsq);
	DECLARE_VIDEO_START(squash);

	INTERRUPT_GEN_MEMBER( gaelco_irq6_line_hold );

	UINT32 screen_update_bigkarnk(screen_device &screen, bitmap_ind16 &bitmap, const rectangle &cliprect);
	UINT32 screen_update_thoop(screen_device &screen, bitmap_ind16 &bitmap, const rectangle &cliprect);
	void draw_sprites( screen_device &screen, bitmap_ind16 &bitmap, const rectangle &cliprect );
	int squash_tilecode_remap(int code);

	DECLARE_DRIVER_INIT(bigkarnak);
	DECLARE_WRITE16_MEMBER(bigkarnak_prot_w);
	DECLARE_READ16_MEMBER(bigkarnak_prot_r);

	DECLARE_READ16_MEMBER(bigkarnak_skip_rom_r);

	DECLARE_READ16_MEMBER(bigkarnak_skip_r);
	DECLARE_READ16_MEMBER(bigkarnak_skip2_r);

	DECLARE_WRITE16_MEMBER(bigkarnak_skip_w);
	DECLARE_WRITE16_MEMBER(bigkarnak_skip2_w);
	
	DECLARE_DRIVER_INIT(thoop);
	DECLARE_WRITE16_MEMBER(thoop_ramhack_w);

	uint8_t m_sprite_palette_force_high;
	bool m_use_squash_sprite_disable;
};
