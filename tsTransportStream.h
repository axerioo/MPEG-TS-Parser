#pragma once

#include "tsCommon.h"
#include <string>
#include <vector>
#include <cstring>

/*
MPEG-TS packet:
`        3                   2                   1                   0  `
`      1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0  `
`     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ `
`   0 |                             Header                            | `
`     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ `
`   4 |                  Adaptation field + Payload                   | `
`     |                                                               | `
` 184 |                                                               | `
`     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ `


MPEG-TS packet header:
`        3                   2                   1                   0  `
`      1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0  `
`     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ `
`   0 |       SB      |E|S|T|           PID           |TSC|AFC|   CC  | `
`     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ `

Sync byte                    (SB ) :  8 bits
Transport error indicator    (E  ) :  1 bit
Payload unit start indicator (S  ) :  1 bit
Transport priority           (T  ) :  1 bit
Packet Identifier            (PID) : 13 bits
Transport scrambling control (TSC) :  2 bits
Adaptation field control     (AFC) :  2 bits
Continuity counter           (CC ) :  4 bits
*/

//=============================================================================================================================================================================

class xTS
{
public:
	static constexpr uint32_t TS_PacketLength = 188;
	static constexpr uint32_t TS_HeaderLength = 4;

	static constexpr uint32_t PES_HeaderLength = 6;

	static constexpr uint32_t BaseClockFrequency_Hz = 90000;		// Hz
	static constexpr uint32_t ExtendedClockFrequency_Hz = 27000000; // Hz
	static constexpr uint32_t BaseClockFrequency_kHz = 90;			// kHz
	static constexpr uint32_t ExtendedClockFrequency_kHz = 27000;	// kHz
	static constexpr uint32_t BaseToExtendedClockMultiplier = 300;
};

//=============================================================================================================================================================================

class xTS_PacketHeader
{
public:
	enum class ePID : uint16_t {
		PAT  = 0x0000,
		CAT  = 0x0001,
		TSDT = 0x0002,
		IPMT = 0x0003,
		NIT  = 0x0010,	// DVB specific PID
		SDT  = 0x0011,	// DVB specific PID
		NuLL = 0x1FFF,
	};

protected:
	uint8_t  m_SB;
	uint8_t  m_E;
	uint8_t  m_S;
	uint8_t  m_T;
	uint16_t m_PID;
	uint8_t  m_TSC;
	uint8_t  m_AFC;
	uint8_t  m_CC;

public:
	void    Reset();
	int32_t Parse(const uint8_t* Input);
	void    Print() const;

public:
	uint8_t  getSB()  const { return m_SB; }
	uint8_t  getE()   const { return m_E; }
	uint8_t  getS()   const { return m_S; }
	uint8_t  getT()   const { return m_T; }
	uint16_t getPID() const { return m_PID; }
	uint8_t  getTSC() const { return m_TSC; }
	uint8_t  getAFC() const { return m_AFC; }
	uint8_t  getCC()  const { return m_CC; }

public:
	bool hasAdaptationField() const {
		return (m_AFC == 0b10 || m_AFC == 0b11);
	}

	bool hasPayload() const {
		return (m_AFC == 0b01 || m_AFC == 0b11);
	}
};

//=============================================================================================================================================================================

class xTS_AdaptationField
{
protected:
	uint8_t m_AdaptationFieldLength;
	uint8_t m_DC;	// Discontinuity indicator
	uint8_t m_RA;	// Random access indicator
	uint8_t m_SP;	// Elementary stream priority indicator
	uint8_t m_PR;	// Program Clock Reference flag
	uint8_t m_OR;	// Original Program Clock Reference flag
	uint8_t m_SF;	// Splicing point flag
	uint8_t m_TP;	// Transport private data flag
	uint8_t m_EX;	// Adaptation field extension flag

	// optional fields - PCR

public:
	void Reset();
	int32_t Parse(const uint8_t* PacketBuffer, uint8_t AdaptationFieldControl);
	void Print() const;

public:
	uint8_t getAdaptationFieldLength() const { return m_AdaptationFieldLength; }
	uint8_t getDC() const { return m_DC; }
	uint8_t getRA() const { return m_RA; }
	uint8_t getSP() const { return m_SP; }
	uint8_t getPR() const { return m_PR; }
	uint8_t getOR() const { return m_OR; }
	uint8_t getSF() const { return m_SF; }
	uint8_t getTP() const { return m_TP; }
	uint8_t getEX() const { return m_EX; }
	
	// Derived values
	uint32_t getNumBytes() const {
		return m_AdaptationFieldLength + 1; //+1 for Adaptation Field Control field
	}
};

//=============================================================================================================================================================================
// xPES_PacketHeader
//=============================================================================================================================================================================

class xPES_PacketHeader
{
public:
	enum eStreamId : uint8_t
	{
		eStreamId_program_stream_map		= 0xBC,
		eStreamId_padding_stream			= 0xBE,
		eStreamId_private_stream_2			= 0xBF,
		eStreamId_ECM						= 0xF0,
		eStreamId_EMM						= 0xF1,
		eStreamId_program_stream_directory	= 0xFF,
		eStreamId_DSMCC_stream				= 0xF2,
		eStreamId_ITUT_H222_1_type_E		= 0xF8,
	};

protected:
	uint32_t m_PacketStartCodePrefix{ 0 };
	uint8_t  m_StreamId{ 0 };
	uint16_t m_PacketLength{ 0 };

public:
	void    Reset();
	int32_t Parse(const uint8_t* Input);
	void    Print() const;

	uint32_t getPacketStartCodePrefix() const { return m_PacketStartCodePrefix; }
	uint8_t  getStreamId()              const { return m_StreamId; }
	uint16_t getPacketLength()          const { return m_PacketLength; }
};

//=============================================================================================================================================================================
// xPES_Assembler
//=============================================================================================================================================================================

class xPES_Assembler
{
public:
	enum class eResult : int32_t
	{
		UnexpectedPID		= 1,
		StreamPackedLost	= 2,
		AssemblingStarted	= 3,
		AssemblingContinue	= 4,
		AssemblingFinished	= 5
	};

protected:
	// setup
	int32_t  m_PID{ -1 };

	// buffer
	std::vector<uint8_t>	m_Buffer;
	uint32_t				m_DataOffset{ 0 };

	// operation
	int8_t				m_LastContinuityCounter{ -1 };
	bool				m_Started{ false };
	xPES_PacketHeader	m_PESH;

public:
	xPES_Assembler();
	~xPES_Assembler() = default;

	void    Init(int32_t PID);
	eResult AbsorbPacket(const uint8_t* TransportStreamPacket, const xTS_PacketHeader* PacketHeader, const xTS_AdaptationField* AdaptationField);

	void	 PrintPESH			() const { m_PESH.Print(); }
	uint8_t* getPacket			()		 { return m_Buffer.data(); }
	int32_t  getNumPacketBytes	() const { return static_cast<int32_t>(m_DataOffset); }

protected:
	void xBufferReset();
	void xBufferAppend(const uint8_t* Data, int32_t Size);
};

//=============================================================================================================================================================================
