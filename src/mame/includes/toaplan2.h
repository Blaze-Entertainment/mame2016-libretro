// license:BSD-3-Clause
// copyright-holders:Quench, Yochizo, David Haywood
/**************** Machine stuff ******************/
//#define USE_HD64x180          /* Define if CPU support is available */
//#define TRUXTON2_STEREO       /* Uncomment to hear truxton2 music in stereo */

// We encode priority with colour in the tilemaps, so need a larger palette
#define T2PALETTE_LENGTH 0x10000

#include "cpu/simpletoaplan_m68000/m68000.h"
#include "machine/eepromser.h"
#include "machine/nmk112.h"
#include "machine/upd4992.h"
#include "video/gp9001.h"
#include "sound/okim6295.h"
#include "cpu/simpletoaplan_nec/v25.h"
#include "cpu/z80/z80.h"
#include "cpu/z180/z180.h"
#include "sound/2151intf.h"
#include "sound/3812intf.h"
#include "sound/ymz280b.h"



struct gp9001layeroffsetsx
{
	int normal;
	int flipped;
};

struct gp9001layerx
{
	UINT16 flip;
	UINT16 scrollx;
	UINT16 scrolly;

	UINT16 realscrollx;
	UINT16 realscrolly;

	gp9001layeroffsetsx extra_xoffset;
	gp9001layeroffsetsx extra_yoffset;
};

struct gp9001tilemaplayerx : gp9001layerx
{
	//tilemap_t *tmap;
};

struct gp9001spritelayerx : gp9001layerx
{
	bool use_sprite_buffer;
	std::unique_ptr<UINT16[]> vram16_buffer; // vram buffer for this layer
};

class toaplan2_state : public driver_device
{
public:
	enum
	{
		TIMER_RAISE_IRQ
	};

	toaplan2_state(const machine_config &mconfig, device_type type, const char *tag)
		: driver_device(mconfig, type, tag),
		m_mainrom(*this, "maincpu"),
		m_in1(*this, "IN1"),
		m_dswa(*this, "DSWA"),
		m_dswb(*this, "DSWB"),
		m_jmpr(*this, "JMPR"),
		m_pad1(*this, "PAD1"),
		m_pad2(*this, "PAD2"),
		m_ymsnd(*this, "ym2151snd"),
		m_mainram(*this, "mainram"),
		m_shared_ram(*this, "shared_ram"),
		m_shared_ram16(*this, "shared_ram16"),
		m_paletteram(*this, "paletteram"),
		m_tx_videoram(*this, "tx_videoram"),
		m_tx_lineselect(*this, "tx_lineselect"),
		m_tx_linescroll(*this, "tx_linescroll"),
		m_tx_gfxram16(*this, "tx_gfxram16"),
		m_mainram16(*this, "mainram16"),
		m_maincpu(*this, "maincpu"),
		m_audiocpu(*this, "audiocpu"),
		m_v25audiocpu(*this, "v25audiocpu"),
		//m_vdp0(*this, "gp9001"),
		m_vdp1(*this, "gp9001_1"),
		m_nmk112(*this, "nmk112"),
		m_oki(*this, "oki"),
		m_oki1(*this, "oki1"),
		m_eeprom(*this, "eeprom"),
		m_rtc(*this, "rtc"),
		m_gfxdecode(*this, "gfxdecode"),
		m_screen(*this, "screen"),
		m_palette(*this, "palette") { }


	///////////// one VDP!!

	void set_status(int status) { m_status = status; }
	UINT16 gp9001_voffs;
	UINT16 gp9001_scroll_reg;

	gp9001tilemaplayerx bg, top, fg;
	gp9001spritelayerx sp;

	// technically this is just rom banking, allowing the chip to see more graphic ROM, however it's easier to handle it
	// in the chip implementation than externally for now (which would require dynamic decoding of the entire charsets every
	// time the bank was changed)
	int gp9001_gfxrom_is_banked;
	int gp9001_gfxrom_bank_dirty;       /* dirty flag of object bank (for Batrider) */
	UINT16 gp9001_gfxrom_bank[8];       /* Batrider object bank */


	void draw_sprites( bitmap_ind16 &bitmap, const rectangle &cliprect );
	void gp9001_render_vdp( bitmap_ind16 &bitmap, const rectangle &cliprect);
	void gp9001_screen_eof(void);
	void create_tilemaps(void);
	void init_scroll_regs(void);

