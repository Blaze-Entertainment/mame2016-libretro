// license:BSD-3-Clause
// copyright-holders:Bryan McPhail

UINT8 reg_b[256];

UINT8 RM_b[256];

#define RegWord(ModRM) Wreg(((ModRM&0x38)>>3))
#define RegByte(ModRM) Breg(reg_b[ModRM])

#define GetRMWord(ModRM) \
	((ModRM) >= 0xc0 ? Wreg((ModRM&7)) : ( GET_EA(ModRM), read_mem_word( m_EA ) ))

#define PutbackRMWord(ModRM,val)                 \
{                                \
	if (ModRM >= 0xc0) Wreg((ModRM&7))=val; \
	else write_mem_word(m_EA,val);  \
}

#define GetnextRMWord read_mem_word((m_EA&0xf0000)|((m_EA+2)&0xffff))

#define PutRMWord(ModRM,val)                \
{                           \
	if (ModRM >= 0xc0)              \
		Wreg((ModRM&7))=val;   \
	else {                      \
		GET_EA(ModRM);         \
		write_mem_word( m_EA ,val);           \
	}                       \
}

#define PutImmRMWord(ModRM)                 \
{                           \
	WORD val;                   \
	if (ModRM >= 0xc0)              \
		Wreg((ModRM&7)) = FETCHWORD(); \
	else {                      \
		GET_EA(ModRM);         \
		val = FETCHWORD();              \
		write_mem_word( m_EA , val);          \
	}                       \
}

#define GetRMByte(ModRM) \
	((ModRM) >= 0xc0 ? Breg(RM_b[ModRM]) : read_mem_byte( GET_EA(ModRM) ))

#define PutRMByte(ModRM,val)                \
{                           \
	if (ModRM >= 0xc0)              \
		Breg(RM_b[ModRM])=val;   \
	else                        \
		write_mem_byte( GET_EA(ModRM) ,val);   \
}

#define PutImmRMByte(ModRM)                 \
{                           \
	if (ModRM >= 0xc0)              \
		Breg(RM_b[ModRM])=FETCH();   \
	else {                      \
		GET_EA(ModRM);         \
		write_mem_byte( m_EA , FETCH() );     \
	}                       \
}

#define PutbackRMByte(ModRM,val)            \
{                           \
	if (ModRM >= 0xc0)              \
		Breg(RM_b[ModRM])=val;   \
	else                        \
		write_mem_byte(m_EA,val);         \
}

#define DEF_br8                         \
	UINT32 ModRM = FETCH(),src,dst;     \
	src = RegByte(ModRM);               \
	dst = GetRMByte(ModRM)

#define DEF_wr16                        \
	UINT32 ModRM = FETCH(),src,dst;     \
	src = RegWord(ModRM);               \
	dst = GetRMWord(ModRM)

#define DEF_r8b                         \
	UINT32 ModRM = FETCH(),src,dst;     \
	dst = RegByte(ModRM);               \
	src = GetRMByte(ModRM)

#define DEF_r16w                        \
	UINT32 ModRM = FETCH(),src,dst;     \
	dst = RegWord(ModRM);               \
	src = GetRMWord(ModRM)

#define DEF_ald8                        \
	UINT32 src = FETCH();                   \
	UINT32 dst = Breg(AL)

#define DEF_axd16                       \
	UINT32 src = FETCH();               \
	UINT32 dst = Wreg(AW);          \
	src += (FETCH() << 8)
