// license:BSD-3-Clause
// copyright-holders:Bryan McPhail, Alex W. Jackson
/* ASG 971222 -- rewrote this interface */
#ifndef __SIMPLETOAPLAN_NEC_SIMPLETOAPLAN_V25_H_
#define __SIMPLETOAPLAN_NEC_SIMPLETOAPLAN_V25_H_


#define SIMPLETOAPLAN_NEC_INPUT_LINE_INTP0 10
#define SIMPLETOAPLAN_NEC_INPUT_LINE_INTP1 11
#define SIMPLETOAPLAN_NEC_INPUT_LINE_INTP2 12
#define SIMPLETOAPLAN_NEC_INPUT_LINE_POLL 20

#define SIMPLETOAPLAN_V25_PORT_P0 0x10000
#define SIMPLETOAPLAN_V25_PORT_P1 0x10002
#define SIMPLETOAPLAN_V25_PORT_P2 0x10004
#define SIMPLETOAPLAN_V25_PORT_PT 0x10006

enum
{
	SIMPLETOAPLAN_V25_PC=0,
	SIMPLETOAPLAN_V25_IP, SIMPLETOAPLAN_V25_AW, SIMPLETOAPLAN_V25_CW, SIMPLETOAPLAN_V25_DW, SIMPLETOAPLAN_V25_BW, SIMPLETOAPLAN_V25_SP, SIMPLETOAPLAN_V25_BP, SIMPLETOAPLAN_V25_IX, SIMPLETOAPLAN_V25_IY,
	SIMPLETOAPLAN_V25_FLAGS, SIMPLETOAPLAN_V25_ES, SIMPLETOAPLAN_V25_CS, SIMPLETOAPLAN_V25_SS, SIMPLETOAPLAN_V25_DS,
	SIMPLETOAPLAN_V25_PENDING
};


#define MCFG_SIMPLETOAPLAN_V25_CONFIG(_table) \
	simpletoaplan_v25_common_device::set_decryption_table(*device, _table);


class simpletoaplan_v25_common_device : public cpu_device
{
public:
	// construction/destruction
	simpletoaplan_v25_common_device(const machine_config &mconfig, device_type type, const char *name, const char *tag, device_t *owner, UINT32 clock, const char *shortname, bool is_16bit, offs_t fetch_xor, UINT8 prefetch_size, UINT8 prefetch_cycles, UINT32 chip_type);

	// static configuration helpers
	static void set_decryption_table(device_t &device, const UINT8 *decryption_table) { downcast<simpletoaplan_v25_common_device &>(device).m_v25simpletoaplan_v35_decryptiontable = decryption_table; }

	TIMER_CALLBACK_MEMBER(simpletoaplan_v25_timer_callback);
	
	UINT8* vvvv_shareram;
	int m_skipbase;
	int m_skiptop;
	int m_skipmask;
	int m_skip_address;
	void set_shareram(uint8_t* share, int skipbase, int skiptop, int skipmask, int skip_address);
	
	device_irq_acknowledge_delegate int_ack_callback;   /* Interrupt Acknowledge */

protected:
	// device-level overrides
	virtual void device_start() override;
	virtual void device_reset() override;
	virtual void device_post_load() override { notify_clock_changed(); }

	// device_execute_interface overrides
	virtual UINT64 execute_clocks_to_cycles(UINT64 clocks) const override { return clocks / m_PCK; }
	virtual UINT64 execute_cycles_to_clocks(UINT64 cycles) const override { return cycles * m_PCK; }
	virtual UINT32 execute_min_cycles() const override { return 1; }
	virtual UINT32 execute_max_cycles() const override { return 80; }
	virtual UINT32 execute_input_lines() const override { return 1; }
	virtual UINT32 execute_default_irq_vector() const override { return 0xff; }
	virtual void execute_run() override;
	virtual void execute_set_input(int inputnum, int state) override;

	// device_memory_interface overrides
	virtual const address_space_config *memory_space_config(address_spacenum spacenum = AS_0) const override { return (spacenum == AS_PROGRAM) ? &m_program_config : ( (spacenum == AS_IO) ? &m_io_config : nullptr); }

	// device_state_interface overrides
	virtual void state_string_export(const device_state_entry &entry, std::string &str) const override;
	virtual void state_import(const device_state_entry &entry) override;
	virtual void state_export(const device_state_entry &entry) override;

	// device_disasm_interface overrides
	virtual UINT32 disasm_min_opcode_bytes() const override { return 1; }
	virtual UINT32 disasm_max_opcode_bytes() const override { return 8; }
	virtual offs_t disasm_disassemble(char *buffer, offs_t pc, const UINT8 *oprom, const UINT8 *opram, UINT32 options) override;

private:
	address_space_config m_program_config;
	address_space_config m_io_config;

/* internal RAM and register banks */
union internalram
{
	UINT16 w[128];
	UINT8  b[256];
};