	bitmap_ind16 *custom_priority_bitmap;

	// access to VDP
	DECLARE_READ16_MEMBER( gp9001_vdp_r );
	DECLARE_WRITE16_MEMBER( gp9001_vdp_w );
	DECLARE_READ16_MEMBER( gp9001_vdp_alt_r );
	DECLARE_WRITE16_MEMBER( gp9001_vdp_alt_w );

	// this bootleg has strange access
	DECLARE_READ16_MEMBER( pipibibi_bootleg_videoram16_r );
	DECLARE_WRITE16_MEMBER( pipibibi_bootleg_videoram16_w );
	DECLARE_READ16_MEMBER( pipibibi_bootleg_spriteram16_r );
	DECLARE_WRITE16_MEMBER( pipibibi_bootleg_spriteram16_w );
	DECLARE_WRITE16_MEMBER( pipibibi_bootleg_scroll_w );

	// internal handlers
	DECLARE_WRITE16_MEMBER( gp9001_bg_tmap_w );
	DECLARE_WRITE16_MEMBER( gp9001_fg_tmap_w );
	DECLARE_WRITE16_MEMBER( gp9001_top_tmap_w );
	
	UINT16 gp9001_vdpstatus_r(void);


	void vdp_start();
	void vdp_reset();

	UINT16 m_maxpri = 0;

	UINT16 m_vram_bg[0x800];
	UINT16 m_vram_fg[0x800];
	UINT16 m_vram_top[0x800];
	UINT16 m_spriteram[0x800];
	UINT16 m_unkram[0x800];
	void draw_tmap_tile(bitmap_ind16& bitmap, const rectangle& cliprect, int sx, int sy, int sprite, int priority, int color);
	void gp9001_draw_a_tilemap(bitmap_ind16& bitmap, const rectangle& cliprect, gp9001tilemaplayerx* tmap, UINT16* vram);

	void draw_tmap_tile_nopri(bitmap_ind16& bitmap, const rectangle& cliprect, int sx, int sy, int sprite, int priority, int color);
	void gp9001_draw_a_tilemap_nopri(bitmap_ind16& bitmap, const rectangle& cliprect, gp9001tilemaplayerx* tmap, UINT16* vram);


	int m_status;

	inline void gp9001_voffs_w(UINT16 data, UINT16 mem_mask);
	inline int gp9001_videoram16_r(void);
	inline void gp9001_videoram16_w(UINT16 data, UINT16 mem_mask);
	inline void gp9001_scroll_reg_select_w(UINT16 data, UINT16 mem_mask);
	inline void gp9001_scroll_reg_data_w(UINT16 data, UINT16 mem_mask);


	// ------------------------------

	///////////// one VDP!!

	void second_set_status(int status) { second_m_status = status; }
	UINT16 second_gp9001_voffs;
	UINT16 second_gp9001_scroll_reg;

	gp9001tilemaplayerx second_bg, second_top, second_fg;
	gp9001spritelayerx second_sp;

	// technically this is just rom banking, allowing the chip to see more graphic ROM, however it's easier to handle it
	// in the chip implementation than externally for now (which would require dynamic decoding of the entire charsets every
	// time the bank was changed)
	//int second_gp9001_gfxrom_is_banked;
	//int second_gp9001_gfxrom_bank_dirty;       /* dirty flag of object bank (for Batrider) */
	//UINT16 second_gp9001_gfxrom_bank[8];       /* Batrider object bank */


	void second_draw_sprites( bitmap_ind16 &bitmap, const rectangle &cliprect );
	void second_gp9001_render_vdp( bitmap_ind16 &bitmap, const rectangle &cliprect);
	void second_gp9001_screen_eof(void);
	void second_create_tilemaps(void);
	void second_init_scroll_regs(void);

	bitmap_ind16 *second_custom_priority_bitmap;

	// access to VDP
	DECLARE_READ16_MEMBER( second_gp9001_vdp_r );
	DECLARE_WRITE16_MEMBER( second_gp9001_vdp_w );
	DECLARE_READ16_MEMBER( second_gp9001_vdp_alt_r );
	DECLARE_WRITE16_MEMBER( second_gp9001_vdp_alt_w );

