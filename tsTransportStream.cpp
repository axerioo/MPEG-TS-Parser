#include "tsTransportStream.h"

//=============================================================================================================================================================================
// xTS_PacketHeader
//=============================================================================================================================================================================

// Reset the packet header fields to default values
void xTS_PacketHeader::Reset()
{
	m_SB	= 0;
	m_E		= 0;
	m_S		= 0;
	m_T		= 0;
	m_PID	= 0;
	m_TSC	= 0;
	m_AFC	= 0;
	m_CC	= 0;
}

// Parse the TS packet header from the input buffer
int32_t xTS_PacketHeader::Parse(const uint8_t* Input)
{
	if (Input == nullptr)
		return NOT_VALID;

	m_SB = Input[0];

	if (m_SB != 0x47) // Check for sync byte
		return NOT_VALID;

	m_E		= (Input[1] & 0b10000000) >> 7;
	m_S		= (Input[1] & 0b01000000) >> 6;
	m_T		= (Input[1] & 0b00100000) >> 5;
	m_PID	= ((Input[1] & 0b00001111) << 8) | Input[2];
	m_TSC	= (Input[3] & 0b11000000) >> 6;
	m_AFC	= (Input[3] & 0b00110000) >> 4;
	m_CC	= (Input[3] & 0b00001111);

	return 0;
}

// Print the packet header information
void xTS_PacketHeader::Print() const
{
	printf("SB: %2d, E: %d, S: %d, T: %d, PID: %4d, TSC: %d, AFC: %d, CC: %2d",
			m_SB, m_E, m_S, m_T, m_PID, m_TSC, m_AFC, m_CC);
}

//=============================================================================================================================================================================
// xTS_AdaptationField
//=============================================================================================================================================================================

// Reset the adaptation field to default values
void xTS_AdaptationField::Reset()
{
	m_AdaptationFieldLength = 0;
	m_DC = 0;
	m_RA = 0;
	m_SP = 0;
	m_PR = 0;
	m_OR = 0;
	m_SF = 0;
	m_TP = 0;
	m_EX = 0;
}

// Parse the adaptation field from the TS packet buffer
int32_t xTS_AdaptationField::Parse(const uint8_t* PacketBuffer, uint8_t AdaptationFieldControl)
{
	if (PacketBuffer == nullptr)
		return NOT_VALID;

	if (AdaptationFieldControl != 0b10 && AdaptationFieldControl != 0b11)
		return NOT_VALID;

	// Get the adaptation field length
	m_AdaptationFieldLength = PacketBuffer[4];

	uint8_t flags = PacketBuffer[5];
	m_DC = (flags & 0b10000000) >> 7;
	m_RA = (flags & 0b01000000) >> 6;
	m_SP = (flags & 0b00100000) >> 5;
	m_PR = (flags & 0b00010000) >> 4;
	m_OR = (flags & 0b00001000) >> 3;
	m_SF = (flags & 0b00000100) >> 2;
	m_TP = (flags & 0b00000010) >> 1;
	m_EX = (flags & 0b00000001);

	// TODO: Parse PCR and OPCR

	return 0;
}

// Print the adaptation field information
void xTS_AdaptationField::Print() const
{
	printf(" AFL: %3d DC: %d RA: %d SP: %d PR: %d OR: %d SF: %d TP: %d EX: %d",
			m_AdaptationFieldLength, m_DC, m_RA, m_SP, m_PR, m_OR, m_SF, m_TP, m_EX);
}

//=============================================================================================================================================================================
// xPES_PacketHeader
//=============================================================================================================================================================================

// Reset the PES packet header fields to default values
void xPES_PacketHeader::Reset()
{
	m_PacketStartCodePrefix = 0;
	m_StreamId		= 0;
	m_PacketLength	= 0;
}

// Parse the PES packet header from the input buffer
int32_t xPES_PacketHeader::Parse(const uint8_t* Input)
{
	if (Input == nullptr)
		return NOT_VALID;

	m_PacketStartCodePrefix = (Input[0] << 16) | (Input[1] << 8) | Input[2];
	m_StreamId		= Input[3];
	m_PacketLength	= (Input[4] << 8) | Input[5];

	return 0;
}

