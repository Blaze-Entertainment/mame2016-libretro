// license:BSD-3-Clause
// copyright-holders:Bryan McPhail
/****************************************************************************

    NEC V20IREM/V30IREM/V33IREM emulator

    ---------------------------------------------

    V20IREM = uPD70108 = 8-bit data bus @ 5MHz or 8MHz
    V20IREMHL = uPD70108H = V20IREM with EMS support (24-bit address bus)

    V25IREM = uPD70320 = V20IREM with on-chip features:
            - 256 bytes on-chip RAM
            - 8 register banks
            - 4-bit input port
            - 20-bit I/O port
            - 2 channel serial interface
            - interrupt controller
            - 2 channel DMA controller
            - 2 channel 16-bit timer
            - new instructions: BTCLR, RETRBI, STOP, BRKCS, TSKSW,
                                MOVSPA, MOVSPB

    V25IREM+ = uPD70325 = V25IREM @ 8MHz or 10MHz plus changes:
            - faster DMA
            - improved serial interface

    ---------------------------------------------

    V30IREM = uPD70116 = 16-bit data bus version of V20IREM
    V30IREMHL = uPD70116H = 16-bit data bus version of V20IREMHL
    V30IREMMX = V30IREMHL with separate address and data busses

    V35IREM = uPD70330 = 16-bit data bus version of V25IREM

    V35IREM+ = uPD70335 = 16-bit data bus version of V25IREM+

    ---------------------------------------------

    V40 = uPD70208 = 8-bit data bus @ 10MHz
    V40HL = uPD70208H = V40 with support up to 20Mhz

    ---------------------------------------------

    V50 = uPD70216 = 16-bit data bus version of V40
    V50HL = uPD70216H = 16-bit data bus version of V40HL

    ---------------------------------------------

    V41 = uPD70270

    V51 = uPD70280



    V33AIREM = uPD70136A (interrupt vector #s compatible with x86)
    V53IREMIREMA = uPD70236A



    Instruction differences:
        V20IREM, V30IREM, V40, V50 have dedicated emulation instructions
            (BRKEM, RETEM, CALLN)

        V33IREM / V33AIREM has dedicated address mode instructions (V53IREMIREM / V53IREMIREMA are based on those cores with extra peripherals)
            (BRKXA, RETXA)



    (Re)Written June-September 2000 by Bryan McPhail (mish@tendril.co.uk) based
    on code by Oliver Bergmann (Raul_Bloodworth@hotmail.com) who based code
    on the i286 emulator by Fabrice Frances which had initial work based on
    David Hedley's pcemu(!).

    This new core features 99% accurate cycle counts for each processor,
    there are still some complex situations where cycle counts are wrong,
    typically where a few instructions have differing counts for odd/even
    source and odd/even destination memory operands.

    Flag settings are also correct for the NEC processors rather than the
    I86 versions.

    Changelist:

    22/02/2003:
        Removed cycle counts from memory accesses - they are certainly wrong,
        and there is already a memory access cycle penalty in the opcodes
        using them.

        Fixed save states.

        Fixed ADJBA/ADJBS/ADJ4A/ADJ4S flags/return values for all situations.
        (Fixes bugs in Geostorm and Thunderblaster)

        Fixed carry flag on NEG (I thought this had been fixed circa Mame 0.58,
        but it seems I never actually submitted the fix).

        Fixed many cycle counts in instructions and bug in cycle count
        macros (odd word cases were testing for odd instruction word address
        not data address).

    Todo!
        Double check cycle timing is 100%.

****************************************************************************/

#include "emu.h"
#include "debugger.h"

typedef UINT8 BOOLEAN;
typedef UINT8 BYTE;
typedef UINT16 WORD;
typedef UINT32 DWORD;

#include "nec.h"
#include "necpriv.h"

const device_type V20IREM = &device_creator<v20_irem_device>;
const device_type V30IREM = &device_creator<v30_irem_device>;
const device_type V33IREM = &device_creator<v33_irem_device>;
const device_type V33AIREM =&device_creator<v33a_irem_device>;



nec_common_irem_device::nec_common_irem_device(const machine_config &mconfig, device_type type, const char *name, const char *tag, device_t *owner, UINT32 clock, const char *shortname, const char *source, bool is_16bit, offs_t fetch_xor, UINT8 prefetch_size, UINT8 prefetch_cycles, UINT32 chip_type)
	: cpu_device(mconfig, type, name, tag, owner, clock, shortname, source)
	, m_fastrom(nullptr)
	//, m_fastram(nullptr)
	, m_program_config("program", ENDIANNESS_LITTLE, is_16bit ? 16 : 8, 20, 0)
	, m_io_config("io", ENDIANNESS_LITTLE, is_16bit ? 16 : 8, 16, 0)
	, m_fetch_xor(0)
	, m_prefetch_size(prefetch_size)
	, m_prefetch_cycles(prefetch_cycles)
	, m_chip_type(0)
{
}


v20_irem_device::v20_irem_device(const machine_config &mconfig, const char *tag, device_t *owner, UINT32 clock)
	: nec_common_irem_device(mconfig, V20IREM, "V20IREM", tag, owner, clock, "v20", __FILE__, false, 0, 4, 4, V20IREM_TYPE)
{
}


v30_irem_device::v30_irem_device(const machine_config &mconfig, const char *tag, device_t *owner, UINT32 clock)
	: nec_common_irem_device(mconfig, V30IREM, "V30IREM", tag, owner, clock, "v30", __FILE__, true, BYTE_XOR_LE(0), 6, 2, V30IREM_TYPE)
{
}


/* FIXME: Need information about prefetch size and cycles for V33IREM.
 * complete guess below, nbbatman will not work
 * properly without. */
v33_irem_device::v33_irem_device(const machine_config &mconfig, const char *tag, device_t *owner, UINT32 clock)
	: nec_common_irem_device(mconfig, V33IREM, "V33IREM", tag, owner, clock, "v33", __FILE__, true, BYTE_XOR_LE(0), 6, 1, V33IREM_TYPE)
{
}


v33a_irem_device::v33a_irem_device(const machine_config &mconfig, const char *tag, device_t *owner, UINT32 clock)
	: nec_common_irem_device(mconfig, V33AIREM, "V33AIREM", tag, owner, clock, "v33A", __FILE__, true, BYTE_XOR_LE(0), 6, 1, V33IREM_TYPE)
{
}


offs_t nec_common_irem_device::disasm_disassemble(char *buffer, offs_t pc, const UINT8 *oprom, const UINT8 *opram, UINT32 options)
{
	extern CPU_DISASSEMBLE( nec_irem );
	return CPU_DISASSEMBLE_NAME(nec_irem)(this, buffer, pc, oprom, opram, options);
}



void nec_common_irem_device::do_prefetch(int previous_ICount)
{
	int diff = previous_ICount - (int) m_icount;

	/* The implementation is not accurate, but comes close.
	 * It does not respect that the V30IREM will fetch two bytes
	 * at once directly, but instead uses only 2 cycles instead
	 * of 4. There are however only very few sources publicly
	 * available and they are vague.
	 */
	while (m_prefetch_count<0)
	{
		m_prefetch_count++;
		if (diff>m_prefetch_cycles)
			diff -= m_prefetch_cycles;
		else
			m_icount -= m_prefetch_cycles;
	}

	if (m_prefetch_reset)
	{
		m_prefetch_count = 0;
		m_prefetch_reset = 0;
		return;
	}

	while (diff>=m_prefetch_cycles && m_prefetch_count < m_prefetch_size)
	{
		diff -= m_prefetch_cycles;
		m_prefetch_count++;
	}

}


#include "necinstr.h"
#include "necmacro.h"
#include "necea.h"
#include "necmodrm.h"

static UINT8 parity_table[256];



/***************************************************************************/

void nec_common_irem_device::device_reset()
{
	memset( &m_regs.w, 0, sizeof(m_regs.w));

	m_ip = 0;
	m_TF = 0;
	m_IF = 0;
	m_DF = 0;
	m_MF = 1;  // brkem should set to 0 when implemented
	m_SignVal = 0;
	m_AuxVal = 0;
	m_OverVal = 0;
	m_ZeroVal = 1;
	m_CarryVal = 0;
	m_ParityVal = 1;
	m_pending_irq = 0;
	m_nmi_state = 0;
	m_irq_state = 0;
	m_poll_state = 1;
	m_halted = 0;

	Sreg(PS) = 0xffff;
	Sreg(SS) = 0;
	Sreg(DS0) = 0;
	Sreg(DS1) = 0;

	CHANGE_PC;
}


void nec_common_irem_device::nec_interrupt(unsigned int_num, int/*INTSOURCES*/ source)
{
	UINT32 dest_seg, dest_off;

	{ UINT16 tmp = CompressFlags(); PUSH(tmp); CLKS(12, 8, 3); };
	m_TF = m_IF = 0;

	if (source == INT_IRQ)  /* get vector */
		int_num = (standard_irq_callback)(0);

	dest_off = read_mem_word(int_num*4);
	dest_seg = read_mem_word(int_num*4+2);

	PUSH(Sreg(PS));
	PUSH(m_ip);
	m_ip = (WORD)dest_off;
	Sreg(PS) = (WORD)dest_seg;
	CHANGE_PC;
}

void nec_common_irem_device::nec_trap()
{
	do_opcode(fetchop());
	nec_interrupt(NEC_IREM_TRAP_VECTOR, BRK);
}

void nec_common_irem_device::external_int()
{
	if (m_pending_irq & NMI_IRQ)
	{
		nec_interrupt(NEC_IREM_NMI_VECTOR, NMI_IRQ);
		m_pending_irq &= ~NMI_IRQ;
	}
	else if (m_pending_irq)
	{
		/* the actual vector is retrieved after pushing flags */
		/* and clearing the IF */
		nec_interrupt((UINT32)-1, INT_IRQ);
		m_irq_state = CLEAR_LINE;
		m_pending_irq &= ~INT_IRQ;
	}
}

/****************************************************************************/
/*                             OPCODES                                      */
/****************************************************************************/

#include "necinstr.hxx"

/*****************************************************************************/

void nec_common_irem_device::execute_set_input(int irqline, int state)
{
	switch (irqline)
	{
		case 0:
			m_irq_state = state;
			if (state == CLEAR_LINE)
				m_pending_irq &= ~INT_IRQ;
			else
			{
				m_pending_irq |= INT_IRQ;
				m_halted = 0;
			}
			break;
		case INPUT_LINE_NMI:
			if (m_nmi_state == state) return;
			m_nmi_state = state;
			if (state != CLEAR_LINE)
			{
				m_pending_irq |= NMI_IRQ;
				m_halted = 0;
			}
			break;
		case NEC_IREM_INPUT_LINE_POLL:
			m_poll_state = state;
			break;
	}
}

void nec_common_irem_device::device_start()
{
	unsigned int i, j, c;

	static const WREGS wreg_name[8]={ AW, CW, DW, BW, SP, BP, IX, IY };
	static const BREGS breg_name[8]={ AL, CL, DL, BL, AH, CH, DH, BH };

	for (i = 0; i < 256; i++)
	{
		for (j = i, c = 0; j > 0; j >>= 1)
			if (j & 1) c++;
		parity_table[i] = !(c & 1);
	}

	for (i = 0; i < 256; i++)
	{
		Mod_RM.reg.b[i] = breg_name[(i & 0x38) >> 3];
		Mod_RM.reg.w[i] = wreg_name[(i & 0x38) >> 3];
	}

	for (i = 0xc0; i < 0x100; i++)
	{
		Mod_RM.RM.w[i] = wreg_name[i & 7];
		Mod_RM.RM.b[i] = breg_name[i & 7];
	}

	m_no_interrupt = 0;
	m_prefetch_count = 0;
	m_prefetch_reset = 0;
	m_prefix_base = 0;
	m_seg_prefix = 0;
	m_EA = 0;
	m_EO = 0;
	m_E16 = 0;
	m_debugger_temp = 0;
	m_ip = 0;

	memset(m_regs.w, 0x00, sizeof(m_regs.w));
	memset(m_sregs, 0x00, sizeof(m_sregs));

	save_item(NAME(m_regs.w));
	save_item(NAME(m_sregs));

	save_item(NAME(m_ip));
	save_item(NAME(m_TF));
	save_item(NAME(m_IF));
	save_item(NAME(m_DF));
	save_item(NAME(m_MF));
	save_item(NAME(m_SignVal));
	save_item(NAME(m_AuxVal));
	save_item(NAME(m_OverVal));
	save_item(NAME(m_ZeroVal));
	save_item(NAME(m_CarryVal));
	save_item(NAME(m_ParityVal));
	save_item(NAME(m_pending_irq));
	save_item(NAME(m_nmi_state));
	save_item(NAME(m_irq_state));
	save_item(NAME(m_poll_state));
	save_item(NAME(m_no_interrupt));
	save_item(NAME(m_halted));
	save_item(NAME(m_prefetch_count));
	save_item(NAME(m_prefetch_reset));

	m_program = &space(AS_PROGRAM);
	m_direct = &m_program->direct();
	m_io = &space(AS_IO);

	state_add( NEC_IREM_PC,    "PC", m_debugger_temp).callimport().callexport().formatstr("%05X");
	state_add( NEC_IREM_IP,    "IP", m_ip).formatstr("%04X");
	state_add( NEC_IREM_SP,    "SP", Wreg(SP)).formatstr("%04X");
	state_add( NEC_IREM_FLAGS, "F", m_debugger_temp).callimport().callexport().formatstr("%04X");
	state_add( NEC_IREM_AW,    "AW", Wreg(AW)).formatstr("%04X");
	state_add( NEC_IREM_CW,    "CW", Wreg(CW)).formatstr("%04X");
	state_add( NEC_IREM_DW,    "DW", Wreg(DW)).formatstr("%04X");
	state_add( NEC_IREM_BW,    "BW", Wreg(BW)).formatstr("%04X");
	state_add( NEC_IREM_BP,    "BP", Wreg(BP)).formatstr("%04X");
	state_add( NEC_IREM_IX,    "IX", Wreg(IX)).formatstr("%04X");
	state_add( NEC_IREM_IY,    "IY", Wreg(IY)).formatstr("%04X");
	state_add( NEC_IREM_ES,    "DS1", Sreg(DS1)).formatstr("%04X");
	state_add( NEC_IREM_CS,    "PS", Sreg(PS)).formatstr("%04X");
	state_add( NEC_IREM_SS,    "SS", Sreg(SS)).formatstr("%04X");
	state_add( NEC_IREM_DS,    "DS0", Sreg(DS0)).formatstr("%04X");

	state_add( STATE_GENPC, "GENPC", m_debugger_temp).callimport().callexport().noshow();
	state_add( STATE_GENSP, "GENSP", m_debugger_temp).callimport().callexport().noshow();
	state_add( STATE_GENFLAGS, "GENFLAGS", m_debugger_temp).formatstr("%16s").noshow();

	m_icountptr = &m_icount;
}

