/*
 * SixLowPanNetDevice.cpp
 *
 *  Created on: Mar 1, 2022
 *      Author: alfonso
 */


/*
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Copyright (c) 2013 Universita' di Firenze, Italy
 * Author: Tommaso Pecorella <tommaso.pecorella@unifi.it>
 *         Michele Muccio <michelemuccio@virgilio.it>
 * Copyright (c) 2022 Universidad de Malaga, Spain
 * Author: Alfonso Ariza aarizaq@uma.es
 */

// TODO: Check the fragmentation.
#include "inet/common/INETDefs.h"
#include "inet/wirelesspan/networklayer/sixlowpan/Ipv6SixLowPan.h"
#include "inet/wirelesspan/networklayer/sixlowpan/SixLowPanDispatchCode.h"
#include "inet/wirelesspan/networklayer/sixlowpan/SixLowPanDispatchCode.h"
#include "inet/wirelesspan/networklayer/sixlowpan/SixLowPanHeader_m.h"
#include "inet/networklayer/ipv6/Ipv6InterfaceData.h"
#include "inet/networklayer/ipv6/Ipv6Header.h"
#include "inet/transportlayer/udp/UdpHeader_m.h"
#include "inet/networklayer/common/L3Tools.h"
#include "inet/transportlayer/udp/Udp.h"
#include "inet/linklayer/common/MacAddressTag_m.h"
#include "inet/linklayer/common/InterfaceTag_m.h"
#include "inet/common/packet/ReassemblyBuffer.h"
#include "inet/networklayer/ipv6/Ipv6ExtensionHeaders_m.h"

