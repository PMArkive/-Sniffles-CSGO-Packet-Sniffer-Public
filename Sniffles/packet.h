#pragma once

#include <stdio.h>
#include <string>
#include <assert.h>
#include "basetypes.h"
#include "platform.h"

#if !defined( MAX_OSPATH )
#define	MAX_OSPATH		260			// max length of a filesystem pathname
#endif

#define STATE_DEFAULT 0
#define STATE_CREATED 1
#define STATE_UPDATED 2
#define STATE_DELETED 3
#define STATE_OVERWRITTEN 4

// packet messages
enum
{
	// it's a startup message, process as fast as possible
	dem_signon = 1,
	// it's a normal network packet that we stored off
	dem_packet,
	// sync client clock to demo tick
	dem_synctick,
	// console command
	dem_consolecmd,
	// user input command
	dem_usercmd,
	// network data tables
	dem_datatables,
	// end of time.
	dem_stop,
	// a blob of binary data understood by a callback function
	dem_customdata,

	dem_stringtables,

	// Last command
	dem_lastcmd = dem_stringtables
};

struct packetheader_t
{
	int32	sequence_in;
	int32	sequence_out;
	int32	unk;
};

#define FPACKET_NORMAL			0
#define FPACKET_USE_ORIGIN2		( 1 << 0 )
#define FPACKET_USE_ANGLES2		( 1 << 1 )
#define FPACKET_NOINTERP		( 1 << 2 )	// don't interpolate between this an last view

#define MAX_SPLITSCREEN_CLIENTS	2

struct QAngle
{
	float x, y, z;
	void Init(void)
	{
		x = y = z = 0.0f;
	}
	void Init(float _x, float _y, float _z)
	{
		x = _x;
		y = _y;
		z = _z;
	}
};

struct Vector
{
	float x, y, z;
	//Vector()
	//{
	//	x = 0;
	//	y = 0;
	//	z = 0;
	//}
	//Vector(float _x, float _y, float _z)
	//{
	//	x = _x;
	//	y = _y;
	//	z = _z;
	//}
	void Init(void)
	{
		x = y = z = 0.0f;
	}
	void Init(float _x, float _y, float _z)
	{
		x = _x;
		y = _y;
		z = _z;
	}
	void Set(float _x, float _y, float _z)
	{
		x = _x;
		y = _y;
		z = _z;
	}
	void Set(Vector v)
	{
		x = v.x;
		y = v.y;
		z = v.z;
	}
};

struct packetcmdinfo_t
{
	packetcmdinfo_t(void)
	{
	}

	struct Split_t
	{
		Split_t(void)
		{
			flags = FPACKET_NORMAL;
			viewOrigin.Init();
			viewAngles.Init();
			localViewAngles.Init();

			// Resampled origin/angles
			viewOrigin2.Init();
			viewAngles2.Init();
			localViewAngles2.Init();
		}

		Split_t& operator=(const Split_t& src)
		{
			if (this == &src)
				return *this;

			flags = src.flags;
			viewOrigin = src.viewOrigin;
			viewAngles = src.viewAngles;
			localViewAngles = src.localViewAngles;
			viewOrigin2 = src.viewOrigin2;
			viewAngles2 = src.viewAngles2;
			localViewAngles2 = src.localViewAngles2;

			return *this;
		}

		const Vector& GetViewOrigin(void)
		{
			if (flags & FPACKET_USE_ORIGIN2)
			{
				return viewOrigin2;
			}
			return viewOrigin;
		}

		const QAngle& GetViewAngles(void)
		{
			if (flags & FPACKET_USE_ANGLES2)
			{
				return viewAngles2;
			}
			return viewAngles;
		}
		const QAngle& GetLocalViewAngles(void)
		{
			if (flags & FPACKET_USE_ANGLES2)
			{
				return localViewAngles2;
			}
			return localViewAngles;
		}

		void Reset(void)
		{
			flags = 0;
			viewOrigin2 = viewOrigin;
			viewAngles2 = viewAngles;
			localViewAngles2 = localViewAngles;
		}

