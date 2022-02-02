//
// Copyright (C) 2005 M. Bohge (bohge@tkn.tu-berlin.de), M. Renwanz
// Copyright (C) 2010 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_AUDIOOUTFILE_H
#define __INET_AUDIOOUTFILE_H

#define __STDC_CONSTANT_MACROS

#include "inet/common/INETDefs.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/mathematics.h>
};

namespace inet {

/**
 * Records audio into a file.
 */
class INET_API AudioOutFile
{
  public:
    AudioOutFile() : opened(false), audio_st(nullptr), oc(nullptr) {};
    ~AudioOutFile();

    void open(const char *resultFile, int sampleRate, short int sampleBits);
    void write(void *inbuf, int inbytes);
    bool close();
    bool isOpen() const { return opened; }

  protected:
    void addAudioStream(enum AVCodecID codec_id, int sampleRate, short int sampleBits);

  protected:
    bool opened;
    AVStream *audio_st;
    AVFormatContext *oc;
};

} // namespace inet

#endif

