//====== Copyright (c) 2014, Valve Corporation, All rights reserved. ========//
//
// Redistribution and use in source and binary forms, with or without 
// modification, are permitted provided that the following conditions are met:
//
// Redistributions of source code must retain the above copyright notice, this
// list of conditions and the following disclaimer.
// Redistributions in binary form must reproduce the above copyright notice, 
// this list of conditions and the following disclaimer in the documentation 
// and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE 
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF 
// THE POSSIBILITY OF SUCH DAMAGE.
//===========================================================================//

#include <assert.h>
#include "packetbitbuf.h"

const uint32 CBitRead::s_nMaskTable[33] = {
	0,
	(1 << 1) - 1,
	(1 << 2) - 1,
	(1 << 3) - 1,
	(1 << 4) - 1,
	(1 << 5) - 1,
	(1 << 6) - 1,
	(1 << 7) - 1,
	(1 << 8) - 1,
	(1 << 9) - 1,
	(1 << 10) - 1,
	(1 << 11) - 1,
	(1 << 12) - 1,
	(1 << 13) - 1,
	(1 << 14) - 1,
	(1 << 15) - 1,
	(1 << 16) - 1,
	(1 << 17) - 1,
	(1 << 18) - 1,
	(1 << 19) - 1,
	(1 << 20) - 1,
	(1 << 21) - 1,
	(1 << 22) - 1,
	(1 << 23) - 1,
	(1 << 24) - 1,
	(1 << 25) - 1,
	(1 << 26) - 1,
	(1 << 27) - 1,
	(1 << 28) - 1,
	(1 << 29) - 1,
	(1 << 30) - 1,
	0x7fffffff,
	0xffffffff,
};

int CBitRead::GetNumBitsRead(void) const
{
	if (!m_pData)									   // pesky null ptr bitbufs. these happen.
		return 0;

	int nCurOfs = ((int(m_pDataIn) - int(m_pData)) / 4) - 1;
	nCurOfs *= 32;
	nCurOfs += (32 - m_nBitsAvail);
	int nAdjust = 8 * (m_nDataBytes & 3);
	return MIN(nCurOfs + nAdjust, m_nDataBits);
}

int CBitRead::GetNumBytesRead(void) const
{
	return ((GetNumBitsRead() + 7) >> 3);
}

void CBitRead::GrabNextDWord(bool bOverFlowImmediately)
{
	if (m_pDataIn == m_pBufferEnd)
	{
		m_nBitsAvail = 1;									// so that next read will run out of words
		m_nInBufWord = 0;
		m_pDataIn++;										// so seek count increments like old
		if (bOverFlowImmediately)
		{
			SetOverflowFlag();
		}
	}
	else 
	{
		if (m_pDataIn > m_pBufferEnd)
		{
			SetOverflowFlag();
			m_nInBufWord = 0;
		}
		else
		{
			//assert(reinterpret_cast< int >(m_pDataIn)+3 < reinterpret_cast< int >(m_pBufferEnd));
			m_nInBufWord = *(m_pDataIn++);
		}
	}
}

void CBitRead::FetchNext(void)
{
	m_nBitsAvail = 32;
	GrabNextDWord(false);
}

int CBitRead::ReadOneBit(void)
{
	int nRet = m_nInBufWord & 1;
	if (--m_nBitsAvail == 0)
	{
		FetchNext();
	}
	else
	{
		m_nInBufWord >>= 1;
	}
	return nRet;
}

int CBitRead::ReadBit(void)
{
	int nRet = m_nInBufWord & 1;
	bool v10 = m_nBitsAvail-- == 1;
	if (v10)
	{
		m_nBitsAvail = 32;
		if (m_pDataIn == m_pBufferEnd)
		{
			m_nBitsAvail = 1;// so that next read will run out of words
			m_nInBufWord = 0;
			m_pDataIn++;// so seek count increments like old
		}
		if (m_pDataIn > m_pBufferEnd)
		{
			m_bOverflow = true;
			m_nInBufWord = 0;
		}	
		//assert(reinterpret_cast< int >(m_pDataIn)+3 < reinterpret_cast< int >(m_pBufferEnd));
		m_nInBufWord = *(m_pDataIn++);
	}
	else
	{
		m_nInBufWord >>= 1;
	}
	return nRet;
}

unsigned int CBitRead::ReadUBitLong(int numbits)
{
	if (m_nBitsAvail >= numbits)
	{
		unsigned int nRet = m_nInBufWord & s_nMaskTable[numbits];
		m_nBitsAvail -= numbits;
		if (m_nBitsAvail)
		{
			m_nInBufWord >>= numbits;
		}
		else
		{
			FetchNext();
		}
		return nRet;
	}
	else
	{
		// need to merge words
		unsigned int nRet = m_nInBufWord;
		numbits -= m_nBitsAvail;
		GrabNextDWord(true);
		if (m_bOverflow)
			return 0;
		nRet |= ((m_nInBufWord & s_nMaskTable[numbits]) << m_nBitsAvail);
		m_nBitsAvail = 32 - numbits;
		m_nInBufWord >>= numbits;
		return nRet;
	}
}

int CBitRead::ReadSBitLong(int numbits)
{
	int nRet = ReadUBitLong(numbits);
	// sign extend
	return (nRet << (32 - numbits)) >> (32 - numbits);
}

#ifdef _WIN32
#pragma warning(push)
#pragma warning(disable : 4715)								// disable warning on not all cases
// returning a value. throwing default:
// in measurably reduces perf in bit
// packing benchmark
#endif

unsigned int CBitRead::ReadVarUBit()
{
	unsigned int ret = ReadUBitLong(7);
	switch (ret & 0x60)
	{
	case 32:
		ret = 32 * ReadUBitLong(2) | ret & 0xFFFFFF9F;
		break;
	case 64:
		ret = 32 * ReadUBitLong(4) | ret & 0xFFFFFF9F;
		break;
	case 96:
		ret = 32 * ReadUBitLong(7) | ret & 0xFFFFFF9F;
		break;
	}
	return ret;
}

unsigned int CBitRead::ReadUBitVar(void)
{
	unsigned int ret = ReadUBitLong(6);
	switch (ret & (16 | 32))
	{
	case 16:
		ret = (ret & 15) | (ReadUBitLong(4) << 4);
		assert(ret >= 16);
		break;

	case 32:
		ret = (ret & 15) | (ReadUBitLong(8) << 4);
		assert(ret >= 256);
		break;
	case 48:
		ret = (ret & 15) | (ReadUBitLong(32 - 4) << 4);
		assert(ret >= 4096);
		break;
	}
	return ret;
}
#ifdef _WIN32
#pragma warning(pop)
#endif

int CBitRead::ReadChar(void)
{
	return ReadSBitLong(sizeof(char) << 3);
}

int CBitRead::ReadByte(void)
{
	return ReadUBitLong(sizeof(unsigned char) << 3);
}

int CBitRead::ReadShort(void)
{
	return ReadSBitLong(sizeof(short) << 3);
}

int CBitRead::ReadWord(void)
{
	return ReadUBitLong(sizeof(unsigned short) << 3);
}

bool CBitRead::Seek(int nPosition)
{
	bool bSucc = true;
	if (nPosition < 0 || nPosition > m_nDataBits)
	{
		SetOverflowFlag();
		bSucc = false;
		nPosition = m_nDataBits;
	}
	int nHead = m_nDataBytes & 3;							// non-multiple-of-4 bytes at head of buffer. We put the "round off"
	// at the head to make reading and detecting the end efficient.

	int nByteOfs = nPosition / 8;
	if ((m_nDataBytes < 4) || (nHead && (nByteOfs < nHead)))
	{
		// partial first dword
		unsigned char const *pPartial = (unsigned char const *)m_pData;
		if (m_pData)
		{
			m_nInBufWord = *(pPartial++);
			if (nHead > 1)
			{
				m_nInBufWord |= (*pPartial++) << 8;
			}
			if (nHead > 2)
			{
				m_nInBufWord |= (*pPartial++) << 16;
			}
		}
		m_pDataIn = (uint32 const *)pPartial;
		m_nInBufWord >>= (nPosition & 31);
		m_nBitsAvail = (nHead << 3) - (nPosition & 31);
	}
	else
	{
		int nAdjPosition = nPosition - (nHead << 3);
		m_pDataIn = reinterpret_cast< uint32 const * > (reinterpret_cast< unsigned char const * >(m_pData)+((nAdjPosition / 32) << 2) + nHead);
		if (m_pData)
		{
			m_nBitsAvail = 32;
			GrabNextDWord();
		}
		else
		{
			m_nInBufWord = 0;
			m_nBitsAvail = 1;
		}
		m_nInBufWord >>= (nAdjPosition & 31);
		m_nBitsAvail = MIN(m_nBitsAvail, 32 - (nAdjPosition & 31));	// in case grabnextdword overflowed
	}
	return bSucc;
}


