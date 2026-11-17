// imgconv.cpp - see imgconv.h. MIT licence (see LICENSE at the top of the repository).
#include "imgconv.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
extern "C" {
#include "pmimg.h"
}
#if defined(BOARD_HAS_PSRAM)
#include <Arduino.h>                      // psramFound (the AtomS3R build only)
#endif

namespace am {

// ===== the GIF writer =========================================================

static const unsigned kClear = 16;       // a 4-bit alphabet: 16 greys
static const unsigned kEoi = 17;
static const unsigned kFirstFree = 18;
static const size_t kHashSize = 5003;    // colour: the dictionary's hash table (a prime > 4096)

GifWriter::GifWriter()
	: iOut(0), iWidth(0), iHeight(0), iFirst(0), iNext(0), iSym(0), iDict(0), iColour(false), iHashKey(0),
	  iHashCode(0), iClear(kClear), iEoi(kEoi), iNextCode(0), iCodeBits(0),
	  iPrefix(-1), iBitBuf(0), iBitCount(0), iBlockLen(0), iWritten(0)
	{
	}

GifWriter::~GifWriter()
	{
	End();
	}

void GifWriter::Put(const uint8_t* aData, size_t aLen)
	{
	if (iOut && aLen)
		{
		iOut->Put((const char*)aData, aLen);
		iWritten += (uint32_t)aLen;
		}
	}

bool GifWriter::Begin(HtmlSink* aOut, int aWidth, int aHeight, bool aColour)
	{
	End();
	if (aWidth <= 0 || aHeight <= 0 || aWidth > 65535 || aHeight > 65535)
		return false;
	size_t n = kDictEntries;
	iColour = aColour;
	iClear = aColour ? 256 : kClear;
	iEoi = iClear + 1;
	if (aColour)
		{
		// (the grey dictionary's child lists would be long with 256
		// symbols: a hash of (prefix, symbol) instead)
		iDict = malloc(kHashSize * (sizeof(uint32_t) + sizeof(uint16_t)));
		if (!iDict)
			return false;
		iHashKey = (uint32_t*)iDict;
		iHashCode = (uint16_t*)(iHashKey + kHashSize);
		}
	else
		{
		iDict = malloc(n * sizeof(uint16_t) * 2 + n);
		if (!iDict)
			return false;
		}
	if (!aColour)
		{
		iFirst = (uint16_t*)iDict;
		iNext = iFirst + n;
		iSym = (uint8_t*)(iNext + n);
		}
	iOut = aOut;
	iWidth = aWidth;
	iHeight = aHeight;
	iBitBuf = 0;
	iBitCount = 0;
	iBlockLen = 0;
	iWritten = 0;
	iPrefix = -1;
	if (aColour)
		{
		// the header: GIF89a, the logical screen, a global colour table of
		// EPOC's 256 colours, the image descriptor, the minimum code size 8
		const uint8_t* pal = pmimg_epoc256();
		uint8_t h[13];
		size_t p = 0;
		memcpy(h + p, "GIF89a", 6); p += 6;
		h[p++] = (uint8_t)(aWidth & 255); h[p++] = (uint8_t)(aWidth >> 8);
		h[p++] = (uint8_t)(aHeight & 255); h[p++] = (uint8_t)(aHeight >> 8);
		h[p++] = 0xf7;                   // a global colour table, 256 entries, 8 bits a colour
		h[p++] = 255;                    // background: white (EPOC's last entry)
		h[p++] = 0;
		Put(h, p);
		Put(pal, 768);
		uint8_t d[11];
		p = 0;
		d[p++] = 0x2c;
		d[p++] = 0; d[p++] = 0; d[p++] = 0; d[p++] = 0;
		d[p++] = (uint8_t)(aWidth & 255); d[p++] = (uint8_t)(aWidth >> 8);
		d[p++] = (uint8_t)(aHeight & 255); d[p++] = (uint8_t)(aHeight >> 8);
		d[p++] = 0;
		d[p++] = 8;                      // LZW minimum code size
		Put(d, p);
		ClearDict();
		Code(iClear);
		return true;
		}
	// the header: GIF89a, the logical screen, a global colour table of 16
	// greys (level k is 17k), the image descriptor, the minimum code size
	uint8_t h[13 + 48 + 10 + 1];
	size_t p = 0;
	memcpy(h + p, "GIF89a", 6); p += 6;
	h[p++] = (uint8_t)(aWidth & 255); h[p++] = (uint8_t)(aWidth >> 8);
	h[p++] = (uint8_t)(aHeight & 255); h[p++] = (uint8_t)(aHeight >> 8);
	h[p++] = 0xf3;                       // a global colour table, 16 entries, 4 bits a colour
	h[p++] = 15;                         // background: white
	h[p++] = 0;                          // pixel aspect ratio: none
	for (int k = 0; k < 16; k++)
		{
		h[p++] = (uint8_t)(k * 17); h[p++] = (uint8_t)(k * 17); h[p++] = (uint8_t)(k * 17);
		}
	h[p++] = 0x2c;                       // image descriptor
	h[p++] = 0; h[p++] = 0; h[p++] = 0; h[p++] = 0;
	h[p++] = (uint8_t)(aWidth & 255); h[p++] = (uint8_t)(aWidth >> 8);
	h[p++] = (uint8_t)(aHeight & 255); h[p++] = (uint8_t)(aHeight >> 8);
	h[p++] = 0;                          // no local colour table, not interlaced
	h[p++] = 4;                          // LZW minimum code size
	Put(h, p);
	ClearDict();
	Code(kClear);
	return true;
	}

void GifWriter::ClearDict()
	{
	if (iColour)
		{
		ClearHash();
		iNextCode = 258;
		iCodeBits = 9;
		return;
		}
	memset(iFirst, 0, kDictEntries * sizeof(uint16_t));
	iNextCode = kFirstFree;
	iCodeBits = 5;
	}

void GifWriter::ClearHash()
	{
	memset(iHashKey, 0, kHashSize * sizeof(uint32_t));
	}

// colour: as Pixel, with the dictionary in a hash table (open addressing)
void GifWriter::Pixel8(uint8_t aV)
	{
	if (iPrefix < 0)
		{
		iPrefix = aV;
		return;
		}
	uint32_t key = (((uint32_t)iPrefix << 8) | aV) + 1;
	size_t h = (((size_t)aV << 12) ^ (size_t)iPrefix) % kHashSize;   // (as compress(1))
	while (iHashKey[h])
		{
		if (iHashKey[h] == key)
			{
			iPrefix = iHashCode[h];
			return;
			}
		if (++h == kHashSize)
			h = 0;
		}
	Code((unsigned)iPrefix);
	if (iNextCode < kDictEntries)
		{
		iHashKey[h] = key;
		iHashCode[h] = (uint16_t)iNextCode++;
		if (iNextCode > (1u << iCodeBits) && iCodeBits < 12)
			iCodeBits++;
		}
	else
		{
		Code(iClear);
		ClearDict();
		}
	iPrefix = aV;
	}

void GifWriter::Byte(uint8_t aB)
	{
	iBlock[iBlockLen++] = aB;
	if (iBlockLen == 255)
		FlushBlock();
	}

void GifWriter::FlushBlock()
	{
	if (!iBlockLen)
		return;
	uint8_t len = (uint8_t)iBlockLen;
	Put(&len, 1);
	Put(iBlock, iBlockLen);
	iBlockLen = 0;
	}

void GifWriter::Code(unsigned aCode)
	{
	iBitBuf |= aCode << iBitCount;
	iBitCount += (int)iCodeBits;
	while (iBitCount >= 8)
		{
		Byte((uint8_t)(iBitBuf & 255));
		iBitBuf >>= 8;
		iBitCount -= 8;
		}
	}

void GifWriter::Pixel(uint8_t aV)
	{
	if (iPrefix < 0)
		{
		iPrefix = aV;
		return;
		}
	// is prefix+v in the dictionary?
	for (unsigned c = iFirst[iPrefix]; c; c = iNext[c])
		if (iSym[c] == aV)
			{
			iPrefix = (int)c;
			return;
			}
	// no: output the prefix, add prefix+v, start again from v
	Code((unsigned)iPrefix);
	if (iNextCode < kDictEntries)
		{
		unsigned c = iNextCode++;
		iSym[c] = aV;
		iNext[c] = iFirst[iPrefix];
		iFirst[iPrefix] = (uint16_t)c;
		iFirst[c] = 0;
		// (the code just output was written with the old size; the size
		// grows once the new entry's code needs it)
		if (iNextCode > (1u << iCodeBits) && iCodeBits < 12)
			iCodeBits++;
		}
	else
		{
		Code(kClear);
		ClearDict();
		}
	iPrefix = aV;
	}

void GifWriter::Row(const uint8_t* aPacked)
	{
	if (!iDict)
		return;
	if (iColour)
		{
		for (int x = 0; x < iWidth; x++)
			Pixel8(aPacked[x]);
		return;
		}
	for (int x = 0; x < iWidth; x++)
		Pixel((uint8_t)((aPacked[x >> 1] >> ((x & 1) * 4)) & 15));
	}

void GifWriter::End()
	{
	if (!iDict)
		return;
	if (iPrefix >= 0)
		Code((unsigned)iPrefix);
	Code(iEoi);
	if (iBitCount > 0)
		Byte((uint8_t)(iBitBuf & 255));
	FlushBlock();
	static const uint8_t kTail[] = { 0, 0x3b };   // the block terminator, the trailer
	Put(kTail, 2);
	free(iDict);
	iDict = 0;
	iFirst = iNext = 0;
	iSym = 0;
	iHashKey = 0;
	iHashCode = 0;
	iOut = 0;
	}

// ===== the converter ===========================================================

int ImageType(const uint8_t* aHead, size_t aLen)
	{
	return aLen >= 8 ? pmimg_type(aHead, (int)aLen) : 0;
	}

ImageConverter::ImageConverter() : iImage(0), iW(0), iH(0), iSrcW(0), iSrcH(0), iRow(0), iStarted(false),
	iColour(false) {}

bool ImageConverter::ColourPossible()
	{
#if defined(BOARD_HAS_PSRAM)
	return psramFound();
#elif defined(ARDUINO)
	return false;                        // (the AtomS3 Lite and the plain Atom: no PSRAM)
#else
	return true;                         // a PC: the host tests, PsiConnect Host
#endif
	}

ImageConverter::~ImageConverter()
	{
	Reset();
	}

void ImageConverter::Reset()
	{
	iGif.End();
	if (iImage)
		{
		pmimg_free((PmImage*)iImage);
		free(iImage);
		iImage = 0;
		}
	iW = iH = iSrcW = iSrcH = 0;
	iRow = 0;
	iStarted = false;
	iColour = false;
	}

bool ImageConverter::Decode(const uint8_t* aData, size_t aLen, int aMaxWidth, char* aWhy, size_t aWhyMax,
	bool aColour)
	{
	Reset();
	if (aWhyMax)
		aWhy[0] = 0;
	if (aMaxWidth < 16)
		aMaxWidth = 16;
	PmImage* img = (PmImage*)malloc(sizeof(PmImage));
	if (!img)
		{
		snprintf(aWhy, aWhyMax, "no memory");
		return false;
		}
	memset(img, 0, sizeof(*img));
	PmImgOpts o;
	memset(&o, 0, sizeof(o));
	o.max_w = aMaxWidth;
	o.max_h = aMaxWidth * 3;
	o.max_src_pixels = 6L * 1000L * 1000L;    // (a 1/8 decode of this still needs a row of it)
	o.max_full_bytes = 96L * 1024L;           // interlaced files above this are refused (no PSRAM)
#if defined(BOARD_HAS_PSRAM)
	if (psramFound())                         // the AtomS3R: a whole interlaced file fits in PSRAM
		o.max_full_bytes = 2L * 1024L * 1024L;
#endif
	o.exact = 1;                              // full detail at the width asked (the Psion's own
	                                          // decoders settle for half, to save its CPU)
	if (aColour)
		{
		// the netBook: in colour, one byte a pixel (the result and, for an
		// interlaced file, the whole source at three bytes a pixel: PSRAM)
		o.colour = 1;
		o.max_h = kColourMaxHeight;
		o.max_full_bytes = 6L * 1024L * 1024L;
		}
	char err[64];
	int r = pmimg_decode_mem(aData, (long)aLen, &o, img, err, sizeof(err));
	if (r != PMIMG_OK)
		{
		snprintf(aWhy, aWhyMax, "%s", err[0] ? err : "not decoded");
		pmimg_free(img);
		free(img);
		return false;
		}
	iImage = img;
	iColour = img->colour != 0;
	iW = img->w;
	iH = img->h;
	iSrcW = img->src_w;
	iSrcH = img->src_h;
	iRow = 0;
	return true;
	}

bool ImageConverter::Write(HtmlSink* aOut, int aRows)
	{
	PmImage* img = (PmImage*)iImage;
	if (!img)
		return true;
	if (!iStarted)
		{
		iStarted = true;
		if (!iGif.Begin(aOut, iW, iH, iColour))
			return true;                         // (no memory: the caller sees nothing written)
		}
	while (iRow < iH && aRows-- > 0)
		{
		iGif.Row(img->bits + (size_t)iRow * (size_t)img->stride);
		iRow++;
		}
	if (iRow >= iH)
		{
		iGif.End();
		return true;
		}
	return false;
	}

} // namespace am
