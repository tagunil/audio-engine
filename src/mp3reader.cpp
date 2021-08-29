#include "mp3reader.h"

#include <cstring>

enum class Id3Flags : uint8_t
{
    Footer = 0x10,
    Experimental = 0x20,
    ExtendedHeader = 0x40,
    Unsynchronisation = 0x80,
};

static const size_t ID3_HEADER_SIZE = 10;
static const size_t ID3_FOOTER_SIZE = 10;

Mp3Reader::Mp3Reader(TellCallback tell_callback,
                     SeekCallback seek_callback,
                     ReadCallback read_callback)
    : AudioReader(tell_callback,
                  seek_callback,
                  read_callback),
      frame_header_(),
      side_info_(),
      scale_factor_info_(),
      huffman_info_(),
      dequant_info_(),
      imdct_info_(),
      subband_info_(),
      mp3_dec_info_(),
      initial_data_offset_(0),
      chunk_data_offset_(0),
      next_data_offset_(0),
      chunk_buffer_(),
      prefetched_bytes_(0),
      current_chunk_(nullptr),
      frame_buffer_(),
      decoded_frames_(0),
      current_frame_(nullptr),
      next_frame_(nullptr)
{
}

bool Mp3Reader::open(void *file,
                     Mode mode,
                     bool preload)
{
    opened_ = false;

    file_ = file;

    mode_ = mode;

    memset(&frame_header_, 0, sizeof(frame_header_));
    memset(&side_info_, 0, sizeof(side_info_));
    memset(&scale_factor_info_, 0, sizeof(scale_factor_info_));
    memset(&huffman_info_, 0, sizeof(huffman_info_));
    memset(&dequant_info_, 0, sizeof(dequant_info_));
    memset(&imdct_info_, 0, sizeof(imdct_info_));
    memset(&subband_info_, 0, sizeof(subband_info_));
    memset(&mp3_dec_info_, 0, sizeof(mp3_dec_info_));

    mp3_dec_info_.FrameHeaderPS = &frame_header_;
    mp3_dec_info_.SideInfoPS = &side_info_;
    mp3_dec_info_.ScaleFactorInfoPS = &scale_factor_info_;
    mp3_dec_info_.HuffmanInfoPS = &huffman_info_;
    mp3_dec_info_.DequantInfoPS = &dequant_info_;
    mp3_dec_info_.IMDCTInfoPS = &imdct_info_;
    mp3_dec_info_.SubbandInfoPS = &subband_info_;

    initial_data_offset_ = 0;
    next_data_offset_ = 0;

    if (!seek(next_data_offset_)) {
        return false;
    }

    char id3_signature[3];

    if (!readCharBuffer(id3_signature, sizeof(id3_signature))) {
        return false;
    }

    if (memcmp(id3_signature, "ID3", sizeof(id3_signature)) == 0) {
        uint8_t id3_version;

        if (!readU8(&id3_version)) {
            return false;
        }

        uint8_t id3_revision;

        if (!readU8(&id3_revision)) {
            return false;
        }

        uint8_t id3_flags;

        if (!readU8(&id3_flags)) {
            return false;
        }

        size_t id3_size = 0;

        for (unsigned int place = 0; place < 4; place++) {
            uint8_t id3_safe_byte;

            if (!readU8(&id3_safe_byte)) {
                return false;
            }

            id3_size = (id3_size << 7) | (id3_safe_byte & 0x7f);
        }

        id3_size += ID3_HEADER_SIZE;

        if ((id3_flags & static_cast<uint8_t>(Id3Flags::Footer)) != 0) {
            id3_size += ID3_FOOTER_SIZE;
        }

        next_data_offset_ += id3_size;
    }

    current_chunk_ = chunk_buffer_;
    chunk_data_offset_ = 0;
    prefetched_bytes_ = 0;

    Helix::MP3FrameInfo frame_info;
    memset(&frame_info, 0, sizeof(frame_info));

    while (true) {
        if (!findNextChunk()) {
            return false;
        }

        int result = Helix::MP3GetNextFrameInfo(&mp3_dec_info_, &frame_info, current_chunk_);
        if (result == Helix::ERR_MP3_NONE) {
            break;
        }

        current_chunk_ += 1;
        prefetched_bytes_ -= 1;
        chunk_data_offset_ += 1;
    }

    initial_data_offset_ = chunk_data_offset_;

    channels_ = frame_info.nChans;
    sampling_rate_ = frame_info.samprate;

    opened_ = true;

    rewind(preload);

    return true;
}

void Mp3Reader::close()
{
    opened_ = false;
}

void Mp3Reader::rewind(bool preload)
{
    next_data_offset_ = initial_data_offset_;

    current_chunk_ = chunk_buffer_;
    chunk_data_offset_ = 0;
    prefetched_bytes_ = 0;

    current_frame_ = frame_buffer_;
    next_frame_ = frame_buffer_;
    decoded_frames_ = 0;

    if (preload) {
        if (findNextChunk()) {
            decodeNextFrames();
        }
    }
}