void CBitRead::StartReading(const void *pData, int nBytes, int iStartBit, int nBits)
{
	// Make sure it's dword aligned and padded.
	assert(((unsigned long)pData & 3) == 0);
	m_pData = (uint32 *)pData;
	m_pDataIn = m_pData;
	m_nDataBytes = nBytes;

	if (nBits == -1)
	{
		m_nDataBits = nBytes << 3;
	}
	else
	{
		assert(nBits <= nBytes * 8);
		m_nDataBits = nBits;
	}
	m_bOverflow = false;
	m_pBufferEnd = reinterpret_cast< uint32 const * > (reinterpret_cast< unsigned char const * >(m_pData)+nBytes);
	if (m_pData)
	{
		Seek(iStartBit);
	}
}

bool CBitRead::ReadString(char *pStr, int maxLen, bool bLine, int *pOutNumChars)
{
	assert(maxLen != 0);

	bool bTooSmall = false;
	int iChar = 0;
	while (1)
	{
		char val = ReadChar();
		if (val == 0)
			break;
		else if (bLine && val == '\n')
			break;

		if (iChar < (maxLen - 1))
		{
			pStr[iChar] = val;
			++iChar;
		}
		else
		{
			bTooSmall = true;
		}
	}

	// Make sure it's null-terminated.
	assert(iChar < maxLen);
	pStr[iChar] = 0;

	if (pOutNumChars)
	{
		*pOutNumChars = iChar;
	}

	return !IsOverflowed() && !bTooSmall;
}

// Read 1-5 bytes in order to extract a 32-bit unsigned value from the
// stream. 7 data bits are extracted from each byte with the 8th bit used
// to indicate whether the loop should continue.
// This allows variable size numbers to be stored with tolerable
// efficiency. Numbers sizes that can be stored for various numbers of
// encoded bits are:
//  8-bits: 0-127
// 16-bits: 128-16383
// 24-bits: 16384-2097151
// 32-bits: 2097152-268435455
// 40-bits: 268435456-0xFFFFFFFF
uint32 CBitRead::ReadVarInt32()
{
	uint32 result = 0;
	int count = 0;
	uint32 b;

	do
	{
		if (count == bitbuf::kMaxVarint32Bytes)
		{
			return result;
		}
		b = ReadUBitLong(8);
		result |= (b & 0x7F) << (7 * count);
		++count;
	} while (b & 0x80);

	return result;
}

uint64 CBitRead::ReadVarInt64()
{
	uint64 result = 0;
	int count = 0;
	uint64 b;

	do
	{
		if (count == bitbuf::kMaxVarintBytes)
		{
			return result;
		}
		b = ReadUBitLong(8);
		result |= static_cast<uint64>(b & 0x7F) << (7 * count);
		++count;
	} while (b & 0x80);

	return result;
}

void CBitRead::ReadBits(void *pOutData, int nBits)
{
	unsigned char *pOut = (unsigned char*)pOutData;
	int nBitsLeft = nBits;


	// align output to dword boundary
	while (((unsigned long)pOut & 3) != 0 && nBitsLeft >= 8)
	{
		*pOut = (unsigned char)ReadUBitLong(8);
		++pOut;
		nBitsLeft -= 8;
	}

	// read dwords
	while (nBitsLeft >= 32)
	{
		*((unsigned long*)pOut) = ReadUBitLong(32);
		pOut += sizeof(unsigned long);
		nBitsLeft -= 32;
	}

	// read remaining bytes
	while (nBitsLeft >= 8)
	{
		*pOut = ReadUBitLong(8);
		++pOut;
		nBitsLeft -= 8;
	}

	// read remaining bits
	if (nBitsLeft)
	{
		*pOut = ReadUBitLong(nBitsLeft);
	}

}

bool CBitRead::ReadBytes(void *pOut, int nBytes)
{
	ReadBits(pOut, nBytes << 3);
	return !IsOverflowed();
}

#define BITS_PER_INT		32
inline int GetBitForBitnum(int bitNum)
{
	static int bitsForBitnum[] =
	{
		(1 << 0),
		(1 << 1),
		(1 << 2),
		(1 << 3),
		(1 << 4),
		(1 << 5),
		(1 << 6),
		(1 << 7),
		(1 << 8),
		(1 << 9),
		(1 << 10),
		(1 << 11),
		(1 << 12),
		(1 << 13),
		(1 << 14),
		(1 << 15),
		(1 << 16),
		(1 << 17),
		(1 << 18),
		(1 << 19),
		(1 << 20),
		(1 << 21),
		(1 << 22),
		(1 << 23),
		(1 << 24),
		(1 << 25),
		(1 << 26),
		(1 << 27),
		(1 << 28),
		(1 << 29),
		(1 << 30),
		(1 << 31),
	};

	return bitsForBitnum[(bitNum)& (BITS_PER_INT - 1)];
}

float CBitRead::ReadBitAngle(int numbits)
{
	float shift = (float)(GetBitForBitnum(numbits));

	int i = ReadUBitLong(numbits);
	float fReturn = (float)i * (360.0f / shift);

	return fReturn;
}

// Basic Coordinate Routines (these contain bit-field size AND fixed point scaling constants)
float CBitRead::ReadBitCoord(void)
{
	int		intval = 0, fractval = 0, signbit = 0;
	float	value = 0.0;


	// Read the required integer and fraction flags
	intval = ReadOneBit();
	fractval = ReadOneBit();

	// If we got either parse them, otherwise it's a zero.
	if (intval || fractval)
	{
		// Read the sign bit
		signbit = ReadOneBit();

		// If there's an integer, read it in
		if (intval)
		{
			// Adjust the integers from [0..MAX_COORD_VALUE-1] to [1..MAX_COORD_VALUE]
			intval = ReadUBitLong(COORD_INTEGER_BITS) + 1;
		}

		// If there's a fraction, read it in
		if (fractval)
		{
			fractval = ReadUBitLong(COORD_FRACTIONAL_BITS);
		}

		// Calculate the correct floating point value
		value = intval + ((float)fractval * COORD_RESOLUTION);

		// Fixup the sign if negative.
		if (signbit)
			value = -value;
	}

	return value;
}

float CBitRead::ReadBitCoordMP(EBitCoordType coordType)
{
	bool bIntegral = (coordType == kCW_Integral);
	bool bLowPrecision = (coordType == kCW_LowPrecision);

	int		intval = 0, fractval = 0, signbit = 0;
	float	value = 0.0;

	bool bInBounds = ReadOneBit() ? true : false;

	if (bIntegral)
	{
		// Read the required integer and fraction flags
		intval = ReadOneBit();
		// If we got either parse them, otherwise it's a zero.
		if (intval)
		{
			// Read the sign bit
			signbit = ReadOneBit();

			// If there's an integer, read it in
			// Adjust the integers from [0..MAX_COORD_VALUE-1] to [1..MAX_COORD_VALUE]
			if (bInBounds)
			{
				value = (float)(ReadUBitLong(COORD_INTEGER_BITS_MP) + 1);
			}
			else
			{
				value = (float)(ReadUBitLong(COORD_INTEGER_BITS) + 1);
			}
		}
	}
	else
	{
		// Read the required integer and fraction flags
		intval = ReadOneBit();

		// Read the sign bit
		signbit = ReadOneBit();

		// If we got either parse them, otherwise it's a zero.
		if (intval)
		{
			if (bInBounds)
			{
				intval = ReadUBitLong(COORD_INTEGER_BITS_MP) + 1;
			}
			else
			{
				intval = ReadUBitLong(COORD_INTEGER_BITS) + 1;
			}
		}

		// If there's a fraction, read it in
		fractval = ReadUBitLong(bLowPrecision ? COORD_FRACTIONAL_BITS_MP_LOWPRECISION : COORD_FRACTIONAL_BITS);

		// Calculate the correct floating point value
		value = intval + ((float)fractval * (bLowPrecision ? COORD_RESOLUTION_LOWPRECISION : COORD_RESOLUTION));
	}

	// Fixup the sign if negative.
	if (signbit)
		value = -value;

	return value;
}

