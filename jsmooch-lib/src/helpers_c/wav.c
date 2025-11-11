//
// Created by . on 3/31/25.
//

#include <string.h>
#include "wav.h"

void wav_stream_create(wav_stream *this, char *fpath, u32 sampling_rate, u32 bytes_per_sample)
{
    memset(this, 0, sizeof(*this));
    struct wav_header *h = &this->header;

    h->sample_rate = sampling_rate;
    h->byte_rate = sampling_rate * bytes_per_sample;
    h->sample_alignment = 2;
    h->bit_depth = bytes_per_sample * 8;

    h->riff_header[0] = 'R';
    h->riff_header[1] = 'I';
    h->riff_header[2] = 'F';
    h->riff_header[3] = 'F';

    h->wave_header[0] = 'W';
    h->wave_header[1] = 'A';
    h->wave_header[2] = 'V';
    h->wave_header[3] = 'E';

    h->data_header[0] = 'd';
    h->data_header[1] = 'a';
    h->data_header[2] = 't';
    h->data_header[3] = 'a';

    h->fmt_header[0] = 'f';
    h->fmt_header[1] = 'm';
    h->fmt_header[2] = 't';
    h->fmt_header[3] = 0;
    h->num_channels = 1;
    h->audio_format = 1;
    h->fmt_chunk_sz = 16;
    h->file_sz = sizeof(wav_header) - 8;
    h->data_sz = 0;

    this->bytes_per_sample = bytes_per_sample;
    this->total_samples_written = 0;
    snprintf(this->fpath, sizeof(this->fpath), "%s", fpath);
    this->f = fopen(this->fpath, "wb");

    //fwrite(h, sizeof(*h), 1, this->f);
}

void wav_stream_output(wav_stream *this, u32 num_samples, void *data)
{
    if (this->f) {
        this->total_samples_written += num_samples;
        this->header.file_sz += num_samples * this->bytes_per_sample;
        this->header.data_sz += num_samples * this->bytes_per_sample;

        fwrite(data, 1, num_samples*this->bytes_per_sample, this->f);
        if (this->total_samples_written > (20 * this->header.sample_rate)) {
            printf("\nCLOSING FILE!");
            wav_stream_close(this);
        }
    }
    else {
        printf("\nBAD WAV!?");
    }
}

void wav_stream_close(wav_stream *this)
{
    if (this->f) {
        //fseek(this->f, 0, SEEK_SET);
        //fwrite(&this->header, sizeof(this->header), 1, this->f);

        fclose(this->f);
        this->f = NULL;

    }
}

