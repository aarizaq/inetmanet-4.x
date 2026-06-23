/*
 * SixLowPanNetDevice.h
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


#ifndef IPV6SIXLOWPAN_H_
#define IPV6SIXLOWPAN_H_

#include <map>
#include <set>
#include <list>
#include "inet/common/INETDefs.h"
#include "inet/common/Units.h"
#include "inet/wirelesspan/networklayer/sixlowpan/SixLowPanHeader_m.h"
#include "inet/networklayer/common/IpProtocolId_m.h"
#include "inet/common/lifecycle/ModuleOperations.h"
#include "inet/common/lifecycle/OperationalBase.h"
#include "inet/common/lifecycle/ILifecycle.h"
#include "inet/common/lifecycle/NodeStatus.h"
#include "inet/common/lifecycle/LifecycleOperation.h"
#include "inet/networklayer/common/NetworkInterface.h"
#include "inet/networklayer/ipv6/Ipv6.h"

#ifndef INET_WITH_IPv6
#error "This module needs inet/inetmanet ipv6"
#endif

#ifdef INET_WITH_IPv6
namespace inet {
namespace wirelesspan {
namespace sixlowpan {

using namespace units::values;

class INET_API Ipv6SixLowPan : public Ipv6 {
public:
    Ipv6SixLowPan() {}
    virtual ~Ipv6SixLowPan();
    struct ContextEntry {
        Ipv6Address contextPrefix;    //!< context prefix to be used in compression/decompression
        int prefixLength = 0;
        bool compressionAllowed;     //!< compression and decompression allowed (true), decompression only (false)
        simtime_t validLifetime;          //!< validity period
        std::string str() const;
    };
private:

     std::map<uint8_t, ContextEntry> m_contextTable; //!< Table of the contexts used in compression/decompression

     /**
      * \brief Finds if the given unicast address matches a context for compression
      *
      * \param[in] address the address to check
      * \param[out] contextId the context found
      * \return true if a valid context has been found
      */
     bool findUnicastCompressionContext (Ipv6Address address, uint8_t& contextId);

     /**
      * \brief Finds if the given multicast address matches a context for compression
      *
      * \param[in] address the address to check
      * \param[out] contextId the context found
      * \return true if a valid context has been found
      */
     bool findMulticastCompressionContext (Ipv6Address address, uint8_t& contextId);
public:
      void addContext (const uint8_t &contextId, const Ipv6Address &contextPrefix, const int &prefixLength, const bool &compressionAllowed, const simtime_t &validLifetime);
      bool getContext (const uint8_t &contextId, Ipv6Address& contextPrefix, int &prefixLength, bool& compressionAllowed, simtime_t& validLifetime);
      void renewContext (const uint8_t &contextId, const simtime_t &validLifetime);
      void invalidateContext (const uint8_t &contextId);
      void removeContext (const uint8_t &contextId);

protected:

      std::set<int> listSixLowPanInterfaces;
      std::vector<std::string> listSixLowPanInterfacesNames;
    // Methods inherited from ancestors
    //virtual bool isInitializeStage(int stage) const override { return stage == INITSTAGE_NETWORK_LAYER_PROTOCOLS; }
    //virtual bool isModuleStartStage(int stage) const override { return stage == ModuleStartOperation::STAGE_ROUTING_PROTOCOLS; }
    //virtual bool isModuleStopStage(int stage) const override { return stage == ModuleStopOperation::STAGE_ROUTING_PROTOCOLS; }

    //virtual bool handleOperationStage(LifecycleOperation *operation, IDoneCallback *doneCallback) override {return false;}

      virtual bool checkSixLowPanInterfaceById(const int &id) const;
      virtual bool checkSixLowPanInterface(const NetworkInterface *ie) const;


      // Overloaded methods from Ipv6
      //virtual int numInitStages() const override { return NUM_INIT_STAGES; }
      virtual void initialize(int) override;
      virtual void handleMessage(cMessage *msg) override;
      virtual void fragmentAndSend(Packet *packet, const NetworkInterface *ie, const MacAddress& nextHopAddr, bool fromHL) override;
      virtual void sendDatagramToOutput(Packet *packet, const NetworkInterface *destIE, const MacAddress& macAddr) override;
      // Handle incoming sixLowPan packets
      virtual bool handleMessageFromNetwork(Packet *packet);

