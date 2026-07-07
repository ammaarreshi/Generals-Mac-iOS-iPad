/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

////////////////////////////////////////////////////////////////////////////////
//																																						//
//  (c) 2001-2003 Electronic Arts Inc.																				//
//																																						//
////////////////////////////////////////////////////////////////////////////////

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "GameNetwork/IPEnumeration.h"
#include "GameNetwork/networkutil.h"
#include "GameClient/ClientInstance.h"

#ifndef _WIN32
#include <errno.h>
#include <ifaddrs.h>
#include <net/if.h>
#endif

IPEnumeration::IPEnumeration()
{
	m_IPlist = nullptr;
	m_isWinsockInitialized = false;
}

IPEnumeration::~IPEnumeration()
{
	if (m_isWinsockInitialized)
	{
		WSACleanup();
		m_isWinsockInitialized = false;
	}

	EnumeratedIP *ip = m_IPlist;
	while (ip)
	{
		ip = ip->getNext();
		deleteInstance(m_IPlist);
		m_IPlist = ip;
	}
}

EnumeratedIP * IPEnumeration::getAddresses()
{
	if (m_IPlist)
		return m_IPlist;

	if (!m_isWinsockInitialized)
	{
		WORD verReq = MAKEWORD(2, 2);
		WSADATA wsadata;

		int err = WSAStartup(verReq, &wsadata);
		if (err != 0) {
			return nullptr;
		}

		if ((LOBYTE(wsadata.wVersion) != 2) || (HIBYTE(wsadata.wVersion) !=2)) {
			WSACleanup();
			return nullptr;
		}
		m_isWinsockInitialized = true;
	}

	// TheSuperHackers @feature Add one unique local host IP address for each multi client instance.
	if (rts::ClientInstance::isMultiInstance())
	{
		const UnsignedInt id = rts::ClientInstance::getInstanceId();
		addNewIP(
			127,
			(UnsignedByte)(id >> 16),
			(UnsignedByte)(id >> 8),
			(UnsignedByte)(id));
	}

#ifndef _WIN32
	// GeneralsX @bugfix BenderAI 31/03/2026 Enumerate active IPv4 interfaces on non-Windows (POSIX) platforms instead of hostname resolution.
	struct ifaddrs *ifaddr = nullptr;
	if (getifaddrs(&ifaddr) == 0)
	{
		for (struct ifaddrs *ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next)
		{
			if (ifa->ifa_addr == nullptr)
			{
				continue;
			}

			if (ifa->ifa_addr->sa_family != AF_INET)
			{
				continue;
			}

			if ((ifa->ifa_flags & IFF_UP) == 0 || (ifa->ifa_flags & IFF_LOOPBACK) != 0)
			{
				continue;
			}

			// GeneralsX @bugfix 07/07/2026 Skip point-to-point interfaces: cellular (pdp_ip* on iOS) and
			// VPN tunnels (utun*) are not reachable by LAN peers, and their addresses sort numerically
			// below the Wi-Fi/Ethernet address, so the LAN lobby would pick them as the default IP.
			if ((ifa->ifa_flags & IFF_POINTOPOINT) != 0)
			{
				continue;
			}

			const sockaddr_in *addr = reinterpret_cast<const sockaddr_in *>(ifa->ifa_addr);
			// GeneralsX @bugfix BenderAI 31/03/2026 Use ntohl to convert from network byte order before extracting octets;
			// reading s_addr byte-by-byte on little-endian platforms reverses the IPv4 octets.
			const UnsignedInt hostAddr = ntohl(addr->sin_addr.s_addr);

			// GeneralsX @bugfix 07/07/2026 Skip 169.254.0.0/16 link-local addresses (awdl0/llw0 on Apple
			// platforms, or any interface that failed DHCP); they cannot host a LAN game.
			if ((hostAddr & 0xFFFF0000u) == 0xA9FE0000u)
			{
				continue;
			}
			addNewIP(
				(UnsignedByte)((hostAddr >> 24) & 0xFF),
				(UnsignedByte)((hostAddr >> 16) & 0xFF),
				(UnsignedByte)((hostAddr >> 8) & 0xFF),
				(UnsignedByte)(hostAddr & 0xFF));
		}
		freeifaddrs(ifaddr);

		if (m_IPlist)
		{
			return m_IPlist;
		}
	}
	else
	{
		DEBUG_LOG(("Failed call to getifaddrs; errno returned %d", errno));
	}
#endif

	// get the local machine's host name
	char hostname[256];
	if (gethostname(hostname, sizeof(hostname)))
	{
		DEBUG_LOG(("Failed call to gethostname; WSAGetLastError returned %d", WSAGetLastError()));
		return m_IPlist;
	}
	DEBUG_LOG(("Hostname is '%s'", hostname));

	// get host information from the host name
	hostent* hostEnt = gethostbyname(hostname);
	if (hostEnt == nullptr)
	{
		DEBUG_LOG(("Failed call to gethostbyname; WSAGetLastError returned %d", WSAGetLastError()));
		return m_IPlist;
	}

	// sanity-check the length of the IP adress
	if (hostEnt->h_length != 4)
	{
		DEBUG_LOG(("gethostbyname returns oddly-sized IP addresses!"));
		return m_IPlist;
	}

	// construct a list of addresses
	int numAddresses = 0;
	char *entry;
	while ( (entry = hostEnt->h_addr_list[numAddresses++]) != nullptr )
	{
		addNewIP(
			(UnsignedByte)entry[0],
			(UnsignedByte)entry[1],
			(UnsignedByte)entry[2],
			(UnsignedByte)entry[3]);
	}

	return m_IPlist;
}

