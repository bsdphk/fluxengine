#ifndef BROTHER_H
#define BROTHER_H

/* Brother word processor format (or at least, one of them) */

#define BROTHER_SECTOR_RECORD            0xFFFFFD57
#define BROTHER_DATA_RECORD              0xFFFFFDDB
#define BROTHER_DATA_RECORD_PAYLOAD      256
#define BROTHER_DATA_RECORD_CHECKSUM     3
#define BROTHER_DATA_RECORD_ENCODED_SIZE 415

class Sector;
class Fluxmap;

class BrotherDecoder : public AbstractSimplifiedDecoder
{
public:
    virtual ~BrotherDecoder() {}

    RecordType advanceToNextRecord();
    void decodeSectorRecord();
    void decodeDataRecord();
};

extern void writeBrotherSectorHeader(std::vector<bool>& bits, unsigned& cursor,
		int track, int sector);
extern void writeBrotherSectorData(std::vector<bool>& bits, unsigned& cursor,
		const Bytes& data);

#endif