protected:
    // Compress/decompress methods
    B compressLowPanHc1 (Packet* packet, const L3Address &src, const L3Address &dst);

     /**
      * \brief Decompress the headers according to HC1 compression.
      * \param [in] packet the packet to be compressed.
      * \param [in] src the MAC source address.
      * \param [in] dst the MAC destination address.
      */
     void decompressLowPanHc1 (Packet *packet, const L3Address &src, const L3Address &dst);

     /**
      * \brief Compress the headers according to IPHC compression.
      * \param [in] packet The packet to be compressed.
      * \param [in] src The MAC source address.
      * \param [in] dst The MAC destination address.
      * \return The size of the removed headers.
      */
     B compressLowPanIphc (Packet* packet, const L3Address &src, const L3Address &dst);

     /**
      * \brief Checks if the next header can be compressed using NHC.
      * \param [in] headerType The header kind to be compressed.
      * \return True if the header can be compressed.
      */
     bool canCompressLowPanNhc (IpProtocolId headerType);

     /**
      * \brief Decompress the headers according to IPHC compression.
      * \param [in] packet The packet to be compressed.
      * \param [in] src The MAC source address.
      * \param [in] dst The MAC destination address.
      * \return true if the packet can not be decompressed due to wrong context informations.
      */
     bool decompressLowPanIphc (Packet* packet, const L3Address &src, const L3Address &dst);

     /**
      * \brief Compress the headers according to NHC compression.
      * \param [in] packet The packet to be compressed.
      * \param [in] omitChecksum Omit UDP checksum (if true).
      * \return The size of the removed headers.
      */
     B compressLowPanUdpNhc (Packet* packet, bool omitChecksum);

     /**
      * \brief Decompress the headers according to NHC compression.
      * \param [in] packet The packet to be compressed.
      * \param [in] saddr The IPv6 source address.
      * \param [in] daddr The IPv6 destination address.
      */
     void decompressLowPanUdpNhc (Packet* packet, Ipv6Address saddr, Ipv6Address daddr);
protected:


    // convenience methods to access the interface information
    virtual MacAddress getAddress(const int &id) const {return ift->getInterfaceById(id)->getMacAddress();}
    virtual uint16_t getMtu (const int &id) const {return (uint16_t)ift->getInterfaceById(id)->getMtu();}
    virtual bool isLinkUp (const int &id) const {return (ift->getInterfaceById(id)->getState() == NetworkInterface::State::UP);}
    virtual bool isBroadcast (const int &id) const {return ift->getInterfaceById(id)->isBroadcast();}
    virtual MacAddress getBroadcast (const int &id) const {return ift->getInterfaceById(id)->getMacAddress().BROADCAST_ADDRESS;}
    virtual bool isMulticast (const int &id) const {return ift->getInterfaceById(id)->isMulticast();}
    virtual bool isPointToPoint (const int &id) const {return ift->getInterfaceById(id)->isPointToPoint();}
    //virtual bool Send (Packet* packet, const L3Address& dest, uint16_t protocolNumber);
    //virtual bool SendFrom (Packet* packet, const L3Address& source, const L3Address& dest, uint16_t protocolNumber);
    //virtual bool SupportsSendFrom () const;


    // process method
    virtual bool processAndSend(Packet *packet, const L3Address &src, const L3Address &dest, const bool &doSendFrom, const int &ifaceId);


public:
    // methods to transform the address

    // Transform internal ipv6 address representation to an array of uint8_t changing the byte order
    static void convertFromIpv6AddressToUint8(const Ipv6Address &addr, uint8_t v[16]);
    // Transform from an array of uint8_t to internal ipv6 address representation changing the byte order
    static void convertFromUint8ToIpv6Address(const uint8_t v[16], Ipv6Address &addr);


    // Transform Mac address to Ipv6 address with link local prefix
    static L3Address makeAutoconfiguredLinkLocalAddress(L3Address addr);

    // Transform Mac address to Ipv6 address using the prefix of the field prefix with length prefixLeng
    static L3Address makeAutoconfiguredAddress (L3Address addr, Ipv6Address prefix, int prefixLeng);

    // Extract mac address from a link local address ipv6 format
    static L3Address fromIpv6ToMac(const L3Address & addr);

    //Set to '0' the bits until prefixLength
    static Ipv6Address cleanPrefix (Ipv6Address address, int prefixLength);

