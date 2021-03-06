#
/*
 *    Copyright (C) 2014 .. 2017
 *    Jan van Katwijk (J.vanKatwijk@gmail.com)
 *    Lazy Chair Computing
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
 * 	superframer for the SDR-J DAB+ receiver (and derived programs)
 * 	This processor handles the whole DAB+ specific part
 ************************************************************************
 *	may 15 2015. A real improvement on the code
 *	is the addition from Stefan Poeschel to create a
 *	header for the aac that matches, really a big help!!!!
 ************************************************************************
 */
#include	"mp4processor.h"
#include	<cstring>
//
#include	"charsets.h"
#include	"pad-handler.h"

//
//	Now for real
/**
  *	\class mp4Processor is the main handler for the aac frames
  *	the class proper processes input and extracts the aac frames
  *	that are processed by the "faadDecoder" class
  */

	mp4Processor::mp4Processor (int16_t		bitRate,
	                            audioOut_t		soundOut,
	                            dataOut_t		dataOut,
	                            programQuality_t	mscQuality,
	                            motdata_t		motdata_Handler,
	                            void		*ctx):
	                                  my_padHandler (dataOut,
	                                                 motdata_Handler,
	                                                 ctx),
	                                  my_rsDecoder (8, 0435, 0, 1, 10),
	                                  aacDecoder (soundOut, ctx) {

	this	-> bitRate	= bitRate;	// input rate
	this	-> soundOut	= soundOut;
	this	-> mscQuality	= mscQuality;	//
	this	-> ctx		= ctx;
	superFramesize		= 110 * (bitRate / 8);
	RSDims			= bitRate / 8;
	frameBytes		= new uint8_t [RSDims * 120];	// input
	outVector		= new uint8_t [RSDims * 110];
	blockFillIndex	= 0;
	blocksInBuffer	= 0;

	blockFillIndex	= 0;
	blocksInBuffer	= 0;
	frameCount      = 0;
        frameErrors     = 0;
        aacErrors       = 0;
        aacFrames       = 0;
        successFrames   = 0;
        rsErrors        = 0;

	frame_quality	= 0;
	rs_quality	= 0;
	aac_quality	= 0;
}

	mp4Processor::~mp4Processor (void) {
	delete[]	frameBytes;
	delete[]	outVector;
}
//
//	we add vector for vector to the superframe. Once we have
//	5 lengths of "old" frames, we check
void	mp4Processor::addtoFrame (uint8_t *V) {
int16_t	i, j;
uint8_t	temp	= 0;
int16_t	nbits	= 24 * bitRate;
//
//	Note that the packing in the entry vector is still one bit
//	per Byte, nbits is the number of Bits (i.e. containing bytes)
	for (i = 0; i < nbits / 8; i ++) {	// in bytes
	   temp = 0;
	   for (j = 0; j < 8; j ++)
	      temp = (temp << 1) | (V [i * 8 + j] & 01);
	   frameBytes [blockFillIndex * nbits / 8 + i] = temp;
	}
//
	blocksInBuffer ++;
	blockFillIndex = (blockFillIndex + 1) % 5;
//
//	we take the last five blocks to look at
	if (blocksInBuffer >= 5) {
	   if (++frameCount >= 50) {
	      frameCount = 0;
	      frame_quality	= 2 * (50 - frameErrors);
	      if (mscQuality != nullptr)
	         mscQuality (frame_quality, rs_quality, aac_quality, ctx);
	      frameErrors = 0;
	   }

//	OK, we give it a try, check the fire code
	   if (fc. check (&frameBytes [blockFillIndex * nbits / 8]) &&
	       (processSuperframe (frameBytes,
	                           blockFillIndex * nbits / 8))) {
//	since we processed a full cycle of 5 blocks, we just start a
//	new sequence, beginning with block blockFillIndex
	      blocksInBuffer	= 0;
	      if (++successFrames > 25) {
	         rs_quality	= 4 * (25 - rsErrors);
                 successFrames  = 0;
                 rsErrors       = 0;
              }
	   }
	   else {	// virtual shift to left in block sizes
	      blocksInBuffer  = 4;
	      frameErrors ++;
	   }
	}
}
//
bool	mp4Processor::processSuperframe (uint8_t frameBytes [],
	                                 int16_t base) {
uint8_t		num_aus;
int16_t		i, j, k;
uint8_t		rsIn	[120];
uint8_t		rsOut	[110];
stream_parms	streamParameters;

/**	apply reed-solomon error repar
  *	OK, what we now have is a vector with RSDims * 120 uint8_t's
  *	Output is a vector with RSDims * 110 uint8_t's
  */
	for (j = 0; j < RSDims; j ++) {
	   int16_t ler	= 0;
	   for (k = 0; k < 120; k ++) 
	      rsIn [k] = frameBytes [(base + j + k * RSDims) % (RSDims * 120)];
//
	   ler = my_rsDecoder. dec (rsIn, rsOut, 135);
	   if (ler < 0)
	      return false;
	   for (k = 0; k < 110; k ++) 
	      outVector [j + k * RSDims] = rsOut [k];
	}
//
//	OK, the result is N * 110 * 8 bits 
//	bits 0 .. 15 is firecode
//	bit 16 is unused
	streamParameters. dacRate = (outVector [2] >> 6) & 01;	// bit 17
	streamParameters. sbrFlag = (outVector [2] >> 5) & 01;	// bit 18
	streamParameters. aacChannelMode = (outVector [2] >> 4) & 01;	// bit 19
	streamParameters. psFlag   = (outVector [2] >> 3) & 01;	// bit 20
	streamParameters. mpegSurround	= (outVector [2] & 07);	// bits 21 .. 23

	switch (2 * streamParameters. dacRate + streamParameters. sbrFlag) {
	   default:		// cannot happen
	   case 0:
	      num_aus = 4;
	      au_start [0] = 8;
	      au_start [1] = outVector [3] * 16 + (outVector [4] >> 4);
	      au_start [2] = (outVector [4] & 0xf) * 256 + outVector [5];
	      au_start [3] = outVector [6] * 16 + (outVector [7] >> 4);
	      au_start [4] = 110 *  (bitRate / 8);
	      break;
//
	   case 1:
	      num_aus = 2;
	      au_start [0] = 5;
	      au_start [1] = outVector [3] * 16 + (outVector [4] >> 4);
	      au_start [2] = 110 * (bitRate / 8);
	      break;
//
	   case 2:
	      num_aus = 6;
	      au_start [0] = 11;
	      au_start [1] = outVector [3] * 16 + (outVector [4] >> 4);
	      au_start [2] = (outVector [4] & 0xf) * 256 + outVector [5];
	      au_start [3] = outVector [6] * 16 + (outVector [7] >> 4);
	      au_start [4] = (outVector [7] & 0xf) * 256 +
	                     outVector [8];
	      au_start [5] = outVector [9] * 16 +
	                     (outVector [10] >> 4);
	      au_start [6] = 110 *  (bitRate / 8);
	      break;
//
	   case 3:
	      num_aus = 3;
	      au_start [0] = 6;
	      au_start [1] = outVector [3] * 16 + (outVector [4] >> 4);
	      au_start [2] = (outVector [4] & 0xf) * 256 +
	                     outVector [5];
	      au_start [3] = 110 * (bitRate / 8);
	      break;
	}
//
//	extract the AU'si and prepare a buffer, sufficiently
//	long for conversion to PCM samples

	for (i = 0; i < num_aus; i ++) {
	   int16_t	aac_frame_length;
//
	   if (au_start [i + 1] < au_start [i]) {
//	      fprintf (stderr, "%d %d\n", au_start [i + 1], au_start [i]);
	      return false;
	   }

	   aac_frame_length = au_start [i + 1] - au_start [i] - 2;
// sanity check
	   if ((aac_frame_length >= 960) || (aac_frame_length < 0)) {
	      return false;
	   }

//	but first the crc check
	   if (check_crc_bytes (&outVector [au_start [i]],
	                        aac_frame_length)) {
	      bool err;
//
//	if there is pad handle it always
	      if (((outVector [au_start [i] + 0] >> 5) & 07) == 4) {
	         int16_t count = outVector [au_start [i] + 1];
                 uint8_t buffer [count];
                 memcpy (buffer, &outVector [au_start [i] + 2], count);
                 uint8_t L0   = buffer [count - 1];
                 uint8_t L1   = buffer [count - 2];
                 my_padHandler. processPAD (buffer, count - 3, L1, L0);
              }
#ifdef	AAC_OUT
	      uint8_t fileBuffer [1024];
	      buildHeader (aac_frame_length, &streamParameters, fileBuffer);
	      memcpy (&fileBuffer [7], 
	              &outVector [au_start [i]],
	              aac_frame_length);
	      if (soundOut != nullptr) 
	         (soundOut)((int16_t *)(&fileBuffer [0]),
	                    aac_frame_length + 7, 0, false, nullptr);
#else	
//	we handle the aac -> PMC conversion here
	
	      uint8_t theAudioUnit [2 * 960 + 10];	// sure, large enough
	      memcpy (theAudioUnit,
	                       &outVector [au_start [i]], aac_frame_length);
	      memset (&theAudioUnit [aac_frame_length], 0, 10);

	      int tmp = aacDecoder. MP42PCM (&streamParameters,
	                                     theAudioUnit,
	                                     aac_frame_length);
	      err = tmp == 0;
//	      handle_aacFrame (&outVector [au_start [i]],
//	                       aac_frame_length,
//	                       &streamParameters,
//	                       &err);
	      isStereo (streamParameters. aacChannelMode);
	      if (err) 
	         aacErrors ++;
	      if (++aacFrames > 25) {
	         aac_quality	= 4 * (25 - aacErrors);
	         aacErrors	= 0;
	         aacFrames	= 0;
	      }
#endif
	   }
	   else {
	      fprintf (stderr, "CRC failure with dab+ frame should not happen\n");
	   }
	}
	return true;
}

