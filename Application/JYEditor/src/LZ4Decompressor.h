#pragma once
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <vector>

class LZ4Decompressor {
public:
    static std::vector<uint8_t> decompress(const std::vector<uint8_t>& input) {
        if (input.empty())
            return {};

        // Detect LZ4 frame format (magic: 0x184D2204)
        if (input.size() >= 4) {
            uint32_t magic = (uint32_t)input[0] | ((uint32_t)input[1] << 8)
                           | ((uint32_t)input[2] << 16) | ((uint32_t)input[3] << 24);
            if (magic == 0x184D2204)
                return decompressFrame(input);
        }

        // Default: raw LZ4 block
        return decompressBlock(input);
    }

private:
    // --- Raw LZ4 block decompression ---
    static std::vector<uint8_t> decompressBlock(const std::vector<uint8_t>& input) {
        // Upper bound for output size: LZ4 worst case is 1 byte -> 2 bytes (token+literal)
        // Safe estimate: input * 255 + a bit more (since LZ4 can expand)
        // Use a generous buffer, will grow if needed
        size_t estOutput = input.size() * 4 + 64;
        if (estOutput > 128 * 1024 * 1024) // cap at 128MB
            estOutput = 128 * 1024 * 1024;

        std::vector<uint8_t> output(estOutput);
        size_t srcPos = 0;
        size_t dstPos = 0;

        while (srcPos < input.size()) {
            // Token
            uint8_t token = input[srcPos++];

            // --- Literal length ---
            size_t literalLen = (token >> 4) & 0x0f;
            if (literalLen == 15) {
                uint8_t extra;
                do {
                    if (srcPos >= input.size())
                        throw std::runtime_error("LZ4: unexpected end in literal length");
                    extra = input[srcPos++];
                    literalLen += extra;
                } while (extra == 255);
            }

            // --- Copy literals ---
            if (srcPos + literalLen > input.size())
                throw std::runtime_error("LZ4: unexpected end of literals");

            if (dstPos + literalLen > output.size())
                output.resize((output.size() + literalLen) * 2);

            memcpy(&output[dstPos], &input[srcPos], literalLen);
            srcPos += literalLen;
            dstPos += literalLen;

            // End of block: last sequence has no match
            if (srcPos >= input.size())
                break;

            // --- Offset ---
            if (srcPos + 2 > input.size())
                throw std::runtime_error("LZ4: unexpected end of offset");
            uint16_t offset = (uint16_t)input[srcPos] | ((uint16_t)input[srcPos + 1] << 8);
            srcPos += 2;
            if (offset == 0)
                throw std::runtime_error("LZ4: invalid zero offset");

            // --- Match length ---
            size_t matchLen = (token & 0x0f) + 4;
            if (matchLen == 19) { // 15 + 4
                uint8_t extra;
                do {
                    if (srcPos >= input.size())
                        throw std::runtime_error("LZ4: unexpected end in match length");
                    extra = input[srcPos++];
                    matchLen += extra;
                } while (extra == 255);
            }

            // --- Copy match ---
            if (offset > dstPos)
                throw std::runtime_error("LZ4: offset beyond output");
            if (dstPos + matchLen > output.size())
                output.resize((output.size() + matchLen) * 2);

            size_t matchPos = dstPos - offset;
            for (size_t i = 0; i < matchLen; i++)
                output[dstPos + i] = output[matchPos + i];
            dstPos += matchLen;
        }

        output.resize(dstPos);
        return output;
    }

    // --- LZ4 Frame format decompression (basic, no checksum verification) ---
    static std::vector<uint8_t> decompressFrame(const std::vector<uint8_t>& input) {
        size_t pos = 0;

        // Magic: 4 bytes (already verified)
        pos += 4;

        // FLG byte
        if (pos >= input.size())
            throw std::runtime_error("LZ4 frame: unexpected end in FLG");
        uint8_t flg = input[pos++];

        bool hasContentSize   = (flg >> 3) & 1;
        bool hasBlockChecksum = (flg >> 1) & 1;
        // bool hasContentChecksum = flg & 1;
        // bool isIndep = (flg >> 5) & 1; // block independence, unused

        // BD byte (block max size)
        if (pos >= input.size())
            throw std::runtime_error("LZ4 frame: unexpected end in BD");
        uint8_t bd = input[pos++];
        int blockMaxSize = (bd >> 4) & 0x07;
        static const size_t blockSizes[] = {0, 0, 0, 0, 65536, 262144, 1048576, 4194304};
        if (blockMaxSize < 4 || blockMaxSize > 7)
            throw std::runtime_error("LZ4 frame: invalid block max size");
        size_t maxBlockSize = blockSizes[blockMaxSize];

        // Optional content size (8 bytes)
        if (hasContentSize)
            pos += 8;

        // Optional dict ID (4 bytes) - bit 0 of FLG
        if (flg & 1)
            pos += 4;

        // Header checksum (1 byte, xxhash32 of descriptor >> 8 & 0xFF)
        if (pos >= input.size())
            throw std::runtime_error("LZ4 frame: unexpected end in header checksum");
        pos++; // skip checksum

        // Data blocks
        std::vector<uint8_t> output;
        output.reserve(maxBlockSize * 4);

        while (pos + 4 <= input.size()) {
            uint32_t blockSize = (uint32_t)input[pos] | ((uint32_t)input[pos + 1] << 8)
                               | ((uint32_t)input[pos + 2] << 16) | ((uint32_t)input[pos + 3] << 24);
            pos += 4;

            if (blockSize == 0) // End mark
                break;

            bool isUncompressed = (blockSize & 0x80000000) != 0;
            blockSize &= 0x7FFFFFFF;

            if (blockSize > maxBlockSize)
                throw std::runtime_error("LZ4 frame: block size exceeds max");

            if (pos + blockSize > input.size())
                throw std::runtime_error("LZ4 frame: unexpected end of block data");

            if (isUncompressed) {
                // Store directly
                size_t oldSize = output.size();
                output.resize(oldSize + blockSize);
                memcpy(&output[oldSize], &input[pos], blockSize);
                pos += blockSize;
            } else {
                // Compressed block
                std::vector<uint8_t> blockInput(input.begin() + pos, input.begin() + pos + blockSize);
                pos += blockSize;
                std::vector<uint8_t> decompressed = decompressBlock(blockInput);
                output.insert(output.end(), decompressed.begin(), decompressed.end());
            }

            // Optional block checksum (4 bytes)
            if (hasBlockChecksum)
                pos += 4;
        }

        // Optional content checksum (4 bytes)
        // if (hasContentChecksum)
        //     pos += 4; // skip

        return output;
    }
};
