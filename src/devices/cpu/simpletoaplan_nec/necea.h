// license:BSD-3-Clause
// copyright-holders:Bryan McPhail

UINT32 simpletoaplan_nec_common_device::EA_000() { m_EO=Wreg(BW)+Wreg(IX); m_EA=DefaultBase(DS0)+m_EO; return m_EA; }
UINT32 simpletoaplan_nec_common_device::EA_001() { m_EO=Wreg(BW)+Wreg(IY); m_EA=DefaultBase(DS0)+m_EO; return m_EA; }
UINT32 simpletoaplan_nec_common_device::EA_002() { m_EO=Wreg(BP)+Wreg(IX); m_EA=DefaultBase(SS)+m_EO; return m_EA; }
UINT32 simpletoaplan_nec_common_device::EA_003() { m_EO=Wreg(BP)+Wreg(IY); m_EA=DefaultBase(SS)+m_EO; return m_EA; }
UINT32 simpletoaplan_nec_common_device::EA_004() { m_EO=Wreg(IX); m_EA=DefaultBase(DS0)+m_EO; return m_EA; }
UINT32 simpletoaplan_nec_common_device::EA_005() { m_EO=Wreg(IY); m_EA=DefaultBase(DS0)+m_EO; return m_EA; }
UINT32 simpletoaplan_nec_common_device::EA_006() { m_EO=FETCH(); m_EO+=FETCH()<<8; m_EA=DefaultBase(DS0)+m_EO; return m_EA; }
UINT32 simpletoaplan_nec_common_device::EA_007() { m_EO=Wreg(BW); m_EA=DefaultBase(DS0)+m_EO; return m_EA; }

UINT32 simpletoaplan_nec_common_device::EA_100() { m_EO=(Wreg(BW)+Wreg(IX)+(INT8)FETCH()); m_EA=DefaultBase(DS0)+m_EO; return m_EA; }
UINT32 simpletoaplan_nec_common_device::EA_101() { m_EO=(Wreg(BW)+Wreg(IY)+(INT8)FETCH()); m_EA=DefaultBase(DS0)+m_EO; return m_EA; }
UINT32 simpletoaplan_nec_common_device::EA_102() { m_EO=(Wreg(BP)+Wreg(IX)+(INT8)FETCH()); m_EA=DefaultBase(SS)+m_EO; return m_EA; }
UINT32 simpletoaplan_nec_common_device::EA_103() { m_EO=(Wreg(BP)+Wreg(IY)+(INT8)FETCH()); m_EA=DefaultBase(SS)+m_EO; return m_EA; }
UINT32 simpletoaplan_nec_common_device::EA_104() { m_EO=(Wreg(IX)+(INT8)FETCH()); m_EA=DefaultBase(DS0)+m_EO; return m_EA; }
UINT32 simpletoaplan_nec_common_device::EA_105() { m_EO=(Wreg(IY)+(INT8)FETCH()); m_EA=DefaultBase(DS0)+m_EO; return m_EA; }
UINT32 simpletoaplan_nec_common_device::EA_106() { m_EO=(Wreg(BP)+(INT8)FETCH()); m_EA=DefaultBase(SS)+m_EO; return m_EA; }
UINT32 simpletoaplan_nec_common_device::EA_107() { m_EO=(Wreg(BW)+(INT8)FETCH()); m_EA=DefaultBase(DS0)+m_EO; return m_EA; }

