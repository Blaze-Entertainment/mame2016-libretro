// license:BSD-3-Clause
// copyright-holders:Manuel Abadia, Peter Ferrie
class thoop2_state : public driver_device
{
public:
	thoop2_state(const machine_config &mconfig, device_type type, const char *tag)
		: driver_device(mconfig, type, tag),
		m_maincpu(*this, "maincpu"),
		m_gfxdecode(*this, "gfxdecode"),
		m_palette(*this, "palette"),
		m_videoram(*this, "videoram"),
		m_vregs(*this, "vregs"),
		m_spriteram(*this, "spriteram"),
		m_shareram(*this, "shareram"),
		m_mcu_ram(*this, "mcu"),
		m_mainrom(*this, "maincpu"),
		m_okibank(*this, "okibank")
	{ }

	DECLARE_WRITE16_MEMBER(OKIM6295_bankswitch_w);
	DECLARE_WRITE16_MEMBER(coin_w);
	DECLARE_READ16_MEMBER(DS5002FP_R);
	DECLARE_WRITE16_MEMBER(vram_w);
	
	DECLARE_READ8_MEMBER(dallas_share_r);
	DECLARE_WRITE8_MEMBER(dallas_share_w);
	
	DECLARE_DRIVER_INIT(thoop2);
	DECLARE_READ16_MEMBER(thoop_rom_r);
	DECLARE_READ16_MEMBER(thoop_rom2_r);

	virtual void machine_start() override;
	virtual void video_start() override;

	TILE_GET_INFO_MEMBER(get_tile_info_screen0);
	TILE_GET_INFO_MEMBER(get_tile_info_screen1);
	UINT32 screen_update(screen_device &screen, bitmap_ind16 &bitmap, const rectangle &cliprect);
	void draw_sprites(screen_device &screen, bitmap_ind16 &bitmap, const rectangle &cliprect);

private:
	tilemap_t *m_pant[2];

	required_device<cpu_device> m_maincpu;
	required_device<gfxdecode_device> m_gfxdecode;
	required_device<palette_device> m_palette;
	
	required_shared_ptr<uint16_t> m_videoram;
	required_shared_ptr<uint16_t> m_vregs;
	required_shared_ptr<uint16_t> m_spriteram;
	required_shared_ptr<uint16_t> m_shareram;
	required_region_ptr<uint8_t> m_mcu_ram;
	required_region_ptr<UINT16> m_mainrom;
	required_memory_bank m_okibank;
};