#ifdef INET_WITH_IPv6
namespace inet {
namespace wirelesspan {
namespace sixlowpan {


std::string Ipv6SixLowPan::ContextEntry::str() const
{
    std::string base = contextPrefix.str();
    base += "/"+std::to_string(prefixLength);
    base +=" Compression allowed: ";
    if (compressionAllowed)
        base +="true";
    else
        base +="false";
    base +=" Valid lifetime: "+validLifetime.str();
    return base;
}

std::ostream& operator<<(std::ostream& os, const Ipv6SixLowPan::ContextEntry& ip)
{
    auto str = ip.str();
    return os << str;
}

Define_Module(Ipv6SixLowPan);
Ipv6SixLowPan::~Ipv6SixLowPan() {
    // TODO Auto-generated destructor stub
    if (m_timeoutEvent)
        cancelAndDelete(m_timeoutEvent);
    m_timeoutEventList.clear();
    for (auto &elem:m_fragments)
      delete elem.second;
    m_fragments.clear();
}

// Convenience methods to handle address
void Ipv6SixLowPan::convertFromIpv6AddressToUint8(const Ipv6Address &addr, uint8_t v[16]) {
    uint32_t word[4];
    memcpy(word, addr.words(), sizeof(word));
    for (int i = 0; i < 4; i++) {
#if BYTE_ORDER == LITTLE_ENDIAN
        uint8_t *s = (uint8_t *)&word[i];
        uint32_t aux = (uint32_t)(s[0] << 24 | s[1] << 16 | s[2] << 8 | s[3]);
        word[i] = aux;
#endif
    }
    memcpy(v, word, sizeof(uint8_t)*16);
}

void Ipv6SixLowPan::convertFromUint8ToIpv6Address(const uint8_t v[16], Ipv6Address & addr) {

    uint32_t word[4];
    for (int i = 0; i < 4; i++) {
        uint32_t aux;
        uint8_t *s = (uint8_t *)&aux;
#if BYTE_ORDER == LITTLE_ENDIAN
        for (int j = 0; j < 4; j++)
            s[j] = v[(i*4)+3-j];
#else
        for (j = 0; j < 4; j++)
            s[j] = v[(i*4)+j];
#endif
        word[i] = aux;
    }
    addr.set(word[0], word[1], word[2], word[3]);
}

L3Address Ipv6SixLowPan::makeAutoconfiguredLinkLocalAddress(L3Address addr)
{
    if (addr.getType() != L3Address::MAC)
        throw cRuntimeError("AdL3Addresss not of type MAC");
    Ipv6Address ret = Ipv6Address::formLinkLocalAddress(addr.toMac().formInterfaceIdentifier());
    return L3Address(ret);
}

L3Address Ipv6SixLowPan::makeAutoconfiguredAddress (L3Address addrAux, Ipv6Address prefix, int prefixLeng)
{
    MacAddress addr = addrAux.toMac();
    Ipv6Address ipv6PrefixAddr;
    ipv6PrefixAddr.setPrefix(prefix, prefixLeng);
    Ipv6Address ret;
    uint8_t buf[16];
    uint8_t buf2[16];
    addr.getAddressBytes(buf);

    Ipv6SixLowPan::convertFromIpv6AddressToUint8(ipv6PrefixAddr, buf2);
    memcpy (buf2 + 8, buf, 3);
    buf2[11] = 0xff;
    buf2[12] = 0xfe;
    memcpy (buf2 + 13, buf + 3, 3);
    buf2[8] ^= 0x02;
    Ipv6SixLowPan::convertFromUint8ToIpv6Address(buf2, ret);
    return ret;
}

Ipv6Address Ipv6SixLowPan::cleanPrefix (Ipv6Address address, int prefixLength)
{

    uint32_t mask[4];
    Ipv6Address::constructMask(prefixLength, mask);
    uint32_t d[4];
    memcpy(d, address.words(), sizeof(d));

    d[0] = (d[0] & ~mask[0]);
    d[1] = (d[1] & ~mask[1]);
    d[2] = (d[2] & ~mask[2]);
    d[3] = (d[3] & ~mask[3]);
    Ipv6Address cleanedAddress(d[0], d[1], d[2], d[3]);
    return cleanedAddress;
}


// Methods to check the interface
bool Ipv6SixLowPan::checkSixLowPanInterfaceById(const int &id) const
{
    auto it = listSixLowPanInterfaces.find(id);
    if (it == listSixLowPanInterfaces.end())
        return false;
    return true;
}



bool Ipv6SixLowPan::checkSixLowPanInterface(const NetworkInterface *ie) const
{
    if (ie == nullptr)
        throw cRuntimeError("Invalid interface");
    return checkSixLowPanInterfaceById(ie->getInterfaceId());
}

void Ipv6SixLowPan::initialize(int stage)
{
    Ipv6::initialize(stage);
    if (stage == INITSTAGE_NETWORK_LAYER_PROTOCOLS) {
//        interface = getContainingNicModule(this);
        m_useIphc = par("useIphc");
        m_omitUdpChecksum = par("OmitUDPchecksum");
        m_fragmentReassemblyListSize = par("FragmentReassemblyListSize");
        m_fragmentExpirationTimeout = par("FragmentExpirationTimeout");
        m_compressionThreshold = B(par("CompressionThreshold").intValue());
        m_meshUnder = par("UseMeshUnder");
        m_meshUnderHopsLeft = par("MeshUnderRadius");
        m_meshCacheLength = par("MeshCacheLength");
        m_meshUnderJitter = &par("MeshUnderJitter");

        aceptAllInterfaces = par("aceptAllInterfaces"); // accept all sixlowpan messages even if the interface is not sixlowpan

        ipv6FragemtationMtu = B(par("ipv6FragemtationMtu").intValue());

        // list of sixlowpan interfaces.
        const char *auxChar = par("sixlowpanInterfaces").stringValue();
        std::vector<std::string> interfaceList = cStringTokenizer(auxChar).asVector();
        for (const auto &elem : interfaceList) {
            auto ie = ift->findInterfaceByName(elem.c_str());
            if (ie) {
                listSixLowPanInterfaces.insert(ie->getInterfaceId());
                listSixLowPanInterfacesNames.push_back(std::string(ie->getInterfaceName()));
            }
        }
        const char *prefixChar = par("ContexCompresionList").stringValue();
        std::vector<std::string> prefixList = cStringTokenizer(prefixChar).asVector();

        for (size_t i = 0; i < prefixList.size(); i++) {
            std::string process = prefixList[i];
            auto posIndex = process.find("?");
            if (posIndex == std::string::npos)
                throw cRuntimeError("Context index not found");
            int index = std::stoi(prefixList[i].substr(0, posIndex));
            process = process.substr(posIndex+1, std::string::npos);
            auto pos = process.find("/");
            if (pos != std::string::npos) {
                std::string strPrefix = process.substr(0, pos);
                Ipv6Address addr(strPrefix.c_str());
                process = process.substr(pos+1, std::string::npos);
                pos = process.find("|");
                int pLen = std::stoi(process.substr(0, pos));
                process = process.substr(pos+1, std::string::npos);
                pos = process.find("|");
                bool valid;
                if (process.substr(0, pos) == "true")
                    valid = true;
                else
                    valid = false;
                process = process.substr(pos+1, std::string::npos);
                double time = std::stof(process);
                addContext (index, addr, pLen, valid, time);
            }
        }
        WATCH_VECTOR(listSixLowPanInterfacesNames);
        WATCH_MAP(m_contextTable);
        registerProtocol(Protocol::sixlowpan, gate("queueOut"), gate("queueIn"));
        // TODO: Prefix processing
    }
}

// Overloaded methods from Ipv6
void Ipv6SixLowPan::handleMessage(cMessage *msg)
{
    if (msg == m_timeoutEvent) {
        handleTimeoutList();
        return;
    }
    auto pkt = dynamic_cast<Packet *>(msg);
    if (pkt) {

        auto protocol = findPacketProtocol(pkt);
        auto chunk = pkt->peekAtFront<Chunk>();
        auto sixLowPandDispach = dynamicPtrCast<const SixLowPanDispatch>(chunk);
        if (protocol == &Protocol::sixlowpan || sixLowPandDispach != nullptr){
            bool process = false;
            if (aceptAllInterfaces)
                process = true;
            else {
                auto tag = pkt->findTag<InterfaceInd>();
                if (tag && checkSixLowPanInterfaceById(tag->getInterfaceId()))
                    process = true;
            }
            if (process) {
                if (handleMessageFromNetwork(pkt)) {
                    msg = pkt; // The packet can change (fragmentation procedure)
                    auto header = pkt->peekAtFront<Ipv6Header>();
                    EV_DEBUG << "Decompressing packet src:" << header->getSrcAddress().str() <<" dest:"<< header->getDestinationAddress().str() << endl;
                }
                else
                    return;
            }
            else {
                EV_TRACE << "Packet discarded by the de-compression process " << endl;
                delete pkt;
                return;
            }

        }
    }
    Ipv6::handleMessage(msg);
}

void Ipv6SixLowPan::fragmentAndSend(Packet *packet, const NetworkInterface *ie, const MacAddress& nextHopAddr, bool fromHL)
{

    if (listSixLowPanInterfaces.empty() || !checkSixLowPanInterface(ie)) {
        Ipv6::fragmentAndSend(packet, ie, nextHopAddr, fromHL);
        return;
    }

    auto ipv6Header = packet->peekAtFront<Ipv6Header>();
    // hop counter check
    if (ipv6Header->getHopLimit() <= 0) {
        // drop datagram, destruction responsibility in ICMP
        EV_INFO << "datagram hopLimit reached zero, sending ICMPv6_TIME_EXCEEDED\n";
        sendIcmpError(packet, ICMPv6_TIME_EXCEEDED, 0); // FIXME check icmp 'code'
        return;
    }

    // ensure source address is filled
    if (fromHL && ipv6Header->getSrcAddress().isUnspecified() &&
        !ipv6Header->getDestAddress().isSolicitedNodeMulticastAddress())
    {
        // source address can be unspecified during DAD
        const Ipv6Address& srcAddr = ie->getProtocolData<Ipv6InterfaceData>()->getPreferredAddress();
        ASSERT(!srcAddr.isUnspecified()); // FIXME what if we don't have an address yet?

        // TODO factor out
        packet->eraseAtFront(ipv6Header->getChunkLength());
        auto ipv6HeaderCopy = staticPtrCast<Ipv6Header>(ipv6Header->dupShared());
        // TODO dup or mark ipv4Header->markMutableIfExclusivelyOwned();
        ipv6HeaderCopy->setSrcAddress(srcAddr);
        packet->insertAtFront(ipv6HeaderCopy);
        ipv6Header = ipv6HeaderCopy;

    #ifdef INET_WITH_xMIPv6
        // if the datagram has a tentative address as source we have to reschedule it
        // as it can not be sent before the address' tentative status is cleared - CB
        if (ie->getProtocolData<Ipv6InterfaceData>()->isTentativeAddress(srcAddr)) {
            EV_INFO << "Source address is tentative - enqueueing datagram for later resubmission." << endl;
            ScheduledDatagram *sDgram = new ScheduledDatagram(packet, ipv6Header.get(), ie, nextHopAddr, fromHL);
//            queue.insert(sDgram);
            scheduleAfter(1.0, sDgram); // KLUDGE wait 1s for tentative->permanent. MISSING: timeout for drop or send back icmpv6 error, processing signals from IE, need another msg queue for waiting (similar to Ipv4 ARP)
            return;
        }
    #endif /* INET_WITH_xMIPv6 */
    }


    int mtu = (ie->getMtu() < ipv6FragemtationMtu.get()) ? ipv6FragemtationMtu.get() : ie->getMtu();
    // check if datagram does not require fragmentation
    if (packet->getByteLength() <= mtu) {
        sendDatagramToOutput(packet, ie, nextHopAddr);
        return;
    }

    // routed datagrams are not fragmented
    if (!fromHL) {
        // FIXME check for multicast datagrams, how many ICMP error should be sent
        sendIcmpError(packet, ICMPv6_PACKET_TOO_BIG, 0); // TODO set MTU
        return;
    }

    // create and send fragments
    ipv6Header = packet->popAtFront<Ipv6Header>();
    B headerLength = ipv6Header->calculateUnfragmentableHeaderByteLength();
    B payloadLength = packet->getDataLength();
    B fragmentLength = ((B(mtu) - headerLength - IPv6_FRAGMENT_HEADER_LENGTH) / 8) * 8;
    ASSERT(fragmentLength > B(0));

    int noOfFragments = B(payloadLength + fragmentLength - B(1)).get() / B(fragmentLength).get();
    EV_INFO << "Breaking datagram into " << noOfFragments << " fragments\n";
    std::string fragMsgName = packet->getName();
    fragMsgName += "-frag-";

    // FIXME is need to remove unfragmentable header extensions? see calculateUnfragmentableHeaderByteLength()

    unsigned int identification = curFragmentId++;
    for (B offset = B(0); offset < payloadLength; offset += fragmentLength) {
        bool lastFragment = (offset + fragmentLength >= payloadLength);
        B thisFragmentLength = lastFragment ? payloadLength - offset : fragmentLength;

        std::string curFragName = fragMsgName + std::to_string(offset.get());
        if (lastFragment)
            curFragName += "-last";
        Packet *fragPk = new Packet(curFragName.c_str());
        const auto& fragHdr = staticPtrCast<Ipv6Header>(ipv6Header->dupShared());
        auto *fh = new Ipv6FragmentHeader();
        fh->setIdentification(identification);
        fh->setFragmentOffset(offset.get());
        fh->setMoreFragments(!lastFragment);
        fragHdr->addExtensionHeader(fh);
        fragHdr->setChunkLength(headerLength + fh->getByteLength()); // KLUDGE
        fragPk->insertAtFront(fragHdr);
        fragPk->insertAtBack(packet->peekDataAt(offset, thisFragmentLength));

        ASSERT(fragPk->getDataLength() == headerLength + fh->getByteLength() + thisFragmentLength);

        sendDatagramToOutput(fragPk, ie, nextHopAddr);
    }

    delete packet;
}

void Ipv6SixLowPan::sendDatagramToOutput(Packet *packet, const NetworkInterface *destIE, const MacAddress& macAddr)
{
    if (listSixLowPanInterfaces.empty() || !checkSixLowPanInterface(destIE)) {
        Ipv6::sendDatagramToOutput(packet, destIE, macAddr);
        return;
    }

    packet->addTagIfAbsent<MacAddressReq>()->setDestAddress(macAddr);
    packet->addTagIfAbsent<InterfaceReq>()->setInterfaceId(destIE->getInterfaceId());
    packet->addTagIfAbsent<PacketProtocolTag>()->setProtocol(&Protocol::sixlowpan);
    packet->addTagIfAbsent<DispatchProtocolInd>()->setProtocol(&Protocol::sixlowpan);
    auto protocol = destIE->getProtocol();
    if (protocol != nullptr)
        packet->addTagIfAbsent<DispatchProtocolReq>()->setProtocol(protocol);
    else
        packet->removeTagIfPresent<DispatchProtocolReq>();
    L3Address dest(macAddr);
    L3Address src(MacAddress::UNSPECIFIED_ADDRESS);
    auto macAddInd = packet->addTagIfAbsent<MacAddressInd>();
    bool doSend = false;
    if (macAddInd)
        src = L3Address(macAddInd->getSrcAddress());
    if (!src.isUnspecified())
        doSend = true;
    if (!processAndSend (packet, src, dest, doSend, destIE->getInterfaceId())) {
        EV_TRACE << "Packet discarded by the compression process " << endl;
        delete packet;
    }
}

// This methods can process the packet and change it to a normal ipv6 packet,
// If false, the packet is handle inside by the method, if true, the packet is now a IPv6 packet that must be processed by the ipv6 module
bool Ipv6SixLowPan::handleMessageFromNetwork (Packet *packet)
{
    Ptr<SixLowPanDispatch> sixlowpanHeader;
    auto chunk = packet->peekAtFront<Chunk>();
    sixlowpanHeader = dynamicPtrCast<SixLowPanDispatch>(constPtrCast<Chunk>(chunk));

    auto protocol = packet->getTag<PacketProtocolTag>()->getProtocol();

    // check packet type
    if (protocol != &Protocol::sixlowpan && sixlowpanHeader != nullptr) {
        throw cRuntimeError("Protocol Id is incorrect");
    }

    auto src = L3Address(packet->getTag<MacAddressInd>()->getSrcAddress());
    int ifaceId = packet->getTag<InterfaceInd>()->getInterfaceId();
    auto dst = L3Address(getAddress(ifaceId));

    EV_DEBUG << "packet" << packet->str() << "incomingPort:" << ifaceId <<  "protocol:" << protocol->getName() << "Src:" << src << "Dst:" <<dst << endl;

    uint8_t dispatchVal = sixlowpanHeader->getDispatch();

    bool isPktDecompressed = false;
    bool fragmented = false;

    Ptr<SixLowPanMesh> meshHdr;
    Ptr<SixLowPanBc0> bc0Hdr;

    bool hasMesh = false;
    bool hasBc0 = false;

    if (dispatchVal == SixLowPanDispatchCode::LOWPAN_MESH )  {
        hasMesh = true;
        meshHdr = packet->removeAtFront<SixLowPanMesh>();
        sixlowpanHeader = constPtrCast<SixLowPanDispatch>(packet->peekAtFront<SixLowPanDispatch>());
        dispatchVal = sixlowpanHeader->getDispatch();
    }
    if ( dispatchVal == SixLowPanDispatchCode::LOWPAN_BC0 ) {
        hasBc0 = true;
        bc0Hdr = packet->removeAtFront<SixLowPanBc0>();
        sixlowpanHeader = constPtrCast<SixLowPanDispatch>(packet->peekAtFront<SixLowPanDispatch>());
        dispatchVal = sixlowpanHeader->getDispatch();
    }

    if (hasMesh) {
        if (!hasBc0) {
            EV << "Dropped packet - we only support mesh if it is paired with a BC0" << endl;
            numDropped++;
            delete packet;
            return false;
        }

        if (find (m_seenPkts[meshHdr->getOriginator()].begin (),
                m_seenPkts[meshHdr->getOriginator()].end (),
                bc0Hdr->getSeqNumber ()) != m_seenPkts[meshHdr->getOriginator()].end ()) {
            EV << "We have already seen this, no further processing." << endl;
            numDropped++;
            delete packet;
            return false;
        }

        m_seenPkts[meshHdr->getOriginator ()].push_back (bc0Hdr->getSeqNumber());
        if (m_seenPkts[meshHdr->getOriginator ()].size () > m_meshCacheLength) {
            m_seenPkts[meshHdr->getOriginator ()].pop_front ();
        }

        //NS_ABORT_MSG_IF (!Mac16Address::IsMatchingType (meshHdr.GetFinalDst ()), "SixLowPan mesh-under flooding can not currently handle extended address final destinations: " << meshHdr.GetFinalDst ());
        //NS_ABORT_MSG_IF (!Mac48Address::IsMatchingType (m_netDevice->GetAddress ()), "SixLowPan mesh-under flooding can not currently handle devices using extended addresses: " << m_netDevice->GetAddress ());

        auto destination = meshHdr->getDestination();

        // check local
        // See if the packet is for others than me. In case forward it.
        if (!ift->isLocalAddress(destination) || destination.isBroadcast ()  || destination.isMulticast ()) {
            uint8_t hopsLeft = meshHdr->getHopsLeft();
            if (hopsLeft == 0) {
                EV_DEBUG << "Not forwarding packet -- hop limit reached" << endl;
                numDropped++;
                delete packet;
            }
            else if (ift->isLocalAddress(meshHdr->getOriginator ())) {
                EV_DEBUG << "Not forwarding packet -- I am the originator" << endl;
                delete packet;
            }
            else  {
                meshHdr->setHopsLeft (hopsLeft - 1);
                auto sendPkt = packet->dup();
                sendPkt->insertAtFront(bc0Hdr);
                sendPkt->insertAtFront(meshHdr);
                // we will use the same incoming interface
                sendPkt->addTagIfAbsent<InterfaceReq>()->setInterfaceId(ifaceId);
                sendPkt->addTagIfAbsent<MacAddressReq>()->setDestAddress(getBroadcast(ifaceId));
                sendDelayed(sendPkt, m_meshUnderJitter->doubleValue() ,"queueOut");
            }
            if (!destination.isBroadcast () && !destination.isMulticast ()) {
                delete packet;
                return false;
            }
        }
    }

    L3Address realDst = dst;
    L3Address realSrc = src;
    if (hasMesh) {
        realSrc = meshHdr->getOriginator ();
        realDst = meshHdr->getDestinationAddress();
    }

  if ( dispatchVal == SixLowPanDispatchCode::LOWPAN_FRAG1 )  {
      isPktDecompressed = processFragment (packet, realSrc, realDst, true);
      fragmented = true;
  }
  else if ( dispatchVal == SixLowPanDispatchCode::LOWPAN_FRAGN ) {
      isPktDecompressed = processFragment (packet, realSrc, realDst, false);
      fragmented = true;
  }
  if (fragmented) {
      if ( !isPktDecompressed ) {
          return false;
      }
      else {
          // check packet
          auto chunk = packet->peekAtFront<Chunk>();
          sixlowpanHeader = dynamicPtrCast<SixLowPanDispatch>(constPtrCast<Chunk>(chunk));
          if (sixlowpanHeader == nullptr)
              throw cRuntimeError("Header is not of the type SixLowPanDispatch");
          dispatchVal = sixlowpanHeader->getDispatch();
         // copyPkt->CopyData (&dispatchRawVal, sizeof(dispatchRawVal));
          //dispatchVal = SixLowPanDispatch::GetDispatchType (dispatchRawVal);
      }
  }

  switch (dispatchVal) {
    case SixLowPanDispatchCode::LOWPAN_IPv6:
      EV_DEBUG << "Packet without compression. Length: " << packet->getByteLength() << endl;
      {
          // uncompressed packet, remove the dispatch and send the the normal ipv6 process
          isPktDecompressed = true;
      }
      break;
    case SixLowPanDispatchCode::LOWPAN_HC1:
        if (!m_useIphc) {
            decompressLowPanHc1 (packet, realSrc, realDst);
            isPktDecompressed = true;
        }
      break;
    case SixLowPanDispatchCode::LOWPAN_IPHC:
        if (m_useIphc) {
            if (!decompressLowPanIphc (packet, realSrc, realDst)) {
                // if false success decompress
                isPktDecompressed = true;
            }
        }
      break;
    default:
        EV << "Unsupported 6LoWPAN encoding: dropping." << endl;
      break;
  }

  if ( !isPktDecompressed ) {
      numDropped++;
      delete packet;
      return false;
  }

//  if (!m_promiscRxCallback.IsNull ())
//    {
//      m_promiscRxCallback (this, copyPkt, Ipv6L3Protocol::PROT_NUMBER, realSrc, realDst, packetType);
//    }
  packet->addTagIfAbsent<PacketProtocolTag>()->setProtocol(&Protocol::ipv6);
  packet->addTagIfAbsent<DispatchProtocolInd>()->setProtocol(&Protocol::ipv6);
  // this packet now can be handle by the normal ipv6 procedure
  return true;
}

B
Ipv6SixLowPan::compressLowPanHc1 (Packet * packet, L3Address const &src, L3Address const &dst)
{
    EV_TRACE << packet->str() << src << dst;
    B size;
    auto chunk = packet->peekAtFront<Chunk>();

    auto ipHeaderAux = dynamicPtrCast<const Ipv6Header>(chunk);

    EV_DEBUG << "Original packet: " << packet->str() << " Size " << packet->getByteLength() << endl;
    size = B(0);
    if (ipHeaderAux != nullptr) {
        auto hc1Header = makeShared<SixLowPanHc1>();
        auto ipHeader = removeNetworkProtocolHeader<Ipv6Header>(packet);
        size += ipHeader->getChunkLength();
        hc1Header->setHopLimit(ipHeader->getHopLimit());

      //uint8_t bufOne[16];
      //uint8_t bufTwo[16];
        Ipv6Address srcAddr = ipHeader->getSrcAddress();
        hc1Header->setOrigSrcAddr(srcAddr);

        Ipv6Address mySrcAddr = makeAutoconfiguredLinkLocalAddress(src).toIpv6();
        EV_DEBUG << "Checking source compression: " << mySrcAddr << " - " << srcAddr<< endl;

        uint8_t bufOne[16];
        uint8_t bufTwo[16];

        convertFromIpv6AddressToUint8(srcAddr, bufOne);
        convertFromIpv6AddressToUint8(mySrcAddr, bufTwo);

        L3Address aux;
        bool isSrcSrc =  (memcmp (bufOne + 8, bufTwo + 8, 8) == 0);
        if (srcAddr.isLinkLocal () && isSrcSrc )
          hc1Header->setSrcCompression (LowPanHc1Addr_e::HC1_PCIC);
        else if (srcAddr.isLinkLocal () )  {
            hc1Header->setSrcCompression (LowPanHc1Addr_e::HC1_PCII);
            //bufOne[0] = 0;
            //bufOne[1] = 0;
            hc1Header->setSrcInterface(L3Address(Ipv6Address(0, 0, srcAddr.words()[2], srcAddr.words()[3])));
        }
        else if (isSrcSrc)  {
            hc1Header->setSrcCompression (LowPanHc1Addr_e::HC1_PIIC);
            hc1Header->setSrcPrefix(L3Address(Ipv6Address(srcAddr.words()[0], srcAddr.words()[1], 0, 0)));
        }
        else {
            hc1Header->setSrcCompression (LowPanHc1Addr_e::HC1_PIII);
            hc1Header->setSrcInterface(L3Address(Ipv6Address(0, 0, srcAddr.words()[2], srcAddr.words()[3])));
            hc1Header->setSrcPrefix(L3Address(Ipv6Address(srcAddr.words()[0], srcAddr.words()[1], 0, 0)));
        }
        Ipv6Address dstAddr = ipHeader->getDestAddress();
        Ipv6Address myDstAddr = makeAutoconfiguredLinkLocalAddress(dst).toIpv6();
        EV_DEBUG << "Checking destination compression: " << myDstAddr << " - " << dstAddr<< endl;

        hc1Header->setOrigDestAddr(dstAddr);

        //memcpy(bufOne,dstAddr.words(),sizeof(bufOne));
        //memcpy(bufTwo,myDstAddr.words(),sizeof(bufTwo));
        convertFromIpv6AddressToUint8(dstAddr, bufOne);
        convertFromIpv6AddressToUint8(myDstAddr, bufTwo);
        //myDstAddr->getBytes (bufTwo);
        bool isDstDst = (memcmp (bufOne + 8, bufTwo + 8, 8) == 0);

        if (dstAddr.isLinkLocal () && isDstDst )
            hc1Header->setDstCompression (LowPanHc1Addr_e::HC1_PCIC);
        else if (dstAddr.isLinkLocal ())  {
            hc1Header->setDstCompression (LowPanHc1Addr_e::HC1_PCII);
            hc1Header->setDstInterface (L3Address(Ipv6Address(0, 0, dstAddr.words()[2], dstAddr.words()[3])));
        }
        else if ( isDstDst ) {
            hc1Header->setDstCompression (LowPanHc1Addr_e::HC1_PIIC);
            hc1Header->setDstPrefix (L3Address(Ipv6Address(dstAddr.words()[0], dstAddr.words()[1], 0, 0)));
        }
        else {
            hc1Header->setDstCompression (LowPanHc1Addr_e::HC1_PIII);
            hc1Header->setDstInterface (L3Address(Ipv6Address(0, 0, dstAddr.words()[2], dstAddr.words()[3])));
            hc1Header->setDstPrefix (L3Address(Ipv6Address(dstAddr.words()[0], dstAddr.words()[1], 0, 0)));
        }

        if ((ipHeader->getFlowLabel() == 0) && (ipHeader->getTrafficClass() == 0) )
            hc1Header->setTcflCompression (true);
        else {
            hc1Header->setTcflCompression (false);
            hc1Header->setTrafficClass (ipHeader->getTrafficClass());
            hc1Header->setFlowLabel (ipHeader->getFlowLabel());
        }

        //uint8_t nextHeader = ipHeader->getNextHeader();
        //hc1Header->setNextHeader (nextHeader);

       // hc1Header->setNextHeaderCompression();
        hc1Header->setProtocolId(ipHeader->getProtocolId());
        hc1Header->setProtocol(ipHeader->getProtocol());

        switch (ipHeader->getProtocolId()) {
            case IpProtocolId::IP_PROT_UDP:
                hc1Header->setNextHeaderCompression(HC1_UDP);
                break;
            case IpProtocolId::IP_PROT_TCP:
                hc1Header->setNextHeaderCompression(HC1_TCP);
                break;
            case IpProtocolId::IP_PROT_IPv6_ICMP:
                hc1Header->setNextHeaderCompression(HC1_ICMP);
                break;
            default:
                hc1Header->setNextHeaderCompression(HC1_NC);
                break;
        }

        // \todo implement HC2 compression
        hc1Header->setHc2HeaderPresent (false);
        // TODO: poner tamaño

        EV_DEBUG <<"HC1 Compression - HC1 header size = " << B(hc1Header->getChunkLength())<< endl;

        hc1Header->adjustHeaderSize();
        insertNetworkProtocolHeader(packet, Protocol::sixlowpan, hc1Header);

        EV_DEBUG <<"HC1 Compression - packet size = " << packet->getByteLength()<< endl;
        size = B(ipHeader->getChunkLength());
        return size;
    }
    return B(0);
}

void
Ipv6SixLowPan::decompressLowPanHc1 (Packet * packet, L3Address const &src, L3Address const &dst)
{
  EV_TRACE << packet->str() << src << dst<< endl;
  
  auto ipHeader = makeShared<Ipv6Header>();
  //SixLowPanHc1 encoding;

  auto encoding = removeNetworkProtocolHeader<SixLowPanHc1>(packet);

  ipHeader->setHopLimit (encoding->getHopLimit());

  Ipv6Address interfaceSrc = encoding->getSrcInterface().toIpv6();
  Ipv6Address prefixSrc = encoding->getSrcPrefix().toIpv6();
  const auto iSrc = interfaceSrc.words();
  const auto pSrc = prefixSrc.words();

  Ipv6Address interfaceDst = encoding->getDstInterface().toIpv6();
  Ipv6Address prefixDst = encoding->getDstPrefix().toIpv6();
  const auto iDst = interfaceDst.words();
  const auto pDst = prefixDst.words();

  uint32_t address[4];

  auto addrAux = makeAutoconfiguredLinkLocalAddress(src).toIpv6();
  uint32_t *w = addrAux.words();

  switch (encoding->getSrcCompression()) {
    case LowPanHc1Addr_e::HC1_PIII:
        address[0] = pSrc[0];
        address[1] = pSrc[1];
        address[2] = iSrc[2];
        address[3] = iSrc[3];
      ipHeader->setSrcAddress(Ipv6Address (address[0], address[1], address[2], address[3]));
      break;
    case LowPanHc1Addr_e::HC1_PIIC:
        address[0] = pSrc[0];
        address[1] = pSrc[1];
        address[2] = 0;
        address[3] = 0;
        address[2] = w[2];
        address[3] = w[3];
        ipHeader->setSrcAddress(Ipv6Address (address[0], address[1], address[2], address[3]));
      break;
    case LowPanHc1Addr_e::HC1_PCII:
//      interface = encoding->getSrcInterface();
      address[0] = 0xFE800000;
      address[1] = 0;
      address[2] = iSrc[2];
      address[3] = iSrc[3];
      ipHeader->setSrcAddress( Ipv6Address(address[0], address[1], address[2], address[3]) );
      break;
    case LowPanHc1Addr_e::HC1_PCIC:
      ipHeader->setSrcAddress(makeAutoconfiguredLinkLocalAddress(src).toIpv6());
      break;
    }

  addrAux = makeAutoconfiguredLinkLocalAddress(dst).toIpv6();
  w = addrAux.words();
  switch (encoding->getDstCompression ()) {
    case LowPanHc1Addr_e::HC1_PIII:
        address[0] = pDst[0];
        address[1] = pDst[1];
        address[2] = iDst[2];
        address[3] = iDst[3];
        ipHeader->setDestAddress(Ipv6Address (address[0], address[1], address[2], address[3]));
        break;
    case LowPanHc1Addr_e::HC1_PIIC:
        address[0] = pSrc[0];
        address[1] = pSrc[1];
        address[2] = 0;
        address[3] = 0;
        address[2] = w[2];
        address[3] = w[3];
        ipHeader->setDestAddress(Ipv6Address (address[0], address[1], address[2], address[3]));
        break;
    case LowPanHc1Addr_e::HC1_PCII:
        address[0] = 0xFE800000;
        address[1] = 0;
        address[2] = iDst[2];
        address[3] = iDst[3];
        ipHeader->setDestAddress(Ipv6Address (address[0], address[1], address[2], address[3]));
        break;
    case LowPanHc1Addr_e::HC1_PCIC:
        ipHeader->setDestAddress(makeAutoconfiguredLinkLocalAddress(dst).toIpv6());
        break;
    }

  if ( !encoding->getTcflCompression()) {
      ipHeader->setFlowLabel (encoding->getFlowLabel ());
      ipHeader->setTrafficClass (encoding->getTrafficClass ());
  }
  else
  {
      ipHeader->setFlowLabel (0);
      ipHeader->setTrafficClass (0);
  }

  //ipHeader->setNextHeader (encoding->getNextHeader ());
  ipHeader->setProtocolId(encoding->getProtocolId());
  ipHeader->setProtocol(encoding->getProtocol());
  ipHeader->setPayloadLength(B(packet->getByteLength()));


  if (encoding->getHc2HeaderPresent())
      throw cRuntimeError("6LoWPAN: error in decompressing HC1 encoding, unsupported L4 compressed header present.");

  insertNetworkProtocolHeader(packet, Protocol::ipv6, ipHeader);
  EV_DEBUG << "Rebuilt packet:  " << packet->str() << " Size " << packet->getByteLength()<< endl;
}

B
Ipv6SixLowPan::compressLowPanIphc (Packet * packet, L3Address const &src, L3Address const &dst)
{
  EV_DEBUG << packet->str() << src << dst<< endl;

  B size = B(0);

  auto chunk = packet->peekAtFront<Chunk>();
  auto ipHeaderAux = dynamicPtrCast<const Ipv6Header>(chunk);

  EV_DEBUG << "Original packet: " << packet->str() << " Size " << packet->getByteLength() << " src: " << src << " dst: " << dst << endl;
  size = B(0);
  if (ipHeaderAux != nullptr) {
      auto ipHeader = removeNetworkProtocolHeader<Ipv6Header>(packet);
      auto iphcHeader = makeShared<SixLowPanIphc>();
      size += B(40); // original Ipv6 header.

      iphcHeader->setProtocol(ipHeader->getProtocol());

      // Set the TF field
      if ((ipHeader->getFlowLabel() == 0) && (ipHeader->getTrafficClass() == 0)) {
          iphcHeader->setTf(SixLowPanIphc::TrafficClassFlowLabel_e::TF_ELIDED);
      }
      else if ((ipHeader->getFlowLabel() != 0) && (ipHeader->getTrafficClass() != 0)) {
          iphcHeader->setTf(SixLowPanIphc::TrafficClassFlowLabel_e::TF_FULL);
          iphcHeader->setEcn((ipHeader->getTrafficClass() & 0xC0) >> 6);
          iphcHeader->setDscp(ipHeader->getTrafficClass() & 0x3F);
          iphcHeader->setFlowLabel(ipHeader->getFlowLabel());
      }
      else if ((ipHeader->getFlowLabel() == 0) && (ipHeader->getTrafficClass() != 0)) {
          iphcHeader->setTf(SixLowPanIphc::TrafficClassFlowLabel_e::TF_FL_ELIDED);
          iphcHeader->setEcn((ipHeader->getTrafficClass() & 0xC0) >> 6);
          iphcHeader->setDscp(ipHeader->getTrafficClass() & 0x3F);
      }
      else {
          iphcHeader->setTf(SixLowPanIphc::TrafficClassFlowLabel_e::TF_DSCP_ELIDED);
          iphcHeader->setEcn((ipHeader->getTrafficClass() & 0xC0) >> 6);
          iphcHeader->setFlowLabel(ipHeader->getFlowLabel());
      }

      // Set the NH field and NextHeader
      auto protocolId = ipHeader->getProtocolId();

      // Compress header extensions
      iphcHeader->setNh(false);
      for (size_t i = 0; i < ipHeader->getExtensionHeaderArraySize(); i++) {
          auto extension = ipHeader->getExtensionHeader(i);
          if (canCompressLowPanNhc((IpProtocolId)extension->getExtensionType())) {
              iphcHeader->setNh (true);
              size += compressLowPanNhc (iphcHeader, extension);
          }
          else {
              iphcHeader->addExtensionHeader(extension->dup());
              //size += extension->getByteLength();
          }
      }

      iphcHeader->setProtocol(ipHeader->getProtocol());
      iphcHeader->setProtocolId(protocolId);
      //uint8_t nextHeader = ipHeader->getNextHeader ();
      if (canCompressLowPanNhc (protocolId)) {
          if (protocolId == IpProtocolId::IP_PROT_UDP) {
              iphcHeader->setNh (true);
              size += compressLowPanUdpNhc (packet, m_omitUdpChecksum);
          }
          else if (protocolId == IpProtocolId::IP_PROT_IPv6) {
              iphcHeader->setNh (true);
              size += compressLowPanIphc (packet, src, dst);
          }
      }

      // Set the HLIM field
      if (ipHeader->getHopLimit() == 1) {
          iphcHeader->setHlim(SixLowPanIphc::HLIM_COMPR_1);
      }
      else if (ipHeader->getHopLimit() == 0x40) {
          iphcHeader->setHlim(SixLowPanIphc::HLIM_COMPR_64);
      }
      else if (ipHeader->getHopLimit() == 0xFF) {
          iphcHeader->setHlim(SixLowPanIphc::HLIM_COMPR_255);
      }
      else {
          iphcHeader->setHlim(SixLowPanIphc::HLIM_INLINE);
          // Set the HopLimit
          iphcHeader->setHopLimit(ipHeader->getHopLimit ());
      }

      // Set the CID + SAC + DAC fields to their default value
      iphcHeader->setCid(false);
      iphcHeader->setSac(false);
      iphcHeader->setDac(false);


      Ipv6Address checker = Ipv6Address ("fe80:0000:0000:0000:0000:00ff:fe00:1");
      uint8_t unicastAddrCheckerBuf[16];
      convertFromIpv6AddressToUint8(checker, unicastAddrCheckerBuf);
      //memcpy(unicastAddrCheckerBuf, checker.words(), sizeof(unicastAddrCheckerBuf));
      //checker->getBytes (unicastAddrCheckerBuf);
      uint8_t addressBuf[16];

      // This is just to limit the scope of some variables.
      if (true)
      {
          Ipv6Address srcAddr = ipHeader->getSrcAddress();
          uint8_t srcContextId;
          iphcHeader->setOrigSrcAddr(srcAddr);

          // The "::" address is compressed as a fake stateful compression.
          // TODO: Check any address format
          if (srcAddr == Ipv6Address::UNSPECIFIED_ADDRESS)
          {
              // No context information is needed.
              iphcHeader->setSam (SixLowPanIphc::HC_INLINE);
              iphcHeader->setSac (true);
          }
          // Check if the address can be compressed with stateful compression
          else if ( findUnicastCompressionContext (srcAddr, srcContextId) )
          {
              // We can do stateful compression.

              iphcHeader->setSac(true);
              if (srcContextId != 0)
              {
                  // the default context is zero, no need to explicit it if it's zero
                  iphcHeader->setSrcContextId(srcContextId);
                  iphcHeader->setCid(true);
              }

              // Note that a context might include parts of the EUI-64 (i.e., be as long as 128 bits).

              if (makeAutoconfiguredAddress (src, m_contextTable[srcContextId].contextPrefix, m_contextTable[srcContextId].prefixLength) == srcAddr) {
                  iphcHeader->setSam (SixLowPanIphc::HC_COMPR_0);
              }
              else {
                  Ipv6Address cleanedAddr = cleanPrefix (srcAddr, m_contextTable[srcContextId].prefixLength);
                  uint8_t serializedCleanedAddress[16];
                  convertFromIpv6AddressToUint8(cleanedAddr, serializedCleanedAddress);
                  //memcpy(serializedCleanedAddress, cleanedAddr.words(), sizeof(serializedCleanedAddress));
                  // cleanedAddr->serialize (serializedCleanedAddress);
                  if ( serializedCleanedAddress[8] == 0x00 && serializedCleanedAddress[9] == 0x00 &&
                          serializedCleanedAddress[10] == 0x00 && serializedCleanedAddress[11] == 0xff &&
                          serializedCleanedAddress[12] == 0xfe && serializedCleanedAddress[13] == 0x00 ) {
                      iphcHeader->setSam (SixLowPanIphc::HC_COMPR_16);
                      iphcHeader->setSrcInlinePart (serializedCleanedAddress+14, 2);
                  }
                  else
                  {
                      iphcHeader->setSam (SixLowPanIphc::HC_COMPR_64);
                      iphcHeader->setSrcInlinePart (serializedCleanedAddress+8, 8);
                  }
              }
          }
          else
          {
              // We must do stateless compression.
              EV << "Checking stateless source compression: " << srcAddr << endl;

              //srcAddr->getBytes (addressBuf);
              convertFromIpv6AddressToUint8(srcAddr, addressBuf);
              //memcpy(addressBuf, srcAddr.words(), sizeof(addressBuf));

              uint8_t serializedSrcAddress[16];
              // srcAddr->serialize (serializedSrcAddress);

              convertFromIpv6AddressToUint8(srcAddr, serializedSrcAddress);
              //memcpy(serializedSrcAddress, srcAddr.words(), sizeof(serializedSrcAddress));

              if (srcAddr == makeAutoconfiguredLinkLocalAddress (src).toIpv6())  {
                  iphcHeader->setSam (SixLowPanIphc::HC_COMPR_0);
              }
              else if (memcmp (addressBuf, unicastAddrCheckerBuf, 14) == 0) {
                  iphcHeader->setSrcInlinePart (serializedSrcAddress+14, 2);
                  iphcHeader->setSam (SixLowPanIphc::HC_COMPR_16);
              }
              else if ( srcAddr.isLinkLocal()) {
                  iphcHeader->setSrcInlinePart (serializedSrcAddress+8, 8);
                  iphcHeader->setSam (SixLowPanIphc::HC_COMPR_64);
              }
              else {
                  iphcHeader->setSrcInlinePart (serializedSrcAddress, 16);
                  iphcHeader->setSam (SixLowPanIphc::HC_INLINE);
              }
          }
      }

      // Set the M field
      if (ipHeader->getDestAddress().isMulticast())  {
          iphcHeader->setM(true);
      }
      else {
          iphcHeader->setM(false);
      }

      // This is just to limit the scope of some variables.
      if (true)
      {
          Ipv6Address dstAddr = ipHeader->getDestAddress();
          convertFromIpv6AddressToUint8(dstAddr, addressBuf);
          //memcpy(addressBuf, dstAddr.words(), sizeof(addressBuf));
          //dstAddr->getBytes (addressBuf);

          iphcHeader->setOrigDestAddr(dstAddr);

          EV << "Checking destination compression: " << dstAddr << endl;

          uint8_t serializedDstAddress[16];
          convertFromIpv6AddressToUint8(dstAddr, serializedDstAddress);
          //memcpy(serializedDstAddress, dstAddr.words(), sizeof(serializedDstAddress));

          // dstAddr->serialize (serializedDstAddress);

          if (!iphcHeader->getM()) {
              // Unicast address
              uint8_t dstContextId;
              if (findUnicastCompressionContext(dstAddr, dstContextId)) {
                  // We can do stateful compression.
                  EV_DEBUG <<"Checking stateful destination compression: " << dstAddr << endl;

                  iphcHeader->setDac (true);
                  if (dstContextId != 0) {
                      // the default context is zero, no need to explicit it if it's zero
                      iphcHeader->setDstContextId (dstContextId);
                      iphcHeader->setCid (true);
                  }

                  // Note that a context might include parts of the EUI-64 (i.e., be as long as 128 bits).
                  if (makeAutoconfiguredAddress (dst, m_contextTable[dstContextId].contextPrefix,m_contextTable[dstContextId].prefixLength) == dstAddr) {
                      iphcHeader->setDam (SixLowPanIphc::HC_COMPR_0);
                  }
                  else {
                      Ipv6Address cleanedAddr = cleanPrefix(dstAddr, m_contextTable[dstContextId].prefixLength);

                      uint8_t serializedCleanedAddress[16];
                      convertFromIpv6AddressToUint8(cleanedAddr, serializedCleanedAddress);
                      // memcpy(serializedCleanedAddress, cleanedAddr.words(), sizeof(serializedCleanedAddress));
                      //cleanedAddr->serialize (serializedCleanedAddress);

                      if ( serializedCleanedAddress[8] == 0x00 && serializedCleanedAddress[9] == 0x00 &&
                              serializedCleanedAddress[10] == 0x00 && serializedCleanedAddress[11] == 0xff &&
                              serializedCleanedAddress[12] == 0xfe && serializedCleanedAddress[13] == 0x00 ) {
                          iphcHeader->setDam (SixLowPanIphc::HC_COMPR_16);
                          iphcHeader->setDstInlinePart (serializedCleanedAddress+14, 2);
                      }
                      else {
                          iphcHeader->setDam (SixLowPanIphc::HC_COMPR_64);
                          iphcHeader->setDstInlinePart (serializedCleanedAddress+8, 8);
                      }
                  }
              }
              else {
                  EV_DEBUG << "Checking stateless destination compression: " << dstAddr << endl;

                  if (dstAddr == makeAutoconfiguredLinkLocalAddress (dst).toIpv6()) {
                      iphcHeader->setDam (SixLowPanIphc::HC_COMPR_0);
                  }
                  else if (memcmp (addressBuf, unicastAddrCheckerBuf, 14) == 0) {
                      iphcHeader->setDstInlinePart (serializedDstAddress+14, 2);
                      iphcHeader->setDam (SixLowPanIphc::HC_COMPR_16);
                  }
                  else if (dstAddr.isLinkLocal()) {
                      iphcHeader->setDstInlinePart (serializedDstAddress+8, 8);
                      iphcHeader->setDam (SixLowPanIphc::HC_COMPR_64);
                  }
                  else {
                      iphcHeader->setDstInlinePart (serializedDstAddress, 16);
                      iphcHeader->setDam (SixLowPanIphc::HC_INLINE);
                  }
              }
          }
          else {
              // Multicast address

              uint8_t dstContextId;
              if ( findMulticastCompressionContext (dstAddr, dstContextId) )
              {
                  // Stateful compression (only one possible case)

                  // ffXX:XXLL:PPPP:PPPP:PPPP:PPPP:XXXX:XXXX
                  uint8_t dstInlinePart[6] = {};
                  dstInlinePart[0] = serializedDstAddress[1];
                  dstInlinePart[1] = serializedDstAddress[2];
                  dstInlinePart[2] = serializedDstAddress[12];
                  dstInlinePart[3] = serializedDstAddress[13];
                  dstInlinePart[4] = serializedDstAddress[14];
                  dstInlinePart[5] = serializedDstAddress[15];

                  iphcHeader->setDac (true);
                  if (dstContextId != 0) {
                      // the default context is zero, no need to explicit it if it's zero
                      iphcHeader->setDstContextId (dstContextId);
                      iphcHeader->setCid (true);
                  }
                  iphcHeader->setDstInlinePart (dstInlinePart, 6);
                  iphcHeader->setDam (SixLowPanIphc::HC_INLINE);
              }
              else {
                  // Stateless compression

                  uint8_t multicastAddrCheckerBuf[16];
                  Ipv6Address multicastCheckAddress = Ipv6Address ("ff02::1");
                  convertFromIpv6AddressToUint8(multicastCheckAddress, multicastAddrCheckerBuf);
                  //memcpy(words, multicastCheckAddress.words(),sizeof(multicastAddrCheckerBuf));
                  //multicastCheckAddress->getBytes (multicastAddrCheckerBuf);

                  // The address takes the form ff02::00XX.
                  if ( memcmp (addressBuf, multicastAddrCheckerBuf, 15) == 0 ) {
                      iphcHeader->setDstInlinePart (serializedDstAddress+15, 1);
                      iphcHeader->setDam (SixLowPanIphc::HC_COMPR_0);
                  }
                  // The address takes the form ffXX::00XX:XXXX.
                  //                            ffXX:0000:0000:0000:0000:0000:00XX:XXXX.
                  else if ( (addressBuf[0] == multicastAddrCheckerBuf[0])
                          && (memcmp (addressBuf + 2, multicastAddrCheckerBuf + 2, 11) == 0) ) {
                      uint8_t dstInlinePart[4] = {};
                      memcpy (dstInlinePart, serializedDstAddress+1, 1);
                      memcpy (dstInlinePart+1, serializedDstAddress+13, 3);
                      iphcHeader->setDstInlinePart (dstInlinePart, 4);
                      iphcHeader->setDam (SixLowPanIphc::HC_COMPR_16);
                  }
                  // The address takes the form ffXX::00XX:XXXX:XXXX.
                  //                            ffXX:0000:0000:0000:0000:00XX:XXXX:XXXX.
                  else if ( (addressBuf[0] == multicastAddrCheckerBuf[0])
                          && (memcmp (addressBuf + 2, multicastAddrCheckerBuf + 2, 9) == 0)) {
                      uint8_t dstInlinePart[6] = {};
                      memcpy (dstInlinePart, serializedDstAddress+1, 1);
                      memcpy (dstInlinePart+1, serializedDstAddress+11, 5);
                      iphcHeader->setDstInlinePart (dstInlinePart, 6);
                      iphcHeader->setDam (SixLowPanIphc::HC_COMPR_64);
                  }
                  else {
                      iphcHeader->setDstInlinePart (serializedDstAddress, 16);
                      iphcHeader->setDam (SixLowPanIphc::HC_INLINE);
                  }
              }
          }
      }

      iphcHeader->adjustHeaderSize();
      EV_DEBUG << "IPHC Compression - IPHC header size = " << iphcHeader->getSerializedSize() << endl;
      EV_DEBUG << "IPHC Compression - packet size = " << packet->getByteLength() << endl;

      insertNetworkProtocolHeader(packet, Protocol::sixlowpan, iphcHeader);
      //packet->insertAtFront(iphcHeader);
      EV_DEBUG << "Packet after IPHC compression: " << packet->getByteLength() << endl;;
      return size;
  }
  return B(0);
}

bool
Ipv6SixLowPan::canCompressLowPanNhc (IpProtocolId nextHeader)
{
  bool ret = false;

  switch (nextHeader)
    {
    case IpProtocolId::IP_PROT_UDP:
    case IpProtocolId::IP_PROT_IPv6EXT_HOP:
    case IpProtocolId::IP_PROT_IPv6EXT_ROUTING:
    case IpProtocolId::IP_PROT_IPv6EXT_FRAGMENT:
    case IpProtocolId::IP_PROT_IPv6:
    case IpProtocolId::IP_PROT_IPv6EXT_DEST:
      ret = true;
      break;
    case IpProtocolId::IP_PROT_IPv6EXT_MOB:
    default:
      ret = false;
    }
  return ret;
}

bool
Ipv6SixLowPan::decompressLowPanIphc (Packet * packet, L3Address const &src, L3Address const &dst)
{
    EV_DEBUG << packet->str() << src << dst << endl;

    auto ipHeader = makeShared<Ipv6Header>();
    auto encoding = removeNetworkProtocolHeader<SixLowPanIphc>(packet);

    //uint32_t ret = packet->RemoveHeader (encoding);
    //NS_LOG_DEBUG ("removed " << ret << " bytes - pkt is " << *packet);
    //NS_UNUSED (ret);

    // Hop Limit


    // Set Hop limit using the HLIM field
    if (encoding->getHlim() == SixLowPanIphc::HLIM_COMPR_1)
        ipHeader->setHopLimit(1);
    else if (encoding->getHlim() == SixLowPanIphc::HLIM_COMPR_64)
        ipHeader->setHopLimit(0x40);
    else if (encoding->getHlim() == SixLowPanIphc::HLIM_COMPR_255)
        ipHeader->setHopLimit(255);
    else if (encoding->getHlim() == SixLowPanIphc::HLIM_INLINE)
        ipHeader->setHopLimit(encoding->getHopLimit());
    else
        throw cRuntimeError("Incorrect Hlim code");


    ipHeader->setProtocolId(encoding->getProtocolId());
    ipHeader->setProtocol(encoding->getProtocol());

    // Source address
    if (encoding->getSac()) {
        // Source address compression uses stateful, context-based compression.
        if (encoding->getSam () == SixLowPanIphc::HC_INLINE ) {
            //ipHeader->setSource ( Ipv6Address::GetAny () );
            ipHeader->setSrcAddress(Ipv6Address());
        }
        else {
            uint8_t contextId = encoding->getSrcContextId();
            if (m_contextTable.find(contextId) == m_contextTable.end()) {
                EV << "Unknown Source compression context (" << contextId << "), dropping packet" << endl;
                return true;
            }
            if (m_contextTable[contextId].validLifetime < simTime()) {
                EV << "Expired Source compression context (" << contextId << "), dropping packet" << endl;
                return true;
            }

            uint8_t contexPrefix[16];
            convertFromIpv6AddressToUint8(m_contextTable[contextId].contextPrefix, contexPrefix);
            //memcpy(contexPrefix, m_contextTable[contextId].contextPrefix.words(), sizeof(contexPrefix));
            uint8_t contextLength = m_contextTable[contextId].prefixLength;

            uint8_t srcAddress[16] = { };
            if (encoding->getSam() == SixLowPanIphc::HC_COMPR_64) {
                memcpy (srcAddress +8, encoding->getSrcInlinePart(), 8);
            }
            else if (encoding->getSam() == SixLowPanIphc::HC_COMPR_16) {
                srcAddress[11] = 0xff;
                srcAddress[12] = 0xfe;
                memcpy (srcAddress +14, encoding->getSrcInlinePart(), 2);
            }
            else  {// SixLowPanIphc::HC_COMPR_0
                convertFromIpv6AddressToUint8(makeAutoconfiguredLinkLocalAddress (src).toIpv6(), srcAddress);
                //memcpy(srcAddress, makeAutoconfiguredLinkLocalAddress (src).toIpv6().words(), sizeof(srcAddress));
                //Ipv6Address::MakeAutoconfiguredLinkLocalAddress (src)->getBytes (srcAddress);
            }

            uint8_t bytesToCopy = contextLength / 8;
            uint8_t bitsToCopy = contextLength % 8;

            // Do not combine the prefix - we want to override the bytes.
            for (uint8_t i=0; i<bytesToCopy; i++) {
                srcAddress[i] = contexPrefix[i];
            }
            if (bitsToCopy) {
                uint8_t addressBitMask = (1<<(8-bitsToCopy))-1;
                uint8_t prefixBitMask = ~addressBitMask;
                srcAddress[bytesToCopy] = (contexPrefix[bytesToCopy] & prefixBitMask) | (srcAddress[bytesToCopy] & addressBitMask);
            }
            //uint32_t *w = reinterpret_cast<uint32_t *>(srcAddress);
            Ipv6Address addrAux;
            convertFromUint8ToIpv6Address(srcAddress, addrAux);
            ipHeader->setSrcAddress(addrAux);
            //ipHeader->setSource ( Ipv6Address::Deserialize (srcAddress) );
        }
    }
    else {
        // Source address compression uses stateless compression.
        if (encoding->getSam () == SixLowPanIphc::HC_INLINE) {
            uint8_t srcAddress[16] = { };
            memcpy (srcAddress, encoding->getSrcInlinePart (), 16);
            Ipv6Address addrAux;
            convertFromUint8ToIpv6Address(srcAddress, addrAux);
            //uint32_t *w = reinterpret_cast<uint32_t *>(srcAddress);
            //ipHeader->setSource ( Ipv6Address::Deserialize (srcAddress) );
            ipHeader->setSrcAddress(addrAux);
        }
        else if (encoding->getSam() == SixLowPanIphc::HC_COMPR_64) {
            uint8_t srcAddress[16] = { };
            memcpy (srcAddress +8, encoding->getSrcInlinePart(), 8);
            srcAddress[0] = 0xfe;
            srcAddress[1] = 0x80;
            Ipv6Address addrAux;
            convertFromUint8ToIpv6Address(srcAddress, addrAux);
            //uint32_t *w = reinterpret_cast<uint32_t *>(srcAddress);
            //ipHeader->setSource ( Ipv6Address::Deserialize (srcAddress) );
            ipHeader->setSrcAddress(addrAux);
        }
        else if (encoding->getSam() == SixLowPanIphc::HC_COMPR_16 ) {
            uint8_t srcAddress[16] = { };
            memcpy (srcAddress +14, encoding->getSrcInlinePart (), 2);
            srcAddress[0] = 0xfe;
            srcAddress[1] = 0x80;
            srcAddress[11] = 0xff;
            srcAddress[12] = 0xfe;
            Ipv6Address addrAux;
            convertFromUint8ToIpv6Address(srcAddress, addrAux);
            //uint32_t *w = reinterpret_cast<uint32_t *>(srcAddress);
            //ipHeader->setSource ( Ipv6Address::Deserialize (srcAddress) );
            ipHeader->setSrcAddress(addrAux);
        }
        else {// SixLowPanIphc::HC_COMPR_0
            //ipHeader->setSource (Ipv6Address::MakeAutoconfiguredLinkLocalAddress (src));
            ipHeader->setSourceAddress(makeAutoconfiguredLinkLocalAddress (src));
        }
    }
    // Destination address
    if (encoding->getDac()) {
        // Destination address compression uses stateful, context-based compression.
        if ((encoding->getDam () == SixLowPanIphc::HC_INLINE  && !encoding->getM ())
                || (encoding->getDam () == SixLowPanIphc::HC_COMPR_64  && encoding->getM ())
                || (encoding->getDam () == SixLowPanIphc::HC_COMPR_16  && encoding->getM ())
                || (encoding->getDam () == SixLowPanIphc::HC_COMPR_0  && encoding->getM ()) ) {
            throw cRuntimeError("Reserved code found");
        }

        uint8_t contextId = encoding->getDstContextId ();
        if (m_contextTable.find (contextId) == m_contextTable.end ())  {
            EV_DEBUG <<  "Unknown Destination compression context (" << contextId << "), dropping packet" << endl;
            return true;
        }
        if (m_contextTable[contextId].validLifetime < simTime()) {
            EV_DEBUG << "Expired Destination compression context (" << contextId << "), dropping packet" << endl;
            return true;
        }

        uint8_t contexPrefix[16];
        convertFromIpv6AddressToUint8(m_contextTable[contextId].contextPrefix, contexPrefix);
        //memcpy(contexPrefix, m_contextTable[contextId].contextPrefix.words(), sizeof(contexPrefix));
        uint8_t contextLength = m_contextTable[contextId].prefixLength;

        if (encoding->getM () == false) {
            // unicast
            uint8_t dstAddress[16] = { };
            if (encoding->getDam() == SixLowPanIphc::HC_COMPR_64) {
                memcpy (dstAddress +8, encoding->getDstInlinePart (), 8);
            }
            else if (encoding->getDam() == SixLowPanIphc::HC_COMPR_16) {
                dstAddress[11] = 0xff;
                dstAddress[12] = 0xfe;
                memcpy (dstAddress +14, encoding->getDstInlinePart (), 2);
            }
            else {// SixLowPanIphc::HC_COMPR_0
                convertFromIpv6AddressToUint8(makeAutoconfiguredLinkLocalAddress (dst).toIpv6(), dstAddress);
                //memcpy(dstAddress, makeAutoconfiguredLinkLocalAddress (dst).toIpv6().words(), sizeof(dstAddress));
                //Ipv6Address::MakeAutoconfiguredLinkLocalAddress (dst)->getBytes (dstAddress);
            }

            uint8_t bytesToCopy = m_contextTable[contextId].prefixLength / 8;
            uint8_t bitsToCopy = contextLength % 8;

            // Do not combine the prefix - we want to override the bytes.
            for (uint8_t i=0; i<bytesToCopy; i++) {
                dstAddress[i] = contexPrefix[i];
            }
            if (bitsToCopy) {
                uint8_t addressBitMask = (1<<(8-bitsToCopy))-1;
                uint8_t prefixBitMask = ~addressBitMask;
                dstAddress[bytesToCopy] = (contexPrefix[bytesToCopy] & prefixBitMask) | (dstAddress[bytesToCopy] & addressBitMask);
            }
            Ipv6Address addrAux;
            convertFromUint8ToIpv6Address(dstAddress, addrAux);
            //uint32_t *w = reinterpret_cast<uint32_t *>(dstAddress);
            ipHeader->setDestAddress(addrAux);
            //ipHeader->setDestination ( Ipv6Address::Deserialize (dstAddress) );
        }
        else {
            // multicast
            // Just one possibility: ffXX:XXLL:PPPP:PPPP:PPPP:PPPP:XXXX:XXXX
            uint8_t dstAddress[16] = { };
            dstAddress[0] = 0xff;
            memcpy (dstAddress +1, encoding->getDstInlinePart (), 2);
            dstAddress[3] = contextLength;
            memcpy (dstAddress +4, contexPrefix, 8);
            memcpy (dstAddress +12, encoding->getDstInlinePart ()+2, 4);
            Ipv6Address addrAux;
            convertFromUint8ToIpv6Address(dstAddress, addrAux);
            //uint32_t *w = reinterpret_cast<uint32_t *>(dstAddress);
            ipHeader->setDestAddress(addrAux);

            //ipHeader->setDestination ( Ipv6Address::Deserialize (dstAddress) );
        }
    }
    else
    {
        // Destination address compression uses stateless compression.
        if (encoding->getM() == false) {
            // unicast
            if (encoding->getDam() == SixLowPanIphc::HC_INLINE) {
                uint8_t dstAddress[16] = { };
                memcpy (dstAddress, encoding->getDstInlinePart (), 16);
                Ipv6Address addrAux;
                convertFromUint8ToIpv6Address(dstAddress, addrAux);
                //uint32_t *w = reinterpret_cast<uint32_t *>(dstAddress);
                ipHeader->setDestAddress(addrAux);
                //ipHeader->setDestination ( Ipv6Address::Deserialize (dstAddress) );
            }
            else if (encoding->getDam() == SixLowPanIphc::HC_COMPR_64) {
                uint8_t dstAddress[16] = { };
                memcpy (dstAddress +8, encoding->getDstInlinePart (), 8);
                dstAddress[0] = 0xfe;
                dstAddress[1] = 0x80;
                Ipv6Address addrAux;
                convertFromUint8ToIpv6Address(dstAddress, addrAux);
                //uint32_t *w = reinterpret_cast<uint32_t *>(dstAddress);
                ipHeader->setDestAddress(addrAux);
                //ipHeader->setDestination ( Ipv6Address::Deserialize (dstAddress) );
            }
            else if (encoding->getDam() == SixLowPanIphc::HC_COMPR_16) {
                uint8_t dstAddress[16] = { };
                memcpy (dstAddress +14, encoding->getDstInlinePart (), 2);
                dstAddress[0] = 0xfe;
                dstAddress[1] = 0x80;
                dstAddress[11] = 0xff;
                dstAddress[12] = 0xfe;
                Ipv6Address addrAux;
                convertFromUint8ToIpv6Address(dstAddress, addrAux);
                //uint32_t *w = reinterpret_cast<uint32_t *>(dstAddress);
                ipHeader->setDestAddress(addrAux);
                //ipHeader->setDestination ( Ipv6Address::Deserialize (dstAddress) );
            }
            else {// SixLowPanIphc::HC_COMPR_0
                //ipHeader->setDestination (Ipv6Address::MakeAutoconfiguredLinkLocalAddress (dst));
                ipHeader->setDestinationAddress(makeAutoconfiguredLinkLocalAddress (dst));
            }
        }
        else  {
            // multicast
            if (encoding->getDam () == SixLowPanIphc::HC_INLINE ) {
                uint8_t dstAddress[16] = { };
                memcpy (dstAddress, encoding->getDstInlinePart (), 16);
                Ipv6Address addrAux;
                convertFromUint8ToIpv6Address(dstAddress, addrAux);
                //uint32_t *w = reinterpret_cast<uint32_t *>(dstAddress);
                ipHeader->setDestAddress(addrAux);
                //ipHeader->setDestination ( Ipv6Address::Deserialize (dstAddress) );
            }
            else if (encoding->getDam () == SixLowPanIphc::HC_COMPR_64) {
                uint8_t dstAddress[16] = { };
                dstAddress[0] = 0xff;
                memcpy (dstAddress +1, encoding->getDstInlinePart (), 1);
                memcpy (dstAddress +11, encoding->getDstInlinePart ()+1, 5);
                //ipHeader->setDestination ( Ipv6Address::Deserialize (dstAddress) );
                Ipv6Address addrAux;
                convertFromUint8ToIpv6Address(dstAddress, addrAux);
                //uint32_t *w = reinterpret_cast<uint32_t *>(dstAddress);
                ipHeader->setDestAddress(addrAux);
            }
            else if (encoding->getDam () == SixLowPanIphc::HC_COMPR_16) {
                uint8_t dstAddress[16] = { };
                dstAddress[0] = 0xff;
                memcpy (dstAddress +1, encoding->getDstInlinePart (), 1);
                memcpy (dstAddress +13, encoding->getDstInlinePart ()+1, 3);
                Ipv6Address addrAux;
                convertFromUint8ToIpv6Address(dstAddress, addrAux);
                //uint32_t *w = reinterpret_cast<uint32_t *>(dstAddress);
                ipHeader->setDestAddress(addrAux);
                //ipHeader->setDestination ( Ipv6Address::Deserialize (dstAddress) );
            }
            else {// SixLowPanIphc::HC_COMPR_0
                uint8_t dstAddress[16] = { };
                dstAddress[0] = 0xff;
                dstAddress[1] = 0x02;
                memcpy (dstAddress+15, encoding->getDstInlinePart (), 1);
                Ipv6Address addrAux;
                convertFromUint8ToIpv6Address(dstAddress, addrAux);
                //uint32_t *w = reinterpret_cast<uint32_t *>(dstAddress);
                ipHeader->setDestAddress(addrAux);
                //ipHeader->setDestination ( Ipv6Address::Deserialize (dstAddress) );
            }
        }
    }

    // Traffic class and Flow Label
    uint8_t traf = 0x00;
    switch (encoding->getTf ())  {
    case SixLowPanIphc::TF_FULL:
        traf |= encoding->getEcn ();
        traf = ( traf << 6 ) | encoding->getDscp ();
        ipHeader->setTrafficClass (traf);
        ipHeader->setFlowLabel ( encoding->getFlowLabel () & 0xfff ); //Add 4-bit pad
        break;
    case SixLowPanIphc::TF_DSCP_ELIDED:
        traf |= encoding->getEcn ();
        traf <<= 2;   // Add 2-bit pad
        ipHeader->setTrafficClass (traf);
        ipHeader->setFlowLabel (encoding->getFlowLabel ());
        break;
    case SixLowPanIphc::TF_FL_ELIDED:
        traf |= encoding->getEcn ();
        traf = ( traf << 6 ) | encoding->getDscp ();
        ipHeader->setTrafficClass (traf);
        ipHeader->setFlowLabel (0);
        break;
    case SixLowPanIphc::TF_ELIDED:
        ipHeader->setFlowLabel (0);
        ipHeader->setTrafficClass (0);
        break;
    }
    if (encoding->getNh()) {
        // Compressed headers, first decompress EXT headers
        for (size_t i = 0; i < encoding->getExtensionHeaderArraySize(); i++) {
            if (decompressLowPanNhc(ipHeader, encoding->getExtensionHeaderForUpdate(i))==B(0)) {
                ipHeader->appendExtensionHeader(encoding->getExtensionHeaderForUpdate(i)->dup());
            }
        }

        // check next protocol
        auto prot = encoding->getProtocolId();
        if (prot == IpProtocolId::IP_PROT_IPv6) {
            auto chunk = packet->peekAtFront<Chunk>();
            if (dynamicPtrCast<const SixLowPanIphc>(chunk))
                decompressLowPanIphc (packet,src, dst);
        }
        else if (prot == IpProtocolId::IP_PROT_UDP) {
            auto chunk = packet->peekAtFront<Chunk>();
            if (dynamicPtrCast<const SixLowPanUdpNhcExtension>(chunk))
                decompressLowPanUdpNhc(packet, ipHeader->getSrcAddress(), ipHeader->getDestAddress());
        }

    }
    else {
        for (size_t i = 0; i < encoding->getExtensionHeaderArraySize(); i++) {
            ipHeader->appendExtensionHeader(encoding->getExtensionHeaderForUpdate(i)->dup());
        }
    }
    ipHeader->setChunkLength(B(ipHeader->calculateHeaderByteLength()));
    ipHeader->setPayloadLength (B(packet->getByteLength()));
    packet->insertAtFront(ipHeader);
    EV << "Rebuilt packet:  " << packet->str() << " Size " << packet->getByteLength () << endl;
    return false;
}

B
Ipv6SixLowPan::compressLowPanNhc (Ptr<SixLowPanIphc> &header, const Ipv6ExtensionHeader *extension)
{

  //SixLowPanNhcExtension nhcHeader;
  B size = B(0);

  if (extension->getByteLength() >= B(0xff)) {
      EV << "LOWPAN_NHC MUST NOT be used to encode IPv6 Extension Headers " << endl <<
                    "that have more than 255 octets following the Length field after compression. " << endl <<
                    "Packet uncompressed." << endl;
      return size;
  }

  uint8_t headerType = extension->getExtensionType();
  if ((headerType == IpProtocolId::IP_PROT_IPv6EXT_HOP ) ||
          (headerType == IpProtocolId::IP_PROT_IPv6EXT_ROUTING) ||
          (headerType == IpProtocolId::IP_PROT_IPv6EXT_ROUTING) ||
          (headerType == IpProtocolId::IP_PROT_IPv6EXT_DEST)) {
      auto extHeader = extension->dup();
      size = extension->getByteLength();
      if (extension->getByteLength() > B(2))
          extHeader->setByteLength(extension->getByteLength()-B(2));
      header->appendExtensionHeader(extHeader);
    }
  else {
      throw cRuntimeError("Unexpected Extension Header");
  }
  return size;
}

B
Ipv6SixLowPan::decompressLowPanNhc (Ptr<Ipv6Header> &header, const Ipv6ExtensionHeader *extension)
{
    B size = B(0);
    uint8_t headerType = extension->getExtensionType();
    if ((headerType == IpProtocolId::IP_PROT_IPv6EXT_HOP ) ||
            (headerType == IpProtocolId::IP_PROT_IPv6EXT_ROUTING) ||
            (headerType == IpProtocolId::IP_PROT_IPv6EXT_ROUTING) ||
            (headerType == IpProtocolId::IP_PROT_IPv6EXT_DEST)) {
        auto extHeader = extension->dup();
        size = extension->getByteLength();
        extHeader->setByteLength(extension->getByteLength()+B(2));
        header->appendExtensionHeader(extHeader);
    }
    return size;
}

B
Ipv6SixLowPan::compressLowPanUdpNhc (Packet * packet, bool omitChecksum)
{
  EV_TRACE << packet->str() << int(omitChecksum) << endl;

  //UdpHeader udpHeader;

  B size = B(0);

  auto udpHeader = packet->removeAtFront<UdpHeader>();

  auto udpNhcHeader = makeShared<SixLowPanUdpNhcExtension>();

  size += B(udpHeader->getChunkLength());

  // Set the C field and checksum
  udpNhcHeader->setC (false);
  udpNhcHeader->setChecksum(udpHeader->getChecksum());
  udpNhcHeader->setChecksumMode(udpHeader->getChecksumMode());

  if (Udp::isCorrectPacket(packet, udpHeader))
      udpNhcHeader->setC (true);

  // Set the value of the ports
  udpNhcHeader->setSrcPort(udpHeader->getSrcPort());
  udpNhcHeader->setDstPort(udpHeader->getDestPort());

  //Set the P field
  if ( (udpHeader->getSrcPort() >> 4 ) == 0xf0b && (udpHeader->getDestPort() >> 4 ) == 0xf0b )
      udpNhcHeader->setPorts (SixLowPanUdpNhcExtension::PORTS_LAST_SRC_LAST_DST);
  else if ( (udpHeader->getSrcPort() >> 8 ) == 0xf0 && (udpHeader->getDestPort() >> 8 ) != 0xf0 )
      udpNhcHeader->setPorts (SixLowPanUdpNhcExtension::PORTS_LAST_SRC_ALL_DST);
  else if ( (udpHeader->getSrcPort() >> 8 ) != 0xf0 && (udpHeader->getDestPort() >> 8 ) == 0xf0 )
      udpNhcHeader->setPorts (SixLowPanUdpNhcExtension::PORTS_ALL_SRC_LAST_DST);
  else
      udpNhcHeader->setPorts (SixLowPanUdpNhcExtension::PORTS_INLINE);

  packet->insertAtFront(udpNhcHeader);

  EV_DEBUG << "UDP_NHC Compression - UDP_NHC header size = " << udpNhcHeader->getSerializedSize () << endl;
  EV_DEBUG << "UDP_NHC Compression - packet size = " << packet->getByteLength() << endl;

  EV_DEBUG << "Packet after UDP_NHC compression: " << packet->str() << endl;

  return size;
}

void
Ipv6SixLowPan::decompressLowPanUdpNhc (Packet * packet, Ipv6Address saddr, Ipv6Address daddr)
{
  EV_DEBUG << packet ->str();

  auto udpHeader = makeShared<UdpHeader>();
  auto encoding = packet->removeAtFront<SixLowPanUdpNhcExtension>();
  udpHeader->setChecksum(encoding->getChecksum());
  udpHeader->setChecksumMode(encoding->getChecksumMode());


  // Set the value of the ports
  switch (encoding->getPorts()) {
    uint16_t temp;
    case SixLowPanUdpNhcExtension::PORTS_INLINE:
      udpHeader->setSourcePort (encoding->getSrcPort ());
      udpHeader->setDestinationPort (encoding->getDstPort ());
      break;
    case SixLowPanUdpNhcExtension::PORTS_ALL_SRC_LAST_DST:
      udpHeader->setSourcePort (encoding->getSrcPort ());
      temp = 0xf0;
      temp |= (temp << 8) | encoding->getDstPort ();
      udpHeader->setDestinationPort (temp);
      break;
    case SixLowPanUdpNhcExtension::PORTS_LAST_SRC_ALL_DST:
      temp = 0xf0;
      temp |= (temp << 8) | encoding->getSrcPort ();
      udpHeader->setSourcePort (temp);
      udpHeader->setDestinationPort (encoding->getDstPort ());
      break;
    case SixLowPanUdpNhcExtension::PORTS_LAST_SRC_LAST_DST:
      temp = 0xf0b;
      temp |= (temp << 4) | encoding->getSrcPort ();
      udpHeader->setSourcePort (temp);
      temp = 0xf0b;
      temp |= (temp << 4) | encoding->getDstPort ();
      udpHeader->setDestinationPort (temp);
      break;
    }
  B totalLength = udpHeader->getChunkLength() + packet->getDataLength();
  udpHeader->setTotalLengthField(totalLength);
  if (udpHeader->getChecksumMode() == CHECKSUM_COMPUTED) {
      udpHeader->setChecksum(0x0000); // crcMode == CRC_COMPUTED is done in an INetfilter hook
  }
  else {
      Udp::insertChecksum(&Protocol::ipv6, saddr, daddr, udpHeader, packet);
  }

  packet->insertAtFront(udpHeader);

  EV_DEBUG << "Rebuilt packet: " << packet->str() << " Size " << packet->getByteLength() << endl;
}



bool Ipv6SixLowPan::processAndSend(Packet *packet, const L3Address &src,
        const L3Address &dest, const bool & doSendFrom, const int &ifaceId )
{
    EV_INFO << packet << "src : " << src << "dest : " << dest <<  doSendFrom << endl;
    auto origPacket = packet->dup();
    B origHdrSize = B(0);
    B origPacketSize = B(packet->getByteLength());
    bool ret = false;
    L3Address destination = dest;
    bool useMesh = m_meshUnder;

    if (m_useIphc) {
        EV_TRACE << "Compressing packet using IPHC" << endl;
        origHdrSize += compressLowPanIphc(packet, L3Address(getAddress(ifaceId)),  destination);
    }
    else {
        EV_TRACE << "Compressing packet using HC1" << endl;
        origHdrSize += compressLowPanHc1(packet, L3Address(getAddress(ifaceId)), destination);
    }

    auto pktSize = B(packet->getByteLength());
    auto meshHdr = makeShared<SixLowPanMesh>();
    auto bc0Hdr = makeShared<SixLowPanBc0>();
    B extraHdrSize = B(0);
    bool sourceShort = false;
    bool destinationShort = false;
    if (useMesh) {
        L3Address source = src;
        if (!doSendFrom) {
            source = L3Address(getAddress(ifaceId));
        }
        if (source.getType() == L3Address::MAC) {
            // We got a Mac48 pseudo-MAC. We need its original Mac16 here.
            sourceShort = true;
            //source = Get16MacFrom48Mac(source);
        }
        if (destination.getType() == L3Address::MAC) {
            // We got a Mac48 pseudo-MAC. We need its original Mac16 here.
            destinationShort = true;
            //destination = Get16MacFrom48Mac(destination);
        }
        meshHdr->setOriginator(source);
        meshHdr->setV(sourceShort);
        meshHdr->setF(destinationShort);
        meshHdr->setDestination(destination);
        meshHdr->setHopsLeft(m_meshUnderHopsLeft);
        destination = getBroadcast(ifaceId);
        packet->addTagIfAbsent<MacAddressReq>()->setDestAddress(destination.toMac());

        // We are storing sum of mesh and bc0 header sizes. We will need it if packet is fragmented.
        extraHdrSize = meshHdr->getSerializedSize() + bc0Hdr->getSerializedSize();
        pktSize += extraHdrSize;
    }

    if (pktSize < m_compressionThreshold) {
        EV_TRACE << "Compressed packet too short, using uncompressed one" << endl;
        delete packet;
        packet = origPacket;
        origPacket = nullptr;
        auto ipv6UncompressedHdr = makeShared<SixLowPanIpv6>();
        ipv6UncompressedHdr->adjustHeaderSize();
        packet->insertAtFront(ipv6UncompressedHdr);
        pktSize = B(packet->getByteLength());
        if (useMesh) {
            pktSize += meshHdr->getSerializedSize() + bc0Hdr->getSerializedSize();
        }
    }
    else
        delete origPacket;

    if (pktSize > B(getMtu(ifaceId))) {
        EV_TRACE << "Fragmentation: Packet size " << packet->getByteLength() << " - Mtu " << getMtu(ifaceId) << endl;
        // fragment
        std::list<Packet * > fragmentList;
        doFragmentation(packet, origPacketSize, origHdrSize, extraHdrSize, fragmentList);
        // The packet has been fragmented, it can now be deleted
        delete packet;
        std::list<Packet * >::iterator it;
        for (it = fragmentList.begin(); it != fragmentList.end(); it++) {
            EV_DEBUG <<"SixLowPanNetDevice::Send (Fragment) " << **it << endl;
            //m_txTrace(*it, m_node->GetObject<SixLowPanNetDevice>(), GetIfIndex());

            if (useMesh) {
                bc0Hdr->setSeqNumber(m_bc0Serial++);
                (*it)->insertAtFront(bc0Hdr->dupShared());
                (*it)->insertAtFront(meshHdr->dupShared());
            }
            send(*it, "queueOut");
            /*if (doSendFrom) {
                success &= m_netDevice->SendFrom(*it, src, destination, protocolNumber);
            }
            else {
                success &= m_netDevice->Send(*it, destination, protocolNumber);
            }*/
        }
        ret = true;
    }
    else {
        ret = true;
        //m_txTrace(packet, m_node->GetObject<Ipv6SixLowPan>(),  GetIfIndex());
        if (useMesh) {
            bc0Hdr->setSeqNumber(m_bc0Serial++);
            packet->insertAtFront(bc0Hdr);
            packet->insertAtFront(meshHdr);
        }
        if (doSendFrom) {
            EV_DEBUG <<  "Ipv6SixLowPan::SendFrom " << " "  << packet->str();
            //ret = m_netDevice->SendFrom(packet, src, destination, protocolNumber);
        }
        else {
            EV_DEBUG <<  "Ipv6SixLowPan::Send " << " "  << packet->str();
            //ret = m_netDevice->Send(packet, destination, protocolNumber);
        }
        send(packet, "queueOut");
    }
    return ret;
}

L3Address Ipv6SixLowPan::fromIpv6ToMac(const L3Address & addr) {
    if (addr.getType() != L3Address::IPv6)
        throw cRuntimeError("AdL3Addresss not of type ipv6");
    Ipv6Address add = addr.toIpv6();
    auto w = add.words();
    unsigned char macAddr[6];
    macAddr[5] = w[3] & 0xFF;
    macAddr[4] = w[3] >> 8 & 0xFF;
    macAddr[3] = w[3] >> 16 & 0xFF;
    macAddr[2] = w[2] >> 8 & 0xFF;
    macAddr[1] = w[2] >> 16 & 0xFF;
    macAddr[0] = w[2] >> 24 & 0xFF;
    if (macAddr[0] & 0x2)
        macAddr[0] = macAddr[0] & 0xFD;
    else
        macAddr[0] = macAddr[0] | 0x2;
    MacAddress macAdd;
    macAdd.setAddressBytes(macAddr);
    return L3Address(macAdd);
}

void Ipv6SixLowPan::addContext (const uint8_t &contextId, const Ipv6Address &contextPrefix,const int &prefixLen, const bool &compressionAllowed, const simtime_t &validLifetime)
{
    if (contextId > 15)  {
        EV_DEBUG <<"Invalid context ID (" << +contextId << "), ignoring" << endl;
        return;
    }

    if (validLifetime == SimTime::ZERO) {
        EV_DEBUG << "Context (" << contextId << "), removed (validity time is zero)" << endl;
        m_contextTable.erase (contextId);
        return;
    }

    m_contextTable[contextId].contextPrefix = contextPrefix;
    m_contextTable[contextId].prefixLength = prefixLen;
    m_contextTable[contextId].compressionAllowed = compressionAllowed;
    m_contextTable[contextId].validLifetime = simTime() + validLifetime;
    return;
}

bool Ipv6SixLowPan::getContext (const uint8_t &contextId, Ipv6Address& contextPrefix, int &prefixLen, bool& compressionAllowed, simtime_t& validLifetime)
{
    if (contextId > 15)  {
        EV_DEBUG << "Invalid context ID (" << contextId << "), ignoring" << endl;
        return false;
    }
    if (m_contextTable.find (contextId) == m_contextTable.end ()) {
        EV_DEBUG << "Context not found (" << contextId << "), ignoring" << endl;
        return false;
    }
    contextPrefix = m_contextTable[contextId].contextPrefix;
    compressionAllowed = m_contextTable[contextId].compressionAllowed;
    prefixLen = m_contextTable[contextId].prefixLength;
    validLifetime = m_contextTable[contextId].validLifetime;
    return true;
}

void Ipv6SixLowPan::renewContext (const uint8_t &contextId, const simtime_t &validLifetime)
{

    if (contextId > 15) {
        EV_DEBUG <<"Invalid context ID (" << contextId << "), ignoring" << endl;
        return;
    }
    if (m_contextTable.find (contextId) == m_contextTable.end ()) {
        EV_DEBUG << "Context not found (" << contextId << "), ignoring" << endl;
        return;
    }
    m_contextTable[contextId].compressionAllowed = true;
    m_contextTable[contextId].validLifetime = simTime() + validLifetime;
    return;
}


void Ipv6SixLowPan::invalidateContext (const uint8_t &contextId)
{

    if (contextId > 15) {
        EV_DEBUG <<"Invalid context ID (" << contextId << "), ignoring" << endl;
        return;
    }
    if (m_contextTable.find (contextId) == m_contextTable.end ()) {
        EV_DEBUG << "Context not found (" << contextId << "), ignoring" << endl;
        return;
    }
    m_contextTable[contextId].compressionAllowed = false;
    return;
}

void Ipv6SixLowPan::removeContext (const uint8_t &contextId)
{


    if (contextId > 15) {
        EV_DEBUG <<"Invalid context ID (" << contextId << "), ignoring" << endl;
        return;
    }
    if (m_contextTable.find (contextId) == m_contextTable.end ()) {
        EV_DEBUG << "Context not found (" << contextId << "), ignoring" << endl;
        return;
    }
    m_contextTable.erase (contextId);
    return;
}

bool Ipv6SixLowPan::findUnicastCompressionContext (Ipv6Address address, uint8_t& contextId)
{
  for (const auto& iter: m_contextTable) {
      ContextEntry context = iter.second;
      if (context.compressionAllowed && (context.validLifetime > simTime())) {
          if (address.matches (context.contextPrefix, context.prefixLength)) {
              contextId = iter.first;
              return true;
          }
      }
  }
  return false;
}

bool Ipv6SixLowPan::findMulticastCompressionContext (Ipv6Address address, uint8_t& contextId)
{
  // The only allowed context-based compressed multicast address is in the form
  // ffXX:XXLL:PPPP:PPPP:PPPP:PPPP:XXXX:XXXX

  for (const auto& iter: m_contextTable) {
      ContextEntry context = iter.second;
      if (context.compressionAllowed  && (context.validLifetime > simTime())) {
          uint8_t contextLength = context.prefixLength;
          if (contextLength <= 64) {// only 64-bit prefixes or less are allowed.
              uint8_t contextBytes[16];
              uint8_t addressBytes[16];

              Ipv6Address p = context.contextPrefix.getPrefix(contextLength);

              convertFromIpv6AddressToUint8(p, contextBytes);
              convertFromIpv6AddressToUint8(address, addressBytes);

              //memcpy(contextBytes, p.words(), sizeof(contextBytes));
              //memcpy(addressBytes, address.words(), sizeof(addressBytes));

              if (addressBytes[3] == contextLength &&
                  addressBytes[4] == contextBytes[0] &&
                  addressBytes[5] == contextBytes[1] &&
                  addressBytes[6] == contextBytes[2] &&
                  addressBytes[7] == contextBytes[3] &&
                  addressBytes[8] == contextBytes[4] &&
                  addressBytes[9] == contextBytes[5] &&
                  addressBytes[10] == contextBytes[6] &&
                  addressBytes[11] == contextBytes[7])
                {
                  contextId = iter.first;
                  return true;
                }
            }
        }
    }
  return false;
}



// Fragmentation methods

Ipv6SixLowPan::Fragments::Fragments ()
{
  m_packetSize = 0;
}

Ipv6SixLowPan::Fragments::~Fragments ()
{
    EraseFragments();
}

void Ipv6SixLowPan::Fragments::AddFragment (Packet *fragment, uint16_t fragmentOffset)
{
    auto it = m_fragments.begin();
    bool duplicate = false;
    for (it = m_fragments.begin (); it != m_fragments.end (); it++) {
        if (it->second > fragmentOffset)
            break;
        if (it->second == fragmentOffset) {
            duplicate = true;
            if (fragment->getByteLength() != it->first->getByteLength())
                throw cRuntimeError("Duplicate fragment size differs. Aborting.");
            delete fragment;
            break;
        }
    }
    if (!duplicate) {
        m_fragments.insert (it, std::make_pair (fragment, fragmentOffset));
    }
}

void Ipv6SixLowPan::Fragments::AddFirstFragment (Packet *fragment)
{
    m_firstFragment = fragment;
}

bool Ipv6SixLowPan::Fragments::IsEntire () const
{
    B offset = B(m_firstFragment->getByteLength());
    ReassemblyBuffer curBuf;
    curBuf.setExpectedLength(B(m_packetSize));
    curBuf.replace(B(0), m_firstFragment->peekDataAt(B(0), B(m_firstFragment->getByteLength())));

    auto lastEndOffset = m_firstFragment->getByteLength ();

    for (auto it = m_fragments.begin (); it != m_fragments.end (); it++) {

        if (lastEndOffset > it->second ) {
            throw cRuntimeError("Overlapping fragments found, forbidden condition");
        }
        else  {
            curBuf.replace(offset, it->first->peekDataAt(B(0), B(it->first->getByteLength())));
        }
        offset += B(it->first->getByteLength());
        lastEndOffset += it->first->getByteLength();
    }
    return curBuf.isComplete();
}

Packet* Ipv6SixLowPan::Fragments::GetPacket () const
{
    if (m_firstFragment == nullptr)
        return nullptr;

    std::string pkName(m_firstFragment->getName());
    std::size_t found = pkName.find("-6lowpanfrag");

    if (found != std::string::npos)
        pkName.resize(found);

    Packet* p = new Packet(pkName.c_str());

    B offset = B(m_firstFragment->getByteLength());
    ReassemblyBuffer curBuf;
    curBuf.setExpectedLength(B(m_packetSize));
    curBuf.replace(B(0), m_firstFragment->peekDataAt(B(0), B(m_firstFragment->getByteLength())));

    auto lastEndOffset = m_firstFragment->getByteLength ();

    for (auto it = m_fragments.begin (); it != m_fragments.end (); it++) {

        if (lastEndOffset > it->second ) {
            throw cRuntimeError("Overlapping fragments found, forbidden condition");
        }
        else  {
            curBuf.replace(offset, it->first->peekDataAt(B(0), B(it->first->getByteLength())));
        }
        offset += B(it->first->getByteLength());
        lastEndOffset += it->first->getByteLength();
    }

    const auto& payload = curBuf.getReassembledData();
    auto header = dynamicPtrCast<SixLowPanDispatch>(m_firstFragment->peekAtFront<SixLowPanDispatch>()->dupShared());
    header->adjustHeaderSize();
    p->insertAtFront(header);
    p->insertAtBack(payload);
    return p;
}

void Ipv6SixLowPan::Fragments::SetPacketSize (uint32_t packetSize)
{
    m_packetSize = packetSize;
}

void Ipv6SixLowPan::Fragments::EraseFragments()
{
    while(!m_fragments.empty()) {
        delete m_fragments.front().first;
        m_fragments.pop_front();
    }
    m_packetSize = 0;
}

std::list<Packet* > Ipv6SixLowPan::Fragments::GetFraments () const
{
    std::list< Packet* > fragments;
    for (auto iter = m_fragments.begin (); iter != m_fragments.end (); ++iter) {
        fragments.push_back (iter->first);
    }
    return fragments;
    if (m_firstFragment)
        delete  m_firstFragment ;
}

void Ipv6SixLowPan::Fragments::SetTimeoutIter (FragmentsTimeoutsListI_t iter)
{
    m_timeoutIter = iter;
    return;
}

Ipv6SixLowPan::FragmentsTimeoutsListI_t
Ipv6SixLowPan::Fragments::GetTimeoutIter ()
{
    return m_timeoutIter;
}

Ipv6SixLowPan::FragmentsTimeoutsListI_t Ipv6SixLowPan::setTimeout (FragmentKey_t key, uint32_t iif)
{
    simtime_t next = simTime() + m_fragmentExpirationTimeout;
    if (m_timeoutEventList.empty ())
        scheduleAt(next, m_timeoutEvent);
    m_timeoutEventList.emplace_back (next, key, iif);
    Ipv6SixLowPan::FragmentsTimeoutsListI_t iter = --m_timeoutEventList.end();
    return (iter);
}

void Ipv6SixLowPan::handleFragmentsTimeout (FragmentKey_t key, uint32_t iif)
{
    auto it = m_fragments.find (key);
    if (it != m_fragments.end())
        it->second->EraseFragments();
    // clear the buffers
    delete it->second;
    m_fragments.erase (key);
}

void Ipv6SixLowPan::handleTimeoutList (void)
{
    simtime_t now = simTime();
    while (!m_timeoutEventList.empty () && std::get<0> (*m_timeoutEventList.begin ()) <= now) {
        handleFragmentsTimeout (std::get<1> (*m_timeoutEventList.begin ()), std::get<2> (*m_timeoutEventList.begin ()));
        m_timeoutEventList.pop_front ();
    }

    if (m_timeoutEventList.empty ())
        return;

    simtime_t next = std::get<0> (*m_timeoutEventList.begin ());
    scheduleAt(next, m_timeoutEvent);
    return;
}

void Ipv6SixLowPan::doFragmentation (Packet * packet, const B &origPacketSize,
        const B &origHdrSize,
        const B &extraHdrSize,
        std::list<Packet * >& listFragments)
{


    B offsetData = B(0);
    B offset = B(0);
    int ifaceId = packet->getTag<InterfaceReq>()->getInterfaceId();
    B l2Mtu = B(getMtu(ifaceId));
    B packetSize = B(packet->getByteLength());
    B compressedHeaderSize = packetSize - (origPacketSize - origHdrSize);

    uint16_t tag = (uint16_t) intuniform(0,65535);//(m_rng->GetValue (0, 65535));
    EV_DEBUG << "random tag " << tag << " - test " << packetSize;

    // first fragment
    auto frag1Hdr = makeShared<SixLowPanFrag1>();
    frag1Hdr->setDatagramTag(tag);

    B size;
    if (B(l2Mtu) <= frag1Hdr->getSerializedSize ())
        throw cRuntimeError("6LoWPAN: can not fragment, 6LoWPAN headers are bigger than MTU");


    // All the headers are substracted to get remaining units for data
    size = l2Mtu - frag1Hdr->getSerializedSize() - compressedHeaderSize - extraHdrSize;
    size -= B(size.get() % 8);
    size += compressedHeaderSize;

    //frag1Hdr->setDatagramSize (origPacketSize.get());
    frag1Hdr->setDatagramSize (packetSize.get());
/*
    // remove the network header
    auto dispatch = packet->peekAtFront<SixLowPanDispatch>();

    if (dispatch->getDispatch() == SixLowPanDispatchCode::LOWPAN_IPv6) {
        // a packet with error, an uncompressed header cannot arrive here,
        throw cRuntimeError("Fragmentation of a packet with uncompressed header, check CompressionThreshold it is bigger than the MTU");
    }

    Ptr<SixLowPanHc1> headerHc1;
    Ptr<SixLowPanIphc> headerIphc;
    B headerSize = B(0);
    auto dispatchCode = dispatch->getDispatch();
    switch (dispatchCode) {
    case SixLowPanDispatchCode::LOWPAN_HC1:
        headerHc1 = removeNetworkProtocolHeader<SixLowPanHc1>(packet);
        headerSize = headerHc1->getChunkLength();
        break;
    case SixLowPanDispatchCode::LOWPAN_IPHC:
        headerIphc = removeNetworkProtocolHeader<SixLowPanIphc>(packet);
        headerSize = headerIphc->getChunkLength();
        break;
    default:
        throw cRuntimeError("Fragmentation error, the header is not of type HCi or IPCH");
    }

    size -= headerSize;
*/
    const auto& fragData = packet->peekDataAt(offsetData, size);
    offsetData += size;
    offset += size + origHdrSize - compressedHeaderSize;
    offsetData += size;

    // create and send fragments
    std::string baseName = packet->getName();

    std::string fragMsgName = baseName+"-6lowpanfrag-0";
    auto fragment1 = new Packet(fragMsgName.c_str());
    fragment1->insertAtBack(fragData);
/*
    switch (dispatchCode) {
    case SixLowPanDispatchCode::LOWPAN_HC1:
        insertNetworkProtocolHeader(packet, Protocol::sixlowpan, headerHc1);
        break;
    case SixLowPanDispatchCode::LOWPAN_IPHC:
        insertNetworkProtocolHeader(packet, Protocol::sixlowpan, headerIphc);
        break;
    default:
        throw cRuntimeError("Fragmentation error, the header is not of type HCi or IPCH");
    }
*/
    frag1Hdr->adjustHeaderSize();
    fragment1->insertAtFront(frag1Hdr);
    fragment1->copyTags(*packet);
    listFragments.push_back (fragment1);

    int frag = 1;
    bool moreFrag = true;
    do
    {
        auto fragNHdr = makeShared<SixLowPanFragN>();
        fragNHdr->adjustHeaderSize();
        fragNHdr->setDatagramTag (tag);
        fragNHdr->setDatagramSize (packetSize.get());
        fragNHdr->setDatagramOffset ((offset.get()) >> 3);

        size = l2Mtu - fragNHdr->getSerializedSize() - extraHdrSize;
        size -= B(size.get() % 8);

        if ( (offsetData + size) > packetSize ) {
            size = packetSize - offsetData;
            moreFrag = false;
            fragMsgName = baseName+"-6lowpanfrag-last";
        }
        else
            fragMsgName = baseName+"-6lowpanfrag-"+std::to_string(frag);


        if (size > B(0))
        {
            auto fragment = new Packet(fragMsgName.c_str());
            EV_DEBUG << "Fragment creation - " << offset << ", " << offset << endl;
            const auto& fragData = packet->peekDataAt(offsetData, size);
            // Packet * fragment = p->CreateFragment (offsetData, size);
            fragment->insertAtBack(fragData);
            fragment->copyTags(*packet);

            EV_DEBUG << "Fragment created - " << offset << ", " << fragment->getBitLength() << endl;

            offset += size;
            offsetData += size;

            fragment->insertAtFront(fragNHdr);
            listFragments.push_back (fragment);
        }
    }
    while (moreFrag);

    return;
}


bool Ipv6SixLowPan::processFragment (Packet *packet, L3Address const &src, L3Address const &dst, bool isFirst)
{

  //Ptr<SixLowPanFrag1> frag1Header;
  //Ptr<SixLowPanFragN> fragNHeader;
  FragmentKey_t key;
  uint16_t packetSize;
  key.first = std::pair<L3Address, L3Address> (src, dst);

  auto iface = packet->getTag<InterfaceInd>()->getInterfaceId();
  uint16_t offset = 0;
  /* Implementation note:
   *
   * The fragment offset is relative to the *uncompressed* packet.
   * On the other hand, the packet can not be uncompressed correctly without all
   * its fragments, as the UDP checksum can not be computed otherwise.
   *
   * As a consequence we must uncompress the packet twice, and save its first
   * fragment for the final one.
   */
  if (isFirst) {
      //SixLowPanDispatch::Dispatch_e dispatchValFrag1;

      auto frag1Header = packet->removeAtFront<SixLowPanFrag1>();
     // auto dispatch = packet->peekAtFront<SixLowPanDispatch>();
      //uint8_t dispatchRawValFrag1 = dispatch->getDispatchType();
      packetSize = frag1Header->getDatagramSize();
      //p->CopyData (&dispatchRawValFrag1, sizeof(dispatchRawValFrag1));
      //dispatchValFrag1 = SixLowPanDispatch::GetDispatchType (dispatchRawValFrag1);
      //NS_LOG_DEBUG ( "Dispatches: " << int(dispatchRawValFrag1) << " - " << int(dispatchValFrag1) );
      //NS_LOG_DEBUG ( "Packet: " << *p );
      key.second = std::pair<uint16_t, uint16_t> (frag1Header->getDatagramSize(), frag1Header->getDatagramTag ());
  }
  else {
      auto fragNHeader = packet->removeAtFront<SixLowPanFragN>();
      packetSize = fragNHeader->getDatagramSize ();
      offset = fragNHeader->getDatagramOffset () << 3;
      key.second = std::pair<uint16_t, uint16_t> (fragNHeader->getDatagramSize (), fragNHeader->getDatagramTag ());
  }

  Fragments* fragments = nullptr;
  MapFragments_t::iterator it = m_fragments.find (key);
  if (it == m_fragments.end ())  {
      // erase the oldest packet.
      if (m_fragmentReassemblyListSize && (m_fragments.size () >= m_fragmentReassemblyListSize) ) {
          FragmentsTimeoutsListI_t iter = m_timeoutEventList.begin ();
          FragmentKey_t oldestKey = std::get<1> (*iter);
          numDropped++; //
          auto fragmentsAux = m_fragments[oldestKey];

          m_timeoutEventList.erase (fragmentsAux->GetTimeoutIter());
          delete fragmentsAux;
          m_fragments.erase (oldestKey);
      }
      fragments = new Fragments();
      fragments->SetPacketSize(packetSize);
      m_fragments.insert (std::make_pair (key, fragments));
      FragmentsTimeoutsListI_t iter = setTimeout (key, iface);
      fragments->SetTimeoutIter(iter);
  }
  else  {
      fragments = it->second;
  }

  // add the very first fragment so we can correctly decode the packet once is rebuilt.
  // this is needed because otherwise the UDP header length and checksum can not be calculated.
  if (isFirst)  {
      fragments->AddFirstFragment (packet);
  }
  else
      fragments->AddFragment (packet, offset);

  if (fragments->IsEntire ()) {
      packet = fragments->GetPacket ();
      m_timeoutEventList.erase (fragments->GetTimeoutIter ());
      m_fragments.erase (key);
      delete fragments;
      fragments = nullptr;
      return true;
    }
  return false;
}

}
}
}
#endif