	// this bootleg has strange access
	DECLARE_READ16_MEMBER( second_pipibibi_bootleg_videoram16_r );
	DECLARE_WRITE16_MEMBER( second_pipibibi_bootleg_videoram16_w );
	DECLARE_READ16_MEMBER( second_pipibibi_bootleg_spriteram16_r );
	DECLARE_WRITE16_MEMBER( second_pipibibi_bootleg_spriteram16_w );
	DECLARE_WRITE16_MEMBER( second_pipibibi_bootleg_scroll_w );

	// internal handlers
	DECLARE_WRITE16_MEMBER( second_gp9001_bg_tmap_w );
	DECLARE_WRITE16_MEMBER( second_gp9001_fg_tmap_w );
	DECLARE_WRITE16_MEMBER( second_gp9001_top_tmap_w );
	
	UINT16 second_gp9001_vdpstatus_r(void);


	void second_vdp_start();
	void second_vdp_reset();


	UINT16 second_m_vram_bg[0x800];
	UINT16 second_m_vram_fg[0x800];
	UINT16 second_m_vram_top[0x800];
	UINT16 second_m_spriteram[0x800];
	UINT16 second_m_unkram[0x800];
	void second_draw_tmap_tile(bitmap_ind16& bitmap, const rectangle& cliprect, int sx, int sy, int sprite, int priority, int color);
	void second_gp9001_draw_a_tilemap(bitmap_ind16& bitmap, const rectangle& cliprect, gp9001tilemaplayerx* tmap, UINT16* vram);

	void second_draw_tmap_tile_nopri(bitmap_ind16& bitmap, const rectangle& cliprect, int sx, int sy, int sprite, int priority, int color);
	void second_gp9001_draw_a_tilemap_nopri(bitmap_ind16& bitmap, const rectangle& cliprect, gp9001tilemaplayerx* tmap, UINT16* vram);


	int second_m_status;

	inline void second_gp9001_voffs_w(UINT16 data, UINT16 mem_mask);
	inline int second_gp9001_videoram16_r(void);
	inline void second_gp9001_videoram16_w(UINT16 data, UINT16 mem_mask);
	inline void second_gp9001_scroll_reg_select_w(UINT16 data, UINT16 mem_mask);
	inline void second_gp9001_scroll_reg_data_w(UINT16 data, UINT16 mem_mask);


	TIMER_DEVICE_CALLBACK_MEMBER(toaplan2_scanline);
	required_region_ptr<UINT16> m_mainrom;
	optional_ioport m_in1;
	optional_ioport m_dswa;
	optional_ioport m_dswb;
	optional_ioport m_jmpr;  // 	return ioport("JMPR")->read() ^ 0xff;
	optional_ioport m_pad1;
	optional_ioport m_pad2;

	optional_device<ym2151_device> m_ymsnd;
	optional_shared_ptr<UINT16> m_mainram;
	optional_shared_ptr<UINT8> m_shared_ram; // 8 bit RAM shared between 68K and sound CPU
	optional_shared_ptr<UINT16> m_shared_ram16;     // Really 8 bit RAM connected to Z180
	optional_shared_ptr<UINT16> m_paletteram;
	optional_shared_ptr<UINT16> m_tx_videoram;
	optional_shared_ptr<UINT16> m_tx_lineselect;
	optional_shared_ptr<UINT16> m_tx_linescroll;
	optional_shared_ptr<UINT16> m_tx_gfxram16;
	optional_shared_ptr<UINT16> m_mainram16;

	required_device<simpletoaplan_m68000_base_device> m_maincpu;
	optional_device<cpu_device> m_audiocpu;
	optional_device<simpletoaplan_v25_common_device> m_v25audiocpu;
	//required_device<gp9001vdp_device> m_vdp0;
	optional_device<gp9001vdp_device> m_vdp1;
	optional_device<nmk112_device> m_nmk112;
	optional_device<okim6295_device> m_oki;
	optional_device<okim6295_device> m_oki1;
	optional_device<eeprom_serial_93cxx_device> m_eeprom;
	optional_device<upd4992_device> m_rtc;
	optional_device<gfxdecode_device> m_gfxdecode;
	required_device<screen_device> m_screen;
	required_device<palette_device> m_palette;

	DECLARE_READ8_MEMBER(vfive_sound_skip_r);
	DECLARE_READ8_MEMBER(fixeight_alt_sound_skip_r);
	DECLARE_READ8_MEMBER(batsugun_sound_skip_r);
	DECLARE_READ8_MEMBER(dogyuun_sound_skip_r);