void IPEnumeration::addNewIP( UnsignedByte a, UnsignedByte b, UnsignedByte c, UnsignedByte d )
{
	UnsignedInt ip = AssembleIp(a, b, c, d);
	for (EnumeratedIP *current = m_IPlist; current != nullptr; current = current->getNext())
	{
		if (current->getIP() == ip)
		{
			return;
		}
	}

	EnumeratedIP *newIP = newInstance(EnumeratedIP);

	AsciiString str;
	str.format("%d.%d.%d.%d", (int)a, (int)b, (int)c, (int)d);

	newIP->setIPstring(str);
	newIP->setIP(ip);

	DEBUG_LOG(("IP: 0x%8.8X (%s)", ip, str.str()));

	// Add the IP to the list in ascending order
	if (!m_IPlist)
	{
		m_IPlist = newIP;
		newIP->setNext(nullptr);
	}
	else
	{
		if (newIP->getIP() < m_IPlist->getIP())
		{
			newIP->setNext(m_IPlist);
			m_IPlist = newIP;
		}
		else
		{
			EnumeratedIP *p = m_IPlist;
			while (p->getNext() && p->getNext()->getIP() < newIP->getIP())
			{
				p = p->getNext();
			}
			newIP->setNext(p->getNext());
			p->setNext(newIP);
		}
	}
}

AsciiString IPEnumeration::getMachineName()
{
	if (!m_isWinsockInitialized)
	{
		WORD verReq = MAKEWORD(2, 2);
		WSADATA wsadata;

		int err = WSAStartup(verReq, &wsadata);
		if (err != 0) {
			return "";
		}

		if ((LOBYTE(wsadata.wVersion) != 2) || (HIBYTE(wsadata.wVersion) !=2)) {
			WSACleanup();
			return "";
		}
		m_isWinsockInitialized = true;
	}

	// get the local machine's host name
	char hostname[256];
	if (gethostname(hostname, sizeof(hostname)))
	{
		DEBUG_LOG(("Failed call to gethostname; WSAGetLastError returned %d", WSAGetLastError()));
		return "";
	}

	return AsciiString(hostname);
}


