#pragma once

#include <cstddef>
#include <cstdint>

namespace Helix
{
#include <mp3common.h>
}

#include "audioreader.h"

#ifndef MP3READER_BUFFER_SIZE
#define MP3READER_BUFFER_SIZE 2048
#endif

#define MP3READER_CHUNK_BUFFER_SIZE MP3READER_BUFFER_SIZE
#define MP3READER_FRAME_BUFFER_SIZE 4608

class Mp3Reader : public AudioReader
{
public:
    static const unsigned int MAX_CHANNELS = 2;

    static const unsigned int MAX_FRAME_SIZE = 16;

public:
    Mp3Reader(TellCallback tell_callback,
              SeekCallback seek_callback,
              ReadCallback read_callback);

    bool open(void *file,
              Mode mode = Mode::Single,
              bool preload = true) override;

    void close() override;

    void rewind(bool preload = true) override;

    size_t decodeToI16(int16_t *buffer, size_t frames, unsigned int upmixing = 1) override;

    unsigned int bitsPerSample()
    {
        return 16;
    }

private:
    inline size_t tell();
    inline bool seek(size_t offset);
    inline size_t read(uint8_t *buffer, size_t length);

    inline bool readU8(uint8_t *value);
    inline bool readCharBuffer(char *buffer, size_t length);

    size_t retrieveNextFrames(size_t frames);

    bool findNextChunk();
    bool refillNextChunk();
    bool decodeNextFrames();

private:
    Helix::FrameHeader frame_header_;
    Helix::SideInfo side_info_;
    Helix::ScaleFactorInfo scale_factor_info_;
    Helix::HuffmanInfo huffman_info_;
    Helix::DequantInfo dequant_info_;
    Helix::IMDCTInfo imdct_info_;
    Helix::SubbandInfo subband_info_;
    Helix::MP3DecInfo mp3_dec_info_;

    size_t initial_data_offset_;
    size_t chunk_data_offset_;
    size_t next_data_offset_;

    alignas(4) uint8_t chunk_buffer_[MP3READER_CHUNK_BUFFER_SIZE];
    size_t prefetched_bytes_;
    uint8_t *current_chunk_;

    alignas(4) int16_t frame_buffer_[MP3READER_FRAME_BUFFER_SIZE / 2];
    size_t decoded_frames_;
    int16_t *current_frame_;
    int16_t *next_frame_;
};