	DECLARE_WRITE16_MEMBER(v5_shared_ram_w);
	IRQ_CALLBACK_MEMBER(irqack);
	DECLARE_WRITE8_MEMBER(sound_status_changed);
	DECLARE_READ16_MEMBER(ghox_vdp_status_r);
	DECLARE_READ16_MEMBER(kbash_vdp_status_r);
	DECLARE_READ16_MEMBER(v5_vdp_status_r);
	DECLARE_READ16_MEMBER(batsugun_vdp_status_r);
	DECLARE_WRITE16_MEMBER(toaplan2_palette_w);
	// Teki Paki sound
	uint8_t m_cmdavailable;

	DECLARE_READ16_MEMBER(in1_r);
	DECLARE_WRITE16_MEMBER(tekipaki_mcu_w);
	DECLARE_READ8_MEMBER(tekipaki_soundlatch_r);
	DECLARE_READ8_MEMBER(tekipaki_cmdavailable_r);
	DECLARE_READ8_MEMBER(tekipaki_sound_skip_r);
	DECLARE_READ16_MEMBER(tekipaki_main_skip_r);
	DECLARE_READ16_MEMBER(truxton2_main_skip_r);

	UINT16 m_mcu_data;
	INT8 m_old_p1_paddle_h; /* For Ghox */
	INT8 m_old_p2_paddle_h;
	UINT8 m_v25_reset_line; /* 0x20 for dogyuun/batsugun, 0x10 for vfive, 0x08 for fixeight */
	UINT8 m_sndirq_line;        /* IRQ4 for batrider, IRQ2 for bbakraid */
	UINT8 m_z80_busreq;

	bitmap_ind16 m_custom_priority_bitmap;

	bitmap_ind16 m_secondary_render_bitmap;

	tilemap_t *m_tx_tilemap;    /* Tilemap for extra-text-layer */
	DECLARE_READ16_MEMBER(video_count_r);
	DECLARE_WRITE8_MEMBER(toaplan2_coin_w);
	DECLARE_WRITE16_MEMBER(toaplan2_coin_word_w);
	DECLARE_WRITE16_MEMBER(toaplan2_v25_coin_word_w);
	DECLARE_WRITE16_MEMBER(shippumd_coin_word_w);
	DECLARE_READ16_MEMBER(shared_ram_r);
	DECLARE_WRITE16_MEMBER(shared_ram_w);
	DECLARE_WRITE16_MEMBER(toaplan2_hd647180_cpu_w);
	DECLARE_READ16_MEMBER(ghox_p1_h_analog_r);
	DECLARE_READ16_MEMBER(ghox_p2_h_analog_r);

	//rgb_t expanded_palette[0x10000];


	DECLARE_WRITE16_MEMBER(fixeight_subcpu_ctrl_w);
	DECLARE_WRITE16_MEMBER(fixeightbl_oki_bankswitch_w);
	DECLARE_READ8_MEMBER(v25_dswa_r);
	DECLARE_READ8_MEMBER(v25_dswb_r);
	DECLARE_READ8_MEMBER(v25_jmpr_r);
	DECLARE_READ8_MEMBER(fixeight_region_r);
	DECLARE_WRITE8_MEMBER(raizing_z80_bankswitch_w);
	DECLARE_WRITE8_MEMBER(raizing_oki_bankswitch_w);
	DECLARE_WRITE16_MEMBER(bgaregga_soundlatch_w);
	DECLARE_READ8_MEMBER(bgaregga_E01D_r);
	DECLARE_WRITE8_MEMBER(bgaregga_E00C_w);
	DECLARE_READ16_MEMBER(batrider_z80_busack_r);
	DECLARE_WRITE16_MEMBER(batrider_z80_busreq_w);
	DECLARE_READ16_MEMBER(batrider_z80rom_r);
	DECLARE_WRITE16_MEMBER(batrider_soundlatch_w);
	DECLARE_WRITE16_MEMBER(batrider_soundlatch2_w);
	DECLARE_WRITE16_MEMBER(batrider_unknown_sound_w);
	DECLARE_WRITE16_MEMBER(batrider_clear_sndirq_w);
	DECLARE_WRITE8_MEMBER(batrider_sndirq_w);
	DECLARE_WRITE8_MEMBER(batrider_clear_nmi_w);
	DECLARE_READ16_MEMBER(bbakraid_eeprom_r);
	DECLARE_WRITE16_MEMBER(bbakraid_eeprom_w);
	DECLARE_WRITE16_MEMBER(toaplan2_tx_videoram_w);
	DECLARE_WRITE16_MEMBER(toaplan2_tx_linescroll_w);
	DECLARE_WRITE16_MEMBER(toaplan2_tx_gfxram16_w);
	DECLARE_WRITE16_MEMBER(batrider_textdata_dma_w);
	DECLARE_WRITE16_MEMBER(batrider_unknown_dma_w);
	DECLARE_WRITE16_MEMBER(batrider_objectbank_w);
	DECLARE_CUSTOM_INPUT_MEMBER(c2map_r);
	DECLARE_WRITE16_MEMBER(oki_bankswitch_w);
	DECLARE_WRITE16_MEMBER(oki1_bankswitch_w);
	DECLARE_DRIVER_INIT(batsugun);
	DECLARE_DRIVER_INIT(bbakraid);
	DECLARE_DRIVER_INIT(pipibibsbl);
	DECLARE_DRIVER_INIT(dogyuun);
	DECLARE_DRIVER_INIT(fixeight);
	DECLARE_DRIVER_INIT(bgaregga);
	DECLARE_DRIVER_INIT(fixeightbl);
	DECLARE_DRIVER_INIT(vfive);
	DECLARE_DRIVER_INIT(batrider);
	DECLARE_DRIVER_INIT(tekipaki);
	DECLARE_DRIVER_INIT(truxton2);
	TILE_GET_INFO_MEMBER(get_text_tile_info);
	DECLARE_MACHINE_START(toaplan2);

