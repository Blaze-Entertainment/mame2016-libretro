// license:BSD-3-Clause
// copyright-holders:Manuel Abadia
class targeth_state : public driver_device
{
public:
	targeth_state(const machine_config &mconfig, device_type type, const char *tag)
		: driver_device(mconfig, type, tag),
		m_maincpu(*this,"maincpu"),
		m_gfxdecode(*this, "gfxdecode"),
		m_screen(*this, "screen"),
		m_palette(*this, "palette"),
		m_videoram(*this, "videoram"),
		m_vregs(*this, "vregs"),
		m_spriteram(*this, "spriteram"),
		m_shareram(*this, "shareram"),
		m_gun1x(*this, "GUNX1"),
		m_gun1y(*this,"GUNY1"),
		m_gun2x(*this,"GUNX2"),
		m_gun2y(*this,"GUNY2"),
		m_fake(*this,"FAKE")
	{ }

	required_device<cpu_device> m_maincpu;
	required_device<gfxdecode_device> m_gfxdecode;
	required_device<screen_device> m_screen;
	required_device<palette_device> m_palette;

	required_shared_ptr<UINT16> m_videoram;
	required_shared_ptr<UINT16> m_vregs;
	required_shared_ptr<UINT16> m_spriteram;
	required_shared_ptr<uint16_t> m_shareram;
	required_ioport m_gun1x;
	required_ioport m_gun1y;
	required_ioport m_gun2x;
	required_ioport m_gun2y;
	required_ioport m_fake;

	tilemap_t *m_pant[2];
	
	emu_timer       *m_gun_irq_timer[2];
	void handle_gunhack(ioport_field* fldsx, ioport_field* fldsy, int shift);

	DECLARE_READ8_MEMBER(dallas_share_r);
	DECLARE_WRITE8_MEMBER(dallas_share_w);

	DECLARE_WRITE16_MEMBER(OKIM6295_bankswitch_w);
	DECLARE_WRITE16_MEMBER(coin_counter_w);
	DECLARE_WRITE16_MEMBER(vram_w);

	TILE_GET_INFO_MEMBER(get_tile_info_screen0);
	TILE_GET_INFO_MEMBER(get_tile_info_screen1);

	TIMER_CALLBACK_MEMBER(gun1_irq);
	TIMER_CALLBACK_MEMBER(gun2_irq);

	virtual void video_start() override;
	virtual void machine_start() override;

	UINT32 screen_update(screen_device &screen, bitmap_ind16 &bitmap, const rectangle &cliprect);
	void draw_sprites(bitmap_ind16 &bitmap, const rectangle &cliprect);
};
