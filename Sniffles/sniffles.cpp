// Sniffles.cpp : Defines the entry point for the console application.
//
#include "sniffles.h"

const unsigned char g_iceKey[] = { 0x43, 0x53, 0x47, 0x4F, 0xCC, 0x34, 0x00, 0x00, 0x33, 0x0D, 0x00, 0x00, 0x4C, 0x03, 0x00, 0x00 };

int ReadPacket(unsigned char* packetData, int size)
{	
	if (size < 8)
		return size;

	unsigned char data[NET_MAX_PAYLOAD];
	CBitRead buf(data, NET_MAX_PAYLOAD);
	memcpy((void*)buf.GetBasePointer(), packetData, size);
	buf.Seek(0);

	int32 nSeqNrIn = buf.ReadSBitLong(32); // SeqNrIn
	int32 nSeqNrOut = buf.ReadSBitLong(32); // SeqNrOut
	int32 nFlags = buf.ReadVarInt32(); // nFlags
	
	int unk0 = buf.ReadSBitLong(16); // dunno what this is
	int unk1 = buf.ReadSignedVarInt32(); // dunno what this is

	if (!nFlags || ((unsigned char)nFlags) >= 0xE1u)
	{

		while (buf.GetNumBytesRead() < size)
		{
			int Cmd = buf.ReadVarInt32();
			int Size = buf.ReadVarInt32();

			void* parseBuffer = (void*)((int)buf.GetBasePointer() + buf.GetNumBytesRead());
			size_t bufferSize = Size;

			switch (Cmd)
			{

			case net_Tick:
			{
				CNETMsg_Tick msg;
				if (msg.ParseFromArray(parseBuffer, bufferSize))
					MsgPrintf(msg, bufferSize, "%s", msg.DebugString().c_str());
			}
			break;

			case net_SignonState:
			{
				CNETMsg_SignonState msg;
				if (msg.ParseFromArray(parseBuffer, bufferSize))
					MsgPrintf(msg, bufferSize, "%s", msg.DebugString().c_str());
			}
			break;

			case svc_ServerInfo:
			{
				CSVCMsg_ServerInfo msg;
				if (msg.ParseFromArray(parseBuffer, bufferSize))
					MsgPrintf(msg, bufferSize, "%s", msg.DebugString().c_str());
			}
			break;

			case svc_PacketEntities:
			{
				CSVCMsg_PacketEntities msg;
				if (msg.ParseFromArray(parseBuffer, bufferSize))
					MsgPrintf(msg, bufferSize, "%s", msg.DebugString().c_str());

			}
			break;

			default:
				break;

			}

			buf.SeekRelative(Size * 8);
		}
	}

	return size;
}

bool packet_loop_handler(PDU& eth) 
{
	//TCP* tcp = eth.find_pdu<TCP>();
	IP* ip = eth.find_pdu<IP>();
	if (!ip)
		return 1;
	UDP* udp = eth.find_pdu<UDP>();
	if (!udp)
		return 1;

	if (udp->sport() == 27015)
	{
		outf("\npacket %i:\n", ip->id());
		outf("  ver: %i\n", ip->version());
		outf("  src addr: %s:%i\n", ip->src_addr().to_string().c_str(), udp->dport());
		outf("  dst addr: %s:%i\n", ip->dst_addr().to_string().c_str(), udp->dport());

		RawPDU* raw = eth.find_pdu<RawPDU>();

		std::vector<uint8> payload = raw->payload();
		size_t size = raw->payload_size();
		outf("  payload size: %d\n", size);

		unsigned char* pData = payload.data();

		IceKey ice(2);
		ice.set(g_iceKey);
		
		uint8* pDataOut = (uint8*)malloc(size);
		
		int32 blockSize = ice.blockSize();
		
		uint8* p1 = pData;
		uint8* p2 = pDataOut;
		
		// encrypt data in 8 byte blocks
		int32 bytesLeft = size;
		
		while (bytesLeft >= blockSize)
		{
			ice.decrypt(p1, p2);
		
			bytesLeft -= blockSize;
			p1 += blockSize;
			p2 += blockSize;
		}
		
		//The end chunk doesn't get an encryption. it sux.
		memcpy(p2, p1, bytesLeft);

		unsigned char deltaOffset = *(unsigned char*)pDataOut;
		outf("  deltaOffset: %d\n", deltaOffset);
		if (deltaOffset > 0 && (uint32)deltaOffset + 5 < size)
		{
			uint32 dataFinalSize = _byteswap_ulong(*(uint32*)&pDataOut[deltaOffset + 1]);
			outf("  dataFinalSize: %d\n", dataFinalSize);

			if (dataFinalSize + deltaOffset + 5 == size)
			{
				uint8* packetData = (uint8*)malloc(dataFinalSize);
				memcpy(packetData, &pDataOut[deltaOffset + 5], dataFinalSize);
				free(pDataOut);
				if (packetData)
				{
					ReadPacket(packetData, dataFinalSize);
					free(packetData);
				}
			}
		}			
		
	}

	return 1;
}