void	mp4Processor::handle_aacFrame (uint8_t *v,
	                               int16_t frame_length,
	                               stream_parms *sp,
	                               bool	*error) {
uint8_t theAudioUnit [2 * 960 + 10];	// sure, large enough

	memcpy (theAudioUnit, v, frame_length);
	memset (&theAudioUnit [frame_length], 0, 10);

//	if (((theAudioUnit [0] >> 5) & 07) == 4) {
//	   int16_t count = theAudioUnit [1];
//           uint8_t buffer [count];
//           memcpy (buffer, &theAudioUnit [2], count);
//           uint8_t L0   = buffer [count - 1];
//           uint8_t L1   = buffer [count - 2];
//           my_padHandler. processPAD (buffer, count - 3, L1, L0);
//        }

	int tmp = aacDecoder. MP42PCM (sp,
	                               theAudioUnit,
	                               frame_length);
	*error	= tmp == 0;
}

void	mp4Processor::show_frameErrors	(int s) {
	(void)s;
}

void	mp4Processor::show_rsErrors	(int s) {
	(void)s;
}

void	mp4Processor::show_aacErrors	(int s) {
	(void)s;
}

void	mp4Processor::isStereo		(bool b) {
	(void)b;
}


void	mp4Processor::buildHeader (int16_t framelen,
	                           stream_parms *sp,
	                           uint8_t *header) {
	struct adts_fixed_header {
		unsigned                     : 4;
		unsigned home                : 1;
		unsigned orig                : 1;
		unsigned channel_config      : 3;
		unsigned private_bit         : 1;
		unsigned sampling_freq_index : 4;
		unsigned profile             : 2;
		unsigned protection_absent   : 1;
		unsigned layer               : 2;
		unsigned id                  : 1;
		unsigned syncword            : 12;
	} fh;
	struct adts_variable_header {
		unsigned                            : 4;
		unsigned num_raw_data_blks_in_frame : 2;
		unsigned adts_buffer_fullness       : 11;
		unsigned frame_length               : 13;
		unsigned copyright_id_start         : 1;
		unsigned copyright_id_bit           : 1;
	} vh;
	/* 32k 16k 48k 24k */
	const unsigned short samptab[] = {0x5, 0x8, 0x3, 0x6};

	fh. syncword = 0xfff;
	fh. id = 0;
	fh. layer = 0;
	fh. protection_absent = 1;
	fh. profile = 0;
	fh. sampling_freq_index = samptab [sp -> dacRate << 1 | sp -> sbrFlag];

	fh. private_bit = 0;
	switch (sp -> mpegSurround) {
	   default:
	      fprintf (stderr, "Unrecognized mpeg_surround_config ignored\n");
//	not nice, but deliberate: fall through
	   case 0:
	      if (sp -> sbrFlag && !sp -> aacChannelMode && sp -> psFlag)
	         fh. channel_config = 2; /* Parametric stereo */
	      else
	         fh. channel_config = 1 << sp -> aacChannelMode ;
	      break;

	   case 1:
	      fh. channel_config = 6;
	      break;
	}

	fh. orig = 0;
	fh. home = 0;
	vh. copyright_id_bit = 0;
	vh. copyright_id_start = 0;
	vh. frame_length = framelen + 7;  /* Includes header length */
	vh. adts_buffer_fullness = 1999;
	vh. num_raw_data_blks_in_frame = 0;
	header [0]	= fh. syncword >> 4;
	header [1]	= (fh. syncword & 0xf) << 4;
	header [1]	|= fh. id << 3;
	header [1]	|= fh. layer << 1;
	header [1]	|= fh. protection_absent;
        header [2]	= fh. profile << 6;
	header [2]	|= fh. sampling_freq_index << 2;
	header [2]	|= fh. private_bit << 1;
	header [2]	|= (fh. channel_config & 0x4);
	header [3]	= (fh. channel_config & 0x3) << 6;
	header [3]	|= fh. orig << 5;
	header [3]	|= fh. home << 4;
	header [3]	|= vh. copyright_id_bit << 3;
	header [3]	|= vh. copyright_id_start << 2;
	header [3]	|= (vh. frame_length >> 11) & 0x3;
	header [4]	= (vh. frame_length >> 3) & 0xff;
	header [5]	= (vh. frame_length & 0x7) << 5;
	header [5]	|= vh. adts_buffer_fullness >> 6;
	header [6]	= (vh. adts_buffer_fullness & 0x3f) << 2;
	header [6]	|= vh. num_raw_data_blks_in_frame;
}