protected:
    // Fragmentation structures and methods

    typedef std::pair< std::pair<L3Address, L3Address>, std::pair<uint16_t, uint16_t> > FragmentKey_t;

    /// Container for fragment timeouts.
    typedef std::list< std::tuple <simtime_t, FragmentKey_t, uint32_t > > FragmentsTimeoutsList_t;
    /// Container Iterator for fragment timeouts.
    typedef FragmentsTimeoutsList_t::iterator FragmentsTimeoutsListI_t;

    FragmentsTimeoutsList_t m_timeoutEventList;  //!< Timeout "events" container
    cMessage *m_timeoutEvent = nullptr;

    void handleTimeout (void);

    class Fragments : public omnetpp::cOwnedObject
     {
   public:
       /**
        * \brief Constructor.
        */
       Fragments ();

       /**
        * \brief Destructor.
        */
       ~Fragments ();

       /**
        * \brief Add a fragment to the pool.
        * \param [in] fragment the fragment.
        * \param [in] fragmentOffset the offset of the fragment.
        */
       void AddFragment (Packet* fragment, uint16_t fragmentOffset);

       /**
        * \brief Add the first packet fragment. The first fragment is needed to
        * allow the post-defragmentation decompression.
        * \param [in] fragment The fragment.
        */
       void AddFirstFragment (Packet* fragment);

       /**
        * \brief If all fragments have been added.
        * \returns True if the packet is entire.
        */
       bool IsEntire () const;

       /**
        * \brief Get the entire packet.
        * \return The entire packet.
        */
       Packet* GetPacket () const;

       /**
        * \brief Set the packet-to-be-defragmented size.
        * \param [in] packetSize The packet size (bytes).
        */
       void SetPacketSize (uint32_t packetSize);

       /**
        * \brief Get a list of the current stored fragments.
        * \returns The current stored fragments.
        */
       std::list<Packet* > GetFraments () const;

       /*
        * Erase the internal list of fragments
        */
       void EraseFragments();

       /**
        * \brief Set the Timeout iterator.
        * \param iter The iterator.
        */
       void SetTimeoutIter (FragmentsTimeoutsListI_t iter);

       /**
        * \brief Get the Timeout iterator.
        * \returns The iterator.
        */
       FragmentsTimeoutsListI_t GetTimeoutIter ();

   private:


       /**
        * \brief The size of the reconstructed packet (bytes).
        */
       uint32_t m_packetSize;

       /**
        * \brief The current fragments.
        */
       std::list<std::pair<Packet*, uint16_t> > m_fragments;

       /**
        * \brief The very first fragment.
        */
       Packet* m_firstFragment = nullptr;

       /**
        * \brief Timeout iterator to "event" handler
        */
       FragmentsTimeoutsListI_t m_timeoutIter;
     };
       /**
       * Container for fragment key -> fragments.
       */
      typedef std::map< FragmentKey_t, Fragments* > MapFragments_t;
      /**
       * Container Iterator for fragment key -> fragments.
       */
      typedef std::map< FragmentKey_t, Fragments* >::iterator MapFragmentsI_t;
      std::map <L3Address /* OriginatorAdddress */, std::list <uint8_t  /* SequenceNumber */> > m_seenPkts; //!< Seen packets, memorized by OriginatorAdddress, SequenceNumber.

      MapFragments_t    m_fragments; //!< Fragments hold to be rebuilt.
      simtime_t         m_fragmentExpirationTimeout; //!< Time limit for fragment rebuilding.

   /**
    * \brief Performs a packet fragmentation.
    * \param [in] packet the packet to be fragmented (with headers already compressed with 6LoWPAN).
    * \param [in] origPacketSize the size of the IP packet before the 6LoWPAN header compression, including the IP/L4 headers.
    * \param [in] origHdrSize the size of the IP header before the 6LoWPAN header compression.
    * \param [in] extraHdrSize the sum of the sizes of BC0 header and MESH header if mesh routing is used or 0.
    * \param [out] listFragments A reference to the list of the resulting packets, all with the proper headers in place.
    */
   void doFragmentation (Packet* packet, const B &origPacketSize, const B &origHdrSize, const B &extraHdrSize,
                         std::list<Packet* >& listFragments);

   /**
    * \brief Process a packet fragment.
    * \param [in] packet The packet.
    * \param [in] src The source MAC address.
    * \param [in] dst The destination MAC address.
    * \param [in] isFirst True if it is the first fragment, false otherwise.
    * \return True is the fragment completed the packet.
    */
   bool processFragment (Packet* packet, const L3Address &src, const L3Address &dst, bool isFirst);

   /**
    * \brief Process the timeout for packet fragments.
    * \param [in] key A key representing the packet fragments.
    * \param [in] iif Input Interface.
    */
   void handleFragmentsTimeout (FragmentKey_t key, uint32_t iif);


   FragmentsTimeoutsListI_t setTimeout (FragmentKey_t key, uint32_t iif);

   void handleTimeoutList(void);

   /**
    * \brief Drops the oldest fragment set.
    */
   void dropOldestFragmentSet();


   /**
    * Get a Mac16 from its Mac48 pseudo-MAC
    * \param addr the PseudoMac address
    * \return the Mac16Address
    */
   MacAddress Get16MacFrom48Mac (MacAddress addr);

   std::list<Packet* > getFraments () const;

   bool m_meshUnder = false;               //!< Use a mesh-under routing.
   uint8_t m_bc0Serial = 0;            //!< Serial number used in BC0 header.
   uint8_t m_meshUnderHopsLeft = 1;    //!< Start value for mesh-under hops left.
   uint16_t m_meshCacheLength = 1;     //!< length of the cache for each source.
   uint64_t m_fragmentReassemblyListSize = 0;
   bool m_useIphc;
   bool m_omitUdpChecksum;
   B m_compressionThreshold;
   cPar *m_meshUnderJitter;
   bool aceptAllInterfaces; // accept all sixlowpan messages even if the interface is not sixlowpan
   B ipv6FragemtationMtu;
};

}
}
}
#endif
#endif /* IPV6SIXLOWPAN_H_ */