size_t Mp3Reader::decodeToI16(int16_t *buffer, size_t frames, unsigned int upmixing)
{
    if (!opened_) {
        return 0;
    }

    int16_t *frame_pointer = buffer;
    size_t processed_frames = 0;

    while (processed_frames < frames) {
        size_t retrieved_frames = retrieveNextFrames(frames - processed_frames);
        if (retrieved_frames == 0) {
            break;
        }

        if (upmixing == 1) {
            size_t samples = retrieved_frames * channels_;
            memcpy(frame_pointer, current_frame_, samples * 2);
            frame_pointer += samples;
        } else {
            int16_t *sample_pointer = current_frame_;

            for (size_t frame_index = 0; frame_index < retrieved_frames; frame_index++) {
                for (unsigned int channel = 0; channel < channels_; channel++) {
                    int16_t sample = *sample_pointer;
                    sample_pointer++;

                    for (unsigned int copy = 0; copy < upmixing; copy++) {
                        *frame_pointer = sample;
                        frame_pointer++;
                    }
                }
            }
        }

        processed_frames += retrieved_frames;
    }

    return processed_frames;
}

inline size_t Mp3Reader::tell()
{
    return tell_callback_(file_);
}

inline bool Mp3Reader::seek(size_t offset)
{
    return seek_callback_(file_, offset);
}

inline size_t Mp3Reader::read(uint8_t *buffer, size_t length)
{
    return read_callback_(file_, buffer, length);
}

inline bool Mp3Reader::readU8(uint8_t *value)
{
    if (read(value, sizeof(uint8_t)) < sizeof(uint8_t)) {
        return false;
    }

    return true;
}

inline bool Mp3Reader::readCharBuffer(char *buffer, size_t length)
{
    if (read(reinterpret_cast<uint8_t *>(buffer),
             length) < length) {
        return false;
    }

    return true;
}

size_t Mp3Reader::retrieveNextFrames(size_t frames)
{
    bool do_rewind = mode_ == Mode::Continuous;

    while (decoded_frames_ == 0) {
        if (!findNextChunk()) {
            if (!do_rewind) {
                return 0;
            }

            do_rewind = false;
            rewind(false);

            continue;
        }

        if (!decodeNextFrames()) {
            if (!refillNextChunk()) {
                return 0;
            }

            continue;
        }

        next_frame_ = frame_buffer_;
    }

    current_frame_ = next_frame_;

    if (frames > decoded_frames_) {
        frames = decoded_frames_;
    }

    next_frame_ = current_frame_ + channels_ * frames;
    decoded_frames_ -= frames;

    return frames;
}

bool Mp3Reader::findNextChunk()
{
    int offset = Helix::MP3FindSyncWord(current_chunk_, prefetched_bytes_);
    while (offset < 0) {
        if (!seek(next_data_offset_)) {
            return false;
        }

        uint8_t *read_buffer = chunk_buffer_;
        size_t bytes_to_read = MP3READER_CHUNK_BUFFER_SIZE;

        if (prefetched_bytes_ > 0) {
            if ((current_chunk_[prefetched_bytes_ - 1] & SYNCWORDH) == SYNCWORDH) {
                chunk_buffer_[0] = SYNCWORDH;
                bytes_to_read--;
                read_buffer++;
            }
        }

        current_chunk_ = chunk_buffer_;
        prefetched_bytes_ = read_buffer - chunk_buffer_;
        chunk_data_offset_ = next_data_offset_ - prefetched_bytes_;

        size_t read_bytes = read(read_buffer, bytes_to_read);
        if (read_bytes < 1) {
            return false;
        }

        prefetched_bytes_ += read_bytes;
        next_data_offset_ += read_bytes;

        offset = Helix::MP3FindSyncWord(current_chunk_, prefetched_bytes_);
    }

    current_chunk_ += offset;
    prefetched_bytes_ -= offset;
    chunk_data_offset_ += offset;

    return true;
}

bool Mp3Reader::refillNextChunk()
{
    if (current_chunk_ == chunk_buffer_) {
        return false;
    }

    memmove(chunk_buffer_, current_chunk_, prefetched_bytes_);
    current_chunk_ = chunk_buffer_;

    if (!seek(next_data_offset_)) {
        return false;
    }

    uint8_t *read_buffer = chunk_buffer_ + prefetched_bytes_;
    size_t bytes_to_read = MP3READER_CHUNK_BUFFER_SIZE - prefetched_bytes_;

    size_t read_bytes = read(read_buffer, bytes_to_read);
    if (read_bytes < 1) {
        return false;
    }

    prefetched_bytes_ += read_bytes;
    next_data_offset_ += read_bytes;

    return true;
}

bool Mp3Reader::decodeNextFrames()
{
    int bytes_left = prefetched_bytes_;
    uint8_t *next_chunk = current_chunk_;

    decoded_frames_ = 0;

    int result = Helix::MP3Decode(&mp3_dec_info_, &next_chunk, &bytes_left, frame_buffer_, 0);
    if (result != Helix::ERR_MP3_NONE) {
        return false;
    }

    decoded_frames_ = mp3_dec_info_.nGrans * mp3_dec_info_.nGranSamps;

    chunk_data_offset_ += next_chunk - current_chunk_;
    prefetched_bytes_ -= next_chunk - current_chunk_;
    current_chunk_ = next_chunk;

    return true;
}