int _tmain(int argc, _TCHAR* argv[])
{
	out("/////////////////////////////////////////////////////////\n"
		"//::::::::::::::::::: Sniffles 0.1a ::::::::::::::::::://\n"
		"//:::::::::::::::::::  by: dude719  ::::::::::::::::::://\n"
		"//::::::::::::: Press any key to continue ::::::::::::://\n"
		"/////////////////////////////////////////////////////////");

	memory_t<int> cfg_device;
	memory_t<int> cfg_port;
	std::vector<pcap_if_t*> devList;

	bool bFileExisted = check_config_file();
	out("\nUse Existing Config Y or N? ");
	bool bSetConfig = false;
	
	while (1)
	{
		int character = getchar();
		if (character == 'n' || character == 'N')
		{
			bSetConfig = true;
			break;
		}
		else if (character == 'y' || character == 'Y')
			break;
		else if (character != 10)
			out("\nY or N? ");
	}
	


	if (bFileExisted && !bSetConfig)
	{
		cfg_device = get_config_option<int>(DEVICE);
		cfg_port = get_config_option<int>(PORT);

		devList = create_dev_list();
	}
	else
	{
		devList = list_all_devs();
		// Prompt to choose a device
		out("//::::::::::::::::::::::::::::::::::::::::::::::::::::://\n"
			"//:::::::::::::::::: Choose a device :::::::::::::::::://\n"
			"//::::::::::::::::::::::::::::::::::::::::::::::::::::://\n");
		size_t devIndex = 0;
		while (true)
		{
			scanf_s("%d", &devIndex);
			if (devIndex <= devList.size() && devIndex > 0)
				break;
			else
				out("Not valid! Try again: ");
		}

		// Check if chosen device is valid
		if (!devList[devIndex - 1])
		{
			out("Not a valid choice. Closing...");
			return 1;
		}

		// Prompt to choose a port to listen on
		out("//////////////////////////////////////////////////////////\n"
			"/////////////// choose port to listen on: ////////////////\n"
			"//////////////////////////////////////////////////////////\n");
		unsigned short port;
		while (true)
		{
			scanf_s("%d", &port);
			if (port > 0)
				break;
			else
				out("Not valid! Try again: ");
		}

		cfg_device = memory_t<int>(devIndex);
		cfg_port = memory_t<int>(port);
	}

	pcap_if_t* device = devList[cfg_device.value() - 1];
	if (!device) // Check if chosen device is valid
	{
		shout_error("Not a valid choice. Closing...");
		return 1;
	}

	save_config(cfg_device, cfg_port);

	// Spit out the chosen device
	out("Listening on: ");
	print_dev(device);

	String strFilter = String("port ") + StrUtils::NumToStr<int>(cfg_port.value()).CStr();
	out(strFilter.CStr());

	cfg_device.free();
	cfg_port.free();

	SnifferConfiguration config;
	config.set_filter(strFilter.CStr());
	config.set_promisc_mode(true);

	// Create sniffer configuration object.
	Sniffer sniffer(device->name, config);
	sniffer.sniff_loop(packet_loop_handler);

	return 0;
}