UINT32 simpletoaplan_nec_common_device::EA_200() { m_E16=FETCH(); m_E16+=FETCH()<<8; m_EO=Wreg(BW)+Wreg(IX)+(INT16)m_E16; m_EA=DefaultBase(DS0)+m_EO; return m_EA; }
UINT32 simpletoaplan_nec_common_device::EA_201() { m_E16=FETCH(); m_E16+=FETCH()<<8; m_EO=Wreg(BW)+Wreg(IY)+(INT16)m_E16; m_EA=DefaultBase(DS0)+m_EO; return m_EA; }
UINT32 simpletoaplan_nec_common_device::EA_202() { m_E16=FETCH(); m_E16+=FETCH()<<8; m_EO=Wreg(BP)+Wreg(IX)+(INT16)m_E16; m_EA=DefaultBase(SS)+m_EO; return m_EA; }
UINT32 simpletoaplan_nec_common_device::EA_203() { m_E16=FETCH(); m_E16+=FETCH()<<8; m_EO=Wreg(BP)+Wreg(IY)+(INT16)m_E16; m_EA=DefaultBase(SS)+m_EO; return m_EA; }
UINT32 simpletoaplan_nec_common_device::EA_204() { m_E16=FETCH(); m_E16+=FETCH()<<8; m_EO=Wreg(IX)+(INT16)m_E16; m_EA=DefaultBase(DS0)+m_EO; return m_EA; }
UINT32 simpletoaplan_nec_common_device::EA_205() { m_E16=FETCH(); m_E16+=FETCH()<<8; m_EO=Wreg(IY)+(INT16)m_E16; m_EA=DefaultBase(DS0)+m_EO; return m_EA; }
UINT32 simpletoaplan_nec_common_device::EA_206() { m_E16=FETCH(); m_E16+=FETCH()<<8; m_EO=Wreg(BP)+(INT16)m_E16; m_EA=DefaultBase(SS)+m_EO; return m_EA; }
UINT32 simpletoaplan_nec_common_device::EA_207() { m_E16=FETCH(); m_E16+=FETCH()<<8; m_EO=Wreg(BW)+(INT16)m_E16; m_EA=DefaultBase(DS0)+m_EO; return m_EA; }

