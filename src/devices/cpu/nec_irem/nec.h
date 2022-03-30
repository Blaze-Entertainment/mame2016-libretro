// license:BSD-3-Clause
// copyright-holders:Bryan McPhail
/* ASG 971222 -- rewrote this interface */
#ifndef __NECIREM_H_
#define __NECIREM_H_


#define NEC_IREM_INPUT_LINE_INTP0 10
#define NEC_IREM_INPUT_LINE_INTP1 11
#define NEC_IREM_INPUT_LINE_INTP2 12
#define NEC_IREM_INPUT_LINE_POLL 20

enum
{
	NEC_IREM_PC=0,
	NEC_IREM_IP, NEC_IREM_AW, NEC_IREM_CW, NEC_IREM_DW, NEC_IREM_BW, NEC_IREM_SP, NEC_IREM_BP, NEC_IREM_IX, NEC_IREM_IY,
	NEC_IREM_FLAGS, NEC_IREM_ES, NEC_IREM_CS, NEC_IREM_SS, NEC_IREM_DS,
	NEC_IREM_PENDING
};

#ifndef FALSE
#define FALSE 0
#define TRUE 1
#endif

#define FETCH()             fetch()
#define FETCHWORD()         fetchword()

/* interrupt vectors */
enum
{
	NEC_IREM_DIVIDE_VECTOR   = 0,
	NEC_IREM_TRAP_VECTOR     = 1,
	NEC_IREM_NMI_VECTOR      = 2,
	NEC_IREM_BRKV_VECTOR     = 4,
	NEC_IREM_CHKIND_VECTOR   = 5
};

/* interrupt sources */
enum INTSOURCES
{
	BRK = 0,
	INT_IRQ = 1,
	NMI_IRQ = 2
};


enum SREGS { DS1=0, PS, SS, DS0 };
enum WREGS { AW=0, CW, DW, BW, SP, BP, IX, IY };
enum BREGS {
	AL = NATIVE_ENDIAN_VALUE_LE_BE(0x0, 0x1),
	AH = NATIVE_ENDIAN_VALUE_LE_BE(0x1, 0x0),
	CL = NATIVE_ENDIAN_VALUE_LE_BE(0x2, 0x3),
	CH = NATIVE_ENDIAN_VALUE_LE_BE(0x3, 0x2),
	DL = NATIVE_ENDIAN_VALUE_LE_BE(0x4, 0x5),
	DH = NATIVE_ENDIAN_VALUE_LE_BE(0x5, 0x4),
	BL = NATIVE_ENDIAN_VALUE_LE_BE(0x6, 0x7),
	BH = NATIVE_ENDIAN_VALUE_LE_BE(0x7, 0x6)
};



#define Sreg(x)         m_sregs[x]
#define Wreg(x)         m_regs.w[x]
#define Breg(x)         m_regs.b[x]

#define PC()       ((Sreg(PS)<<4)+m_ip)

#define CF      (m_CarryVal!=0)
#define SF      (m_SignVal<0)
#define ZF      (m_ZeroVal==0)
#define PF      parity_table[(BYTE)m_ParityVal]
#define AF      (m_AuxVal!=0)
#define OF      (m_OverVal!=0)

#define DefaultBase(Seg) ((m_seg_prefix && (Seg==DS0 || Seg==SS)) ? m_prefix_base : Sreg(Seg) << 4)


class nec_common_irem_device : public cpu_device
{
public:
	// construction/destruction
	nec_common_irem_device(const machine_config &mconfig, device_type type, const char *name, const char *tag, device_t *owner, UINT32 clock, const char *shortname, const char *source, bool is_16bit, offs_t fetch_xor, UINT8 prefetch_size, UINT8 prefetch_cycles, UINT32 chip_type);

	void set_rom_ptr(UINT8* ROM) { m_fastrom = ROM; }
	void set_ram_ptr(UINT8* RAM) { m_fastram = RAM; }

protected:
	// device-level overrides
	virtual void device_start() override;
	virtual void device_reset() override;

	// device_execute_interface overrides
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
	UINT8* m_fastrom;
	UINT8* m_fastram;
	void do_opcode(const UINT8 opcode);

	address_space_config m_program_config;
	address_space_config m_io_config;

/* NEC registers */
union necbasicregs
{                   /* eight general registers */
	UINT16 w[8];    /* viewed as 16 bits registers */
	UINT8  b[16];   /* or as 8 bit registers */
};

	necbasicregs m_regs;
	offs_t  m_fetch_xor;
	UINT16  m_sregs[4];

	UINT16  m_ip;

	/* PSW flags */
	INT32   m_SignVal;
	UINT32  m_AuxVal;   /* 0 or non-0 valued flags */
	UINT32  m_OverVal;
	UINT32  m_ZeroVal;
	UINT32  m_CarryVal;
	UINT32  m_ParityVal;
	UINT8   m_TF; /* 0 or 1 valued flags */
	UINT8   m_IF;
	UINT8   m_DF;
	UINT8   m_MF;