	internalram m_ram;
	offs_t  m_fetch_xor;

	UINT16  m_ip;

	/* PSW flags */
	INT32   m_SignVal;
	UINT32  m_AuxVal, m_OverVal, m_ZeroVal, m_CarryVal, m_ParityVal;  /* 0 or non-0 valued flags */
	UINT8   m_IBRK, m_F0, m_F1, m_TF, m_IF, m_DF, m_MF;   /* 0 or 1 valued flags */
	UINT8   m_RBW, m_RBB;   /* current register bank base, preshifted for word and byte registers */

	/* interrupt related */
	UINT32  m_pending_irq;
	UINT32  m_unmasked_irq;
	UINT32  m_bankswitch_irq;
	UINT8   m_priority_inttu, m_priority_intd, m_priority_intp, m_priority_ints0, m_priority_ints1;
	UINT8   m_IRQS, m_ISPR;
	UINT32  m_nmi_state;
	UINT32  m_irq_state;
	UINT32  m_poll_state;
	UINT32  m_mode_state;
	UINT32  m_intp_state[3];
	UINT8   m_no_interrupt;
	UINT8   m_halted;

	/* timer related */
	UINT16  m_TM0, m_MD0, m_TM1, m_MD1;
	UINT8   m_TMC0, m_TMC1;
	emu_timer *m_timers[4];

	/* system control */
	UINT8   m_RAMEN, m_TB, m_PCK; /* PRC register */
	UINT32  m_IDB;

	address_space *m_program;
	direct_read_data *m_direct;
	address_space *m_io;
	int     m_icount;

	UINT8   m_prefetch_size;
	UINT8   m_prefetch_cycles;
	INT8    m_prefetch_count;
	UINT8   m_prefetch_reset;
	UINT32  m_chip_type;

	UINT32  m_prefix_base;    /* base address of the latest prefix segment */
	UINT8   m_seg_prefix;     /* prefix segment indicator */

	UINT32 m_EA;
	UINT16 m_EO;
	UINT16 m_E16;

	UINT32 m_debugger_temp;

	const UINT8 *m_v25simpletoaplan_v35_decryptiontable;  // internal decryption table

	typedef void (simpletoaplan_v25_common_device::*simpletoaplan_nec_ophandler)();
	typedef UINT32 (simpletoaplan_v25_common_device::*simpletoaplan_nec_eahandler)();
	static const simpletoaplan_nec_ophandler s_simpletoaplan_nec_instruction[256];
	static const simpletoaplan_nec_eahandler s_GetEA[192];

	inline void prefetch();
	void do_prefetch(int previous_ICount);
	inline UINT8 fetch();
	inline UINT16 fetchword();
	inline UINT8 fetchop();
	void simpletoaplan_nec_interrupt(unsigned int_num, int /*INTSOURCES*/ source);
	void simpletoaplan_nec_bankswitch(unsigned bank_num);
	void simpletoaplan_nec_trap();
	void external_int();
	UINT8 read_irqcontrol(int /*INTSOURCES*/ source, UINT8 priority);
	UINT8 read_sfr(unsigned o);
	UINT16 read_sfr_word(unsigned o);
	void write_irqcontrol(int /*INTSOURCES*/ source, UINT8 d);
	void write_sfr(unsigned o, UINT8 d);
	void write_sfr_word(unsigned o, UINT16 d);

	inline UINT8 simpletoaplan_v25_read_byte(unsigned a)
	{
		if (vvvv_shareram && (a >= m_skipbase) && (a < m_skiptop))
		{
			return vvvv_shareram[a&m_skipmask];
		}


		if((a & 0xFFE00) == m_IDB || a == 0xFFFFF)
		{
			unsigned o = a & 0x1FF;

			if(m_RAMEN && o < 0x100)
				return m_ram.b[BYTE_XOR_LE(o)];

			if(o >= 0x100)
				return read_sfr(o-0x100);
		}

		return m_program->read_byte(a);
	}

	inline UINT16 simpletoaplan_v25_read_word(unsigned a)
	{
		if( a & 1 )
			return (simpletoaplan_v25_read_byte(a) | (simpletoaplan_v25_read_byte(a + 1) << 8));

		if((a & 0xFFE00) == m_IDB)
		{
			unsigned o = a & 0x1FF;

			if(m_RAMEN && o < 0x100)
				return m_ram.w[o/2];

			if(o >= 0x100)
				return read_sfr_word(o-0x100);
		}

		if(a == 0xFFFFE)    /* not sure about this - manual says FFFFC-FFFFE are "reserved" */
			return (m_program->read_byte(a) | (read_sfr(0xFF) << 8));

		return m_program->read_word(a);
	}

