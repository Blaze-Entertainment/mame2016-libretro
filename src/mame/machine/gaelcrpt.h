// license:BSD-3-Clause
// copyright-holders:David Haywood

#pragma once

#ifndef __GAELCO_VRAM_CRYPT__
#define __GAELCO_VRAM_CRYPT__

extern const device_type GAELCO_VRAM_CRYPT;

#define MCFG_GAELCO_VRAM_CRYPT_ADD(_tag) \
	MCFG_DEVICE_ADD(_tag, GAELCO_VRAM_CRYPT, 0)


class gaelco_crypt_device :  public device_t
{
public:
	// construction/destruction
	gaelco_crypt_device(const machine_config &mconfig, const char *tag, device_t *owner, UINT32 clock);

	int decrypt(int const param1, int const param2, int const enc_prev_word, int const dec_prev_word, int const enc_word);
	UINT16 gaelco_decrypt(address_space& space, int offset, int data, int param1, int param2);

protected:
	virtual void device_start() override;
	virtual void device_reset() override;

private:
	int lastpc, lastoffset, lastencword, lastdecword;

};

#endif