float CBitRead::ReadBitCellCoord(int bits, EBitCoordType coordType)
{
	bool bIntegral = (coordType == kCW_Integral);
	bool bLowPrecision = (coordType == kCW_LowPrecision);

	int		intval = 0, fractval = 0;
	float	value = 0.0;

	if (bIntegral)
	{
		value = (float)(ReadUBitLong(bits));
	}
	else
	{
		intval = ReadUBitLong(bits);

		// If there's a fraction, read it in
		fractval = ReadUBitLong(bLowPrecision ? COORD_FRACTIONAL_BITS_MP_LOWPRECISION : COORD_FRACTIONAL_BITS);

		// Calculate the correct floating point value
		value = intval + ((float)fractval * (bLowPrecision ? COORD_RESOLUTION_LOWPRECISION : COORD_RESOLUTION));
	}

	return value;
}

void CBitRead::ReadBitVec3Coord(Vector& fa)
{
	int		xflag, yflag, zflag;

	// This vector must be initialized! Otherwise, If any of the flags aren't set, 
	// the corresponding component will not be read and will be stack garbage.
	fa.Init(0, 0, 0);

	xflag = ReadOneBit();
	yflag = ReadOneBit();
	zflag = ReadOneBit();

	if (xflag)
		fa.x = ReadBitCoord();
	if (yflag)
		fa.y = ReadBitCoord();
	if (zflag)
		fa.z = ReadBitCoord();
}

float CBitRead::ReadBitNormal(void)
{
	// Read the sign bit
	int	signbit = ReadOneBit();

	// Read the fractional part
	unsigned int fractval = ReadUBitLong(NORMAL_FRACTIONAL_BITS);

	// Calculate the correct floating point value
	float value = (float)fractval * NORMAL_RESOLUTION;

	// Fixup the sign if negative.
	if (signbit)
		value = -value;

	return value;
}

void CBitRead::ReadBitVec3Normal(Vector& fa)
{
	int xflag = ReadOneBit();
	int yflag = ReadOneBit();

	if (xflag)
		fa.x = ReadBitNormal();
	else
		fa.x = 0.0f;

	if (yflag)
		fa.y = ReadBitNormal();
	else
		fa.y = 0.0f;

	// The first two imply the third (but not its sign)
	int znegative = ReadOneBit();

	float fafafbfb = fa.x * fa.x + fa.y * fa.y;
	if (fafafbfb < 1.0f)
		fa.z = sqrt(1.0f - fafafbfb);
	else
		fa.z = 0.0f;

	if (znegative)
		fa.z = -fa.z;
}

void CBitRead::ReadBitAngles(QAngle& fa)
{
	Vector tmp;
	ReadBitVec3Coord(tmp);
	fa.Init(tmp.x, tmp.y, tmp.z);
}

float CBitRead::ReadBitFloat(void)
{
	uint32 nvalue = ReadUBitLong(32);
	return *((float *)&nvalue);
}