		int32		flags;

		// original origin/viewangles
		Vector		viewOrigin;
		QAngle		viewAngles;
		QAngle		localViewAngles;

		// Resampled origin/viewangles
		Vector		viewOrigin2;
		QAngle		viewAngles2;
		QAngle		localViewAngles2;
	};

	void Reset(void)
	{
		for (int i = 0; i < MAX_SPLITSCREEN_CLIENTS; ++i)
			u[i].Reset();
	}

	Split_t			u[MAX_SPLITSCREEN_CLIENTS];
};

class CPacket
{
public:
	CPacket() {}
	~CPacket() {}

	bool Open(unsigned char *data, size_t dataSize)
	{
		if (data && dataSize > 0)
		{
			size_t Length = dataSize;

			m_packetBuffer.resize(Length);
			memcpy(&m_packetBuffer[0], data, Length);

			m_packetBufferPos = 0;

			return true;
		}

		return false;
	}

	void CPacket::ReadSequenceInfo(int32 &nSeqNrIn, int32 &nSeqNrOut)
	{
		if (!m_packetBuffer.size())
			return;

		nSeqNrIn = *(int32 *)(&m_packetBuffer[m_packetBufferPos]);
		m_packetBufferPos += sizeof(int32);
		nSeqNrOut = *(int32 *)(&m_packetBuffer[m_packetBufferPos]);
		m_packetBufferPos += sizeof(int32);
	}

	void CPacket::ReadCmdInfo(packetcmdinfo_t& info)
	{
		if (!m_packetBuffer.size())
			return;

		memcpy(&info, &m_packetBuffer[m_packetBufferPos], sizeof(packetcmdinfo_t));
		m_packetBufferPos += sizeof(packetcmdinfo_t);
	}

	void CPacket::ReadCmdHeader(unsigned char& cmd, int32& tick, unsigned char& playerSlot)
	{
		if (!m_packetBuffer.size())
			return;

		// Read the command
		cmd = *(unsigned char *)(&m_packetBuffer[m_packetBufferPos]);
		m_packetBufferPos += sizeof(unsigned char);

		if (cmd <= 0)
		{
			fprintf(stderr, "Packet::ReadCmdHeader: Missing end tag in packet.\n");
			cmd = dem_stop;
			return;
		}

		assert(cmd >= 1 && cmd <= dem_lastcmd);

		// Read the timestamp
		tick = *(int32 *)(&m_packetBuffer[m_packetBufferPos]);
		m_packetBufferPos += sizeof(int32);

		// read playerslot
		playerSlot = *(unsigned char *)(&m_packetBuffer[m_packetBufferPos]);
		m_packetBufferPos += sizeof(unsigned char);
	}

	int32 CPacket::ReadUserCmd(char *buffer, int32 &size)
	{
		if (!m_packetBuffer.size())
			return 0;

		int32 outgoing_sequence = *(int32 *)(&m_packetBuffer[m_packetBufferPos]);
		m_packetBufferPos += sizeof(int32);

		size = ReadRawData(buffer, size);

		return outgoing_sequence;
	}

	int32 CPacket::ReadRawData(char *buffer, int32 length)
	{
		if (!m_packetBuffer.size())
			return 0;

		// read length of data block
		int32 size = *(int32 *)(&m_packetBuffer[m_packetBufferPos]);
		m_packetBufferPos += sizeof(int32);

		if (buffer && (length < size))
		{
			fprintf(stderr, "CPacket::ReadRawData: buffer overflow (%i).\n", size);
			return -1;
		}

		if (buffer)
		{
			// read data into buffer
			memcpy(buffer, &m_packetBuffer[m_packetBufferPos], size);
			m_packetBufferPos += size;
		}
		else
		{
			// just skip it
			m_packetBufferPos += size;
		}

		return size;
	}

public:
	packetheader_t    m_packetHeader;  //general demo info

	size_t m_packetBufferPos;
	std::string m_packetBuffer;
};