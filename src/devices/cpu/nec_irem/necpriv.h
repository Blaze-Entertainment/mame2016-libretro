// license:BSD-3-Clause
// copyright-holders:Bryan McPhail
/* Cpu types, steps of 8 to help the cycle count calculation */
#define V33IREM_TYPE 0
#define V30IREM_TYPE 8
#define V20IREM_TYPE 16



/************************************************************************/

#define read_mem_byte(a)			m_program->read_byte(a)
#define read_mem_word(a)            m_program->read_word_unaligned(a)
#define write_mem_byte(a,d)         m_program->write_byte((a),(d))
#define write_mem_word(a,d)         m_program->write_word_unaligned((a),(d))

#define read_port_byte(a)       m_io->read_byte(a)
#define read_port_word(a)       m_io->read_word_unaligned(a)
#define write_port_byte(a,d)    m_io->write_byte((a),(d))
#define write_port_word(a,d)    m_io->write_word_unaligned((a),(d))

/************************************************************************/

#define CHANGE_PC do { EMPTY_PREFETCH(); } while (0)

#define SegBase(Seg) (Sreg(Seg) << 4)


#define GetMemB(Seg,Off) (read_mem_byte(DefaultBase(Seg) + (Off)))
#define GetMemW(Seg,Off) (read_mem_word(DefaultBase(Seg) + (Off)))

#define PutMemB(Seg,Off,x) { write_mem_byte(DefaultBase(Seg) + (Off), (x)); }
#define PutMemW(Seg,Off,x) { write_mem_word(DefaultBase(Seg) + (Off), (x)); }



/* prefetch timing */


#define EMPTY_PREFETCH()    m_prefetch_reset = 1


#define PUSH(val) { Wreg(SP) -= 2; write_mem_word(((Sreg(SS)<<4)+Wreg(SP)), val); }
#define POP(var) { Wreg(SP) += 2; var = read_mem_word(((Sreg(SS)<<4) + ((Wreg(SP)-2) & 0xffff))); }

#define GetModRM UINT32 ModRM=FETCH()

/* Cycle count macros:
    CLK  - cycle count is the same on all processors
    CLKS - cycle count differs between processors, list all counts
    CLKW - cycle count for word read/write differs for odd/even source/destination address
    CLKM - cycle count for reg/mem instructions
    CLKR - cycle count for reg/mem instructions with different counts for odd/even addresses


    Prefetch & buswait time is not emulated.
    Extra cycles for PUSH'ing or POP'ing registers to odd addresses is not emulated.
*/

#define CLK(all) m_icount-=all
#define CLKS(v20,v30,v33) { const UINT32 ccount=v33; m_icount-=(ccount); }
#define CLKW(v20o,v30o,v33o,v20e,v30e,v33e,addr) { const UINT32 ocount=v33o, ecount=v33e; m_icount-=(addr&1)?((ocount)):((ecount)); }
#define CLKM(v20,v30,v33,v20m,v30m,v33m) { const UINT32 ccount=v33, mcount=v33m; m_icount-=( ModRM >=0xc0 )?((ccount)):((mcount)); }
#define CLKR(v20o,v30o,v33o,v20e,v30e,v33e,vall,addr) { const UINT32 ocount=v33o, ecount=v33e; if (ModRM >=0xc0) m_icount-=vall; else m_icount-=(addr&1)?((ocount)):((ecount)); }

/************************************************************************/
#define CompressFlags() (WORD)(int(CF) | 0x02 | (int(PF) << 2) | (int(AF) << 4) | (int(ZF) << 6) \
				| (int(SF) << 7) | (m_TF << 8) | (m_IF << 9) \
				| (m_DF << 10) | (int(OF) << 11) | 0x7000 | (m_MF << 15))

#define ExpandFlags(f) \
{ \
	m_CarryVal = (f) & 0x0001; \
	m_ParityVal = !((f) & 0x0004); \
	m_AuxVal = (f) & 0x0010; \
	m_ZeroVal = !((f) & 0x0040); \
	m_SignVal = (f) & 0x0080 ? -1 : 0; \
	m_TF = ((f) & 0x0100) == 0x0100; \
	m_IF = ((f) & 0x0200) == 0x0200; \
	m_DF = ((f) & 0x0400) == 0x0400; \
	m_OverVal = (f) & 0x0800; \
	m_MF = ((f) & 0x8000) == 0x8000; \
}
