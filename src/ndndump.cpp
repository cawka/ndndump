/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/**
 * Copyright (c) 2011-2014, Regents of the University of California,
 *
 * This file is part of ndndump, the packet capture and analysis tool for Named Data
 * Networking (NDN).
 *
 * ndndump is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later version.
 *
 * ndndump is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * ndndump, e.g., in COPYING file.  If not, see <http://www.gnu.org/licenses/>.
 **/

#include "ndndump.hpp"

#include "config.hpp"

#include "tcpdump/tcpdump-stdinc.h"

namespace ndn {
namespace tools {
// namespace is necessary for now to prevent clashing with system includes
// this will not be needed after ndn-cxx/common.hpp will stop including <boost/asio.hpp>

#include "tcpdump/ether.h"
#include "tcpdump/ip.h"
#include "tcpdump/udp.h"
#include "tcpdump/tcp.h"

} // namespace tools
} // namespace ndn

#include <boost/lexical_cast.hpp>

#include <iomanip>

#include <ndn-cxx/interest.hpp>
#include <ndn-cxx/data.hpp>

namespace ndn {
namespace tools {

// const uint8_t NDNLP_HEADER[] = {'N', 'd', 'n', 'l', 'p'};

const size_t MAX_SNAPLEN = 65535;

void
Ndndump::run()
{
  if (inputFile.empty() && interface.empty()) {
    char errbuf[PCAP_ERRBUF_SIZE];
    const char* pcapDevice = pcap_lookupdev(errbuf);

    if (pcapDevice == 0) {
      throw Error(errbuf);
    }

    interface = pcapDevice;
  }

  if (isVerbose) {
    if (!interface.empty()) {
      std::cerr << "ndndump: listening on " << interface << std::endl;
    }
    else {
      std::cerr << "ndndump: reading from " << inputFile << std::endl;
    }

    if (!nameFilter.empty()) {
      std::cerr << "ndndump: using name filter " << nameFilter << std::endl;
    }
  }

  if (!interface.empty()) {
    char errbuf[PCAP_ERRBUF_SIZE];
    m_pcap = pcap_open_live(interface.c_str(), MAX_SNAPLEN, 0, 1000, errbuf);
    if (m_pcap == 0) {
      throw Error("Cannot open interface " + interface + "(" + errbuf + ")");
    }
  }
  else {
    char errbuf[PCAP_ERRBUF_SIZE];
    m_pcap = pcap_open_offline(inputFile.c_str(), errbuf);
    if (m_pcap == 0) {
      throw Error("Cannot file " + inputFile + " for reading (" + errbuf + ")");
    }
  }

  if (!pcapProgram.empty()) {
    if (isVerbose) {
      std::cerr << "ndndump: pcap_filter = " << pcapProgram << std::endl;
    }

    bpf_program program;
    int returnValue = pcap_compile(m_pcap, &program, pcapProgram.c_str(), 0, PCAP_NETMASK_UNKNOWN);

    if (returnValue < 0) {
      throw Error("Cannot parse tcpdump expression '" + pcapProgram +
                  "' (" + pcap_geterr(m_pcap) + ")");
    }

    returnValue = pcap_setfilter(m_pcap, &program);
    pcap_freecode(&program);

    if (returnValue < 0) {
      throw Error(std::string("pcap_setfilter failed (") + pcap_geterr(m_pcap) + ")");
    }
  }

  m_dataLinkType = pcap_datalink(m_pcap);
  if (m_dataLinkType != DLT_EN10MB &&
      m_dataLinkType != DLT_PPP)
    {
      throw Error("Unsupported pcap format (" + boost::lexical_cast<std::string>(m_dataLinkType));
    }

  pcap_loop(m_pcap, -1, &Ndndump::onCapturedPacket, reinterpret_cast<uint8_t*>(this));
}


void
Ndndump::onCapturedPacket(const struct pcap_pkthdr* header, const uint8_t* packet)
{
  std::ostringstream os;
  printInterceptTime(os, header);

  const uint8_t* payload = packet;
  ssize_t payloadSize = header->len;

  int frameType = skipDataLinkHeaderAndGetFrameType(payload, payloadSize);
  if (frameType < 0) {
    std::cerr << "Unknown frame type" << std::endl;
    return;
  }

  int returnValue = skipAndProcessFrameHeader(frameType, payload, payloadSize, os);
  if (returnValue < 0) {
    return;
  }

  Block block;
  bool isOk = Block::fromBuffer(payload, payloadSize, block);
  if (!isOk) {
    // if packet is fragmented, we will not be able to process it
    return;
  }

  /// \todo Detect various header (LocalControlHeader, NDNLP, etc.)

  try {
    if (block.type() == tlv::Interest) {
      Interest interest(block);
      if (matchesFilter(interest.getName())) {
        std::cout << os.str() << ", " << "INTEREST: " << interest << std::endl;
      }
    }
    else if (block.type() == tlv::Data) {
      Data data(block);
      if (matchesFilter(data.getName())) {
        std::cout << os.str() << ", " << "DATA: " << data.getName() << std::endl;
      }
    }
  }
  catch (tlv::Error& e) {
    std::cerr << e.what() << std::endl;
  }
}

void
Ndndump::printInterceptTime(std::ostream& os, const struct pcap_pkthdr* header)
{
  os << header->ts.tv_sec
     << "."
     << std::setfill('0') << std::setw(6) << header->ts.tv_usec;

  // struct tm* tm;
  // if (flags.unit_time) {
  //   os << (int) header->ts.tv_sec
  //      << "."
  //      << setfill('0') << setw(6) << (int)header->ts.tv_usec;
  // } else {
  //   tm = localtime(&(header->ts.tv_sec));
  //   os << (int)tm->tm_hour << ":"
  //      << setfill('0') << setw(2) << (int)tm->tm_min<< ":"
  //      << setfill('0') << setw(2) << (int)tm->tm_sec<< "."
  //      << setfill('0') << setw(6) << (int)header->ts.tv_usec;
  // }
  os << " ";
}

int
Ndndump::skipDataLinkHeaderAndGetFrameType(const uint8_t*& payload, ssize_t& payloadSize)
{
  int frameType = 0;

  switch (m_dataLinkType) {
  case DLT_EN10MB: // Ethernet frames can have Ethernet or 802.3 encapsulation
    {
      const ether_header* etherHeader = reinterpret_cast<const ether_header*>(payload);

      if (payloadSize < 0) {
        std::cerr << "Invalid pcap Ethernet frame" << std::endl;
        return -1;
      }

      frameType = ntohs(etherHeader->ether_type);
      payloadSize -= ETHER_HDRLEN;
      payload += ETHER_HDRLEN;

      break;
    }
  case DLT_PPP:
    {
      frameType = *payload;
      payloadSize--;
      payload++;

      if (!(frameType & 1)) {
        frameType = (frameType << 8) | *payload;
        payloadSize--;
        payload++;
      }

      if (payloadSize < 0) {
        std::cerr << "Invalid PPP frame" << std::endl;
        return -1;
      }

      break;
    }
  }

  return frameType;
}

int
Ndndump::skipAndProcessFrameHeader(int frameType,
                                   const uint8_t*& payload, ssize_t& payloadSize,
                                   std::ostream& os)
{
  switch (frameType)
    {
    case /*ETHERTYPE_IP*/0x0800:
    case DLT_EN10MB: // pcap encapsulation
      {
        const ip* ipHeader = reinterpret_cast<const ip*>(payload);
        size_t ipHeaderSize = IP_HL(ipHeader) * 4;
        if (ipHeaderSize < 20) {
          std::cerr << "invalid IP header len " << ipHeaderSize << " bytes" << std::endl;
          return -1;
        }

        os << "From: " << inet_ntoa(ipHeader->ip_src) << ", ";
        os << "To: "   << inet_ntoa(ipHeader->ip_dst);

        payloadSize -= ipHeaderSize;
        payload += ipHeaderSize;

        if (payloadSize < 0) {
          std::cerr << "Invalid pcap IP packet" << std::endl;
          return -1;
        }

        switch (ipHeader->ip_p) {
        case IPPROTO_UDP:
          {
            // if (!flags.udp)
            //   return -1;

            payloadSize -= sizeof(udphdr);
            payload += sizeof(udphdr);

            if (payloadSize < 0) {
                std::cerr << "Invalid pcap UDP/IP packet" << std::endl;
                return -1;
              }

            os << ", Tunnel Type: UDP";
            break;
          }
        case IPPROTO_TCP:
          {
            // if (!flags.tcp)
            //   return -1;

            const tcphdr* tcpHeader = reinterpret_cast<const tcphdr*>(payload);
            size_t tcpHeaderSize = TH_OFF(tcpHeader) * 4;

            if (tcpHeaderSize < 20) {
              std::cerr << "Invalid TCP Header len: "<< tcpHeaderSize <<" bytes" << std::endl;
              return -1;
            }

            payloadSize -= tcpHeaderSize;
            payload += tcpHeaderSize;

            if (payloadSize < 0) {
              std::cerr << "Invalid pcap TCP/IP packet" << std::endl;
              return -1;
            }

            os << ", Tunnel Type: TCP";
            break;
          }
        default:
          return -1;
        }

        break;
      }
    case /*ETHERTYPE_NDN*/0x7777:
      os << "Tunnel Type: EthernetFrame";
      break;
    case /*ETHERTYPE_NDNLP*/0x8624:
      os << "Tunnel Type: EthernetFrame";
      break;
    case 0x0077: // pcap
      os << "Tunnel Type: PPP";
      payloadSize -= 2;
      payload += 2;
      break;
    default:
      return -1;
      break; // do nothing if it is not a recognized type of a packet
    }

  return 0;
}

} // namespace tools
} // namespace ndn
