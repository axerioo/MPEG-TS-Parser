#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>

#include "tsCommon.h"
#include "tsTransportStream.h"

int main(int argc, char* argv[], char* envp[]) {
	FILE* file = nullptr;
	FILE* audioFile = nullptr;

	const char* filename = "./example_new.ts";
	const char* audioFilename = "PID136.mp2";

	// Check if a filename is provided as a command line argument
	if (argc > 1) {
		filename = argv[1];
	}

	// Open the input file in binary read mode
	if (fopen_s(&file, filename, "rb") != 0) {
		printf("Error: Could not open input file\n");
		return EXIT_FAILURE;
	}

	// Open the output audio file in binary write mode
	if (fopen_s(&audioFile, audioFilename, "wb") != 0) {
		printf("Error: Could not create audio output file\n");
		fclose(file);
		return EXIT_FAILURE;
	}

	constexpr uint16_t audioPID = 136;

	xTS_PacketHeader	TS_PacketHeader{};
	xTS_AdaptationField TS_AdaptationField{};
	xPES_Assembler		PES_Assembler{};
	PES_Assembler.Init(audioPID);

	uint8_t TS_PacketBuffer[xTS::TS_PacketLength] = { 0 };
	int32_t TS_PacketId = 0;
	int32_t totalAudioPackets = 0;
	int32_t totalAudioBytes = 0;

	printf("Starting MPEG-TS parsing for audio PID %d...\n", audioPID);
	printf("Audio data will be saved to: %s\n\n", audioFilename);

	while (!feof(file)) {
		// Read entire TS packet
		size_t bytesRead = fread(TS_PacketBuffer, sizeof(uint8_t), xTS::TS_PacketLength, file);

		// Check if packet is complete
		if (bytesRead != xTS::TS_PacketLength) {	/* EOF */
			break;
		}

		// Parse TS packet header
		TS_PacketHeader.Reset();
		if (TS_PacketHeader.Parse(TS_PacketBuffer) == NOT_VALID) {		/* Skipping sync errors */
			printf("Error: Invalid packet at position %d, skipping\n", TS_PacketId);
			TS_PacketId++;
			continue;
		}

		// Parse adaptation field if present
		TS_AdaptationField.Reset();
		if (TS_PacketHeader.hasAdaptationField()) {
			TS_AdaptationField.Parse(TS_PacketBuffer, TS_PacketHeader.getAFC());
		}

		// Print packet information

		/*
		printf("%010d ", TS_PacketId);
		TS_PacketHeader.Print();

		if (TS_PacketHeader.hasAdaptationField())
			TS_AdaptationField.Print();
		else
			printf("\n");
		*/

		// Process PES packets for audio PID
		if (TS_PacketHeader.getPID() == audioPID) {
			xPES_Assembler::eResult result = PES_Assembler.AbsorbPacket(TS_PacketBuffer, &TS_PacketHeader, &TS_AdaptationField);

			switch (result) {
			case xPES_Assembler::eResult::AssemblingStarted:
				printf("Assembling Started\n");
				PES_Assembler.PrintPESH();
				break;

			case xPES_Assembler::eResult::AssemblingContinue:
				printf("Assembling Continue\n");
				break;

			case xPES_Assembler::eResult::AssemblingFinished:
			{
				printf("Assembling Finished\n");

				// Get the assembled PES packet
				uint8_t* pesPacket = PES_Assembler.getPacket();
				int32_t pesPacketLength = PES_Assembler.getNumPacketBytes();

				printf("PES: PcktLen=%d ", pesPacketLength);

				// Calculate header length and payload data length
				// Standard PES header is 6 bytes (packet_start_code_prefix + stream_id + PES_packet_length)

				int32_t headerLength = 0;
				int32_t dataLength = 0;

				if (pesPacketLength >= 6) {
					// Basic PES header analysis
					uint8_t streamId = pesPacket[3];
					uint16_t packetLength = (pesPacket[4] << 8) | pesPacket[5];

					// For audio streams (110X XXXX - 0xC0-0xDF)
					if ((streamId >= 0xC0 && streamId <= 0xDF) || streamId == 0xBD) {
						// Audio stream - may have additional PES header fields
						if (pesPacketLength >= 9) {
							uint8_t pesHeaderDataLength = pesPacket[8];
							headerLength = 9 + pesHeaderDataLength; // 9 bytes fixed + variable length
						}
						else {
							headerLength = 6; // Minimum PES header
						}
					}
					else {
						headerLength = 6; // Basic PES header for other streams
					}

					dataLength = pesPacketLength - headerLength;

					if (dataLength > 0 && headerLength < pesPacketLength) {
						// Write only the elementary stream data (skip PES header)
						size_t bytesWritten = fwrite(pesPacket + headerLength, sizeof(uint8_t), dataLength, audioFile);

						if (bytesWritten == dataLength) {
							totalAudioBytes += dataLength;
							totalAudioPackets++;
							printf("HeadLen=%d DataLen=%d - Audio data written to file\n", headerLength, dataLength);
						}
						else {
							printf("HeadLen=%d DataLen=%d - Error writing audio data\n", headerLength, dataLength);
						}
					}
					else {
						printf("HeadLen=%d DataLen=%d - No payload data to write\n", headerLength, dataLength);
					}
				}
			}
			break;

			case xPES_Assembler::eResult::StreamPackedLost:
				printf("Packet lost\n");
				break;

			default:
				break;
			}
		}
		TS_PacketId++;
	}

	printf("\n=== Summary ===\n");
	printf("Total TS packets processed: %d\n", TS_PacketId);
	printf("Total audio PES packets assembled: %d\n", totalAudioPackets);
	printf("Total audio bytes written: %d\n", totalAudioBytes);
	printf("Audio file saved as: %s\n", audioFilename);

	fclose(file);
	fclose(audioFile);

	return EXIT_SUCCESS;
}