////========= Copyright Valve Corporation, All rights reserved. ============//
////
//// Purpose: 
////
//// $NoKeywords: $
////
////=============================================================================//
//
//#include "packetbitbuf.h"
//#include "coordsize.h"
//#include "bitvec.h"
//
//// FIXME: Can't use this until we get multithreaded allocations in tier0 working for tools
//// This is used by VVIS and fails to link
//// NOTE: This must be the last file included!!!
////#include "tier0/memdbgon.h"
//
//#ifdef _X360
//// mandatory ... wary of above comment and isolating, tier0 is built as MT though
//#include "tier0/memdbgon.h"
//#endif
//
//#if _WIN32
//#define FAST_BIT_SCAN 1
//#if _X360
//#define CountLeadingZeros(x) _CountLeadingZeros(x)
//inline unsigned int CountTrailingZeros(unsigned int elem)
//{
//	// this implements CountTrailingZeros() / BitScanForward()
//	unsigned int mask = elem - 1;
//	unsigned int comp = ~elem;
//	elem = mask & comp;
//	return (32 - _CountLeadingZeros(elem));
//}
//#else
//#include <intrin.h>
//#pragma intrinsic(_BitScanReverse)
//#pragma intrinsic(_BitScanForward)
//
//inline unsigned int CountLeadingZeros(unsigned int x)
//{
//	unsigned long firstBit;
//	if (_BitScanReverse(&firstBit, x))
//		return 31 - firstBit;
//	return 32;
//}
//inline unsigned int CountTrailingZeros(unsigned int elem)
//{
//	unsigned long out;
//	if (_BitScanForward(&out, elem))
//		return out;
//	return 32;
//}
//
//#endif
//#else
//#define FAST_BIT_SCAN 0
//#endif
//
//
//inline int BitForBitnum(int bitnum)
//{
//	return GetBitForBitnum(bitnum);
//}
//
//// #define BB_PROFILING
//
//unsigned long g_LittleBits[32];
//
//// Precalculated bit masks for WriteUBitLong. Using these tables instead of 
//// doing the calculations gives a 33% speedup in WriteUBitLong.
//unsigned long g_BitWriteMasks[32][33];
//
//// (1 << i) - 1
//unsigned long g_ExtraMasks[33];
//
//class CBitWriteMasksInit
//{
//public:
//	CBitWriteMasksInit()
//	{
//		for (unsigned int startbit = 0; startbit < 32; startbit++)
//		{
//			for (unsigned int nBitsLeft = 0; nBitsLeft < 33; nBitsLeft++)
//			{
//				unsigned int endbit = startbit + nBitsLeft;
//				g_BitWriteMasks[startbit][nBitsLeft] = BitForBitnum(startbit) - 1;
//				if (endbit < 32)
//					g_BitWriteMasks[startbit][nBitsLeft] |= ~(BitForBitnum(endbit) - 1);
//			}
//		}
//
//		for (unsigned int maskBit = 0; maskBit < 32; maskBit++)
//			g_ExtraMasks[maskBit] = BitForBitnum(maskBit) - 1;
//		g_ExtraMasks[32] = ~0ul;
//
//		for (unsigned int littleBit = 0; littleBit < 32; littleBit++)
//			StoreLittleDWord(&g_LittleBits[littleBit], 0, 1u << littleBit);
//	}
//};
//static CBitWriteMasksInit g_BitWriteMasksInit;
//
//
//// ---------------------------------------------------------------------------------------- //
//// bf_write
//// ---------------------------------------------------------------------------------------- //
//
//bf_write::bf_write()
//{
//	m_pData = NULL;
//	m_nDataBytes = 0;
//	m_nDataBits = -1; // set to -1 so we generate overflow on any operation
//	m_iCurBit = 0;
//	m_bOverflow = false;
//	m_bAssertOnOverflow = true;
//	m_pDebugName = NULL;
//}
//
//bf_write::bf_write(const char *pDebugName, void *pData, int nBytes, int nBits)
//{
//	m_bAssertOnOverflow = true;
//	m_pDebugName = pDebugName;
//	StartWriting(pData, nBytes, 0, nBits);
//}
//
//bf_write::bf_write(void *pData, int nBytes, int nBits)
//{
//	m_bAssertOnOverflow = true;
//	m_pDebugName = NULL;
//	StartWriting(pData, nBytes, 0, nBits);
//}
//
//void bf_write::StartWriting(void *pData, int nBytes, int iStartBit, int nBits)
//{
//	// Make sure it's dword aligned and padded.
//	if ((nBytes % 4) == 0)
//	{
//
//		if (((unsigned long)pData & 3) == 0)
//		{
//
//			// The writing code will overrun the end of the buffer if it isn't dword aligned, so truncate to force alignment
//			nBytes &= ~3;
//
//			m_pData = (unsigned long*)pData;
//			m_nDataBytes = nBytes;
//
//			if (nBits == -1)
//			{
//				m_nDataBits = nBytes << 3;
//			}
//			else
//			{
//				if (nBits <= nBytes * 8)
//					m_nDataBits = nBits;
//			}
//
//			m_iCurBit = iStartBit;
//			m_bOverflow = false;
//		}
//	}
//}
//
//void bf_write::Reset()
//{
//	m_iCurBit = 0;
//	m_bOverflow = false;
//}
//
//
//void bf_write::SetAssertOnOverflow(bool bAssert)
//{
//	m_bAssertOnOverflow = bAssert;
//}
//
//
//const char* bf_write::GetDebugName()
//{
//	return m_pDebugName;
//}
//
//
//void bf_write::SetDebugName(const char *pDebugName)
//{
//	m_pDebugName = pDebugName;
//}
//
//
//void bf_write::SeekToBit(int bitPos)
//{
//	m_iCurBit = bitPos;
//}
//
//
//// Sign bit comes first
//void bf_write::WriteSBitLong(int data, int numbits)
//{
//	// Force the sign-extension bit to be correct even in the case of overflow.
//	int nValue = data;
//	int nPreserveBits = (0x7FFFFFFF >> (32 - numbits));
//	int nSignExtension = (nValue >> 31) & ~nPreserveBits;
//	nValue &= nPreserveBits;
//	nValue |= nSignExtension;
//
//	if (nValue == data) 
//	{
//		WriteUBitLong(nValue, numbits, false);
//	}
//	else
//	{
//		printf("WriteSBitLong: 0x%08x does not fit in %d bits\n", data, numbits);
//	}
//}
//
//void bf_write::WriteVarInt32(uint32 data)
//{
//	// Check if align and we have room, slow path if not
//	if ((m_iCurBit & 7) == 0 && (m_iCurBit + bitbuf::kMaxVarint32Bytes * 8) <= m_nDataBits)
//	{
//		uint8 *target = ((uint8*)m_pData) + (m_iCurBit >> 3);
//
//		target[0] = static_cast<uint8>(data | 0x80);
//		if (data >= (1 << 7))
//		{
//			target[1] = static_cast<uint8>((data >> 7) | 0x80);
//			if (data >= (1 << 14))
//			{
//				target[2] = static_cast<uint8>((data >> 14) | 0x80);
//				if (data >= (1 << 21))
//				{
//					target[3] = static_cast<uint8>((data >> 21) | 0x80);
//					if (data >= (1 << 28))
//					{
//						target[4] = static_cast<uint8>(data >> 28);
//						m_iCurBit += 5 * 8;
//						return;
//					}
//					else
//					{
//						target[3] &= 0x7F;
//						m_iCurBit += 4 * 8;
//						return;
//					}
//				}
//				else
//				{
//					target[2] &= 0x7F;
//					m_iCurBit += 3 * 8;
//					return;
//				}
//			}
//			else
//			{
//				target[1] &= 0x7F;
//				m_iCurBit += 2 * 8;
//				return;
//			}
//		}
//		else
//		{
//			target[0] &= 0x7F;
//			m_iCurBit += 1 * 8;
//			return;
//		}
//	}
//	else // Slow path
//	{
//		while (data > 0x7F)
//		{
//			WriteUBitLong((data & 0x7F) | 0x80, 8);
//			data >>= 7;
//		}
//		WriteUBitLong(data & 0x7F, 8);
//	}
//}
//
//void bf_write::WriteVarInt64(uint64 data)
//{
//	// Check if align and we have room, slow path if not
//	if ((m_iCurBit & 7) == 0 && (m_iCurBit + bitbuf::kMaxVarintBytes * 8) <= m_nDataBits)
//	{
//		uint8 *target = ((uint8*)m_pData) + (m_iCurBit >> 3);
//
//		// Splitting into 32-bit pieces gives better performance on 32-bit
//		// processors.
//		uint32 part0 = static_cast<uint32>(data);
//		uint32 part1 = static_cast<uint32>(data >> 28);
//		uint32 part2 = static_cast<uint32>(data >> 56);
//
//		int size;
//
//		// Here we can't really optimize for small numbers, since the data is
//		// split into three parts.  Cheking for numbers < 128, for instance,
//		// would require three comparisons, since you'd have to make sure part1
//		// and part2 are zero.  However, if the caller is using 64-bit integers,
//		// it is likely that they expect the numbers to often be very large, so
//		// we probably don't want to optimize for small numbers anyway.  Thus,
//		// we end up with a hardcoded binary search tree...
//		if (part2 == 0)
//		{
//			if (part1 == 0)
//			{
//				if (part0 < (1 << 14))
//				{
//					if (part0 < (1 << 7))
//					{
//						size = 1; goto size1;
//					}
//					else
//					{
//						size = 2; goto size2;
//					}
//				}
//				else
//				{
//					if (part0 < (1 << 21))
//					{
//						size = 3; goto size3;
//					}
//					else
//					{
//						size = 4; goto size4;
//					}
//				}
//			}
//			else
//			{
//				if (part1 < (1 << 14))
//				{
//					if (part1 < (1 << 7))
//					{
//						size = 5; goto size5;
//					}
//					else
//					{
//						size = 6; goto size6;
//					}
//				}
//				else
//				{
//					if (part1 < (1 << 21))
//					{
//						size = 7; goto size7;
//					}
//					else
//					{
//						size = 8; goto size8;
//					}
//				}
//			}
//		}
//		else
//		{
//			if (part2 < (1 << 7))
//			{
//				size = 9; goto size9;
//			}
//			else
//			{
//				size = 10; goto size10;
//			}
//		}
//
//		if (true)
//		{
//			printf("Can't get here.");
//			exit(0);
//		}
//
//	size10: target[9] = static_cast<uint8>((part2 >> 7) | 0x80);
//	size9:  target[8] = static_cast<uint8>((part2) | 0x80);
//	size8:  target[7] = static_cast<uint8>((part1 >> 21) | 0x80);
//	size7:  target[6] = static_cast<uint8>((part1 >> 14) | 0x80);
//	size6:  target[5] = static_cast<uint8>((part1 >> 7) | 0x80);
//	size5:  target[4] = static_cast<uint8>((part1) | 0x80);
//	size4:  target[3] = static_cast<uint8>((part0 >> 21) | 0x80);
//	size3:  target[2] = static_cast<uint8>((part0 >> 14) | 0x80);
//	size2:  target[1] = static_cast<uint8>((part0 >> 7) | 0x80);
//	size1:  target[0] = static_cast<uint8>((part0) | 0x80);
//
//		target[size - 1] &= 0x7F;
//		m_iCurBit += size * 8;
//	}
//	else // slow path
//	{
//		while (data > 0x7F)
//		{
//			WriteUBitLong((data & 0x7F) | 0x80, 8);
//			data >>= 7;
//		}
//		WriteUBitLong(data & 0x7F, 8);
//	}
//}
//
//void bf_write::WriteSignedVarInt32(int32 data)
//{
//	WriteVarInt32(bitbuf::ZigZagEncode32(data));
//}
//
//void bf_write::WriteSignedVarInt64(int64 data)
//{
//	WriteVarInt64(bitbuf::ZigZagEncode64(data));
//}
//
//int	bf_write::ByteSizeVarInt32(uint32 data)
//{
//	int size = 1;
//	while (data > 0x7F) {
//		size++;
//		data >>= 7;
//	}
//	return size;
//}
//
//int	bf_write::ByteSizeVarInt64(uint64 data)
//{
//	int size = 1;
//	while (data > 0x7F) {
//		size++;
//		data >>= 7;
//	}
//	return size;
//}
//
//int bf_write::ByteSizeSignedVarInt32(int32 data)
//{
//	return ByteSizeVarInt32(bitbuf::ZigZagEncode32(data));
//}
//
//int bf_write::ByteSizeSignedVarInt64(int64 data)
//{
//	return ByteSizeVarInt64(bitbuf::ZigZagEncode64(data));
//}
//
//void bf_write::WriteBitLong(unsigned int data, int numbits, bool bSigned)
//{
//	if (bSigned)
//		WriteSBitLong((int)data, numbits);
//	else
//		WriteUBitLong(data, numbits);
//}
//
//bool bf_write::WriteBits(const void *pInData, int nBits)
//{
//	unsigned char *pOut = (unsigned char*)pInData;
//	int nBitsLeft = nBits;
//
//	// Bounds checking..
//	if ((m_iCurBit + nBits) > m_nDataBits)
//	{
//		SetOverflowFlag();
//		return false;
//	}
//
//	// Align output to dword boundary
//	while (((unsigned long)pOut & 3) != 0 && nBitsLeft >= 8)
//	{
//
//		WriteUBitLong(*pOut, 8, false);
//		++pOut;
//		nBitsLeft -= 8;
//	}
//
//	if ((nBitsLeft >= 32) && (m_iCurBit & 7) == 0)
//	{
//		// current bit is byte aligned, do block copy
//		int numbytes = nBitsLeft >> 3;
//		int numbits = numbytes << 3;
//
//		memcpy((char*)m_pData + (m_iCurBit >> 3), pOut, numbytes);
//		pOut += numbytes;
//		nBitsLeft -= numbits;
//		m_iCurBit += numbits;
//	}
//
//	// X360TBD: Can't write dwords in WriteBits because they'll get swapped
//	if ( nBitsLeft >= 32)
//	{
//		unsigned long iBitsRight = (m_iCurBit & 31);
//		unsigned long iBitsLeft = 32 - iBitsRight;
//		unsigned long bitMaskLeft = g_BitWriteMasks[iBitsRight][32];
//		unsigned long bitMaskRight = g_BitWriteMasks[0][iBitsRight];
//
//		unsigned long *pData = &m_pData[m_iCurBit >> 5];
//
//		// Read dwords.
//		while (nBitsLeft >= 32)
//		{
//			unsigned long curData = *(unsigned long*)pOut;
//			pOut += sizeof(unsigned long);
//
//			*pData &= bitMaskLeft;
//			*pData |= curData << iBitsRight;
//
//			pData++;
//
//			if (iBitsLeft < 32)
//			{
//				curData >>= iBitsLeft;
//				*pData &= bitMaskRight;
//				*pData |= curData;
//			}
//
//			nBitsLeft -= 32;
//			m_iCurBit += 32;
//		}
//	}
//
//
//	// write remaining bytes
//	while (nBitsLeft >= 8)
//	{
//		WriteUBitLong(*pOut, 8, false);
//		++pOut;
//		nBitsLeft -= 8;
//	}
//
//	// write remaining bits
//	if (nBitsLeft)
//	{
//		WriteUBitLong(*pOut, nBitsLeft, false);
//	}
//
//	return !IsOverflowed();
//}
//
//
//bool bf_write::WriteBitsFromBuffer(bf_read *pIn, int nBits)
//{
//	// This could be optimized a little by
//	while (nBits > 32)
//	{
//		WriteUBitLong(pIn->ReadUBitLong(32), 32);
//		nBits -= 32;
//	}
//
//	WriteUBitLong(pIn->ReadUBitLong(nBits), nBits);
//	return !IsOverflowed() && !pIn->IsOverflowed();
//}
//
//
//void bf_write::WriteBitAngle(float fAngle, int numbits)
//{
//	int d;
//	unsigned int mask;
//	unsigned int shift;
//
//	shift = BitForBitnum(numbits);
//	mask = shift - 1;
//
//	d = (int)((fAngle / 360.0) * shift);
//	d &= mask;
//
//	WriteUBitLong((unsigned int)d, numbits);
//}
//
//void bf_write::WriteBitCoordMP(const float f, bool bIntegral, bool bLowPrecision)
//{
//#if defined( BB_PROFILING )
//	VPROF("bf_write::WriteBitCoordMP");
//#endif
//	int		signbit = (f <= -(bLowPrecision ? COORD_RESOLUTION_LOWPRECISION : COORD_RESOLUTION));
//	int		intval = (int)abs(f);
//	int		fractval = bLowPrecision ?
//		(abs((int)(f*COORD_DENOMINATOR_LOWPRECISION)) & (COORD_DENOMINATOR_LOWPRECISION - 1)) :
//		(abs((int)(f*COORD_DENOMINATOR)) & (COORD_DENOMINATOR - 1));
//
//	bool    bInBounds = intval < (1 << COORD_INTEGER_BITS_MP);
//
//	unsigned int bits, numbits;
//
//	if (bIntegral)
//	{
//		// Integer encoding: in-bounds bit, nonzero bit, optional sign bit + integer value bits
//		if (intval)
//		{
//			// Adjust the integers from [1..MAX_COORD_VALUE] to [0..MAX_COORD_VALUE-1]
//			--intval;
//			bits = intval * 8 + signbit * 4 + 2 + bInBounds;
//			numbits = 3 + (bInBounds ? COORD_INTEGER_BITS_MP : COORD_INTEGER_BITS);
//		}
//		else
//		{
//			bits = bInBounds;
//			numbits = 2;
//		}
//	}
//	else
//	{
//		// Float encoding: in-bounds bit, integer bit, sign bit, fraction value bits, optional integer value bits
//		if (intval)
//		{
//			// Adjust the integers from [1..MAX_COORD_VALUE] to [0..MAX_COORD_VALUE-1]
//			--intval;
//			bits = intval * 8 + signbit * 4 + 2 + bInBounds;
//			bits += bInBounds ? (fractval << (3 + COORD_INTEGER_BITS_MP)) : (fractval << (3 + COORD_INTEGER_BITS));
//			numbits = 3 + (bInBounds ? COORD_INTEGER_BITS_MP : COORD_INTEGER_BITS)
//				+ (bLowPrecision ? COORD_FRACTIONAL_BITS_MP_LOWPRECISION : COORD_FRACTIONAL_BITS);
//		}
//		else
//		{
//			bits = fractval * 8 + signbit * 4 + 0 + bInBounds;
//			numbits = 3 + (bLowPrecision ? COORD_FRACTIONAL_BITS_MP_LOWPRECISION : COORD_FRACTIONAL_BITS);
//		}
//	}
//
//	WriteUBitLong(bits, numbits);
//}
//
//void bf_write::WriteBitCoord(const float f)
//{
//#if defined( BB_PROFILING )
//	VPROF("bf_write::WriteBitCoord");
//#endif
//	int		signbit = (f <= -COORD_RESOLUTION);
//	int		intval = (int)abs(f);
//	int		fractval = abs((int)(f*COORD_DENOMINATOR)) & (COORD_DENOMINATOR - 1);
//
//
//	// Send the bit flags that indicate whether we have an integer part and/or a fraction part.
//	WriteOneBit(intval);
//	WriteOneBit(fractval);
//
//	if (intval || fractval)
//	{
//		// Send the sign bit
//		WriteOneBit(signbit);
//
//		// Send the integer if we have one.
//		if (intval)
//		{
//			// Adjust the integers from [1..MAX_COORD_VALUE] to [0..MAX_COORD_VALUE-1]
//			intval--;
//			WriteUBitLong((unsigned int)intval, COORD_INTEGER_BITS);
//		}
//
//		// Send the fraction if we have one
//		if (fractval)
//		{
//			WriteUBitLong((unsigned int)fractval, COORD_FRACTIONAL_BITS);
//		}
//	}
//}
//
//void bf_write::WriteBitVec3Coord(const Vector& fa)
//{
//	int		xflag, yflag, zflag;
//
//	xflag = (fa.x >= COORD_RESOLUTION) || (fa.x <= -COORD_RESOLUTION);
//	yflag = (fa.y >= COORD_RESOLUTION) || (fa.y <= -COORD_RESOLUTION);
//	zflag = (fa.z >= COORD_RESOLUTION) || (fa.z <= -COORD_RESOLUTION);
//
//	WriteOneBit(xflag);
//	WriteOneBit(yflag);
//	WriteOneBit(zflag);
//
//	if (xflag)
//		WriteBitCoord(fa.x);
//	if (yflag)
//		WriteBitCoord(fa.y);
//	if (zflag)
//		WriteBitCoord(fa.z);
//}
//
//void bf_write::WriteBitNormal(float f)
//{
//	int	signbit = (f <= -NORMAL_RESOLUTION);
//
//	// NOTE: Since +/-1 are valid values for a normal, I'm going to encode that as all ones
//	unsigned int fractval = abs((int)(f*NORMAL_DENOMINATOR));
//
//	// clamp..
//	if (fractval > NORMAL_DENOMINATOR)
//		fractval = NORMAL_DENOMINATOR;
//
//	// Send the sign bit
//	WriteOneBit(signbit);
//
//	// Send the fractional component
//	WriteUBitLong(fractval, NORMAL_FRACTIONAL_BITS);
//}
//
//void bf_write::WriteBitVec3Normal(const Vector& fa)
//{
//	int		xflag, yflag;
//
//	xflag = (fa.x >= NORMAL_RESOLUTION) || (fa.x <= -NORMAL_RESOLUTION);
//	yflag = (fa.y >= NORMAL_RESOLUTION) || (fa.y <= -NORMAL_RESOLUTION);
//
//	WriteOneBit(xflag);
//	WriteOneBit(yflag);
//
//	if (xflag)
//		WriteBitNormal(fa.x);
//	if (yflag)
//		WriteBitNormal(fa.y);
//
//	// Write z sign bit
//	int	signbit = (fa.z <= -NORMAL_RESOLUTION);
//	WriteOneBit(signbit);
//}
//
//void bf_write::WriteBitAngles(const QAngle& fa)
//{
//	// FIXME:
//	Vector tmp;
//	tmp.Init(fa.x, fa.y, fa.z);
//	WriteBitVec3Coord(tmp);
//}
//
//void bf_write::WriteChar(int val)
//{
//	WriteSBitLong(val, sizeof(char) << 3);
//}
//
//void bf_write::WriteByte(int val)
//{
//	WriteUBitLong(val, sizeof(unsigned char) << 3);
//}
//
//void bf_write::WriteShort(int val)
//{
//	WriteSBitLong(val, sizeof(short) << 3);
//}
//
//void bf_write::WriteWord(int val)
//{
//	WriteUBitLong(val, sizeof(unsigned short) << 3);
//}
//
//void bf_write::WriteLong(long val)
//{
//	WriteSBitLong(val, sizeof(long) << 3);
//}
//
//void bf_write::WriteLongLong(int64 val)
//{
//	unsigned int *pLongs = (unsigned int*)&val;
//
//	// Insert the two DWORDS according to network endian
//	const short endianIndex = 0x0100;
//	byte *idx = (byte*)&endianIndex;
//	WriteUBitLong(pLongs[*idx++], sizeof(long) << 3);
//	WriteUBitLong(pLongs[*idx], sizeof(long) << 3);
//}
//
//void bf_write::WriteFloat(float val)
//{
//	// Pre-swap the float, since WriteBits writes raw data
//	LittleFloat(&val, &val);
//
//	WriteBits(&val, sizeof(val) << 3);
//}
//
//bool bf_write::WriteBytes(const void *pBuf, int nBytes)
//{
//	return WriteBits(pBuf, nBytes << 3);
//}
//
//bool bf_write::WriteString(const char *pStr)
//{
//	if (pStr)
//	{
//		do
//		{
//			WriteChar(*pStr);
//			++pStr;
//		} while (*(pStr - 1) != 0);
//	}
//	else
//	{
//		WriteChar(0);
//	}
//
//	return !IsOverflowed();
//}
//
//// ---------------------------------------------------------------------------------------- //
//// bf_read
//// ---------------------------------------------------------------------------------------- //
//
//bf_read::bf_read()
//{
//	m_pData = NULL;
//	m_nDataBytes = 0;
//	m_nDataBits = -1; // set to -1 so we overflow on any operation
//	m_iCurBit = 0;
//	m_bOverflow = false;
//	m_bAssertOnOverflow = true;
//	m_pDebugName = NULL;
//}
//
//bf_read::bf_read(const void *pData, int nBytes, int nBits)
//{
//	m_bAssertOnOverflow = true;
//	StartReading(pData, nBytes, 0, nBits);
//}
//
//bf_read::bf_read(const char *pDebugName, const void *pData, int nBytes, int nBits)
//{
//	m_bAssertOnOverflow = true;
//	m_pDebugName = pDebugName;
//	StartReading(pData, nBytes, 0, nBits);
//}
//
//void bf_read::StartReading(const void *pData, int nBytes, int iStartBit, int nBits)
//{
//	// Make sure we're dword aligned.
//	if (((size_t)pData & 3) == 0)
//	{
//
//		m_pData = (unsigned char*)pData;
//		m_nDataBytes = nBytes;
//
//		if (nBits == -1)
//		{
//			m_nDataBits = m_nDataBytes << 3;
//		}
//		else
//		{
//			m_nDataBits = nBits;
//		}
//
//		m_iCurBit = iStartBit;
//		m_bOverflow = false;
//	}
//}
//
//void bf_read::Reset()
//{
//	m_iCurBit = 0;
//	m_bOverflow = false;
//}
//
//void bf_read::SetAssertOnOverflow(bool bAssert)
//{
//	m_bAssertOnOverflow = bAssert;
//}
//
//void bf_read::SetDebugName(const char *pName)
//{
//	m_pDebugName = pName;
//}
//
//void bf_read::SetOverflowFlag()
//{
//	//if (m_bAssertOnOverflow)
//	//{
//	//	Assert(false);
//	//}
//	m_bOverflow = true;
//}
//
//unsigned int bf_read::CheckReadUBitLong(int numbits)
//{
//	// Ok, just read bits out.
//	int i, nBitValue;
//	unsigned int r = 0;
//
//	for (i = 0; i < numbits; i++)
//	{
//		nBitValue = ReadOneBitNoCheck();
//		r |= nBitValue << i;
//	}
//	m_iCurBit -= numbits;
//
//	return r;
//}
//
//void bf_read::ReadBits(void *pOutData, int nBits)
//{
//
//	unsigned char *pOut = (unsigned char*)pOutData;
//	int nBitsLeft = nBits;
//
//
//	// align output to dword boundary
//	while (((size_t)pOut & 3) != 0 && nBitsLeft >= 8)
//	{
//		*pOut = (unsigned char)ReadUBitLong(8);
//		++pOut;
//		nBitsLeft -= 8;
//	}
//
//	//// X360TBD: Can't read dwords in ReadBits because they'll get swapped
//	//if (IsPC())
//	//{
//		// read dwords
//		while (nBitsLeft >= 32)
//		{
//			*((unsigned long*)pOut) = ReadUBitLong(32);
//			pOut += sizeof(unsigned long);
//			nBitsLeft -= 32;
//		}
//	//}
//
//	// read remaining bytes
//	while (nBitsLeft >= 8)
//	{
//		*pOut = ReadUBitLong(8);
//		++pOut;
//		nBitsLeft -= 8;
//	}
//
//	// read remaining bits
//	if (nBitsLeft)
//	{
//		*pOut = ReadUBitLong(nBitsLeft);
//	}
//
//}
//
//int bf_read::ReadBitsClamped_ptr(void *pOutData, size_t outSizeBytes, size_t nBits)
//{
//	size_t outSizeBits = outSizeBytes * 8;
//	size_t readSizeBits = nBits;
//	int skippedBits = 0;
//	if (readSizeBits > outSizeBits)
//	{
//		// Should we print a message when we clamp the data being read? Only
//		// in debug builds I think.
//		//printf("Oversized network packet received, and clamped.\n");
//		readSizeBits = outSizeBits;
//		skippedBits = (int)(nBits - outSizeBits);
//		// What should we do in this case, which should only happen if nBits
//		// is negative for some reason?
//		//if ( skippedBits < 0 )
//		//	return 0;
//	}
//
//	ReadBits(pOutData, readSizeBits);
//	SeekRelative(skippedBits);
//
//	// Return the number of bits actually read.
//	return (int)readSizeBits;
//}
//
//float bf_read::ReadBitAngle(int numbits)
//{
//	float fReturn;
//	int i;
//	float shift;
//
//	shift = (float)(BitForBitnum(numbits));
//
//	i = ReadUBitLong(numbits);
//	fReturn = (float)i * (360.0f / shift);
//
//	return fReturn;
//}
//
//unsigned int bf_read::PeekUBitLong(int numbits)
//{
//	unsigned int r;
//	int i, nBitValue;
//
//	bf_read savebf;
//
//	savebf = *this;  // Save current state info
//
//	r = 0;
//	for (i = 0; i < numbits; i++)
//	{
//		nBitValue = ReadOneBit();
//
//		// Append to current stream
//		if (nBitValue)
//		{
//			r |= BitForBitnum(i);
//		}
//	}
//
//	*this = savebf;
//
//
//	return r;
//}
//
//unsigned int bf_read::ReadUBitLongNoInline(int numbits)
//{
//	return ReadUBitLong(numbits);
//}
//
//unsigned int bf_read::ReadUBitVarInternal(int encodingType)
//{
//	m_iCurBit -= 4;
//	// int bits = { 4, 8, 12, 32 }[ encodingType ];
//	int bits = 4 + encodingType * 4 + (((2 - encodingType) >> 31) & 16);
//	return ReadUBitLong(bits);
//}
//
//void* bf_read::GetParseBuffer()
//{
//	return (void*)(m_pData + ((signed int)(((((unsigned __int64)(m_iCurBit + 7) >> 32) & 7) + m_iCurBit + 7) & 0xFFFFFFF8) >> 3));
//}
//
//// Append numbits least significant bits from data to the current bit stream
//int bf_read::ReadSBitLong(int numbits)
//{
//	unsigned int r = ReadUBitLong(numbits);
//	unsigned int s = 1 << (numbits - 1);
//	if (r >= s)
//	{
//		// sign-extend by removing sign bit and then subtracting sign bit again
//		r = r - s - s;
//	}
//	return r;
//}
//
//uint32 bf_read::ReadVarInt32()
//{
//	uint32 result = 0;
//	int count = 0;
//	uint32 b;
//
//	do
//	{
//		if (count == bitbuf::kMaxVarint32Bytes)
//		{
//			return result;
//		}
//		b = ReadUBitLong(8);
//		result |= (b & 0x7F) << (7 * count);
//		++count;
//	} while (b & 0x80);
//
//	return result;
//}
//
//uint64 bf_read::ReadVarInt64()
//{
//	uint64 result = 0;
//	int count = 0;
//	uint64 b;
//
//	do
//	{
//		if (count == bitbuf::kMaxVarintBytes)
//		{
//			return result;
//		}
//		b = ReadUBitLong(8);
//		result |= static_cast<uint64>(b & 0x7F) << (7 * count);
//		++count;
//	} while (b & 0x80);
//
//	return result;
//}
//
//int32 bf_read::ReadSignedVarInt32()
//{
//	uint32 value = ReadVarInt32();
//	return bitbuf::ZigZagDecode32(value);
//}
//
//int64 bf_read::ReadSignedVarInt64()
//{
//	uint64 value = ReadVarInt64();
//	return bitbuf::ZigZagDecode64(value);
//}
//
//unsigned int bf_read::ReadBitLong(int numbits, bool bSigned)
//{
//	if (bSigned)
//		return (unsigned int)ReadSBitLong(numbits);
//	else
//		return ReadUBitLong(numbits);
//}
//
//
//// Basic Coordinate Routines (these contain bit-field size AND fixed point scaling constants)
//float bf_read::ReadBitCoord(void)
//{
//	int		intval = 0, fractval = 0, signbit = 0;
//	float	value = 0.0;
//
//
//	// Read the required integer and fraction flags
//	intval = ReadOneBit();
//	fractval = ReadOneBit();
//
//	// If we got either parse them, otherwise it's a zero.
//	if (intval || fractval)
//	{
//		// Read the sign bit
//		signbit = ReadOneBit();
//
//		// If there's an integer, read it in
//		if (intval)
//		{
//			// Adjust the integers from [0..MAX_COORD_VALUE-1] to [1..MAX_COORD_VALUE]
//			intval = ReadUBitLong(COORD_INTEGER_BITS) + 1;
//		}
//
//		// If there's a fraction, read it in
//		if (fractval)
//		{
//			fractval = ReadUBitLong(COORD_FRACTIONAL_BITS);
//		}
//
//		// Calculate the correct floating point value
//		value = intval + ((float)fractval * COORD_RESOLUTION);
//
//		// Fixup the sign if negative.
//		if (signbit)
//			value = -value;
//	}
//
//	return value;
//}
//
//float bf_read::ReadBitCoordMP(EBitCoordType coordType)
//{
//	bool bIntegral = (coordType == kCW_Integral);
//	bool bLowPrecision = (coordType == kCW_LowPrecision);
//	// BitCoordMP float encoding: inbounds bit, integer bit, sign bit, optional int bits, float bits
//	// BitCoordMP integer encoding: inbounds bit, integer bit, optional sign bit, optional int bits.
//	// int bits are always encoded as (value - 1) since zero is handled by the integer bit
//
//	// With integer-only encoding, the presence of the third bit depends on the second
//	int flags = ReadUBitLong(3 - bIntegral);
//	enum { INBOUNDS = 1, INTVAL = 2, SIGN = 4 };
//
//	if (bIntegral)
//	{
//		if (flags & INTVAL)
//		{
//			// Read the third bit and the integer portion together at once
//			unsigned int bits = ReadUBitLong((flags & INBOUNDS) ? COORD_INTEGER_BITS_MP + 1 : COORD_INTEGER_BITS + 1);
//			// Remap from [0,N] to [1,N+1]
//			int intval = (bits >> 1) + 1;
//			return (float)((bits & 1) ? -intval : intval);
//		}
//		return 0.f; 
//	}
//
//	static const float mul_table[4] =
//	{
//		1.f / (1 << COORD_FRACTIONAL_BITS),
//		-1.f / (1 << COORD_FRACTIONAL_BITS),
//		1.f / (1 << COORD_FRACTIONAL_BITS_MP_LOWPRECISION),
//		-1.f / (1 << COORD_FRACTIONAL_BITS_MP_LOWPRECISION)
//	};
//	//equivalent to: float multiply = mul_table[ ((flags & SIGN) ? 1 : 0) + bLowPrecision*2 ];
//	float multiply = *(float*)((uintptr_t)&mul_table[0] + (flags & 4) + bLowPrecision * 8);
//
//	static const unsigned char numbits_table[8] =
//	{
//		COORD_FRACTIONAL_BITS,
//		COORD_FRACTIONAL_BITS,
//		COORD_FRACTIONAL_BITS + COORD_INTEGER_BITS,
//		COORD_FRACTIONAL_BITS + COORD_INTEGER_BITS_MP,
//		COORD_FRACTIONAL_BITS_MP_LOWPRECISION,
//		COORD_FRACTIONAL_BITS_MP_LOWPRECISION,
//		COORD_FRACTIONAL_BITS_MP_LOWPRECISION + COORD_INTEGER_BITS,
//		COORD_FRACTIONAL_BITS_MP_LOWPRECISION + COORD_INTEGER_BITS_MP
//	};
//	unsigned int bits = ReadUBitLong(numbits_table[(flags & (INBOUNDS | INTVAL)) + bLowPrecision * 4]);
//
//	if (flags & INTVAL)
//	{
//		// Shuffle the bits to remap the integer portion from [0,N] to [1,N+1]
//		// and then paste in front of the fractional parts so we only need one
//		// int-to-float conversion.
//
//		uint fracbitsMP = bits >> COORD_INTEGER_BITS_MP;
//		uint fracbits = bits >> COORD_INTEGER_BITS;
//
//		uint intmaskMP = ((1 << COORD_INTEGER_BITS_MP) - 1);
//		uint intmask = ((1 << COORD_INTEGER_BITS) - 1);
//
//		uint selectNotMP = (flags & INBOUNDS) - 1;
//
//		fracbits -= fracbitsMP;
//		fracbits &= selectNotMP;
//		fracbits += fracbitsMP;
//
//		intmask -= intmaskMP;
//		intmask &= selectNotMP;
//		intmask += intmaskMP;
//
//		uint intpart = (bits & intmask) + 1;
//		uint intbitsLow = intpart << COORD_FRACTIONAL_BITS_MP_LOWPRECISION;
//		uint intbits = intpart << COORD_FRACTIONAL_BITS;
//		uint selectNotLow = (uint)bLowPrecision - 1;
//
//		intbits -= intbitsLow;
//		intbits &= selectNotLow;
//		intbits += intbitsLow;
//
//		bits = fracbits | intbits;
//	}
//
//	return (int)bits * multiply;
//}
//
//unsigned int bf_read::ReadBitCoordBits(void)
//{
//
//	unsigned int flags = ReadUBitLong(2);
//	if (flags == 0)
//		return 0;
//
//	static const int numbits_table[3] =
//	{
//		COORD_INTEGER_BITS + 1,
//		COORD_FRACTIONAL_BITS + 1,
//		COORD_INTEGER_BITS + COORD_FRACTIONAL_BITS + 1
//	};
//	return ReadUBitLong(numbits_table[flags - 1]) * 4 + flags;
//}
//
//unsigned int bf_read::ReadBitCoordMPBits(bool bIntegral, bool bLowPrecision)
//{
//
//	unsigned int flags = ReadUBitLong(2);
//	enum { INBOUNDS = 1, INTVAL = 2 };
//	int numbits = 0;
//
//	if (bIntegral)
//	{
//		if (flags & INTVAL)
//		{
//			numbits = (flags & INBOUNDS) ? (1 + COORD_INTEGER_BITS_MP) : (1 + COORD_INTEGER_BITS);
//		}
//		else
//		{
//			return flags; // no extra bits
//		}
//	}
//	else
//	{
//		static const unsigned char numbits_table[8] =
//		{
//			1 + COORD_FRACTIONAL_BITS,
//			1 + COORD_FRACTIONAL_BITS,
//			1 + COORD_FRACTIONAL_BITS + COORD_INTEGER_BITS,
//			1 + COORD_FRACTIONAL_BITS + COORD_INTEGER_BITS_MP,
//			1 + COORD_FRACTIONAL_BITS_MP_LOWPRECISION,
//			1 + COORD_FRACTIONAL_BITS_MP_LOWPRECISION,
//			1 + COORD_FRACTIONAL_BITS_MP_LOWPRECISION + COORD_INTEGER_BITS,
//			1 + COORD_FRACTIONAL_BITS_MP_LOWPRECISION + COORD_INTEGER_BITS_MP
//		};
//		numbits = numbits_table[flags + bLowPrecision * 4];
//	}
//
//	return flags + ReadUBitLong(numbits) * 4;
//}
//
//float bf_read::ReadBitCellCoord(int bits, EBitCoordType coordType)
//{
//	bool bIntegral = (coordType == kCW_Integral);
//	bool bLowPrecision = (coordType == kCW_LowPrecision);
//
//	int		intval = 0, fractval = 0;
//	float	value = 0.0;
//
//	if (bIntegral)
//	{
//		value = (float)(ReadUBitLong(bits));
//	}
//	else
//	{
//		intval = ReadUBitLong(bits);
//
//		// If there's a fraction, read it in
//		fractval = ReadUBitLong(bLowPrecision ? COORD_FRACTIONAL_BITS_MP_LOWPRECISION : COORD_FRACTIONAL_BITS);
//
//		// Calculate the correct floating point value
//		value = intval + ((float)fractval * (bLowPrecision ? COORD_RESOLUTION_LOWPRECISION : COORD_RESOLUTION));
//	}
//
//	return value;
//}
//
//void bf_read::ReadBitVec3Coord(Vector& fa)
//{
//	int		xflag, yflag, zflag;
//
//	// This vector must be initialized! Otherwise, If any of the flags aren't set, 
//	// the corresponding component will not be read and will be stack garbage.
//	fa.Init(0, 0, 0);
//
//	xflag = ReadOneBit();
//	yflag = ReadOneBit();
//	zflag = ReadOneBit();
//
//	if (xflag)
//		fa.x = ReadBitCoord();
//	if (yflag)
//		fa.y = ReadBitCoord();
//	if (zflag)
//		fa.z = ReadBitCoord();
//}
//
//float bf_read::ReadBitNormal(void)
//{
//	// Read the sign bit
//	int	signbit = ReadOneBit();
//
//	// Read the fractional part
//	unsigned int fractval = ReadUBitLong(NORMAL_FRACTIONAL_BITS);
//
//	// Calculate the correct floating point value
//	float value = (float)fractval * NORMAL_RESOLUTION;
//
//	// Fixup the sign if negative.
//	if (signbit)
//		value = -value;
//
//	return value;
//}
//
//void bf_read::ReadBitVec3Normal(Vector& fa)
//{
//	int xflag = ReadOneBit();
//	int yflag = ReadOneBit();
//
//	if (xflag)
//		fa.x = ReadBitNormal();
//	else
//		fa.x = 0.0f;
//
//	if (yflag)
//		fa.y = ReadBitNormal();
//	else
//		fa.y = 0.0f;
//
//	// The first two imply the third (but not its sign)
//	int znegative = ReadOneBit();
//
//	float fafafbfb = fa.x * fa.x + fa.y * fa.y;
//	if (fafafbfb < 1.0f)
//		fa.z = sqrt(1.0f - fafafbfb);
//	else
//		fa.z = 0.0f;
//
//	if (znegative)
//		fa.z = -fa.z;
//}
//
//void bf_read::ReadBitAngles(QAngle& fa)
//{
//	Vector tmp;
//	ReadBitVec3Coord(tmp);
//	fa.Init(tmp.x, tmp.y, tmp.z);
//}
//
//int64 bf_read::ReadLongLong()
//{
//	int64 retval;
//	uint *pLongs = (uint*)&retval;
//
//	// Read the two DWORDs according to network endian
//	const short endianIndex = 0x0100;
//	byte *idx = (byte*)&endianIndex;
//	pLongs[*idx++] = ReadUBitLong(sizeof(long) << 3);
//	pLongs[*idx] = ReadUBitLong(sizeof(long) << 3);
//
//	return retval;
//}
//
//float bf_read::ReadFloat()
//{
//	float ret;
//
//	ReadBits(&ret, 32);
//
//	// Swap the float, since ReadBits reads raw data
//	LittleFloat(&ret, &ret);
//	return ret;
//}
//
//bool bf_read::ReadBytes(void *pOut, int nBytes)
//{
//	ReadBits(pOut, nBytes << 3);
//	return !IsOverflowed();
//}
//
//bool bf_read::ReadString(char *pStr, int maxLen, bool bLine, int *pOutNumChars)
//{
//	bool bTooSmall = false;
//	int iChar = 0;
//	while (1)
//	{
//		char val = ReadChar();
//		if (val == 0)
//			break;
//		else if (bLine && val == '\n')
//			break;
//
//		if (iChar < (maxLen - 1))
//		{
//			pStr[iChar] = val;
//			++iChar;
//		}
//		else
//		{
//			bTooSmall = true;
//		}
//	}
//
//	// Make sure it's null-terminated.
//	//Assert(iChar < maxLen);
//	pStr[iChar] = 0;
//
//	if (pOutNumChars)
//		*pOutNumChars = iChar;
//
//	return !IsOverflowed() && !bTooSmall;
//}
//
//
//char* bf_read::ReadAndAllocateString(bool *pOverflow)
//{
//	char str[2048];
//
//	int nChars;
//	bool bOverflow = !ReadString(str, sizeof(str), false, &nChars);
//	if (pOverflow)
//		*pOverflow = bOverflow;
//
//	// Now copy into the output and return it;
//	char *pRet = new char[nChars + 1];
//	for (int i = 0; i <= nChars; i++)
//		pRet[i] = str[i];
//
//	return pRet;
//}
//
//void bf_read::ExciseBits(int startbit, int bitstoremove)
//{
//	int endbit = startbit + bitstoremove;
//	int remaining_to_end = m_nDataBits - endbit;
//
//	bf_write temp;
//	temp.StartWriting((void *)m_pData, m_nDataBits << 3, startbit);
//
//	Seek(endbit);
//
//	for (int i = 0; i < remaining_to_end; i++)
//	{
//		temp.WriteOneBit(ReadOneBit());
//	}
//
//	Seek(startbit);
//
//	m_nDataBits -= bitstoremove;
//	m_nDataBytes = m_nDataBits >> 3;
//}
//
//int bf_read::CompareBitsAt(int offset, bf_read * RESTRICT other, int otherOffset, int numbits) RESTRICT
//{
//	extern unsigned long g_ExtraMasks[33];
//
//	if (numbits == 0)
//		return 0;
//
//	int overflow1 = offset + numbits > m_nDataBits;
//	int overflow2 = otherOffset + numbits > other->m_nDataBits;
//
//	int x = overflow1 | overflow2;
//	if (x != 0)
//		return x;
//
//	unsigned int iStartBit1 = offset & 31u;
//	unsigned int iStartBit2 = otherOffset & 31u;
//	unsigned long *pData1 = (unsigned long*)m_pData + (offset >> 5);
//	unsigned long *pData2 = (unsigned long*)other->m_pData + (otherOffset >> 5);
//	unsigned long *pData1End = pData1 + ((offset + numbits - 1) >> 5);
//	unsigned long *pData2End = pData2 + ((otherOffset + numbits - 1) >> 5);
//
//	while (numbits > 32)
//	{
//		x = LoadLittleDWord((unsigned long*)pData1, 0) >> iStartBit1;
//		x ^= LoadLittleDWord((unsigned long*)pData1, 1) << (32 - iStartBit1);
//		x ^= LoadLittleDWord((unsigned long*)pData2, 0) >> iStartBit2;
//		x ^= LoadLittleDWord((unsigned long*)pData2, 1) << (32 - iStartBit2);
//		if (x != 0)
//		{
//			return x;
//		}
//		++pData1;
//		++pData2;
//		numbits -= 32;
//	}
//
//	x = LoadLittleDWord((unsigned long*)pData1, 0) >> iStartBit1;
//	x ^= LoadLittleDWord((unsigned long*)pData1End, 0) << (32 - iStartBit1);
//	x ^= LoadLittleDWord((unsigned long*)pData2, 0) >> iStartBit2;
//	x ^= LoadLittleDWord((unsigned long*)pData2End, 0) << (32 - iStartBit2);
//	return x & g_ExtraMasks[numbits];
//}