// Print the PES packet header information
void xPES_PacketHeader::Print() const
{
	printf("PSCP: %d, SID: %d, L: %d",
			m_PacketStartCodePrefix, m_StreamId, m_PacketLength);
}

//=============================================================================================================================================================================
// xPES_Assembler
//=============================================================================================================================================================================

// Initializes the buffer and reserves space
xPES_Assembler::xPES_Assembler()
{
	m_Buffer.reserve(65536); // Reserve space for the buffer to avoid reallocations
}

// Initialize the assembler with a specific PID
void xPES_Assembler::Init(int32_t PID)
{
	m_PID = PID;
	xBufferReset();
}

// Process a Transport Stream packet and absorb it into the PES assembler
xPES_Assembler::eResult
xPES_Assembler::AbsorbPacket (const uint8_t* TransportStreamPacket, const xTS_PacketHeader* PacketHeader, const xTS_AdaptationField* AdaptationField)
{
	if (TransportStreamPacket == nullptr || PacketHeader == nullptr)
		return eResult::UnexpectedPID;

	if (PacketHeader->getPID() != m_PID) {
		return eResult::UnexpectedPID;
	}

	// Check for continuity counter errors
	if (m_Started) {

		// Check if the continuity counter matches the expected value
		int8_t expectedCC = (m_LastContinuityCounter + 1) & 0b0001111;
		if (PacketHeader->getCC() != expectedCC) {

			m_Started = false;
			xBufferReset();

			return eResult::StreamPackedLost;
		}
	}

	m_LastContinuityCounter = PacketHeader->getCC();

	// Calculate payload start position
	uint32_t payloadStart = xTS::TS_HeaderLength;
	if (PacketHeader->hasAdaptationField()) {
		payloadStart += AdaptationField->getNumBytes();
	}

	// If this is the start of a new PES packet
	if (PacketHeader->getS() == 1) {
		if (m_Started) {

			// Finish previous PES packet
			m_Started = false;

			// Start new PES packet
			xBufferReset();
			m_PESH.Reset();

			if (m_PESH.Parse(&TransportStreamPacket[payloadStart]) == 0) {
				m_Started = true;
				xBufferAppend(&TransportStreamPacket[payloadStart], xTS::TS_PacketLength - payloadStart);
				return eResult::AssemblingStarted;
			}

			return eResult::AssemblingFinished;

		}
		else {
			// Start new PES packet
			xBufferReset();
			m_PESH.Reset();

			if (m_PESH.Parse(&TransportStreamPacket[payloadStart]) == 0) {
				m_Started = true;
				xBufferAppend(&TransportStreamPacket[payloadStart], xTS::TS_PacketLength - payloadStart);
				return eResult::AssemblingStarted;
			}
		}
	}
	else {
		// Continue existing PES packet
		if (m_Started && PacketHeader->hasPayload()) {
			xBufferAppend(&TransportStreamPacket[payloadStart], xTS::TS_PacketLength - payloadStart);

			// Check if we have complete PES packet
			if (m_DataOffset >= xTS::PES_HeaderLength) {
				uint16_t pesLength = m_PESH.getPacketLength();
				if (pesLength > 0 && m_DataOffset >= pesLength + xTS::PES_HeaderLength) {
					m_Started = false;
					return eResult::AssemblingFinished;
				}
			}
			return eResult::AssemblingContinue;
		}
	}

	return eResult::UnexpectedPID;
}

// Reset the buffer and data offset
void xPES_Assembler::xBufferReset()
{
	m_Buffer.clear();
	m_DataOffset = 0;
}

// Append data to the buffer and update the data offset
void xPES_Assembler::xBufferAppend(const uint8_t* Data, int32_t Size)
{
	if (Data != nullptr && Size > 0) {
		m_Buffer.insert(m_Buffer.end(), Data, Data + Size);
		m_DataOffset += Size;
	}
}