const simpletoaplan_nec_common_device::simpletoaplan_nec_eahandler simpletoaplan_nec_common_device::s_GetEA[192]=
{
	&simpletoaplan_nec_common_device::EA_000, &simpletoaplan_nec_common_device::EA_001, &simpletoaplan_nec_common_device::EA_002, &simpletoaplan_nec_common_device::EA_003, &simpletoaplan_nec_common_device::EA_004, &simpletoaplan_nec_common_device::EA_005, &simpletoaplan_nec_common_device::EA_006, &simpletoaplan_nec_common_device::EA_007,
	&simpletoaplan_nec_common_device::EA_000, &simpletoaplan_nec_common_device::EA_001, &simpletoaplan_nec_common_device::EA_002, &simpletoaplan_nec_common_device::EA_003, &simpletoaplan_nec_common_device::EA_004, &simpletoaplan_nec_common_device::EA_005, &simpletoaplan_nec_common_device::EA_006, &simpletoaplan_nec_common_device::EA_007,
	&simpletoaplan_nec_common_device::EA_000, &simpletoaplan_nec_common_device::EA_001, &simpletoaplan_nec_common_device::EA_002, &simpletoaplan_nec_common_device::EA_003, &simpletoaplan_nec_common_device::EA_004, &simpletoaplan_nec_common_device::EA_005, &simpletoaplan_nec_common_device::EA_006, &simpletoaplan_nec_common_device::EA_007,
	&simpletoaplan_nec_common_device::EA_000, &simpletoaplan_nec_common_device::EA_001, &simpletoaplan_nec_common_device::EA_002, &simpletoaplan_nec_common_device::EA_003, &simpletoaplan_nec_common_device::EA_004, &simpletoaplan_nec_common_device::EA_005, &simpletoaplan_nec_common_device::EA_006, &simpletoaplan_nec_common_device::EA_007,
	&simpletoaplan_nec_common_device::EA_000, &simpletoaplan_nec_common_device::EA_001, &simpletoaplan_nec_common_device::EA_002, &simpletoaplan_nec_common_device::EA_003, &simpletoaplan_nec_common_device::EA_004, &simpletoaplan_nec_common_device::EA_005, &simpletoaplan_nec_common_device::EA_006, &simpletoaplan_nec_common_device::EA_007,
	&simpletoaplan_nec_common_device::EA_000, &simpletoaplan_nec_common_device::EA_001, &simpletoaplan_nec_common_device::EA_002, &simpletoaplan_nec_common_device::EA_003, &simpletoaplan_nec_common_device::EA_004, &simpletoaplan_nec_common_device::EA_005, &simpletoaplan_nec_common_device::EA_006, &simpletoaplan_nec_common_device::EA_007,
	&simpletoaplan_nec_common_device::EA_000, &simpletoaplan_nec_common_device::EA_001, &simpletoaplan_nec_common_device::EA_002, &simpletoaplan_nec_common_device::EA_003, &simpletoaplan_nec_common_device::EA_004, &simpletoaplan_nec_common_device::EA_005, &simpletoaplan_nec_common_device::EA_006, &simpletoaplan_nec_common_device::EA_007,
	&simpletoaplan_nec_common_device::EA_000, &simpletoaplan_nec_common_device::EA_001, &simpletoaplan_nec_common_device::EA_002, &simpletoaplan_nec_common_device::EA_003, &simpletoaplan_nec_common_device::EA_004, &simpletoaplan_nec_common_device::EA_005, &simpletoaplan_nec_common_device::EA_006, &simpletoaplan_nec_common_device::EA_007,

	&simpletoaplan_nec_common_device::EA_100, &simpletoaplan_nec_common_device::EA_101, &simpletoaplan_nec_common_device::EA_102, &simpletoaplan_nec_common_device::EA_103, &simpletoaplan_nec_common_device::EA_104, &simpletoaplan_nec_common_device::EA_105, &simpletoaplan_nec_common_device::EA_106, &simpletoaplan_nec_common_device::EA_107,
	&simpletoaplan_nec_common_device::EA_100, &simpletoaplan_nec_common_device::EA_101, &simpletoaplan_nec_common_device::EA_102, &simpletoaplan_nec_common_device::EA_103, &simpletoaplan_nec_common_device::EA_104, &simpletoaplan_nec_common_device::EA_105, &simpletoaplan_nec_common_device::EA_106, &simpletoaplan_nec_common_device::EA_107,
	&simpletoaplan_nec_common_device::EA_100, &simpletoaplan_nec_common_device::EA_101, &simpletoaplan_nec_common_device::EA_102, &simpletoaplan_nec_common_device::EA_103, &simpletoaplan_nec_common_device::EA_104, &simpletoaplan_nec_common_device::EA_105, &simpletoaplan_nec_common_device::EA_106, &simpletoaplan_nec_common_device::EA_107,
	&simpletoaplan_nec_common_device::EA_100, &simpletoaplan_nec_common_device::EA_101, &simpletoaplan_nec_common_device::EA_102, &simpletoaplan_nec_common_device::EA_103, &simpletoaplan_nec_common_device::EA_104, &simpletoaplan_nec_common_device::EA_105, &simpletoaplan_nec_common_device::EA_106, &simpletoaplan_nec_common_device::EA_107,
	&simpletoaplan_nec_common_device::EA_100, &simpletoaplan_nec_common_device::EA_101, &simpletoaplan_nec_common_device::EA_102, &simpletoaplan_nec_common_device::EA_103, &simpletoaplan_nec_common_device::EA_104, &simpletoaplan_nec_common_device::EA_105, &simpletoaplan_nec_common_device::EA_106, &simpletoaplan_nec_common_device::EA_107,
	&simpletoaplan_nec_common_device::EA_100, &simpletoaplan_nec_common_device::EA_101, &simpletoaplan_nec_common_device::EA_102, &simpletoaplan_nec_common_device::EA_103, &simpletoaplan_nec_common_device::EA_104, &simpletoaplan_nec_common_device::EA_105, &simpletoaplan_nec_common_device::EA_106, &simpletoaplan_nec_common_device::EA_107,
	&simpletoaplan_nec_common_device::EA_100, &simpletoaplan_nec_common_device::EA_101, &simpletoaplan_nec_common_device::EA_102, &simpletoaplan_nec_common_device::EA_103, &simpletoaplan_nec_common_device::EA_104, &simpletoaplan_nec_common_device::EA_105, &simpletoaplan_nec_common_device::EA_106, &simpletoaplan_nec_common_device::EA_107,
	&simpletoaplan_nec_common_device::EA_100, &simpletoaplan_nec_common_device::EA_101, &simpletoaplan_nec_common_device::EA_102, &simpletoaplan_nec_common_device::EA_103, &simpletoaplan_nec_common_device::EA_104, &simpletoaplan_nec_common_device::EA_105, &simpletoaplan_nec_common_device::EA_106, &simpletoaplan_nec_common_device::EA_107,

	&simpletoaplan_nec_common_device::EA_200, &simpletoaplan_nec_common_device::EA_201, &simpletoaplan_nec_common_device::EA_202, &simpletoaplan_nec_common_device::EA_203, &simpletoaplan_nec_common_device::EA_204, &simpletoaplan_nec_common_device::EA_205, &simpletoaplan_nec_common_device::EA_206, &simpletoaplan_nec_common_device::EA_207,
	&simpletoaplan_nec_common_device::EA_200, &simpletoaplan_nec_common_device::EA_201, &simpletoaplan_nec_common_device::EA_202, &simpletoaplan_nec_common_device::EA_203, &simpletoaplan_nec_common_device::EA_204, &simpletoaplan_nec_common_device::EA_205, &simpletoaplan_nec_common_device::EA_206, &simpletoaplan_nec_common_device::EA_207,
	&simpletoaplan_nec_common_device::EA_200, &simpletoaplan_nec_common_device::EA_201, &simpletoaplan_nec_common_device::EA_202, &simpletoaplan_nec_common_device::EA_203, &simpletoaplan_nec_common_device::EA_204, &simpletoaplan_nec_common_device::EA_205, &simpletoaplan_nec_common_device::EA_206, &simpletoaplan_nec_common_device::EA_207,
	&simpletoaplan_nec_common_device::EA_200, &simpletoaplan_nec_common_device::EA_201, &simpletoaplan_nec_common_device::EA_202, &simpletoaplan_nec_common_device::EA_203, &simpletoaplan_nec_common_device::EA_204, &simpletoaplan_nec_common_device::EA_205, &simpletoaplan_nec_common_device::EA_206, &simpletoaplan_nec_common_device::EA_207,
	&simpletoaplan_nec_common_device::EA_200, &simpletoaplan_nec_common_device::EA_201, &simpletoaplan_nec_common_device::EA_202, &simpletoaplan_nec_common_device::EA_203, &simpletoaplan_nec_common_device::EA_204, &simpletoaplan_nec_common_device::EA_205, &simpletoaplan_nec_common_device::EA_206, &simpletoaplan_nec_common_device::EA_207,
	&simpletoaplan_nec_common_device::EA_200, &simpletoaplan_nec_common_device::EA_201, &simpletoaplan_nec_common_device::EA_202, &simpletoaplan_nec_common_device::EA_203, &simpletoaplan_nec_common_device::EA_204, &simpletoaplan_nec_common_device::EA_205, &simpletoaplan_nec_common_device::EA_206, &simpletoaplan_nec_common_device::EA_207,
	&simpletoaplan_nec_common_device::EA_200, &simpletoaplan_nec_common_device::EA_201, &simpletoaplan_nec_common_device::EA_202, &simpletoaplan_nec_common_device::EA_203, &simpletoaplan_nec_common_device::EA_204, &simpletoaplan_nec_common_device::EA_205, &simpletoaplan_nec_common_device::EA_206, &simpletoaplan_nec_common_device::EA_207,
	&simpletoaplan_nec_common_device::EA_200, &simpletoaplan_nec_common_device::EA_201, &simpletoaplan_nec_common_device::EA_202, &simpletoaplan_nec_common_device::EA_203, &simpletoaplan_nec_common_device::EA_204, &simpletoaplan_nec_common_device::EA_205, &simpletoaplan_nec_common_device::EA_206, &simpletoaplan_nec_common_device::EA_207
};
