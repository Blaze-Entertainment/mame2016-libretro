// license:BSD-3-Clause
// copyright-holders:David Haywood, Jonathan Gevaryahu
#ifndef MAME_TAITO_TAITOCCHIP_H
#define MAME_TAITO_TAITOCCHIP_H

#pragma once

#include "machine/bankdev.h"

//DECLARE_DEVICE_TYPE(TAITO_CCHIP, taito_cchip_device)
extern const device_type TAITO_CCHIP;

#define MCFG_CCHIP_IN_PORTA_CB(_devcb) \
	devcb = &taito_cchip_device::set_in_pa_callback(*device, DEVCB_##_devcb);
#define MCFG_CCHIP_IN_PORTB_CB(_devcb) \
	devcb = &taito_cchip_device::set_in_pb_callback(*device, DEVCB_##_devcb);
#define MCFG_CCHIP_IN_PORTC_CB(_devcb) \
	devcb = &taito_cchip_device::set_in_pc_callback(*device, DEVCB_##_devcb);
#define MCFG_CCHIP_IN_PORTD_CB(_devcb) \
	devcb = &taito_cchip_device::set_in_pd_callback(*device, DEVCB_##_devcb);


#define MCFG_CCHIP_OUT_PORTA_CB(_devcb) \
	devcb = &taito_cchip_device::set_out_pa_callback(*device, DEVCB_##_devcb);
#define MCFG_CCHIP_OUT_PORTB_CB(_devcb) \
	devcb = &taito_cchip_device::set_out_pb_callback(*device, DEVCB_##_devcb);
#define MCFG_CCHIP_OUT_PORTC_CB(_devcb) \
	devcb = &taito_cchip_device::set_out_pc_callback(*device, DEVCB_##_devcb);



class taito_cchip_device :  public device_t
{
public:
	// construction/destruction
	taito_cchip_device(const machine_config &mconfig, const char *tag, device_t *owner, UINT32 clock);

//	auto in_pa_callback()  { return m_in_pa_cb.bind(); }
//	auto in_pb_callback()  { return m_in_pb_cb.bind(); }
//	auto in_pc_callback()  { return m_in_pc_cb.bind(); }
//	auto in_ad_callback()  { return m_in_ad_cb.bind(); }
//	auto out_pa_callback() { return m_out_pa_cb.bind(); }
//	auto out_pb_callback() { return m_out_pb_cb.bind(); }
//	auto out_pc_callback() { return m_out_pc_cb.bind(); }

	template<class _Object> static devcb_base &set_in_pa_callback(device_t &device, _Object object) { return downcast<taito_cchip_device &>(device).m_in_pa_cb.set_callback(object); }
	template<class _Object> static devcb_base &set_in_pb_callback(device_t &device, _Object object) { return downcast<taito_cchip_device &>(device).m_in_pb_cb.set_callback(object); }
	template<class _Object> static devcb_base &set_in_pc_callback(device_t &device, _Object object) { return downcast<taito_cchip_device &>(device).m_in_pc_cb.set_callback(object); }
	template<class _Object> static devcb_base &set_in_pd_callback(device_t &device, _Object object) { return downcast<taito_cchip_device &>(device).m_in_ad_cb.set_callback(object); }

	template<class _Object> static devcb_base &set_out_pa_callback(device_t &device, _Object object) { return downcast<taito_cchip_device &>(device).m_out_pa_cb.set_callback(object); }
	template<class _Object> static devcb_base &set_out_pb_callback(device_t &device, _Object object) { return downcast<taito_cchip_device &>(device).m_out_pb_cb.set_callback(object); }
	template<class _Object> static devcb_base &set_out_pc_callback(device_t &device, _Object object) { return downcast<taito_cchip_device &>(device).m_out_pc_cb.set_callback(object); }


	// can be accessed externally
	DECLARE_READ8_MEMBER(asic_r);
	DECLARE_WRITE8_MEMBER(asic_w);
	DECLARE_WRITE8_MEMBER(asic68_w);

	DECLARE_READ8_MEMBER( mem_r ) { return m_sharedram[(m_upd4464_bank * 0x400) + (offset & 0x03ff)]; }
	DECLARE_WRITE8_MEMBER( mem_w ) { m_sharedram[(m_upd4464_bank * 0x400) + (offset & 0x03ff)] = data; }

	DECLARE_READ8_MEMBER( mem68_r ) { return m_sharedram[(m_upd4464_bank68 * 0x400) + (offset & 0x03ff)]; }
	DECLARE_WRITE8_MEMBER( mem68_w ){ m_sharedram[(m_upd4464_bank68 * 0x400) + (offset & 0x03ff)] = data; }

	void ext_interrupt(int state);

	DECLARE_READ_LINE_MEMBER(an0_r);
	DECLARE_READ_LINE_MEMBER(an1_r);
	DECLARE_READ_LINE_MEMBER(an2_r);
	DECLARE_READ_LINE_MEMBER(an3_r);
	DECLARE_READ_LINE_MEMBER(an4_r);
	DECLARE_READ_LINE_MEMBER(an5_r);
	DECLARE_READ_LINE_MEMBER(an6_r);
	DECLARE_READ_LINE_MEMBER(an7_r);


	DECLARE_READ8_MEMBER(pa_r);
	DECLARE_READ8_MEMBER(pb_r);
	DECLARE_READ8_MEMBER(pc_r);

	DECLARE_WRITE8_MEMBER(pa_w);
	DECLARE_WRITE8_MEMBER(pb_w);
	DECLARE_WRITE8_MEMBER(pc_w);
	DECLARE_WRITE8_MEMBER(pf_w);


protected:
	//void cchip_map(address_map &map) ATTR_COLD;

	virtual machine_config_constructor device_mconfig_additions() const override;
	virtual void device_start() override ATTR_COLD;
	//virtual const tiny_rom_entry *device_rom_region() const override ATTR_COLD;
	virtual const rom_entry *device_rom_region() const override;

private:
	UINT8 m_asic_ram[4];

	required_device<cpu_device> m_upd7810;
	//optional_memory_bank m_upd4464_bank;
	//optional_memory_bank m_upd4464_bank68;
	//memory_sharereator<UINT8> m_sharedram;
	int m_upd4464_bank;
	int m_upd4464_bank68;
	UINT8 m_sharedram[0x2000];

	devcb_read8        m_in_pa_cb;
	devcb_read8        m_in_pb_cb;
	devcb_read8        m_in_pc_cb;
	devcb_read8        m_in_ad_cb;
	devcb_write8       m_out_pa_cb;
	devcb_write8       m_out_pb_cb;
	devcb_write8       m_out_pc_cb;

};

#endif // MAME_TAITO_TAITOCCHIP_H