	inline void simpletoaplan_v25_write_byte(unsigned a, UINT8 d)
	{
		if (vvvv_shareram && (a >= m_skipbase) && (a < m_skiptop))
		{
			vvvv_shareram[a & m_skipmask] = d;
			return;
		}

		if((a & 0xFFE00) == m_IDB || a == 0xFFFFF)
		{
			unsigned o = a & 0x1FF;

			if(m_RAMEN && o < 0x100)
			{
				m_ram.b[BYTE_XOR_LE(o)] = d;
				return;
			}

			if(o >= 0x100)
			{
				write_sfr(o-0x100, d);
				return;
			}
		}

		m_program->write_byte(a, d);
	}

	inline void simpletoaplan_v25_write_word(unsigned a, UINT16 d)
	{
		if( a & 1 )
		{
			simpletoaplan_v25_write_byte(a, d);
			simpletoaplan_v25_write_byte(a + 1, d >> 8);
			return;
		}

		if((a & 0xFFE00) == m_IDB)
		{
			unsigned o = a & 0x1FF;

			if(m_RAMEN && o < 0x100)
			{
				m_ram.w[o/2] = d;
				return;
			}

			if(o >= 0x100)
			{
				write_sfr_word(o-0x100, d);
				return;
			}
		}

		if(a == 0xFFFFE)    /* not sure about this - manual says FFFFC-FFFFE are "reserved" */
		{
			m_program->write_byte(a, d);
			write_sfr(0xFF, d >> 8);
			return;
		}

		m_program->write_word(a, d);
	}