void nec_common_irem_device::state_string_export(const device_state_entry &entry, std::string &str) const
{
	UINT16 flags = CompressFlags();

	switch (entry.index())
	{
		case STATE_GENFLAGS:
			str = string_format("%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c",
				flags & 0x8000 ? 'N':'E',
				flags & 0x4000 ? '?':'.',
				flags & 0x2000 ? '?':'.',
				flags & 0x1000 ? '?':'.',
				flags & 0x0800 ? 'O':'.',
				flags & 0x0400 ? 'D':'.',
				flags & 0x0200 ? 'I':'.',
				flags & 0x0100 ? 'T':'.',
				flags & 0x0080 ? 'S':'.',
				flags & 0x0040 ? 'Z':'.',
				flags & 0x0020 ? '?':'.',
				flags & 0x0010 ? 'A':'.',
				flags & 0x0008 ? '?':'.',
				flags & 0x0004 ? 'P':'.',
				flags & 0x0002 ? '.':'?',
				flags & 0x0001 ? 'C':'.');
			break;
	}
}

void nec_common_irem_device::state_import(const device_state_entry &entry)
{
	switch (entry.index())
	{
		case NEC_IREM_PC:
			if( m_debugger_temp - (Sreg(PS)<<4) < 0x10000 )
			{
				m_ip = m_debugger_temp - (Sreg(PS)<<4);
			}
			else
			{
				Sreg(PS) = m_debugger_temp >> 4;
				m_ip = m_debugger_temp & 0x0000f;
			}
			break;

		case NEC_IREM_FLAGS:
			ExpandFlags(m_debugger_temp);
			break;
	}
}


void nec_common_irem_device::state_export(const device_state_entry &entry)
{
	switch (entry.index())
	{
		case STATE_GENPC:
		case NEC_IREM_PC:
			m_debugger_temp = (Sreg(PS)<<4) + m_ip;
			break;

		case STATE_GENSP:
			m_debugger_temp = (Sreg(SS)<<4) + Wreg(SP);
			break;

		case NEC_IREM_FLAGS:
			m_debugger_temp = CompressFlags();
			break;
	}
}


void nec_common_irem_device::execute_run()
{
	int prev_ICount;

	if (m_halted)
	{
		m_icount = 0;
	//	debugger_instruction_hook(this, (Sreg(PS)<<4) + m_ip);
		return;
	}

	while(m_icount>0) {
		/* Dispatch IRQ */
		if (m_pending_irq && m_no_interrupt==0)
		{
			if (m_pending_irq & NMI_IRQ)
				external_int();
			else if (m_IF)
				external_int();
		}

		/* No interrupt allowed between last instruction and this one */
		if (m_no_interrupt)
			m_no_interrupt--;

	//	debugger_instruction_hook(this, (Sreg(PS)<<4) + m_ip);
		prev_ICount = m_icount;
		do_opcode(fetchop());
		do_prefetch(prev_ICount);
	}
}

// license:BSD-3-Clause
// copyright-holders:Bryan McPhail

//#define OP(num,func_name) void nec_common_irem_device::func_name()