	/* interrupt related */
	UINT32  m_pending_irq;
	UINT32  m_nmi_state;
	UINT32  m_irq_state;
	UINT32  m_poll_state;
	UINT8   m_no_interrupt;
	UINT8   m_halted;

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

	//typedef void (nec_common_irem_device::*nec_ophandler)();
	//typedef UINT32 (nec_common_irem_device::*nec_eahandler)();
	//static const nec_ophandler s_nec_instruction[256];

	inline void prefetch()
	{
		m_prefetch_count--;
	}

	void do_prefetch(int previous_ICount);


	inline UINT8 fetch()
	{
		prefetch();
		const UINT32 addr = (Sreg(PS) << 4) + m_ip++;

		if (m_fastrom)
		{
			if (addr < 0xc0000)
				return m_fastrom[addr];
		}

		return m_direct->read_byte(addr, 0);
	}

	inline UINT16 fetchword()
	{
		UINT16 r = FETCH();
		r |= (FETCH()<<8);
		return r;
	}

	inline UINT8 fetchop()
	{
		prefetch();
		const UINT32 addr = (Sreg(PS) << 4) + m_ip++;

		if (m_fastrom)
		{
			if (addr < 0xc0000)
				return m_fastrom[addr];
		}

		return m_direct->read_byte(addr, 0);
	}




	void nec_interrupt(unsigned int_num, int source);
	void nec_trap();
	void external_int();




