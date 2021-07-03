#include <cstdio>
#include <cstdlib>
#include <vector>

#include <pulse/simple.h>
#include <pulse/error.h>

#include "audiomixer.h"

size_t tell_callback(void *file_context)
{
    long result = ftell(reinterpret_cast<FILE *>(file_context));

    if (result >= 0) {
        return static_cast<size_t>(result);
    }

    return 0;
}

bool seek_callback(void *file_context, size_t offset)
{
    int result = fseek(reinterpret_cast<FILE *>(file_context),
                       static_cast<long>(offset),
                       SEEK_SET);

    if (result >= 0) {
        return true;
    }

    return false;
}

size_t read_callback(void *file_context, uint8_t *buffer, size_t length)
{
    return fread(buffer,
                 1,
                 length,
                 reinterpret_cast<FILE *>(file_context));
}

void track_end_callback(int track)
{
    fprintf(stderr, "Track %d ended\n", track);
}

int main(int argc, char *argv[])
{
    if (argc < 5) {
        fprintf(stderr, "Usage: %s <file_1> <file_2> <level_1> <level_2>\n", argv[0]);
        return 1;
    }

    FILE *wav_file_1 = fopen(argv[1], "r");
    if (wav_file_1 == nullptr) {
        fprintf(stderr, "Cannot open file \"%s\"\n", argv[1]);
        return 1;
    }

    FILE *wav_file_2 = fopen(argv[2], "r");
    if (wav_file_2 == nullptr) {
        fprintf(stderr, "Cannot open file \"%s\"\n", argv[2]);
        return 1;
    }

    WavReader reader_1(&tell_callback,
                       &seek_callback,
                       &read_callback);
    AudioTrack track_1(&reader_1, 2);

    WavReader reader_2(&tell_callback,
                       &seek_callback,
                       &read_callback);
    AudioTrack track_2(&reader_2, 2);

    AudioMixer mixer(&track_end_callback,
                     2);
    mixer.addTrack(&track_1);
    mixer.addTrack(&track_2);

    AudioTrack::Mode mode = AudioTrack::Mode::Single;

    uint16_t level_1 = static_cast<uint16_t>(atof(argv[3]) * AudioMixer::UNIT_LEVEL);
    uint16_t level_2 = static_cast<uint16_t>(atof(argv[4]) * AudioMixer::UNIT_LEVEL);

    if (mixer.start(wav_file_1, mode, true, level_1) < 0) {
        fprintf(stderr, "Cannot play file 1\n");
        return 1;
    }

    if (mixer.start(wav_file_2, mode, true, level_2) < 0) {
        fprintf(stderr, "Cannot play file 2\n");
        return 1;
    }

    pa_sample_spec sample_format = {
        .format = PA_SAMPLE_S16NE,
        .rate = static_cast<uint32_t>(mixer.samplingRate()),
        .channels = static_cast<uint8_t>(mixer.channels())
    };

    pa_simple *stream = nullptr;
    int error;

    stream = pa_simple_new(nullptr,
                           argv[0],
                           PA_STREAM_PLAYBACK,
                           nullptr,
                           "playback",
                           &sample_format,
                           nullptr,
                           nullptr,
                           &error);
    if (stream == nullptr) {
        fprintf(stderr,
                "pa_simple_new() failed: %s\n",
                pa_strerror(error));
        return 1;
    }

    static const size_t MAX_BUFFER_FRAMES = 1024;
    std::vector<int16_t> buffer(MAX_BUFFER_FRAMES * mixer.channels());

    for (;;) {
        size_t frames = mixer.play(buffer.data(), MAX_BUFFER_FRAMES);
        if (frames == 0) {
            break;
        }

        if (pa_simple_write(stream,
                            buffer.data(),
                            frames * mixer.channels() * sizeof(int16_t),
                            &error) < 0) {
            fprintf(stderr,
                    "pa_simple_write() failed: %s\n",
                    pa_strerror(error));
            pa_simple_free(stream);
            return 1;
        }
    }

    if (pa_simple_drain(stream, &error) < 0) {
        fprintf(stderr,
                "pa_simple_drain() failed: %s\n",
                pa_strerror(error));
        pa_simple_free(stream);
        return 1;
    }

    pa_simple_free(stream);

    return 0;
}