void nec_common_irem_device::do_opcode(const UINT8 opcode)
{
	switch (opcode)
	{


	case 0x00:/* i_add_br8  ) */ { DEF_br8;   ADDB;   PutbackRMByte(ModRM, dst);   CLKM(2, 2, 2, 16, 16, 7);        }; break;
	case 0x01:/*  i_add_wr16 ) */ { DEF_wr16;  ADDW;   PutbackRMWord(ModRM, dst);   CLKR(24, 24, 11, 24, 16, 7, 2, m_EA); }; break;
	case 0x02:/*  i_add_r8b  ) */ { DEF_r8b;   ADDB;   RegByte(ModRM) = dst;         CLKM(2, 2, 2, 11, 11, 6);        }; break;
	case 0x03:/*  i_add_r16w ) */ { DEF_r16w;  ADDW;   RegWord(ModRM) = dst;         CLKR(15, 15, 8, 15, 11, 6, 2, m_EA); }; break;
	case 0x04:/*  i_add_ald8 ) */ { DEF_ald8;  ADDB;   Breg(AL) = dst;           CLKS(4, 4, 2);                }; break;
	case 0x05:/*  i_add_axd16) */ { DEF_axd16; ADDW;   Wreg(AW) = dst;           CLKS(4, 4, 2);                }; break;
	case 0x06:/*  i_push_es  ) */ { PUSH(Sreg(DS1));   CLKS(12, 8, 3);   }; break;
	case 0x07:/*  i_pop_es   ) */ { POP(Sreg(DS1));    CLKS(12, 8, 5);   }; break;

	case 0x08:/*  i_or_br8   ) */ { DEF_br8;   ORB;    PutbackRMByte(ModRM, dst);   CLKM(2, 2, 2, 16, 16, 7);        }; break;
	case 0x09:/*  i_or_wr16  ) */ { DEF_wr16;  ORW;    PutbackRMWord(ModRM, dst);   CLKR(24, 24, 11, 24, 16, 7, 2, m_EA); }; break;
	case 0x0a:/*  i_or_r8b   ) */ { DEF_r8b;   ORB;    RegByte(ModRM) = dst;         CLKM(2, 2, 2, 11, 11, 6);        }; break;
	case 0x0b:/*  i_or_r16w  ) */ { DEF_r16w;  ORW;    RegWord(ModRM) = dst;         CLKR(15, 15, 8, 15, 11, 6, 2, m_EA); }; break;
	case 0x0c:/*  i_or_ald8  ) */ { DEF_ald8;  ORB;    Breg(AL) = dst;           CLKS(4, 4, 2);                }; break;
	case 0x0d:/*  i_or_axd16 ) */ { DEF_axd16; ORW;    Wreg(AW) = dst;           CLKS(4, 4, 2);                }; break;
	case 0x0e:/*  i_push_cs  ) */ { PUSH(Sreg(PS));    CLKS(12, 8, 3);   }
	case 0x0f:/*  i_pre_nec  ) */ { UINT32 ModRM, tmp, tmp2;
		switch (FETCH()) {
		case 0x10: BITOP_BYTE; CLKS(3, 3, 4); tmp2 = Breg(CL) & 0x7; m_ZeroVal = (tmp & (1 << tmp2)) ? 1 : 0; m_CarryVal = m_OverVal = 0; break; /* Test */
		case 0x11: BITOP_WORD; CLKS(3, 3, 4); tmp2 = Breg(CL) & 0xf; m_ZeroVal = (tmp & (1 << tmp2)) ? 1 : 0; m_CarryVal = m_OverVal = 0; break; /* Test */
		case 0x12: BITOP_BYTE; CLKS(5, 5, 4); tmp2 = Breg(CL) & 0x7; tmp &= ~(1 << tmp2);  PutbackRMByte(ModRM, tmp);   break; /* Clr */
		case 0x13: BITOP_WORD; CLKS(5, 5, 4); tmp2 = Breg(CL) & 0xf; tmp &= ~(1 << tmp2);  PutbackRMWord(ModRM, tmp);   break; /* Clr */
		case 0x14: BITOP_BYTE; CLKS(4, 4, 4); tmp2 = Breg(CL) & 0x7; tmp |= (1 << tmp2);   PutbackRMByte(ModRM, tmp);   break; /* Set */
		case 0x15: BITOP_WORD; CLKS(4, 4, 4); tmp2 = Breg(CL) & 0xf; tmp |= (1 << tmp2);   PutbackRMWord(ModRM, tmp);   break; /* Set */
		case 0x16: BITOP_BYTE; CLKS(4, 4, 4); tmp2 = Breg(CL) & 0x7; BIT_NOT;            PutbackRMByte(ModRM, tmp);   break; /* Not */
		case 0x17: BITOP_WORD; CLKS(4, 4, 4); tmp2 = Breg(CL) & 0xf; BIT_NOT;            PutbackRMWord(ModRM, tmp);   break; /* Not */

		case 0x18: BITOP_BYTE; CLKS(4, 4, 4); tmp2 = (FETCH()) & 0x7;    m_ZeroVal = (tmp & (1 << tmp2)) ? 1 : 0; m_CarryVal = m_OverVal = 0; break; /* Test */
		case 0x19: BITOP_WORD; CLKS(4, 4, 4); tmp2 = (FETCH()) & 0xf;    m_ZeroVal = (tmp & (1 << tmp2)) ? 1 : 0; m_CarryVal = m_OverVal = 0; break; /* Test */
		case 0x1a: BITOP_BYTE; CLKS(6, 6, 4); tmp2 = (FETCH()) & 0x7;    tmp &= ~(1 << tmp2);      PutbackRMByte(ModRM, tmp);   break; /* Clr */
		case 0x1b: BITOP_WORD; CLKS(6, 6, 4); tmp2 = (FETCH()) & 0xf;    tmp &= ~(1 << tmp2);      PutbackRMWord(ModRM, tmp);   break; /* Clr */
		case 0x1c: BITOP_BYTE; CLKS(5, 5, 4); tmp2 = (FETCH()) & 0x7;    tmp |= (1 << tmp2);       PutbackRMByte(ModRM, tmp);   break; /* Set */
		case 0x1d: BITOP_WORD; CLKS(5, 5, 4); tmp2 = (FETCH()) & 0xf;    tmp |= (1 << tmp2);       PutbackRMWord(ModRM, tmp);   break; /* Set */
		case 0x1e: BITOP_BYTE; CLKS(5, 5, 4); tmp2 = (FETCH()) & 0x7;    BIT_NOT;                PutbackRMByte(ModRM, tmp);   break; /* Not */
		case 0x1f: BITOP_WORD; CLKS(5, 5, 4); tmp2 = (FETCH()) & 0xf;    BIT_NOT;                PutbackRMWord(ModRM, tmp);   break; /* Not */

		case 0x20: ADD4S; CLKS(7, 7, 2); break;
		case 0x22: SUB4S; CLKS(7, 7, 2); break;
		case 0x26: CMP4S; CLKS(7, 7, 2); break;
		case 0x28: ModRM = FETCH(); tmp = GetRMByte(ModRM); tmp <<= 4; tmp |= Breg(AL) & 0xf; Breg(AL) = (Breg(AL) & 0xf0) | ((tmp >> 8) & 0xf); tmp &= 0xff; PutbackRMByte(ModRM, tmp); CLKM(13, 13, 9, 28, 28, 15); break;
		case 0x2a: ModRM = FETCH(); tmp = GetRMByte(ModRM); tmp2 = (Breg(AL) & 0xf) << 4; Breg(AL) = (Breg(AL) & 0xf0) | (tmp & 0xf); tmp = tmp2 | (tmp >> 4);   PutbackRMByte(ModRM, tmp); CLKM(17, 17, 13, 32, 32, 19); break;
		case 0x31: ModRM = FETCH(); ModRM = 0; logerror("%06x: Unimplemented bitfield INS\n", PC()); break;
		case 0x33: ModRM = FETCH(); ModRM = 0; logerror("%06x: Unimplemented bitfield EXT\n", PC()); break;
		case 0xe0: ModRM = FETCH(); ModRM = 0; logerror("%06x: V33IREM unimplemented BRKXA (break to expansion address)\n", PC()); break;
		case 0xf0: ModRM = FETCH(); ModRM = 0; logerror("%06x: V33IREM unimplemented RETXA (return from expansion address)\n", PC()); break;
		case 0xff: ModRM = FETCH(); ModRM = 0; logerror("%06x: unimplemented BRKEM (break to 8080 emulation mode)\n", PC()); break;
		default:    logerror("%06x: Unknown V20IREM instruction\n", PC()); break;
		}; break;
	}

	case 0x10:/*  i_adc_br8  ) */ { DEF_br8;   src += CF;    ADDB;   PutbackRMByte(ModRM, dst);   CLKM(2, 2, 2, 16, 16, 7);        }; break;
	case 0x11:/*  i_adc_wr16 ) */ { DEF_wr16;  src += CF;    ADDW;   PutbackRMWord(ModRM, dst);   CLKR(24, 24, 11, 24, 16, 7, 2, m_EA); }; break;
	case 0x12:/*  i_adc_r8b  ) */ { DEF_r8b;   src += CF;    ADDB;   RegByte(ModRM) = dst;         CLKM(2, 2, 2, 11, 11, 6);        }; break;
	case 0x13:/*  i_adc_r16w ) */ { DEF_r16w;  src += CF;    ADDW;   RegWord(ModRM) = dst;         CLKR(15, 15, 8, 15, 11, 6, 2, m_EA); }; break;
	case 0x14:/*  i_adc_ald8 ) */ { DEF_ald8;  src += CF;    ADDB;   Breg(AL) = dst;           CLKS(4, 4, 2);                }; break;
	case 0x15:/*  i_adc_axd16) */ { DEF_axd16; src += CF;    ADDW;   Wreg(AW) = dst;           CLKS(4, 4, 2);                }; break;
	case 0x16:/*  i_push_ss  ) */ { PUSH(Sreg(SS));        CLKS(12, 8, 3);   }; break;
	case 0x17:/*  i_pop_ss   ) */ { POP(Sreg(SS));     CLKS(12, 8, 5);   m_no_interrupt = 1; }; break;

	case 0x18:/*  i_sbb_br8  ) */ { DEF_br8;   src += CF;    SUBB;   PutbackRMByte(ModRM, dst);   CLKM(2, 2, 2, 16, 16, 7);        }; break;
	case 0x19:/*  i_sbb_wr16 ) */ { DEF_wr16;  src += CF;    SUBW;   PutbackRMWord(ModRM, dst);   CLKR(24, 24, 11, 24, 16, 7, 2, m_EA); }; break;
	case 0x1a:/*  i_sbb_r8b  ) */ { DEF_r8b;   src += CF;    SUBB;   RegByte(ModRM) = dst;         CLKM(2, 2, 2, 11, 11, 6);        }; break;
	case 0x1b:/*  i_sbb_r16w ) */ { DEF_r16w;  src += CF;    SUBW;   RegWord(ModRM) = dst;         CLKR(15, 15, 8, 15, 11, 6, 2, m_EA); }; break;
	case 0x1c:/*  i_sbb_ald8 ) */ { DEF_ald8;  src += CF;    SUBB;   Breg(AL) = dst;           CLKS(4, 4, 2);                }; break;
	case 0x1d:/*  i_sbb_axd16) */ { DEF_axd16; src += CF;    SUBW;   Wreg(AW) = dst;           CLKS(4, 4, 2);    }; break;
	case 0x1e:/*  i_push_ds  ) */ { PUSH(Sreg(DS0));       CLKS(12, 8, 3);   }; break;
	case 0x1f:/*  i_pop_ds   ) */ { POP(Sreg(DS0));        CLKS(12, 8, 5);   }; break;

	case 0x20:/*  i_and_br8  ) */ { DEF_br8;   ANDB;   PutbackRMByte(ModRM, dst);   CLKM(2, 2, 2, 16, 16, 7);        }; break;
	case 0x21:/*  i_and_wr16 ) */ { DEF_wr16;  ANDW;   PutbackRMWord(ModRM, dst);   CLKR(24, 24, 11, 24, 16, 7, 2, m_EA); }; break;
	case 0x22:/*  i_and_r8b  ) */ { DEF_r8b;   ANDB;   RegByte(ModRM) = dst;         CLKM(2, 2, 2, 11, 11, 6);        }; break;
	case 0x23:/*  i_and_r16w ) */ { DEF_r16w;  ANDW;   RegWord(ModRM) = dst;         CLKR(15, 15, 8, 15, 11, 6, 2, m_EA); }; break;
	case 0x24:/*  i_and_ald8 ) */ { DEF_ald8;  ANDB;   Breg(AL) = dst;           CLKS(4, 4, 2);                }; break;
	case 0x25:/*  i_and_axd16) */ { DEF_axd16; ANDW;   Wreg(AW) = dst;           CLKS(4, 4, 2);    }; break;
	case 0x26:/*  i_es       ) */ { m_seg_prefix = TRUE;    m_prefix_base = Sreg(DS1) << 4;    CLK(2);     do_opcode(fetchop()); m_seg_prefix = FALSE; }; break;
	case 0x27:/*  i_daa      ) */ { ADJ4(6, 0x60);                                  CLKS(3, 3, 2);    }; break;

	case 0x28:/*  i_sub_br8  ) */ { DEF_br8;   SUBB;   PutbackRMByte(ModRM, dst);   CLKM(2, 2, 2, 16, 16, 7);        }; break;
	case 0x29:/*  i_sub_wr16 ) */ { DEF_wr16;  SUBW;   PutbackRMWord(ModRM, dst);   CLKR(24, 24, 11, 24, 16, 7, 2, m_EA); }; break;
	case 0x2a:/*  i_sub_r8b  ) */ { DEF_r8b;   SUBB;   RegByte(ModRM) = dst;         CLKM(2, 2, 2, 11, 11, 6);        }; break;
	case 0x2b:/*  i_sub_r16w ) */ { DEF_r16w;  SUBW;   RegWord(ModRM) = dst;         CLKR(15, 15, 8, 15, 11, 6, 2, m_EA); }; break;
	case 0x2c:/*  i_sub_ald8 ) */ { DEF_ald8;  SUBB;   Breg(AL) = dst;           CLKS(4, 4, 2);                }; break;
	case 0x2d:/*  i_sub_axd16) */ { DEF_axd16; SUBW;   Wreg(AW) = dst;           CLKS(4, 4, 2);    }; break;
	case 0x2e:/*  i_cs       ) */ { m_seg_prefix = TRUE;    m_prefix_base = Sreg(PS) << 4; CLK(2);     do_opcode(fetchop()); m_seg_prefix = FALSE; }; break;
	case 0x2f:/*  i_das      ) */ { ADJ4(-6, -0x60);                                CLKS(3, 3, 2);    }; break;

	case 0x30:/*  i_xor_br8  ) */ { DEF_br8;   XORB;   PutbackRMByte(ModRM, dst);   CLKM(2, 2, 2, 16, 16, 7);        }; break;
	case 0x31:/*  i_xor_wr16 ) */ { DEF_wr16;  XORW;   PutbackRMWord(ModRM, dst);   CLKR(24, 24, 11, 24, 16, 7, 2, m_EA); }; break;
	case 0x32:/*  i_xor_r8b  ) */ { DEF_r8b;   XORB;   RegByte(ModRM) = dst;         CLKM(2, 2, 2, 11, 11, 6);        }; break;
	case 0x33:/*  i_xor_r16w ) */ { DEF_r16w;  XORW;   RegWord(ModRM) = dst;         CLKR(15, 15, 8, 15, 11, 6, 2, m_EA); }; break;
	case 0x34:/*  i_xor_ald8 ) */ { DEF_ald8;  XORB;   Breg(AL) = dst;           CLKS(4, 4, 2);                }; break;
	case 0x35:/*  i_xor_axd16) */ { DEF_axd16; XORW;   Wreg(AW) = dst;           CLKS(4, 4, 2);    }; break;
	case 0x36:/*  i_ss       ) */ { m_seg_prefix = TRUE;    m_prefix_base = Sreg(SS) << 4; CLK(2);     do_opcode(fetchop()); m_seg_prefix = FALSE; }; break;
	case 0x37:/*  i_aaa      ) */ { ADJB(6, (Breg(AL) > 0xf9) ? 2 : 1);        CLKS(7, 7, 4);    }; break;

	case 0x38:/*  i_cmp_br8  ) */ { DEF_br8;   SUBB;                   CLKM(2, 2, 2, 11, 11, 6); }; break;
	case 0x39:/*  i_cmp_wr16 ) */ { DEF_wr16;  SUBW;                   CLKR(15, 15, 8, 15, 11, 6, 2, m_EA); }; break;
	case 0x3a:/*  i_cmp_r8b  ) */ { DEF_r8b;   SUBB;                   CLKM(2, 2, 2, 11, 11, 6); }; break;
	case 0x3b:/*  i_cmp_r16w ) */ { DEF_r16w;  SUBW;                   CLKR(15, 15, 8, 15, 11, 6, 2, m_EA); }; break;
	case 0x3c:/*  i_cmp_ald8 ) */ { DEF_ald8;  SUBB;                   CLKS(4, 4, 2); }; break;
	case 0x3d:/*  i_cmp_axd16) */ { DEF_axd16; SUBW;                   CLKS(4, 4, 2);    }; break;
	case 0x3e:/*  i_ds       ) */ { m_seg_prefix = TRUE;    m_prefix_base = Sreg(DS0) << 4;    CLK(2);     do_opcode(fetchop()); m_seg_prefix = FALSE; }; break;
	case 0x3f:/*  i_aas      ) */ { ADJB(-6, (Breg(AL) < 6) ? -2 : -1);        CLKS(7, 7, 4);    }; break;

	case 0x40:/*  i_inc_ax  ) */ { IncWordReg(AW);                     CLK(2); }; break;
	case 0x41:/*  i_inc_cx  ) */ { IncWordReg(CW);                     CLK(2); }; break;
	case 0x42:/*  i_inc_dx  ) */ { IncWordReg(DW);                     CLK(2); }; break;
	case 0x43:/*  i_inc_bx  ) */ { IncWordReg(BW);                     CLK(2); }; break;
	case 0x44:/*  i_inc_sp  ) */ { IncWordReg(SP);                     CLK(2); }; break;
	case 0x45:/*  i_inc_bp  ) */ { IncWordReg(BP);                     CLK(2); }; break;
	case 0x46:/*  i_inc_si  ) */ { IncWordReg(IX);                     CLK(2); }; break;
	case 0x47:/*  i_inc_di  ) */ { IncWordReg(IY);                     CLK(2); }; break;

	case 0x48:/*  i_dec_ax  ) */ { DecWordReg(AW);                     CLK(2); }; break;
	case 0x49:/*  i_dec_cx  ) */ { DecWordReg(CW);                     CLK(2); }; break;
	case 0x4a:/*  i_dec_dx  ) */ { DecWordReg(DW);                     CLK(2); }; break;
	case 0x4b:/*  i_dec_bx  ) */ { DecWordReg(BW);                     CLK(2); }; break;
	case 0x4c:/*  i_dec_sp  ) */ { DecWordReg(SP);                     CLK(2); }; break;
	case 0x4d:/*  i_dec_bp  ) */ { DecWordReg(BP);                     CLK(2); }; break;
	case 0x4e:/*  i_dec_si  ) */ { DecWordReg(IX);                     CLK(2); }; break;
	case 0x4f:/*  i_dec_di  ) */ { DecWordReg(IY);                     CLK(2); }; break;

	case 0x50:/*  i_push_ax ) */ { PUSH(Wreg(AW));                 CLKS(12, 8, 3); }; break;
	case 0x51:/*  i_push_cx ) */ { PUSH(Wreg(CW));                 CLKS(12, 8, 3); }; break;
	case 0x52:/*  i_push_dx ) */ { PUSH(Wreg(DW));                 CLKS(12, 8, 3); }; break;
	case 0x53:/*  i_push_bx ) */ { PUSH(Wreg(BW));                 CLKS(12, 8, 3); }; break;
	case 0x54:/*  i_push_sp ) */ { PUSH(Wreg(SP));                 CLKS(12, 8, 3); }; break;
	case 0x55:/*  i_push_bp ) */ { PUSH(Wreg(BP));                 CLKS(12, 8, 3); }; break;
	case 0x56:/*  i_push_si ) */ { PUSH(Wreg(IX));                 CLKS(12, 8, 3); }; break;
	case 0x57:/*  i_push_di ) */ { PUSH(Wreg(IY));                 CLKS(12, 8, 3); }; break;

	case 0x58:/*  i_pop_ax  ) */ { POP(Wreg(AW));                  CLKS(12, 8, 5); }; break;
	case 0x59:/*  i_pop_cx  ) */ { POP(Wreg(CW));                  CLKS(12, 8, 5); }; break;
	case 0x5a:/*  i_pop_dx  ) */ { POP(Wreg(DW));                  CLKS(12, 8, 5); }; break;
	case 0x5b:/*  i_pop_bx  ) */ { POP(Wreg(BW));                  CLKS(12, 8, 5); }; break;
	case 0x5c:/*  i_pop_sp  ) */ { POP(Wreg(SP));                  CLKS(12, 8, 5); }; break;
	case 0x5d:/*  i_pop_bp  ) */ { POP(Wreg(BP));                  CLKS(12, 8, 5); }; break;
	case 0x5e:/*  i_pop_si  ) */ { POP(Wreg(IX));                  CLKS(12, 8, 5); }; break;
	case 0x5f:/*  i_pop_di  ) */ { POP(Wreg(IY));                  CLKS(12, 8, 5); }; break;

	case 0x60:/*  i_pusha  ) */ {
		unsigned tmp = Wreg(SP);
		PUSH(Wreg(AW));
		PUSH(Wreg(CW));
		PUSH(Wreg(DW));
		PUSH(Wreg(BW));
		PUSH(tmp);
		PUSH(Wreg(BP));
		PUSH(Wreg(IX));
		PUSH(Wreg(IY));
		CLKS(67, 35, 20);
	}; break;
		static unsigned nec_popa_tmp;
	case 0x61:/*  i_popa  ) */ {
		POP(Wreg(IY));
		POP(Wreg(IX));
		POP(Wreg(BP));
		POP(nec_popa_tmp);
		logerror("%02x\n", nec_popa_tmp);
		POP(Wreg(BW));
		POP(Wreg(DW));
		POP(Wreg(CW));
		POP(Wreg(AW));
		CLKS(75, 43, 22);
	}; break;
	case 0x62:/*  i_chkind  ) */ {
		UINT32 low, high, tmp;
		GetModRM;
		low = GetRMWord(ModRM);
		high = GetnextRMWord;
		tmp = RegWord(ModRM);
		if (tmp<low || tmp>high) {
			nec_interrupt(NEC_IREM_CHKIND_VECTOR, BRK);
		}
		m_icount -= 20;
		logerror("%06x: bound %04x high %04x low %04x tmp\n", PC(), high, low, tmp);
	}; break;
	case 0x64:/*  i_repnc  ) */ {  UINT32 next = fetchop();   UINT16 c = Wreg(CW);
		switch (next) { /* Segments */
		case 0x26:  m_seg_prefix = TRUE; m_prefix_base = Sreg(DS1) << 4;    next = fetchop();  CLK(2); break;
		case 0x2e:  m_seg_prefix = TRUE; m_prefix_base = Sreg(PS) << 4; next = fetchop();  CLK(2); break;
		case 0x36:  m_seg_prefix = TRUE; m_prefix_base = Sreg(SS) << 4; next = fetchop();  CLK(2); break;
		case 0x3e:  m_seg_prefix = TRUE; m_prefix_base = Sreg(DS0) << 4;    next = fetchop();  CLK(2); break;
		}

		switch (next) {
		case 0x6c:  CLK(2); if (c) do { { PutMemB(DS1, Wreg(IY), read_port_byte(Wreg(DW))); Wreg(IY) += -2 * m_DF + 1; CLK(8); };  c--; } while (c > 0 && !CF); Wreg(CW) = c; break;
		case 0x6d:  CLK(2); if (c) do { { PutMemW(DS1, Wreg(IY), read_port_word(Wreg(DW))); Wreg(IY) += -4 * m_DF + 2; CLKS(18, 10, 8); };  c--; } while (c > 0 && !CF); Wreg(CW) = c; break;
		case 0x6e:  CLK(2); if (c) do { { write_port_byte(Wreg(DW), GetMemB(DS0, Wreg(IX))); Wreg(IX) += -2 * m_DF + 1; CLK(8); }; c--; } while (c > 0 && !CF); Wreg(CW) = c; break;
		case 0x6f:  CLK(2); if (c) do { { write_port_word(Wreg(DW), GetMemW(DS0, Wreg(IX))); Wreg(IX) += -4 * m_DF + 2; CLKS(18, 10, 8); }; c--; } while (c > 0 && !CF); Wreg(CW) = c; break;
		case 0xa4:  CLK(2); if (c) do { { UINT32 tmp = GetMemB(DS0, Wreg(IX)); PutMemB(DS1, Wreg(IY), tmp); Wreg(IY) += -2 * m_DF + 1; Wreg(IX) += -2 * m_DF + 1; CLKS(8, 8, 6); }; c--; } while (c > 0 && !CF); Wreg(CW) = c; break;
		case 0xa5:  CLK(2); if (c) do { { UINT32 tmp = GetMemW(DS0, Wreg(IX)); PutMemW(DS1, Wreg(IY), tmp); Wreg(IY) += -4 * m_DF + 2; Wreg(IX) += -4 * m_DF + 2; CLKS(16, 16, 10); }; c--; } while (c > 0 && !CF); Wreg(CW) = c; break;
		case 0xa6:  CLK(2); if (c) do { { UINT32 src = D1GetMemB(Wreg(IY)); UINT32 dst = GetMemB(DS0, Wreg(IX)); SUBB; Wreg(IY) += -2 * m_DF + 1; Wreg(IX) += -2 * m_DF + 1; CLKS(14, 14, 14); }; c--; } while (c > 0 && !CF); Wreg(CW) = c; break;
		case 0xa7:  CLK(2); if (c) do { { UINT32 src = GetMemW(DS1, Wreg(IY)); UINT32 dst = GetMemW(DS0, Wreg(IX)); SUBW; Wreg(IY) += -4 * m_DF + 2; Wreg(IX) += -4 * m_DF + 2; CLKS(14, 14, 14); }; c--; } while (c > 0 && !CF); Wreg(CW) = c; break;
		case 0xaa:  CLK(2); if (c) do { { PutMemB(DS1, Wreg(IY), Breg(AL));  Wreg(IY) += -2 * m_DF + 1; CLKS(4, 4, 3);  }; c--; } while (c > 0 && !CF); Wreg(CW) = c; break;
		case 0xab:  CLK(2); if (c) do { { PutMemW(DS1, Wreg(IY), Wreg(AW));  Wreg(IY) += -4 * m_DF + 2; CLKW(8, 8, 5, 8, 4, 3, Wreg(IY)); }; c--; } while (c > 0 && !CF); Wreg(CW) = c; break;
		case 0xac:  CLK(2); if (c) do { { Breg(AL) = GetMemB(DS0, Wreg(IX)); Wreg(IX) += -2 * m_DF + 1; CLKS(4, 4, 3);  }; c--; } while (c > 0 && !CF); Wreg(CW) = c; break;
		case 0xad:  CLK(2); if (c) do { { Wreg(AW) = GetMemW(DS0, Wreg(IX)); Wreg(IX) += -4 * m_DF + 2; CLKW(8, 8, 5, 8, 4, 3, Wreg(IX)); }; c--; } while (c > 0 && !CF); Wreg(CW) = c; break;
		case 0xae:  CLK(2); if (c) do { { UINT32 src = D1GetMemB(Wreg(IY)); UINT32 dst = Breg(AL); SUBB; Wreg(IY) += -2 * m_DF + 1; CLKS(4, 4, 3);  }; c--; } while (c > 0 && !CF); Wreg(CW) = c; break;
		case 0xaf:  CLK(2); if (c) do { { UINT32 src = GetMemW(DS1, Wreg(IY)); UINT32 dst = Wreg(AW); SUBW; Wreg(IY) += -4 * m_DF + 2; CLKW(8, 8, 5, 8, 4, 3, Wreg(IY)); }; c--; } while (c > 0 && !CF); Wreg(CW) = c; break;
		default:    logerror("%06x: REPNC invalid\n", PC());    	do_opcode(next);
			;
		}
		m_seg_prefix = FALSE;
	}; break;

	case 0x65:/*  i_repc  ) */ {   UINT32 next = fetchop();   UINT16 c = Wreg(CW);
		switch (next) { /* Segments */
		case 0x26:  m_seg_prefix = TRUE; m_prefix_base = Sreg(DS1) << 4;    next = fetchop();  CLK(2); break;
		case 0x2e:  m_seg_prefix = TRUE; m_prefix_base = Sreg(PS) << 4; next = fetchop();  CLK(2); break;
		case 0x36:  m_seg_prefix = TRUE; m_prefix_base = Sreg(SS) << 4; next = fetchop();  CLK(2); break;
		case 0x3e:  m_seg_prefix = TRUE; m_prefix_base = Sreg(DS0) << 4;    next = fetchop();  CLK(2); break;
		}

		switch (next) {
		case 0x6c:  CLK(2); if (c) do { { PutMemB(DS1, Wreg(IY), read_port_byte(Wreg(DW))); Wreg(IY) += -2 * m_DF + 1; CLK(8); };  c--; } while (c > 0 && CF);   Wreg(CW) = c; break;
		case 0x6d:  CLK(2); if (c) do { { PutMemW(DS1, Wreg(IY), read_port_word(Wreg(DW))); Wreg(IY) += -4 * m_DF + 2; CLKS(18, 10, 8); };  c--; } while (c > 0 && CF);   Wreg(CW) = c; break;
		case 0x6e:  CLK(2); if (c) do { { write_port_byte(Wreg(DW), GetMemB(DS0, Wreg(IX))); Wreg(IX) += -2 * m_DF + 1; CLK(8); }; c--; } while (c > 0 && CF);   Wreg(CW) = c; break;
		case 0x6f:  CLK(2); if (c) do { { write_port_word(Wreg(DW), GetMemW(DS0, Wreg(IX))); Wreg(IX) += -4 * m_DF + 2; CLKS(18, 10, 8); };; c--; } while (c > 0 && CF);   Wreg(CW) = c; break;
		case 0xa4:  CLK(2); if (c) do { { UINT32 tmp = GetMemB(DS0, Wreg(IX)); PutMemB(DS1, Wreg(IY), tmp); Wreg(IY) += -2 * m_DF + 1; Wreg(IX) += -2 * m_DF + 1; CLKS(8, 8, 6); }; c--; } while (c > 0 && CF);   Wreg(CW) = c; break;
		case 0xa5:  CLK(2); if (c) do { { UINT32 tmp = GetMemW(DS0, Wreg(IX)); PutMemW(DS1, Wreg(IY), tmp); Wreg(IY) += -4 * m_DF + 2; Wreg(IX) += -4 * m_DF + 2; CLKS(16, 16, 10); }; c--; } while (c > 0 && CF);   Wreg(CW) = c; break;
		case 0xa6:  CLK(2); if (c) do { { UINT32 src = D1GetMemB(Wreg(IY)); UINT32 dst = GetMemB(DS0, Wreg(IX)); SUBB; Wreg(IY) += -2 * m_DF + 1; Wreg(IX) += -2 * m_DF + 1; CLKS(14, 14, 14); }; c--; } while (c > 0 && CF);   Wreg(CW) = c; break;
		case 0xa7:  CLK(2); if (c) do { { UINT32 src = GetMemW(DS1, Wreg(IY)); UINT32 dst = GetMemW(DS0, Wreg(IX)); SUBW; Wreg(IY) += -4 * m_DF + 2; Wreg(IX) += -4 * m_DF + 2; CLKS(14, 14, 14); }; c--; } while (c > 0 && CF);   Wreg(CW) = c; break;
		case 0xaa:  CLK(2); if (c) do { { PutMemB(DS1, Wreg(IY), Breg(AL));  Wreg(IY) += -2 * m_DF + 1; CLKS(4, 4, 3);  }; c--; } while (c > 0 && CF);   Wreg(CW) = c; break;
		case 0xab:  CLK(2); if (c) do { { PutMemW(DS1, Wreg(IY), Wreg(AW));  Wreg(IY) += -4 * m_DF + 2; CLKW(8, 8, 5, 8, 4, 3, Wreg(IY)); }; c--; } while (c > 0 && CF);   Wreg(CW) = c; break;
		case 0xac:  CLK(2); if (c) do { { Breg(AL) = GetMemB(DS0, Wreg(IX)); Wreg(IX) += -2 * m_DF + 1; CLKS(4, 4, 3);  }; c--; } while (c > 0 && CF);   Wreg(CW) = c; break;
		case 0xad:  CLK(2); if (c) do { { Wreg(AW) = GetMemW(DS0, Wreg(IX)); Wreg(IX) += -4 * m_DF + 2; CLKW(8, 8, 5, 8, 4, 3, Wreg(IX)); }; c--; } while (c > 0 && CF);   Wreg(CW) = c; break;
		case 0xae:  CLK(2); if (c) do { { UINT32 src = D1GetMemB(Wreg(IY)); UINT32 dst = Breg(AL); SUBB; Wreg(IY) += -2 * m_DF + 1; CLKS(4, 4, 3);  }; c--; } while (c > 0 && CF);   Wreg(CW) = c; break;
		case 0xaf:  CLK(2); if (c) do { { UINT32 src = GetMemW(DS1, Wreg(IY)); UINT32 dst = Wreg(AW); SUBW; Wreg(IY) += -4 * m_DF + 2; CLKW(8, 8, 5, 8, 4, 3, Wreg(IY)); }; c--; } while (c > 0 && CF);   Wreg(CW) = c; break;
		default:    logerror("%06x: REPC invalid\n", PC()); 	do_opcode(next);

		}
		m_seg_prefix = FALSE;
	}; break;

	case 0x68:/*  i_push_d16 ) */ { UINT32 tmp;    tmp = FETCHWORD(); PUSH(tmp);   CLKW(12, 12, 5, 12, 8, 5, Wreg(SP));  }; break;
	case 0x69:/*  i_imul_d16 ) */ { UINT32 tmp;    DEF_r16w;   tmp = FETCHWORD(); dst = (INT32)((INT16)src) * (INT32)((INT16)tmp); m_CarryVal = m_OverVal = (((INT32)dst) >> 15 != 0) && (((INT32)dst) >> 15 != -1);     RegWord(ModRM) = (WORD)dst;     m_icount -= (ModRM >= 0xc0) ? 38 : 47; }; break;
	case 0x6a:/*  i_push_d8  ) */ { UINT32 tmp = (WORD)((INT16)((INT8)FETCH()));   PUSH(tmp);  CLKW(11, 11, 5, 11, 7, 3, Wreg(SP));  }; break;
	case 0x6b:/*  i_imul_d8  ) */ { UINT32 src2; DEF_r16w; src2 = (WORD)((INT16)((INT8)FETCH())); dst = (INT32)((INT16)src) * (INT32)((INT16)src2); m_CarryVal = m_OverVal = (((INT32)dst) >> 15 != 0) && (((INT32)dst) >> 15 != -1); RegWord(ModRM) = (WORD)dst; m_icount -= (ModRM >= 0xc0) ? 31 : 39; }; break;
	case 0x6c:/*  i_insb     ) */ { PutMemB(DS1, Wreg(IY), read_port_byte(Wreg(DW))); Wreg(IY) += -2 * m_DF + 1; CLK(8); }; break;
	case 0x6d:/*  i_insw     ) */ { PutMemW(DS1, Wreg(IY), read_port_word(Wreg(DW))); Wreg(IY) += -4 * m_DF + 2; CLKS(18, 10, 8); }; break;
	case 0x6e:/*  i_outsb    ) */ { write_port_byte(Wreg(DW), GetMemB(DS0, Wreg(IX))); Wreg(IX) += -2 * m_DF + 1; CLK(8); }; break;
	case 0x6f:/*  i_outsw    ) */ { write_port_word(Wreg(DW), GetMemW(DS0, Wreg(IX))); Wreg(IX) += -4 * m_DF + 2; CLKS(18, 10, 8); }; break;

	case 0x70:/*  i_jo      ) */ { JMP(OF);               CLKS(4, 4, 3); }; break;
	case 0x71:/*  i_jno     ) */ { JMP(!OF);               CLKS(4, 4, 3); }; break;
	case 0x72:/*  i_jc      ) */ { JMP(CF);               CLKS(4, 4, 3); }; break;
	case 0x73:/*  i_jnc     ) */ { JMP(!CF);               CLKS(4, 4, 3); }; break;
	case 0x74:/*  i_jz      ) */ { JMP(ZF);               CLKS(4, 4, 3); }; break;
	case 0x75:/*  i_jnz     ) */ { JMP(!ZF);               CLKS(4, 4, 3); }; break;
	case 0x76:/*  i_jce     ) */ { JMP(CF || ZF);          CLKS(4, 4, 3); }; break;
	case 0x77:/*  i_jnce    ) */ { JMP(!(CF || ZF));       CLKS(4, 4, 3); }; break;
	case 0x78:/*  i_js      ) */ { JMP(SF);               CLKS(4, 4, 3); }; break;
	case 0x79:/*  i_jns     ) */ { JMP(!SF);               CLKS(4, 4, 3); }; break;
	case 0x7a:/*  i_jp      ) */ { JMP(PF);               CLKS(4, 4, 3); }; break;
	case 0x7b:/*  i_jnp     ) */ { JMP(!PF);               CLKS(4, 4, 3); }; break;
	case 0x7c:/*  i_jl      ) */ { JMP((SF != OF) && (!ZF));   CLKS(4, 4, 3); }; break;
	case 0x7d:/*  i_jnl     ) */ { JMP((ZF) || (SF == OF));    CLKS(4, 4, 3); }; break;
	case 0x7e:/*  i_jle     ) */ { JMP((ZF) || (SF != OF));    CLKS(4, 4, 3); }; break;
	case 0x7f:/*  i_jnle    ) */ { JMP((SF == OF) && (!ZF));   CLKS(4, 4, 3); }; break;

	case 0x80:/*  i_80pre   ) */ { UINT32 dst, src; GetModRM; dst = GetRMByte(ModRM); src = FETCH();
		if (ModRM >= 0xc0) CLKS(4, 4, 2) else if ((ModRM & 0x38) == 0x38) CLKS(13, 13, 6) else CLKS(18, 18, 7)
			switch (ModRM & 0x38) {
			case 0x00: ADDB;            PutbackRMByte(ModRM, dst);   break;
			case 0x08: ORB;             PutbackRMByte(ModRM, dst);   break;
			case 0x10: src += CF; ADDB;   PutbackRMByte(ModRM, dst);   break;
			case 0x18: src += CF; SUBB;   PutbackRMByte(ModRM, dst);   break;
			case 0x20: ANDB;            PutbackRMByte(ModRM, dst);   break;
			case 0x28: SUBB;            PutbackRMByte(ModRM, dst);   break;
			case 0x30: XORB;            PutbackRMByte(ModRM, dst);   break;
			case 0x38: SUBB;            break;  /* CMP */
			}
	}; break;

	case 0x81:/*  i_81pre   ) */ { UINT32 dst, src; GetModRM; dst = GetRMWord(ModRM); src = FETCH(); src += (FETCH() << 8);
		if (ModRM >= 0xc0) CLKS(4, 4, 2) else if ((ModRM & 0x38) == 0x38) CLKW(17, 17, 8, 17, 13, 6, m_EA) else CLKW(26, 26, 11, 26, 18, 7, m_EA)
			switch (ModRM & 0x38) {
			case 0x00: ADDW;            PutbackRMWord(ModRM, dst);   break;
			case 0x08: ORW;             PutbackRMWord(ModRM, dst);   break;
			case 0x10: src += CF; ADDW;   PutbackRMWord(ModRM, dst);   break;
			case 0x18: src += CF; SUBW;   PutbackRMWord(ModRM, dst);   break;
			case 0x20: ANDW;            PutbackRMWord(ModRM, dst);   break;
			case 0x28: SUBW;            PutbackRMWord(ModRM, dst);   break;
			case 0x30: XORW;            PutbackRMWord(ModRM, dst);   break;
			case 0x38: SUBW;            break;  /* CMP */
			}
	}; break;

	case 0x82:/*  i_82pre   ) */ { UINT32 dst, src; GetModRM; dst = GetRMByte(ModRM); src = (BYTE)((INT8)FETCH());
		if (ModRM >= 0xc0) CLKS(4, 4, 2) else if ((ModRM & 0x38) == 0x38) CLKS(13, 13, 6) else CLKS(18, 18, 7)
			switch (ModRM & 0x38) {
			case 0x00: ADDB;            PutbackRMByte(ModRM, dst);   break;
			case 0x08: ORB;             PutbackRMByte(ModRM, dst);   break;
			case 0x10: src += CF; ADDB;   PutbackRMByte(ModRM, dst);   break;
			case 0x18: src += CF; SUBB;   PutbackRMByte(ModRM, dst);   break;
			case 0x20: ANDB;            PutbackRMByte(ModRM, dst);   break;
			case 0x28: SUBB;            PutbackRMByte(ModRM, dst);   break;
			case 0x30: XORB;            PutbackRMByte(ModRM, dst);   break;
			case 0x38: SUBB;            break;  /* CMP */
			}
	}; break;

	case 0x83:/*  i_83pre   ) */ { UINT32 dst, src; GetModRM; dst = GetRMWord(ModRM); src = (WORD)((INT16)((INT8)FETCH()));
		if (ModRM >= 0xc0) CLKS(4, 4, 2) else if ((ModRM & 0x38) == 0x38) CLKW(17, 17, 8, 17, 13, 6, m_EA) else CLKW(26, 26, 11, 26, 18, 7, m_EA)
			switch (ModRM & 0x38) {
			case 0x00: ADDW;            PutbackRMWord(ModRM, dst);   break;
			case 0x08: ORW;             PutbackRMWord(ModRM, dst);   break;
			case 0x10: src += CF; ADDW;   PutbackRMWord(ModRM, dst);   break;
			case 0x18: src += CF; SUBW;   PutbackRMWord(ModRM, dst);   break;
			case 0x20: ANDW;            PutbackRMWord(ModRM, dst);   break;
			case 0x28: SUBW;            PutbackRMWord(ModRM, dst);   break;
			case 0x30: XORW;            PutbackRMWord(ModRM, dst);   break;
			case 0x38: SUBW;            break;  /* CMP */
			}
	}; break;

	case 0x84:/*  i_test_br8  ) */ { DEF_br8;  ANDB;   CLKM(2, 2, 2, 10, 10, 6);        }; break;
	case 0x85:/*  i_test_wr16 ) */ { DEF_wr16; ANDW;   CLKR(14, 14, 8, 14, 10, 6, 2, m_EA); }; break;
	case 0x86:/*  i_xchg_br8  ) */ { DEF_br8;  RegByte(ModRM) = dst; PutbackRMByte(ModRM, src); CLKM(3, 3, 3, 16, 18, 8); }; break;
	case 0x87:/*  i_xchg_wr16 ) */ { DEF_wr16; RegWord(ModRM) = dst; PutbackRMWord(ModRM, src); CLKR(24, 24, 12, 24, 16, 8, 3, m_EA); }; break;

	case 0x88:/*  i_mov_br8   ) */ { UINT8  src; GetModRM; src = RegByte(ModRM);   PutRMByte(ModRM, src);   CLKM(2, 2, 2, 9, 9, 3);          }; break;
	case 0x89:/*  i_mov_wr16  ) */ { UINT16 src; GetModRM; src = RegWord(ModRM);   PutRMWord(ModRM, src);   CLKR(13, 13, 5, 13, 9, 3, 2, m_EA);  }; break;
	case 0x8a:/*  i_mov_r8b   ) */ { UINT8  src; GetModRM; src = GetRMByte(ModRM); RegByte(ModRM) = src;     CLKM(2, 2, 2, 11, 11, 5);        }; break;
	case 0x8b:/*  i_mov_r16w  ) */ { UINT16 src; GetModRM; src = GetRMWord(ModRM); RegWord(ModRM) = src;     CLKR(15, 15, 7, 15, 11, 5, 2, m_EA);     }; break;
	case 0x8c:/*  i_mov_wsreg ) */ { GetModRM;
		switch (ModRM & 0x38) {
		case 0x00: PutRMWord(ModRM, Sreg(DS1)); CLKR(14, 14, 5, 14, 10, 3, 2, m_EA); break;
		case 0x08: PutRMWord(ModRM, Sreg(PS)); CLKR(14, 14, 5, 14, 10, 3, 2, m_EA); break;
		case 0x10: PutRMWord(ModRM, Sreg(SS)); CLKR(14, 14, 5, 14, 10, 3, 2, m_EA); break;
		case 0x18: PutRMWord(ModRM, Sreg(DS0)); CLKR(14, 14, 5, 14, 10, 3, 2, m_EA); break;
		default:   logerror("%06x: MOV Sreg - Invalid register\n", PC());
		}
	}; break;
	case 0x8d:/*  i_lea       ) */ { UINT16 ModRM = FETCH(); (void)GET_EA(ModRM); RegWord(ModRM) = m_EO;  CLKS(4, 4, 2); }; break;
	case 0x8e:/*  i_mov_sregw ) */ { UINT16 src; GetModRM; src = GetRMWord(ModRM); CLKR(15, 15, 7, 15, 11, 5, 2, m_EA);
		switch (ModRM & 0x38) {
		case 0x00: Sreg(DS1) = src; break; /* mov es,ew */
		case 0x08: Sreg(PS) = src; break; /* mov cs,ew */
		case 0x10: Sreg(SS) = src; break; /* mov ss,ew */
		case 0x18: Sreg(DS0) = src; break; /* mov ds,ew */
		default:   logerror("%06x: MOV Sreg - Invalid register\n", PC());
		}
		m_no_interrupt = 1;
	}; break;
	case 0x8f:/*  i_popw ) */ { UINT16 tmp; GetModRM; POP(tmp); PutRMWord(ModRM, tmp); m_icount -= 21; }; break;
	case 0x90:/*  i_nop  ) */ { CLK(3); /* { if (m_MF == 0) printf("90 -> %06x: \n",PC()); }  */ }; break;
	case 0x91:/*  i_xchg_axcx ) */ { XchgAWReg(CW); CLK(3); }; break;
	case 0x92:/*  i_xchg_axdx ) */ { XchgAWReg(DW); CLK(3); }; break;
	case 0x93:/*  i_xchg_axbx ) */ { XchgAWReg(BW); CLK(3); }; break;
	case 0x94:/*  i_xchg_axsp ) */ { XchgAWReg(SP); CLK(3); }; break;
	case 0x95:/*  i_xchg_axbp ) */ { XchgAWReg(BP); CLK(3); }; break;
	case 0x96:/*  i_xchg_axsi ) */ { XchgAWReg(IX); CLK(3); }; break;
	case 0x97:/* , i_xchg_axdi ) */ { XchgAWReg(IY); CLK(3); }; break;

	case 0x98:/* , i_cbw       ) */ { Breg(AH) = (Breg(AL) & 0x80) ? 0xff : 0;      CLK(2); }; break;
	case 0x99:/* , i_cwd       ) */ { Wreg(DW) = (Breg(AH) & 0x80) ? 0xffff : 0;    CLK(4); }; break;
	case 0x9a:/* , i_call_far  ) */ { UINT32 tmp, tmp2; tmp = FETCHWORD(); tmp2 = FETCHWORD(); PUSH(Sreg(PS)); PUSH(m_ip); m_ip = (WORD)tmp; Sreg(PS) = (WORD)tmp2; CHANGE_PC; CLKW(29, 29, 13, 29, 21, 9, Wreg(SP)); }; break;
	case 0x9b:/* , i_wait      ) */ { if (!m_poll_state) m_ip--; CLK(5); }; break;
	case 0x9c:/* , i_pushf     ) */ { UINT16 tmp = CompressFlags(); PUSH(tmp); CLKS(12, 8, 3); }; break;
	case 0x9d:/* , i_popf      ) */ { UINT32 tmp; POP(tmp); ExpandFlags(tmp); CLKS(12, 8, 5); if (m_TF) nec_trap(); }; break;
	case 0x9e:/* , i_sahf      ) */ { UINT32 tmp = (CompressFlags() & 0xff00) | (Breg(AH) & 0xd5); ExpandFlags(tmp); CLKS(3, 3, 2); }; break;
	case 0x9f:/* , i_lahf      ) */ { Breg(AH) = CompressFlags() & 0xff; CLKS(3, 3, 2); }; break;

	case 0xa0:/* , i_mov_aldisp ) */ { UINT32 addr; addr = FETCHWORD(); Breg(AL) = GetMemB(DS0, addr); CLKS(10, 10, 5); }; break;
	case 0xa1:/* , i_mov_axdisp ) */ { UINT32 addr; addr = FETCHWORD(); Wreg(AW) = GetMemW(DS0, addr); CLKW(14, 14, 7, 14, 10, 5, addr); }; break;
	case 0xa2:/* , i_mov_dispal ) */ { UINT32 addr; addr = FETCHWORD(); PutMemB(DS0, addr, Breg(AL));  CLKS(9, 9, 3); }; break;
	case 0xa3:/* , i_mov_dispax ) */ { UINT32 addr; addr = FETCHWORD(); PutMemW(DS0, addr, Wreg(AW));  CLKW(13, 13, 5, 13, 9, 3, addr); }; break;
	case 0xa4:/* , i_movsb      ) */ { UINT32 tmp = GetMemB(DS0, Wreg(IX)); PutMemB(DS1, Wreg(IY), tmp); Wreg(IY) += -2 * m_DF + 1; Wreg(IX) += -2 * m_DF + 1; CLKS(8, 8, 6); }; break;
	case 0xa5:/* , i_movsw      ) */ { UINT32 tmp = GetMemW(DS0, Wreg(IX)); PutMemW(DS1, Wreg(IY), tmp); Wreg(IY) += -4 * m_DF + 2; Wreg(IX) += -4 * m_DF + 2; CLKS(16, 16, 10); }; break;
	case 0xa6:/* , i_cmpsb      ) */ { UINT32 src = D1GetMemB(Wreg(IY)); UINT32 dst = GetMemB(DS0, Wreg(IX)); SUBB; Wreg(IY) += -2 * m_DF + 1; Wreg(IX) += -2 * m_DF + 1; CLKS(14, 14, 14); }; break;
	case 0xa7:/* , i_cmpsw      ) */ { UINT32 src = GetMemW(DS1, Wreg(IY)); UINT32 dst = GetMemW(DS0, Wreg(IX)); SUBW; Wreg(IY) += -4 * m_DF + 2; Wreg(IX) += -4 * m_DF + 2; CLKS(14, 14, 14); }; break;

	case 0xa8:/* , i_test_ald8  ) */ { DEF_ald8;  ANDB; CLKS(4, 4, 2); }; break;
	case 0xa9:/* , i_test_axd16 ) */ { DEF_axd16; ANDW; CLKS(4, 4, 2); }; break;
	case 0xaa:/* , i_stosb      ) */ { PutMemB(DS1, Wreg(IY), Breg(AL));  Wreg(IY) += -2 * m_DF + 1; CLKS(4, 4, 3);  }; break;
	case 0xab:/* , i_stosw      ) */ { PutMemW(DS1, Wreg(IY), Wreg(AW));  Wreg(IY) += -4 * m_DF + 2; CLKW(8, 8, 5, 8, 4, 3, Wreg(IY)); }; break;
	case 0xac:/* , i_lodsb      ) */ { Breg(AL) = GetMemB(DS0, Wreg(IX)); Wreg(IX) += -2 * m_DF + 1; CLKS(4, 4, 3);  }; break;
	case 0xad:/* , i_lodsw      ) */ { Wreg(AW) = GetMemW(DS0, Wreg(IX)); Wreg(IX) += -4 * m_DF + 2; CLKW(8, 8, 5, 8, 4, 3, Wreg(IX)); }; break;
	case 0xae:/* , i_scasb      ) */ { UINT32 src = D1GetMemB(Wreg(IY)); UINT32 dst = Breg(AL); SUBB; Wreg(IY) += -2 * m_DF + 1; CLKS(4, 4, 3);  }; break;
	case 0xaf:/* , i_scasw      ) */ { UINT32 src = GetMemW(DS1, Wreg(IY)); UINT32 dst = Wreg(AW); SUBW; Wreg(IY) += -4 * m_DF + 2; CLKW(8, 8, 5, 8, 4, 3, Wreg(IY)); }; break;

	case 0xb0:/* , i_mov_ald8  ) */ { Breg(AL) = FETCH();   CLKS(4, 4, 2); }; break;
	case 0xb1:/* , i_mov_cld8  ) */ { Breg(CL) = FETCH(); CLKS(4, 4, 2); }; break;
	case 0xb2:/* , i_mov_dld8  ) */ { Breg(DL) = FETCH(); CLKS(4, 4, 2); }; break;
	case 0xb3:/* , i_mov_bld8  ) */ { Breg(BL) = FETCH(); CLKS(4, 4, 2); }; break;
	case 0xb4:/* , i_mov_ahd8  ) */ { Breg(AH) = FETCH(); CLKS(4, 4, 2); }; break;
	case 0xb5:/* , i_mov_chd8  ) */ { Breg(CH) = FETCH(); CLKS(4, 4, 2); }; break;
	case 0xb6:/* , i_mov_dhd8  ) */ { Breg(DH) = FETCH(); CLKS(4, 4, 2); }; break;
	case 0xb7:/* , i_mov_bhd8  ) */ { Breg(BH) = FETCH();   CLKS(4, 4, 2); }; break;

	case 0xb8:/* , i_mov_axd16 ) */ { Breg(AL) = FETCH();    Breg(AH) = FETCH();    CLKS(4, 4, 2); }; break;
	case 0xb9:/* , i_mov_cxd16 ) */ { Breg(CL) = FETCH();    Breg(CH) = FETCH();    CLKS(4, 4, 2); }; break;
	case 0xba:/* , i_mov_dxd16 ) */ { Breg(DL) = FETCH();    Breg(DH) = FETCH();    CLKS(4, 4, 2); }; break;
	case 0xbb:/* , i_mov_bxd16 ) */ { Breg(BL) = FETCH();    Breg(BH) = FETCH();    CLKS(4, 4, 2); }; break;
	case 0xbc:/* , i_mov_spd16 ) */ { Wreg(SP) = FETCHWORD();   CLKS(4, 4, 2); }; break;
	case 0xbd:/* , i_mov_bpd16 ) */ { Wreg(BP) = FETCHWORD();   CLKS(4, 4, 2); }; break;
	case 0xbe:/* , i_mov_sid16 ) */ { Wreg(IX) = FETCHWORD();   CLKS(4, 4, 2); }; break;
	case 0xbf:/* , i_mov_did16 ) */ { Wreg(IY) = FETCHWORD();   CLKS(4, 4, 2); }; break;

	case 0xc0:/* , i_rotshft_bd8 ) */ {
		UINT32 src, dst; UINT8 c;
		GetModRM; src = (unsigned)GetRMByte(ModRM); dst = src;
		c = FETCH();
		CLKM(7, 7, 2, 19, 19, 6);
		if (c) switch (ModRM & 0x38) {
		case 0x00: do { ROL_BYTE;  c--; CLK(1); } while (c > 0); PutbackRMByte(ModRM, (BYTE)dst); break;
		case 0x08: do { ROR_BYTE;  c--; CLK(1); } while (c > 0); PutbackRMByte(ModRM, (BYTE)dst); break;
		case 0x10: do { ROLC_BYTE; c--; CLK(1); } while (c > 0); PutbackRMByte(ModRM, (BYTE)dst); break;
		case 0x18: do { RORC_BYTE; c--; CLK(1); } while (c > 0); PutbackRMByte(ModRM, (BYTE)dst); break;
		case 0x20: SHL_BYTE(c); break;
		case 0x28: SHR_BYTE(c); break;
		case 0x30: logerror("%06x: Undefined opcode 0xc0 0x30 (SHLA)\n", PC()); break;
		case 0x38: SHRA_BYTE(c); break;
		}
	}; break;

	case 0xc1:/* , i_rotshft_wd8 ) */ {
		UINT32 src, dst;  UINT8 c;
		GetModRM; src = (unsigned)GetRMWord(ModRM); dst = src;
		c = FETCH();
		CLKM(7, 7, 2, 27, 19, 6);
		if (c) switch (ModRM & 0x38) {
		case 0x00: do { ROL_WORD;  c--; CLK(1); } while (c > 0); PutbackRMWord(ModRM, (WORD)dst); break;
		case 0x08: do { ROR_WORD;  c--; CLK(1); } while (c > 0); PutbackRMWord(ModRM, (WORD)dst); break;
		case 0x10: do { ROLC_WORD; c--; CLK(1); } while (c > 0); PutbackRMWord(ModRM, (WORD)dst); break;
		case 0x18: do { RORC_WORD; c--; CLK(1); } while (c > 0); PutbackRMWord(ModRM, (WORD)dst); break;
		case 0x20: SHL_WORD(c); break;
		case 0x28: SHR_WORD(c); break;
		case 0x30: logerror("%06x: Undefined opcode 0xc1 0x30 (SHLA)\n", PC()); break;
		case 0x38: SHRA_WORD(c); break;
		}
	}; break;

	case 0xc2:/* , i_ret_d16  ) */ { UINT32 count = FETCH(); count += FETCH() << 8; POP(m_ip); Wreg(SP) += count; CHANGE_PC; CLKS(24, 24, 10); }; break;
	case 0xc3:/* , i_ret      ) */ { POP(m_ip); CHANGE_PC; CLKS(19, 19, 10); }; break;
	case 0xc4:/* , i_les_dw   ) */ { GetModRM; WORD tmp = GetRMWord(ModRM); RegWord(ModRM) = tmp; Sreg(DS1) = GetnextRMWord; CLKW(26, 26, 14, 26, 18, 10, m_EA); }; break;
	case 0xc5:/* , i_lds_dw   ) */ { GetModRM; WORD tmp = GetRMWord(ModRM); RegWord(ModRM) = tmp; Sreg(DS0) = GetnextRMWord; CLKW(26, 26, 14, 26, 18, 10, m_EA); }; break;
	case 0xc6:/* , i_mov_bd8  ) */ { GetModRM; PutImmRMByte(ModRM); m_icount -= (ModRM >= 0xc0) ? 4 : 11; }; break;
	case 0xc7:/* , i_mov_wd16 ) */ { GetModRM; PutImmRMWord(ModRM); m_icount -= (ModRM >= 0xc0) ? 4 : 15; }; break;

	case 0xc8:/* , i_enter ) */ {
		UINT32 nb = FETCH();
		UINT32 i, level;

		m_icount -= 23;
		nb += FETCH() << 8;
		level = FETCH();
		PUSH(Wreg(BP));
		Wreg(BP) = Wreg(SP);
		Wreg(SP) -= nb;
		for (i = 1; i < level; i++) {
			PUSH(GetMemW(SS, Wreg(BP) - i * 2));
			m_icount -= 16;
		}
		if (level) PUSH(Wreg(BP));
	}; break;
	case 0xc9:/* , i_leave ) */ {
		Wreg(SP) = Wreg(BP);
		POP(Wreg(BP));
		m_icount -= 8;
	}; break;
	case 0xca:/* , i_retf_d16  ) */ { UINT32 count = FETCH(); count += FETCH() << 8; POP(m_ip); POP(Sreg(PS)); Wreg(SP) += count; CHANGE_PC; CLKS(32, 32, 16); }; break;
	case 0xcb:/* , i_retf      ) */ { POP(m_ip); POP(Sreg(PS)); CHANGE_PC; CLKS(29, 29, 16); }; break;
	case 0xcc:/* , i_int3      ) */ { nec_interrupt(3, BRK); CLKS(50, 50, 24); }; break;
	case 0xcd:/* , i_int       ) */ { nec_interrupt(FETCH(), BRK); CLKS(50, 50, 24); }; break;
	case 0xce:/* , i_into      ) */ { if (OF) { nec_interrupt(NEC_IREM_BRKV_VECTOR, BRK); CLKS(52, 52, 26); } else CLK(3); }; break;
	case 0xcf:/* , i_iret      ) */ { POP(m_ip); POP(Sreg(PS)); { UINT32 tmp; POP(tmp); ExpandFlags(tmp); CLKS(12, 8, 5); if (m_TF) nec_trap(); }; CHANGE_PC; CLKS(39, 39, 19); }; break;

	case 0xd0:/* , i_rotshft_b ) */ {
		UINT32 src, dst; GetModRM; src = (UINT32)GetRMByte(ModRM); dst = src;
		CLKM(6, 6, 2, 16, 16, 7);
		switch (ModRM & 0x38) {
		case 0x00: ROL_BYTE;  PutbackRMByte(ModRM, (BYTE)dst); m_OverVal = (src ^ dst) & 0x80; break;
		case 0x08: ROR_BYTE;  PutbackRMByte(ModRM, (BYTE)dst); m_OverVal = (src ^ dst) & 0x80; break;
		case 0x10: ROLC_BYTE; PutbackRMByte(ModRM, (BYTE)dst); m_OverVal = (src ^ dst) & 0x80; break;
		case 0x18: RORC_BYTE; PutbackRMByte(ModRM, (BYTE)dst); m_OverVal = (src ^ dst) & 0x80; break;
		case 0x20: SHL_BYTE(1); m_OverVal = (src ^ dst) & 0x80; break;
		case 0x28: SHR_BYTE(1); m_OverVal = (src ^ dst) & 0x80; break;
		case 0x30: logerror("%06x: Undefined opcode 0xd0 0x30 (SHLA)\n", PC()); break;
		case 0x38: SHRA_BYTE(1); m_OverVal = 0; break;
		}
	}; break;

	case 0xd1:/* , i_rotshft_w ) */ {
		UINT32 src, dst; GetModRM; src = (UINT32)GetRMWord(ModRM); dst = src;
		CLKM(6, 6, 2, 24, 16, 7);
		switch (ModRM & 0x38) {
		case 0x00: ROL_WORD;  PutbackRMWord(ModRM, (WORD)dst); m_OverVal = (src ^ dst) & 0x8000; break;
		case 0x08: ROR_WORD;  PutbackRMWord(ModRM, (WORD)dst); m_OverVal = (src ^ dst) & 0x8000; break;
		case 0x10: ROLC_WORD; PutbackRMWord(ModRM, (WORD)dst); m_OverVal = (src ^ dst) & 0x8000; break;
		case 0x18: RORC_WORD; PutbackRMWord(ModRM, (WORD)dst); m_OverVal = (src ^ dst) & 0x8000; break;
		case 0x20: SHL_WORD(1); m_OverVal = (src ^ dst) & 0x8000;  break;
		case 0x28: SHR_WORD(1); m_OverVal = (src ^ dst) & 0x8000;  break;
		case 0x30: logerror("%06x: Undefined opcode 0xd1 0x30 (SHLA)\n", PC()); break;
		case 0x38: SHRA_WORD(1); m_OverVal = 0; break;
		}
	}; break;

	case 0xd2:/* , i_rotshft_bcl ) */ {
		UINT32 src, dst; UINT8 c; GetModRM; src = (UINT32)GetRMByte(ModRM); dst = src;
		c = Breg(CL);
		CLKM(7, 7, 2, 19, 19, 6);
		if (c) switch (ModRM & 0x38) {
		case 0x00: do { ROL_BYTE;  c--; CLK(1); } while (c > 0); PutbackRMByte(ModRM, (BYTE)dst); break;
		case 0x08: do { ROR_BYTE;  c--; CLK(1); } while (c > 0); PutbackRMByte(ModRM, (BYTE)dst); break;
		case 0x10: do { ROLC_BYTE; c--; CLK(1); } while (c > 0); PutbackRMByte(ModRM, (BYTE)dst); break;
		case 0x18: do { RORC_BYTE; c--; CLK(1); } while (c > 0); PutbackRMByte(ModRM, (BYTE)dst); break;
		case 0x20: SHL_BYTE(c); break;
		case 0x28: SHR_BYTE(c); break;
		case 0x30: logerror("%06x: Undefined opcode 0xd2 0x30 (SHLA)\n", PC()); break;
		case 0x38: SHRA_BYTE(c); break;
		}
	}; break;

	case 0xd3:/* , i_rotshft_wcl ) */ {
		UINT32 src, dst; UINT8 c; GetModRM; src = (UINT32)GetRMWord(ModRM); dst = src;
		c = Breg(CL);
		CLKM(7, 7, 2, 27, 19, 6);
		if (c) switch (ModRM & 0x38) {
		case 0x00: do { ROL_WORD;  c--; CLK(1); } while (c > 0); PutbackRMWord(ModRM, (WORD)dst); break;
		case 0x08: do { ROR_WORD;  c--; CLK(1); } while (c > 0); PutbackRMWord(ModRM, (WORD)dst); break;
		case 0x10: do { ROLC_WORD; c--; CLK(1); } while (c > 0); PutbackRMWord(ModRM, (WORD)dst); break;
		case 0x18: do { RORC_WORD; c--; CLK(1); } while (c > 0); PutbackRMWord(ModRM, (WORD)dst); break;
		case 0x20: SHL_WORD(c); break;
		case 0x28: SHR_WORD(c); break;
		case 0x30: logerror("%06x: Undefined opcode 0xd3 0x30 (SHLA)\n", PC()); break;
		case 0x38: SHRA_WORD(c); break;
		}
	}; break;

	case 0xd4:/* , i_aam    ) */ { FETCH(); Breg(AH) = Breg(AL) / 10; Breg(AL) %= 10; SetSZPF_Word(Wreg(AW)); CLKS(15, 15, 12); }; break;
	case 0xd5:/* , i_aad    ) */ { FETCH(); Breg(AL) = Breg(AH) * 10 + Breg(AL); Breg(AH) = 0; SetSZPF_Byte(Breg(AL)); CLKS(7, 7, 8); }; break;
	case 0xd6:/* , i_setalc ) */ { Breg(AL) = (CF) ? 0xff : 0x00; m_icount -= 3; logerror("%06x: Undefined opcode (SETALC)\n", PC()); }; break;
	case 0xd7:/* , i_trans  ) */ { UINT32 dest = (Wreg(BW) + Breg(AL)) & 0xffff; Breg(AL) = GetMemB(DS0, dest); CLKS(9, 9, 5); }; break;
	case 0xd8:/* , i_fpo    ) */ { GetModRM; GetRMByte(ModRM); m_icount -= 2;  logerror("%06x: Unimplemented floating point control %04x\n", PC(), ModRM); }; break;

	case 0xe0:/* , i_loopne ) */ { INT8 disp = (INT8)FETCH(); Wreg(CW)--; if (!ZF && Wreg(CW)) { m_ip = (WORD)(m_ip + disp); /*CHANGE_PC;*/ CLKS(14, 14, 6); } else CLKS(5, 5, 3); }; break;
	case 0xe1:/* , i_loope  ) */ { INT8 disp = (INT8)FETCH(); Wreg(CW)--; if (ZF && Wreg(CW)) { m_ip = (WORD)(m_ip + disp); /*CHANGE_PC;*/ CLKS(14, 14, 6); } else CLKS(5, 5, 3); }; break;
	case 0xe2:/* , i_loop   ) */ { INT8 disp = (INT8)FETCH(); Wreg(CW)--; if (Wreg(CW)) { m_ip = (WORD)(m_ip + disp); /*CHANGE_PC;*/ CLKS(13, 13, 6); } else CLKS(5, 5, 3); }; break;
	case 0xe3:/* , i_jcxz   ) */ { INT8 disp = (INT8)FETCH(); if (Wreg(CW) == 0) { m_ip = (WORD)(m_ip + disp); /*CHANGE_PC;*/ CLKS(13, 13, 6); } else CLKS(5, 5, 3); }; break;
	case 0xe4:/* , i_inal   ) */ { UINT8 port = FETCH(); Breg(AL) = read_port_byte(port); CLKS(9, 9, 5);  }; break;
	case 0xe5:/* , i_inax   ) */ { UINT8 port = FETCH(); Wreg(AW) = read_port_word(port); CLKW(13, 13, 7, 13, 9, 5, port); }; break;
	case 0xe6:/* , i_outal  ) */ { UINT8 port = FETCH(); write_port_byte(port, Breg(AL)); CLKS(8, 8, 3);  }; break;
	case 0xe7:/* , i_outax  ) */ { UINT8 port = FETCH(); write_port_word(port, Wreg(AW)); CLKW(12, 12, 5, 12, 8, 3, port);    }; break;

	case 0xe8:/* , i_call_d16 ) */ { UINT32 tmp; tmp = FETCHWORD(); PUSH(m_ip); m_ip = (WORD)(m_ip + (INT16)tmp); CHANGE_PC; m_icount -= 24; }; break;
	case 0xe9:/* , i_jmp_d16  ) */ { UINT32 tmp; tmp = FETCHWORD(); m_ip = (WORD)(m_ip + (INT16)tmp); CHANGE_PC; m_icount -= 15; }; break;
	case 0xea:/* , i_jmp_far  ) */ { UINT32 tmp, tmp1; tmp = FETCHWORD(); tmp1 = FETCHWORD(); Sreg(PS) = (WORD)tmp1;     m_ip = (WORD)tmp; CHANGE_PC; m_icount -= 27;  }; break;
	case 0xeb:/* , i_jmp_d8   ) */ { int tmp = (int)((INT8)FETCH()); m_icount -= 12; m_ip = (WORD)(m_ip + tmp); }; break;
	case 0xec:/* , i_inaldx   ) */ { Breg(AL) = read_port_byte(Wreg(DW)); CLKS(8, 8, 5); }; break;
	case 0xed:/* , i_inaxdx   ) */ { Wreg(AW) = read_port_word(Wreg(DW)); CLKW(12, 12, 7, 12, 8, 5, Wreg(DW)); }; break;
	case 0xee:/* , i_outdxal  ) */ { write_port_byte(Wreg(DW), Breg(AL)); CLKS(8, 8, 3);  }; break;
	case 0xef:/* , i_outdxax  ) */ { write_port_word(Wreg(DW), Wreg(AW)); CLKW(12, 12, 5, 12, 8, 3, Wreg(DW)); }; break;

	case 0xf0:/* , i_lock     ) */ { logerror("%06x: Warning - BUSLOCK\n", PC()); m_no_interrupt = 1; CLK(2); }; break;
	case 0xf2:/* , i_repne    ) */ { UINT32 next = fetchop(); UINT16 c = Wreg(CW);
		switch (next) { /* Segments */
		case 0x26:  m_seg_prefix = TRUE; m_prefix_base = Sreg(DS1) << 4;    next = fetchop();  CLK(2); break;
		case 0x2e:  m_seg_prefix = TRUE; m_prefix_base = Sreg(PS) << 4;     next = fetchop();  CLK(2); break;
		case 0x36:  m_seg_prefix = TRUE; m_prefix_base = Sreg(SS) << 4;     next = fetchop();  CLK(2); break;
		case 0x3e:  m_seg_prefix = TRUE; m_prefix_base = Sreg(DS0) << 4;    next = fetchop();  CLK(2); break;
		}

		switch (next) {
		case 0x6c:  CLK(2); if (c) do { { PutMemB(DS1, Wreg(IY), read_port_byte(Wreg(DW))); Wreg(IY) += -2 * m_DF + 1; CLK(8); };  c--; } while (c > 0); Wreg(CW) = c; break;
		case 0x6d:  CLK(2); if (c) do { { PutMemW(DS1, Wreg(IY), read_port_word(Wreg(DW))); Wreg(IY) += -4 * m_DF + 2; CLKS(18, 10, 8); };  c--; } while (c > 0); Wreg(CW) = c; break;
		case 0x6e:  CLK(2); if (c) do { { write_port_byte(Wreg(DW), GetMemB(DS0, Wreg(IX))); Wreg(IX) += -2 * m_DF + 1; CLK(8); }; c--; } while (c > 0); Wreg(CW) = c; break;
		case 0x6f:  CLK(2); if (c) do { { write_port_word(Wreg(DW), GetMemW(DS0, Wreg(IX))); Wreg(IX) += -4 * m_DF + 2; CLKS(18, 10, 8); }; c--; } while (c > 0); Wreg(CW) = c; break;
		case 0xa4:  CLK(2); if (c) do { { UINT32 tmp = GetMemB(DS0, Wreg(IX)); PutMemB(DS1, Wreg(IY), tmp); Wreg(IY) += -2 * m_DF + 1; Wreg(IX) += -2 * m_DF + 1; CLKS(8, 8, 6); }; c--; } while (c > 0); Wreg(CW) = c; break;
		case 0xa5:  CLK(2); if (c) do { { UINT32 tmp = GetMemW(DS0, Wreg(IX)); PutMemW(DS1, Wreg(IY), tmp); Wreg(IY) += -4 * m_DF + 2; Wreg(IX) += -4 * m_DF + 2; CLKS(16, 16, 10); }; c--; } while (c > 0); Wreg(CW) = c; break;
		case 0xa6:  CLK(2); if (c) do { { UINT32 src = D1GetMemB(Wreg(IY)); UINT32 dst = GetMemB(DS0, Wreg(IX)); SUBB; Wreg(IY) += -2 * m_DF + 1; Wreg(IX) += -2 * m_DF + 1; CLKS(14, 14, 14); }; c--; } while (c > 0 && !ZF);    Wreg(CW) = c; break;
		case 0xa7:  CLK(2); if (c) do { { UINT32 src = GetMemW(DS1, Wreg(IY)); UINT32 dst = GetMemW(DS0, Wreg(IX)); SUBW; Wreg(IY) += -4 * m_DF + 2; Wreg(IX) += -4 * m_DF + 2; CLKS(14, 14, 14); }; c--; } while (c > 0 && !ZF);    Wreg(CW) = c; break;
		case 0xaa:  CLK(2); if (c) do { { PutMemB(DS1, Wreg(IY), Breg(AL));  Wreg(IY) += -2 * m_DF + 1; CLKS(4, 4, 3);  }; c--; } while (c > 0); Wreg(CW) = c; break;
		case 0xab:  CLK(2); if (c) do { { PutMemW(DS1, Wreg(IY), Wreg(AW));  Wreg(IY) += -4 * m_DF + 2; CLKW(8, 8, 5, 8, 4, 3, Wreg(IY)); }; c--; } while (c > 0); Wreg(CW) = c; break;
		case 0xac:  CLK(2); if (c) do { { Breg(AL) = GetMemB(DS0, Wreg(IX)); Wreg(IX) += -2 * m_DF + 1; CLKS(4, 4, 3);  }; c--; } while (c > 0); Wreg(CW) = c; break;
		case 0xad:  CLK(2); if (c) do { { Wreg(AW) = GetMemW(DS0, Wreg(IX)); Wreg(IX) += -4 * m_DF + 2; CLKW(8, 8, 5, 8, 4, 3, Wreg(IX)); }; c--; } while (c > 0); Wreg(CW) = c; break;
		case 0xae:  CLK(2); if (c) do { { UINT32 src = D1GetMemB(Wreg(IY)); UINT32 dst = Breg(AL); SUBB; Wreg(IY) += -2 * m_DF + 1; CLKS(4, 4, 3);  }; c--; } while (c > 0 && !ZF);    Wreg(CW) = c; break;
		case 0xaf:  CLK(2); if (c) do { { UINT32 src = GetMemW(DS1, Wreg(IY)); UINT32 dst = Wreg(AW); SUBW; Wreg(IY) += -4 * m_DF + 2; CLKW(8, 8, 5, 8, 4, 3, Wreg(IY)); }; c--; } while (c > 0 && !ZF);    Wreg(CW) = c; break;
		default:    logerror("%06x: REPNE invalid\n", PC());    do_opcode(next);
		}
		m_seg_prefix = FALSE;
	}; break;
	case 0xf3:/* , i_repe     ) */ { UINT32 next = fetchop(); UINT16 c = Wreg(CW);
		switch (next) { /* Segments */
		case 0x26:  m_seg_prefix = TRUE; m_prefix_base = Sreg(DS1) << 4;    next = fetchop();  CLK(2); break;
		case 0x2e:  m_seg_prefix = TRUE; m_prefix_base = Sreg(PS) << 4; next = fetchop();  CLK(2); break;
		case 0x36:  m_seg_prefix = TRUE; m_prefix_base = Sreg(SS) << 4; next = fetchop();  CLK(2); break;
		case 0x3e:  m_seg_prefix = TRUE; m_prefix_base = Sreg(DS0) << 4;    next = fetchop();  CLK(2); break;
		}

		switch (next) {
		case 0x6c:  CLK(2); if (c) do { { PutMemB(DS1, Wreg(IY), read_port_byte(Wreg(DW))); Wreg(IY) += -2 * m_DF + 1; CLK(8); };  c--; } while (c > 0); Wreg(CW) = c; break;
		case 0x6d:  CLK(2); if (c) do { { PutMemW(DS1, Wreg(IY), read_port_word(Wreg(DW))); Wreg(IY) += -4 * m_DF + 2; CLKS(18, 10, 8); };  c--; } while (c > 0); Wreg(CW) = c; break;
		case 0x6e:  CLK(2); if (c) do { { write_port_byte(Wreg(DW), GetMemB(DS0, Wreg(IX))); Wreg(IX) += -2 * m_DF + 1; CLK(8); }; c--; } while (c > 0); Wreg(CW) = c; break;
		case 0x6f:  CLK(2); if (c) do { { write_port_word(Wreg(DW), GetMemW(DS0, Wreg(IX))); Wreg(IX) += -4 * m_DF + 2; CLKS(18, 10, 8); }; c--; } while (c > 0); Wreg(CW) = c; break;
		case 0xa4:  CLK(2); if (c) do { { UINT32 tmp = GetMemB(DS0, Wreg(IX)); PutMemB(DS1, Wreg(IY), tmp); Wreg(IY) += -2 * m_DF + 1; Wreg(IX) += -2 * m_DF + 1; CLKS(8, 8, 6); }; c--; } while (c > 0); Wreg(CW) = c; break;
		case 0xa5:  CLK(2); if (c) do { { UINT32 tmp = GetMemW(DS0, Wreg(IX)); PutMemW(DS1, Wreg(IY), tmp); Wreg(IY) += -4 * m_DF + 2; Wreg(IX) += -4 * m_DF + 2; CLKS(16, 16, 10); }; c--; } while (c > 0); Wreg(CW) = c; break;
		case 0xa6:  CLK(2); if (c) do { { UINT32 src = D1GetMemB(Wreg(IY)); UINT32 dst = GetMemB(DS0, Wreg(IX)); SUBB; Wreg(IY) += -2 * m_DF + 1; Wreg(IX) += -2 * m_DF + 1; CLKS(14, 14, 14); }; c--; } while (c > 0 && ZF);    Wreg(CW) = c; break;
		case 0xa7:  CLK(2); if (c) do { { UINT32 src = GetMemW(DS1, Wreg(IY)); UINT32 dst = GetMemW(DS0, Wreg(IX)); SUBW; Wreg(IY) += -4 * m_DF + 2; Wreg(IX) += -4 * m_DF + 2; CLKS(14, 14, 14); }; c--; } while (c > 0 && ZF);    Wreg(CW) = c; break;
		case 0xaa:  CLK(2); if (c) do { { PutMemB(DS1, Wreg(IY), Breg(AL));  Wreg(IY) += -2 * m_DF + 1; CLKS(4, 4, 3);  }; c--; } while (c > 0); Wreg(CW) = c; break;
		case 0xab:  CLK(2); if (c) do { { PutMemW(DS1, Wreg(IY), Wreg(AW));  Wreg(IY) += -4 * m_DF + 2; CLKW(8, 8, 5, 8, 4, 3, Wreg(IY)); }; c--; } while (c > 0); Wreg(CW) = c; break;
		case 0xac:  CLK(2); if (c) do { { Breg(AL) = GetMemB(DS0, Wreg(IX)); Wreg(IX) += -2 * m_DF + 1; CLKS(4, 4, 3);  }; c--; } while (c > 0); Wreg(CW) = c; break;
		case 0xad:  CLK(2); if (c) do { { Wreg(AW) = GetMemW(DS0, Wreg(IX)); Wreg(IX) += -4 * m_DF + 2; CLKW(8, 8, 5, 8, 4, 3, Wreg(IX)); }; c--; } while (c > 0); Wreg(CW) = c; break;
		case 0xae:  CLK(2); if (c) do { { UINT32 src = D1GetMemB(Wreg(IY)); UINT32 dst = Breg(AL); SUBB; Wreg(IY) += -2 * m_DF + 1; CLKS(4, 4, 3);  }; c--; } while (c > 0 && ZF);    Wreg(CW) = c; break;
		case 0xaf:  CLK(2); if (c) do { { UINT32 src = GetMemW(DS1, Wreg(IY)); UINT32 dst = Wreg(AW); SUBW; Wreg(IY) += -4 * m_DF + 2; CLKW(8, 8, 5, 8, 4, 3, Wreg(IY)); }; c--; } while (c > 0 && ZF);    Wreg(CW) = c; break;
		default:    logerror("%06x: REPE invalid\n", PC()); 	do_opcode(next);

		}
		m_seg_prefix = FALSE;
	}; break;
	case 0xf4:/* , i_hlt ) */ { logerror("%06x: HALT\n", PC()); m_halted = 1; m_icount = 0; }; break;
	case 0xf5:/* , i_cmc ) */ { m_CarryVal = !CF; CLK(2); }; break;
	case 0xf6:/* , i_f6pre ) */ { UINT32 tmp; UINT32 uresult, uresult2; INT32 result, result2;
		GetModRM; tmp = GetRMByte(ModRM);
		switch (ModRM & 0x38) {
		case 0x00: tmp &= FETCH(); m_CarryVal = m_OverVal = 0; SetSZPF_Byte(tmp); m_icount -= (ModRM >= 0xc0) ? 4 : 11; break; /* TEST */
		case 0x08: logerror("%06x: Undefined opcode 0xf6 0x08\n", PC()); break;
		case 0x10: PutbackRMByte(ModRM, ~tmp); m_icount -= (ModRM >= 0xc0) ? 2 : 16; break; /* NOT */
		case 0x18: m_CarryVal = (tmp != 0); tmp = (~tmp) + 1; SetSZPF_Byte(tmp); PutbackRMByte(ModRM, tmp & 0xff); m_icount -= (ModRM >= 0xc0) ? 2 : 16; break; /* NEG */
		case 0x20: uresult = Breg(AL) * tmp; Wreg(AW) = (WORD)uresult; m_CarryVal = m_OverVal = (Breg(AH) != 0); m_icount -= (ModRM >= 0xc0) ? 30 : 36; break; /* MULU */
		case 0x28: result = (INT16)((INT8)Breg(AL)) * (INT16)((INT8)tmp); Wreg(AW) = (WORD)result; m_CarryVal = m_OverVal = (Breg(AH) != 0); m_icount -= (ModRM >= 0xc0) ? 30 : 36; break; /* MUL */
		case 0x30: if (tmp) { DIVUB; }
				 else nec_interrupt(NEC_IREM_DIVIDE_VECTOR, BRK); m_icount -= (ModRM >= 0xc0) ? 43 : 53; break;
		case 0x38: if (tmp) { DIVB; }
				 else nec_interrupt(NEC_IREM_DIVIDE_VECTOR, BRK); m_icount -= (ModRM >= 0xc0) ? 43 : 53; break;
		}
	}; break;

	case 0xf7:/* , i_f7pre   ) */ { UINT32 tmp, tmp2; UINT32 uresult, uresult2; INT32 result, result2;
		GetModRM; tmp = GetRMWord(ModRM);
		switch (ModRM & 0x38) {
		case 0x00: tmp2 = FETCHWORD(); tmp &= tmp2; m_CarryVal = m_OverVal = 0; SetSZPF_Word(tmp); m_icount -= (ModRM >= 0xc0) ? 4 : 11; break; /* TEST */
		case 0x08: logerror("%06x: Undefined opcode 0xf7 0x08\n", PC()); break;
		case 0x10: PutbackRMWord(ModRM, ~tmp); m_icount -= (ModRM >= 0xc0) ? 2 : 16; break; /* NOT */
		case 0x18: m_CarryVal = (tmp != 0); tmp = (~tmp) + 1; SetSZPF_Word(tmp); PutbackRMWord(ModRM, tmp & 0xffff); m_icount -= (ModRM >= 0xc0) ? 2 : 16; break; /* NEG */
		case 0x20: uresult = Wreg(AW) * tmp; Wreg(AW) = uresult & 0xffff; Wreg(DW) = ((UINT32)uresult) >> 16; m_CarryVal = m_OverVal = (Wreg(DW) != 0); m_icount -= (ModRM >= 0xc0) ? 30 : 36; break; /* MULU */
		case 0x28: result = (INT32)((INT16)Wreg(AW)) * (INT32)((INT16)tmp); Wreg(AW) = result & 0xffff; Wreg(DW) = result >> 16; m_CarryVal = m_OverVal = (Wreg(DW) != 0); m_icount -= (ModRM >= 0xc0) ? 30 : 36; break; /* MUL */
		case 0x30: if (tmp) { DIVUW; }
				 else nec_interrupt(NEC_IREM_DIVIDE_VECTOR, BRK); m_icount -= (ModRM >= 0xc0) ? 43 : 53; break;
		case 0x38: if (tmp) { DIVW; }
				 else nec_interrupt(NEC_IREM_DIVIDE_VECTOR, BRK); m_icount -= (ModRM >= 0xc0) ? 43 : 53; break;
		}
	}; break;

	case 0xf8:/* , i_clc   ) */ { m_CarryVal = 0;  CLK(2); }; break;
	case 0xf9:/* , i_stc   ) */ { m_CarryVal = 1;  CLK(2); }; break;
	case 0xfa:/* , i_di    ) */ { SetIF(0);         CLK(2); }; break;
	case 0xfb:/* , i_ei    ) */ { SetIF(1);         CLK(2); }; break;
	case 0xfc:/* , i_cld   ) */ { SetDF(0);         CLK(2); }; break;
	case 0xfd:/* , i_std   ) */ { SetDF(1);         CLK(2); }; break;
	case 0xfe:/* , i_fepre ) */ { UINT32 tmp, tmp1; GetModRM; tmp = GetRMByte(ModRM);
		switch (ModRM & 0x38) {
		case 0x00: tmp1 = tmp + 1; m_OverVal = (tmp == 0x7f); SetAF(tmp1, tmp, 1); SetSZPF_Byte(tmp1); PutbackRMByte(ModRM, (BYTE)tmp1); CLKM(2, 2, 2, 16, 16, 7); break; /* INC */
		case 0x08: tmp1 = tmp - 1; m_OverVal = (tmp == 0x80); SetAF(tmp1, tmp, 1); SetSZPF_Byte(tmp1); PutbackRMByte(ModRM, (BYTE)tmp1); CLKM(2, 2, 2, 16, 16, 7); break; /* DEC */
		default:   logerror("%06x: FE Pre with unimplemented mod\n", PC());
		}
	}; break;
	case 0xff:/* , i_ffpre ) */ { UINT32 tmp, tmp1; GetModRM; tmp = GetRMWord(ModRM);
		switch (ModRM & 0x38) {
		case 0x00: tmp1 = tmp + 1; m_OverVal = (tmp == 0x7fff); SetAF(tmp1, tmp, 1); SetSZPF_Word(tmp1); PutbackRMWord(ModRM, (WORD)tmp1); CLKM(2, 2, 2, 24, 16, 7); break; /* INC */
		case 0x08: tmp1 = tmp - 1; m_OverVal = (tmp == 0x8000); SetAF(tmp1, tmp, 1); SetSZPF_Word(tmp1); PutbackRMWord(ModRM, (WORD)tmp1); CLKM(2, 2, 2, 24, 16, 7); break; /* DEC */
		case 0x10: PUSH(m_ip); m_ip = (WORD)tmp; CHANGE_PC; m_icount -= (ModRM >= 0xc0) ? 16 : 20; break; /* CALL */
		case 0x18: tmp1 = Sreg(PS); Sreg(PS) = GetnextRMWord; PUSH(tmp1); PUSH(m_ip); m_ip = tmp; CHANGE_PC; m_icount -= (ModRM >= 0xc0) ? 16 : 26; break; /* CALL FAR */
		case 0x20: m_ip = tmp; CHANGE_PC; m_icount -= 13; break; /* JMP */
		case 0x28: m_ip = tmp; Sreg(PS) = GetnextRMWord; CHANGE_PC; m_icount -= 15; break; /* JMP FAR */
		case 0x30: PUSH(tmp); m_icount -= 4; break;
		default:   logerror("%06x: FF Pre with unimplemented mod\n", PC());
		}
	}; break;


	default:
	{
		m_icount -= 10;
		logerror("%06x: Invalid Opcode\n", PC());
	}; break;
	}
}

#if 0
void nec_common_irem_device::i_invalid()
{
	m_icount-=10;
	logerror("%06x: Invalid Opcode\n",PC());
}
#endif