	void i_add_br8();
	void i_add_wr16();
	void i_add_r8b();
	void i_add_r16w();
	void i_add_ald8();
	void i_add_axd16();
	void i_push_es();
	void i_pop_es();
	void i_or_br8();
	void i_or_r8b();
	void i_or_wr16();
	void i_or_r16w();
	void i_or_ald8();
	void i_or_axd16();
	void i_push_cs();
	void i_pre_nec();
	void i_pre_v25();
	void i_adc_br8();
	void i_adc_wr16();
	void i_adc_r8b();
	void i_adc_r16w();
	void i_adc_ald8();
	void i_adc_axd16();
	void i_push_ss();
	void i_pop_ss();
	void i_sbb_br8();
	void i_sbb_wr16();
	void i_sbb_r8b();
	void i_sbb_r16w();
	void i_sbb_ald8();
	void i_sbb_axd16();
	void i_push_ds();
	void i_pop_ds();
	void i_and_br8();
	void i_and_r8b();
	void i_and_wr16();
	void i_and_r16w();
	void i_and_ald8();
	void i_and_axd16();
	void i_es();
	void i_daa();
	void i_sub_br8();
	void i_sub_wr16();
	void i_sub_r8b();
	void i_sub_r16w();
	void i_sub_ald8();
	void i_sub_axd16();
	void i_cs();
	void i_das();
	void i_xor_br8();
	void i_xor_r8b();
	void i_xor_wr16();
	void i_xor_r16w();
	void i_xor_ald8();
	void i_xor_axd16();
	void i_ss();
	void i_aaa();
	void i_cmp_br8();
	void i_cmp_wr16();
	void i_cmp_r8b();
	void i_cmp_r16w();
	void i_cmp_ald8();
	void i_cmp_axd16();
	void i_ds();
	void i_aas();
	void i_inc_ax();
	void i_inc_cx();
	void i_inc_dx();
	void i_inc_bx();
	void i_inc_sp();
	void i_inc_bp();
	void i_inc_si();
	void i_inc_di();
	void i_dec_ax();
	void i_dec_cx();
	void i_dec_dx();
	void i_dec_bx();
	void i_dec_sp();
	void i_dec_bp();
	void i_dec_si();
	void i_dec_di();
	void i_push_ax();
	void i_push_cx();
	void i_push_dx();
	void i_push_bx();
	void i_push_sp();
	void i_push_bp();
	void i_push_si();
	void i_push_di();
	void i_pop_ax();
	void i_pop_cx();
	void i_pop_dx();
	void i_pop_bx();
	void i_pop_sp();
	void i_pop_bp();
	void i_pop_si();
	void i_pop_di();
	void i_pusha();
	void i_popa();
	void i_chkind();
	void i_repnc();
	void i_repc();
	void i_push_d16();
	void i_imul_d16();
	void i_push_d8();
	void i_imul_d8();
	void i_insb();
	void i_insw();
	void i_outsb();
	void i_outsw();
	void i_jo();
	void i_jno();
	void i_jc();
	void i_jnc();
	void i_jz();
	void i_jnz();
	void i_jce();
	void i_jnce();
	void i_js();
	void i_jns();
	void i_jp();
	void i_jnp();
	void i_jl();
	void i_jnl();
	void i_jle();
	void i_jnle();
	void i_80pre();
	void i_82pre();
	void i_81pre();
	void i_83pre();
	void i_test_br8();
	void i_test_wr16();
	void i_xchg_br8();
	void i_xchg_wr16();
	void i_mov_br8();
	void i_mov_r8b();
	void i_mov_wr16();
	void i_mov_r16w();
	void i_mov_wsreg();
	void i_lea();
	void i_mov_sregw();
	void i_invalid();
	void i_popw();
	void i_nop();
	void i_xchg_axcx();
	void i_xchg_axdx();
	void i_xchg_axbx();
	void i_xchg_axsp();
	void i_xchg_axbp();
	void i_xchg_axsi();
	void i_xchg_axdi();
	void i_cbw();
	void i_cwd();
	void i_call_far();
	void i_pushf();
	void i_popf();
	void i_sahf();
	void i_lahf();
	void i_mov_aldisp();
	void i_mov_axdisp();
	void i_mov_dispal();
	void i_mov_dispax();
	void i_movsb();
	void i_movsw();
	void i_cmpsb();
	void i_cmpsw();
	void i_test_ald8();
	void i_test_axd16();
	void i_stosb();
	void i_stosw();
	void i_lodsb();
	void i_lodsw();
	void i_scasb();
	void i_scasw();
	void i_mov_ald8();
	void i_mov_cld8();
	void i_mov_dld8();
	void i_mov_bld8();
	void i_mov_ahd8();
	void i_mov_chd8();
	void i_mov_dhd8();
	void i_mov_bhd8();
	void i_mov_axd16();
	void i_mov_cxd16();
	void i_mov_dxd16();
	void i_mov_bxd16();
	void i_mov_spd16();
	void i_mov_bpd16();
	void i_mov_sid16();
	void i_mov_did16();
	void i_rotshft_bd8();
	void i_rotshft_wd8();
	void i_ret_d16();
	void i_ret();
	void i_les_dw();
	void i_lds_dw();
	void i_mov_bd8();
	void i_mov_wd16();
	void i_enter();
	void i_leave();
	void i_retf_d16();
	void i_retf();
	void i_int3();
	void i_int();
	void i_into();
	void i_iret();
	void i_rotshft_b();
	void i_rotshft_w();
	void i_rotshft_bcl();
	void i_rotshft_wcl();
	void i_aam();
	void i_aad();
	void i_setalc();
	void i_trans();
	void i_fpo();
	void i_loopne();
	void i_loope();
	void i_loop();
	void i_jcxz();
	void i_inal();
	void i_inax();
	void i_outal();
	void i_outax();
	void i_call_d16();
	void i_jmp_d16();
	void i_jmp_far();
	void i_jmp_d8();
	void i_inaldx();
	void i_inaxdx();
	void i_outdxal();
	void i_outdxax();
	void i_lock();
	void i_repne();
	void i_repe();
	void i_hlt();
	void i_cmc();
	void i_f6pre();
	void i_f7pre();
	void i_clc();
	void i_stc();
	void i_di();
	void i_ei();
	void i_cld();
	void i_std();
	void i_fepre();
	void i_ffpre();
	void i_wait();
	void i_brkn();
	void i_brks();

	UINT32 EA_000();
	UINT32 EA_001();
	UINT32 EA_002();
	UINT32 EA_003();
	UINT32 EA_004();
	UINT32 EA_005();
	UINT32 EA_006();
	UINT32 EA_007();
	UINT32 EA_100();
	UINT32 EA_101();
	UINT32 EA_102();
	UINT32 EA_103();
	UINT32 EA_104();
	UINT32 EA_105();
	UINT32 EA_106();
	UINT32 EA_107();
	UINT32 EA_200();
	UINT32 EA_201();
	UINT32 EA_202();
	UINT32 EA_203();
	UINT32 EA_204();
	UINT32 EA_205();
	UINT32 EA_206();
	UINT32 EA_207();
};


class simpletoaplan_v25_device : public simpletoaplan_v25_common_device
{
public:
	simpletoaplan_v25_device(const machine_config &mconfig, const char *tag, device_t *owner, UINT32 clock);
};


class simpletoaplan_v35_device : public simpletoaplan_v25_common_device
{
public:
	simpletoaplan_v35_device(const machine_config &mconfig, const char *tag, device_t *owner, UINT32 clock);
};


extern const device_type SIMPLETOAPLAN_V25;
extern const device_type SIMPLETOAPLAN_V35;


#endif
