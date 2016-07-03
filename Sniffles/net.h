#pragma once

#include "tins\tins.h"
#include "str.h"

#include "google/protobuf/descriptor.h"
#include "google/protobuf/reflection_ops.h"
#include "google/protobuf/descriptor.pb.h"

#include "generated_proto/cstrike15_usermessages_public.pb.h"
#include "generated_proto/netmessages_public.pb.h"

using namespace Tins;

#define PAD_NUMBER(number, boundary) (((number)+((boundary)-1)) / (boundary)) * (boundary)

#define PORT_MASTER				27010
#define PORT_CLIENT				27005
#define PORT_SERVER				27015

#define NETMSG_TYPE_BITS		5	// must be 2^NETMSG_TYPE_BITS > SVC_LASTMSG

#define DELTA_OFFSET_BITS 5
#define DELTA_OFFSET_MAX ( ( 1 << DELTA_OFFSET_BITS ) - 1 )

#define DELTASIZE_BITS		20	// must be: 2^DELTASIZE_BITS > (NET_MAX_PAYLOAD * 8)

// UDP has 28 byte headers
#define UDP_HEADER_SIZE			(20+8)	// IP = 20, UDP = 8

#define HEADER_BYTES			9	// 2*4 bytes seqnr, 1 byte flags

#define NET_MAX_PAYLOAD					( 262144 - 4 )		// largest message we can send in bytes
// How many bits to use to encode an edict.
#define	MAX_EDICT_BITS					11					// # of bits needed to represent max edicts
// Max # of edicts in a level
#define	MAX_EDICTS						( 1 << MAX_EDICT_BITS )

#define MAX_USERDATA_BITS				14
#define	MAX_USERDATA_SIZE				( 1 << MAX_USERDATA_BITS )
#define SUBSTRING_BITS					5

#define NUM_NETWORKED_EHANDLE_SERIAL_NUMBER_BITS	10

#define ENTITY_SENTINEL			9999

#define	NET_MAX_MESSAGE			PAD_NUMBER( ( NET_MAX_PAYLOAD + HEADER_BYTES ), 16 )

#define NET_HEADER_FLAG_SPLITPACKET			-2
#define NET_HEADER_FLAG_COMPRESSEDPACKET	-3


// each channel packet has 1 byte of FLAG bits
#define PACKET_FLAG_RELIABLE			(1<<0)	// packet contains subchannel stream data
#define PACKET_FLAG_COMPRESSED			(1<<1)	// packet is compressed
#define PACKET_FLAG_ENCRYPTED			(1<<2)  // packet is encrypted
#define PACKET_FLAG_SPLIT				(1<<3)  // packet is split
#define PACKET_FLAG_CHOKED				(1<<4)  // packet was choked by sender
//printf("PACKET_FLAG_RELIABLE   = %i\n", PACKET_FLAG_RELIABLE);
//printf("PACKET_FLAG_COMPRESSED = %i\n", PACKET_FLAG_COMPRESSED);
//printf("PACKET_FLAG_ENCRYPTED  = %i\n", PACKET_FLAG_ENCRYPTED);
//printf("PACKET_FLAG_SPLIT	   = %i\n", PACKET_FLAG_SPLIT);
//printf("PACKET_FLAG_CHOKED	   = %i\n", PACKET_FLAG_CHOKED);



enum UpdateType
{
	EnterPVS = 0,	// Entity came back into pvs, create new entity if one doesn't exist
	LeavePVS,		// Entity left pvs
	DeltaEnt,		// There is a delta for this entity.
	PreserveEnt,	// Entity stays alive but no delta ( could be LOD, or just unchanged )
	Finished,		// finished parsing entities successfully
	Failed,			// parsing error occured while reading entities
};

// Flags for delta encoding header
enum HeaderFlags
{
	FHDR_ZERO = 0x0000,
	FHDR_LEAVEPVS = 0x0001,
	FHDR_DELETE = 0x0002,
	FHDR_ENTERPVS = 0x0004,
};

// get sockaddr, IPv4 or IPv6:
static void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET)
		return &(((struct sockaddr_in*)sa)->sin_addr);
	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

void print_dev(pcap_if_t* dev)
{
	outf("\n\n%s\n", dev->description);
	outf("  name: %s\n", dev->name);
	outf("  desc: %s\n", dev->description);
	out("  addresses:\n"); // print out all addresses that the device holds
	for (pcap_addr* curraddr = dev->addresses; curraddr; curraddr = curraddr->next)
	{
		char s[INET6_ADDRSTRLEN]; // make the ip to string
		inet_ntop(curraddr->addr->sa_family, get_in_addr((struct sockaddr*)curraddr->addr), s, INET6_ADDRSTRLEN);
		outf("    [%s] %s\n", curraddr->addr->sa_family == AF_INET ? "ipv4" : "ipv6", s);
	}
	out("\n");
}

std::vector<pcap_if_t*> list_all_devs()
{
	std::vector<pcap_if_t*> devList;
	pcap_if_t *alldevs;
	char errbuf[PCAP_ERRBUF_SIZE];

	/* Retrieve the device list from the local machine */
	if (pcap_findalldevs(&alldevs, errbuf) == -1)
	{
		outf("Error in pcap_findalldevs: %s\n", errbuf);
		return devList;
	}

	// Loop all devices
	for (pcap_if_t* dev = alldevs; dev; dev = dev->next)
	{
		// add device to dev list
		devList.push_back(dev);

		// print out info and index
		outf("\n[ %i ] %s\n", devList.size(), dev->description);
		outf("  name: %s\n", dev->name);
		outf("  desc: %s\n", dev->description);
		out("  addresses:\n"); // print out all addresses that the device holds
		for (pcap_addr* curraddr = dev->addresses; curraddr; curraddr = curraddr->next)
		{
			char s[INET6_ADDRSTRLEN]; // make the ip to string
			inet_ntop(curraddr->addr->sa_family, get_in_addr((struct sockaddr*)curraddr->addr), s, INET6_ADDRSTRLEN);
			outf("    [%s] %s\n", curraddr->addr->sa_family == AF_INET ? "ipv4" : "ipv6", s);
		}
		out("\n");
	}


	// return the built dev list
	return devList;
}

std::vector<pcap_if_t*> create_dev_list()
{
	std::vector<pcap_if_t*> devList;
	pcap_if_t *alldevs;
	char errbuf[PCAP_ERRBUF_SIZE];

	/* Retrieve the device list from the local machine */
	if (pcap_findalldevs(&alldevs, errbuf) == -1)
	{
		outf("Error in pcap_findalldevs: %s\n", errbuf);
		return devList;
	}

	// Loop all devices
	for (pcap_if_t* dev = alldevs; dev; dev = dev->next)
		// add device to dev list
		devList.push_back(dev);

	// return the built dev list
	return devList;
}