	inline UINT32 GET_EA(UINT8 in)
	{
		switch (in)
		{
		case 0x000:	case 0x008:	case 0x010:	case 0x018:	case 0x020:	case 0x028:	case 0x030:	case 0x038: { m_EO = Wreg(BW) + Wreg(IX); m_EA = DefaultBase(DS0) + m_EO; return m_EA; }; break;
		case 0x001: case 0x009:	case 0x011:	case 0x019:	case 0x021:	case 0x029:	case 0x031:	case 0x039:	{ m_EO = Wreg(BW) + Wreg(IY); m_EA = DefaultBase(DS0) + m_EO; return m_EA; }; break;
		case 0x002:	case 0x00a:	case 0x012:	case 0x01a:	case 0x022:	case 0x02a:	case 0x032:	case 0x03a:	{ m_EO = Wreg(BP) + Wreg(IX); m_EA = DefaultBase(SS) + m_EO; return m_EA; }; break;
		case 0x003:	case 0x00b:	case 0x013:	case 0x01b:	case 0x023:	case 0x02b:	case 0x033:	case 0x03b:	{ m_EO = Wreg(BP) + Wreg(IY); m_EA = DefaultBase(SS) + m_EO; return m_EA; }; break;
		case 0x004:	case 0x00c:	case 0x014:	case 0x01c:	case 0x024:	case 0x02c:	case 0x034:	case 0x03c:	{ m_EO = Wreg(IX); m_EA = DefaultBase(DS0) + m_EO; return m_EA; }; break;
		case 0x005:	case 0x00d:	case 0x015: case 0x01d:	case 0x025:	case 0x02d:	case 0x035:	case 0x03d:	{ m_EO = Wreg(IY); m_EA = DefaultBase(DS0) + m_EO; return m_EA; }; break;
		case 0x006:	case 0x00e:	case 0x016:	case 0x01e:	case 0x026:	case 0x02e:	case 0x036:	case 0x03e:	{ m_EO = FETCH(); m_EO += FETCH() << 8; m_EA = DefaultBase(DS0) + m_EO; return m_EA; }; break;
		case 0x007:	case 0x00f:	case 0x017:	case 0x01f:	case 0x027:	case 0x02f:	case 0x037:	case 0x03f:	{ m_EO = Wreg(BW); m_EA = DefaultBase(DS0) + m_EO; return m_EA; }; break;
		case 0x040:	case 0x048:	case 0x050:	case 0x058:	case 0x060:	case 0x068:	case 0x070:	case 0x078:	{ m_EO = (Wreg(BW) + Wreg(IX) + (INT8)FETCH()); m_EA = DefaultBase(DS0) + m_EO; return m_EA; }; break;
		case 0x041:	case 0x049:	case 0x051:	case 0x059:	case 0x061:	case 0x069:	case 0x071:	case 0x079:	{ m_EO = (Wreg(BW) + Wreg(IY) + (INT8)FETCH()); m_EA = DefaultBase(DS0) + m_EO; return m_EA; }; break;
		case 0x042:	case 0x04a:	case 0x052:	case 0x05a:	case 0x062:	case 0x06a:	case 0x072:	case 0x07a:	{ m_EO = (Wreg(BP) + Wreg(IX) + (INT8)FETCH()); m_EA = DefaultBase(SS) + m_EO; return m_EA; }; break;
		case 0x043:	case 0x04b:	case 0x053:	case 0x05b:	case 0x063:	case 0x06b:	case 0x073:	case 0x07b:	{ m_EO = (Wreg(BP) + Wreg(IY) + (INT8)FETCH()); m_EA = DefaultBase(SS) + m_EO; return m_EA; }; break;
		case 0x044:	case 0x04c:	case 0x054:	case 0x05c:	case 0x064:	case 0x06c:	case 0x074:	case 0x07c:	{ m_EO = (Wreg(IX) + (INT8)FETCH()); m_EA = DefaultBase(DS0) + m_EO; return m_EA; }; break;
		case 0x045:	case 0x04d:	case 0x055:	case 0x05d:	case 0x065:	case 0x06d:	case 0x075:	case 0x07d:	{ m_EO = (Wreg(IY) + (INT8)FETCH()); m_EA = DefaultBase(DS0) + m_EO; return m_EA; }; break;
		case 0x046:	case 0x04e:	case 0x056:	case 0x05e:	case 0x066:	case 0x06e:	case 0x076:	case 0x07e:	{ m_EO = (Wreg(BP) + (INT8)FETCH()); m_EA = DefaultBase(SS) + m_EO; return m_EA; }; break;
		case 0x047:	case 0x04f:	case 0x057: case 0x05f:	case 0x067:	case 0x06f:	case 0x077:	case 0x07f:	{ m_EO = (Wreg(BW) + (INT8)FETCH()); m_EA = DefaultBase(DS0) + m_EO; return m_EA; }; break;
		case 0x080:	case 0x088:	case 0x090:	case 0x098:	case 0x0a0:	case 0x0a8:	case 0x0b0:	case 0x0b8:	{ m_E16 = FETCH(); m_E16 += FETCH() << 8; m_EO = Wreg(BW) + Wreg(IX) + (INT16)m_E16; m_EA = DefaultBase(DS0) + m_EO; return m_EA; }; break;
		case 0x081:	case 0x089:	case 0x091:	case 0x099:	case 0x0a1:	case 0x0a9:	case 0x0b1:	case 0x0b9:	{ m_E16 = FETCH(); m_E16 += FETCH() << 8; m_EO = Wreg(BW) + Wreg(IY) + (INT16)m_E16; m_EA = DefaultBase(DS0) + m_EO; return m_EA; }; break;
		case 0x082:	case 0x08a:	case 0x092:	case 0x09a:	case 0x0a2:	case 0x0aa:	case 0x0b2:	case 0x0ba:	{ m_E16 = FETCH(); m_E16 += FETCH() << 8; m_EO = Wreg(BP) + Wreg(IX) + (INT16)m_E16; m_EA = DefaultBase(SS) + m_EO; return m_EA; }; break;
		case 0x083:	case 0x08b:	case 0x093:	case 0x09b:	case 0x0a3:	case 0x0ab:	case 0x0b3:	case 0x0bb:	{ m_E16 = FETCH(); m_E16 += FETCH() << 8; m_EO = Wreg(BP) + Wreg(IY) + (INT16)m_E16; m_EA = DefaultBase(SS) + m_EO; return m_EA; }; break;
		case 0x084:	case 0x08c:	case 0x094:	case 0x09c:	case 0x0a4:	case 0x0ac:	case 0x0b4:	case 0x0bc:	{ m_E16 = FETCH(); m_E16 += FETCH() << 8; m_EO = Wreg(IX) + (INT16)m_E16; m_EA = DefaultBase(DS0) + m_EO; return m_EA; }; break;
		case 0x085:	case 0x08d:	case 0x095:	case 0x09d: case 0x0a5:	case 0x0ad:	case 0x0b5:	case 0x0bd: { m_E16 = FETCH(); m_E16 += FETCH() << 8; m_EO = Wreg(IY) + (INT16)m_E16; m_EA = DefaultBase(DS0) + m_EO; return m_EA; }; break;
		case 0x086: case 0x08e:	case 0x096:	case 0x09e:	case 0x0a6:	case 0x0ae:	case 0x0b6:	case 0x0be:	{ m_E16 = FETCH(); m_E16 += FETCH() << 8; m_EO = Wreg(BP) + (INT16)m_E16; m_EA = DefaultBase(SS) + m_EO; return m_EA; }; break;
		case 0x087:	case 0x08f:	case 0x097:	case 0x09f:	case 0x0a7:	case 0x0af:	case 0x0b7:	case 0x0bf:	{ m_E16 = FETCH(); m_E16 += FETCH() << 8; m_EO = Wreg(BW) + (INT16)m_E16; m_EA = DefaultBase(DS0) + m_EO; return m_EA; }; break;
		}
		return 0x00;
	}

};


class v20_irem_device : public nec_common_irem_device
{
public:
	v20_irem_device(const machine_config &mconfig, const char *tag, device_t *owner, UINT32 clock);
};


class v30_irem_device : public nec_common_irem_device
{
public:
	v30_irem_device(const machine_config &mconfig, const char *tag, device_t *owner, UINT32 clock);
};


class v33_irem_device : public nec_common_irem_device
{
public:
	v33_irem_device(const machine_config &mconfig, const char *tag, device_t *owner, UINT32 clock);
};

class v33a_irem_device : public nec_common_irem_device
{
public:
	v33a_irem_device(const machine_config &mconfig, const char *tag, device_t *owner, UINT32 clock);
};


extern const device_type V20IREM;
extern const device_type V30IREM;
extern const device_type V33IREM;
extern const device_type V33AIREM;



#endif