	DECLARE_MACHINE_START(vfive);
	DECLARE_MACHINE_START(fixeight);
	DECLARE_MACHINE_START(kbash);
	DECLARE_MACHINE_START(dogyuun);
	DECLARE_MACHINE_START(batsugun);

	DECLARE_MACHINE_START(ghox);
	DECLARE_MACHINE_START(tekipaki);
	DECLARE_MACHINE_START(truxton2);
	DECLARE_MACHINE_START(batrider);


	DECLARE_MACHINE_RESET(toaplan2);
	DECLARE_VIDEO_START(toaplan2);
	DECLARE_MACHINE_RESET(ghox);
	DECLARE_VIDEO_START(truxton2);
	DECLARE_VIDEO_START(fixeightbl);
	DECLARE_VIDEO_START(bgaregga);
	DECLARE_VIDEO_START(bgareggabl);
	DECLARE_VIDEO_START(batrider);
	UINT32 screen_update_toaplan2(screen_device &screen, bitmap_ind16 &bitmap, const rectangle &cliprect);
	UINT32 screen_update_dogyuun(screen_device &screen, bitmap_ind16 &bitmap, const rectangle &cliprect);
	UINT32 screen_update_batsugun(screen_device &screen, bitmap_ind16 &bitmap, const rectangle &cliprect);
	UINT32 screen_update_truxton2(screen_device &screen, bitmap_ind16 &bitmap, const rectangle &cliprect);
	UINT32 screen_update_bootleg(screen_device &screen, bitmap_ind16 &bitmap, const rectangle &cliprect);
	void screen_eof_toaplan2(screen_device &screen, bool state);
	INTERRUPT_GEN_MEMBER(toaplan2_vblank_irq1);
	INTERRUPT_GEN_MEMBER(toaplan2_vblank_irq2);
	INTERRUPT_GEN_MEMBER(toaplan2_vblank_irq4);
	INTERRUPT_GEN_MEMBER(bbakraid_snd_interrupt);
	void truxton2_postload();
	void create_tx_tilemap(int dx = 0, int dx_flipped = 0);
	void toaplan2_vblank_irq(int irq_line);
	
	DECLARE_READ8_MEMBER(ghox_sound_skip_r);
	//DECLARE_READ8_MEMBER(fixeight_sound_skip_r);
	DECLARE_READ8_MEMBER(v5_sound_skip_r);

	UINT8 m_pwrkick_hopper;
	DECLARE_CUSTOM_INPUT_MEMBER(pwrkick_hopper_status_r);
	DECLARE_WRITE8_MEMBER(pwrkick_coin_w);

	DECLARE_WRITE_LINE_MEMBER(toaplan2_reset);
protected:
	virtual void device_timer(emu_timer &timer, device_timer_id id, int param, void *ptr) override;
};
