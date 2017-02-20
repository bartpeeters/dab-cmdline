#
/*
 *    Copyright (C) 2013, 2014
 *    Jan van Katwijk (J.vanKatwijk@gmail.com)
 *    Lazy Chair Programming
 *
 *    This file is part of the DAB-library
 *    DAB-library is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    DAB-library is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with DAB-library; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
#
#ifndef	__MP4PROCESSOR
#define	__MP4PROCESSOR
/*
 * 	Handling superframes for DAB+ and delivering
 * 	frames into the ffmpeg or faad decoding library
 */
//
#include	"dab-constants.h"
#include	<stdio.h>
#include	<stdint.h>
#include	"dab-processor.h"
#include	"dab-api.h"
#include	"firecode-checker.h"
#include	"reed-solomon.h"
#include	"faad-decoder.h"
#include	"pad-handler.h"


class	mp4Processor : public dabProcessor {
public:
			mp4Processor	(int16_t,
	                                 cb_audio_t,
	                                 cb_data_t);
			~mp4Processor	(void);
	void		addtoFrame	(uint8_t *);
private:
	bool		processSuperframe (uint8_t [], int16_t);
	cb_audio_t	soundOut;
	cb_data_t	dataOut;
	padHandler	my_padHandler;
	void            handle_aacFrame (uint8_t *,
                                         int16_t,
                                         uint8_t,
                                         uint8_t,
                                         uint8_t,
                                         uint8_t,
                                         bool*);

	int16_t		superFramesize;
	int16_t		blockFillIndex;
	int16_t		blocksInBuffer;
	int16_t		blockCount;
	int16_t		bitRate;
	uint8_t		*frameBytes;
	uint8_t		**RSMatrix;
	int16_t		RSDims;
	int16_t		au_start	[10];
	int32_t		baudRate;

	firecode_checker	fc;
	reedSolomon	my_rsDecoder;
	uint8_t		*outVector;
//	and for the aac decoder
	faadDecoder	aacDecoder;

	int16_t		frameCount;
	int16_t		successFrames;
	int16_t		frameErrors;
	int16_t		rsErrors;
	int16_t		aacErrors;
	int16_t		aacFrames;
	int16_t		charSet;
	void            show_frameErrors        (int);
        void            show_rsErrors           (int);
        void            show_aacErrors          (int);

	void		isStereo		(bool);
};

#endif

