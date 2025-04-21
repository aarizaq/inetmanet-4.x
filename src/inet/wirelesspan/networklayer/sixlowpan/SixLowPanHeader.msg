//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
// 
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/.
//
import inet.common.INETDefs;
import inet.common.packet.chunk.Chunk;
import inet.networklayer.contract.NetworkHeaderBase;
import inet.networklayer.ipv6.Ipv6Header;
import inet.networklayer.contract.ipv6.Ipv6Address;
import inet.networklayer.common.L3Address;
import inet.common.checksum.ChecksumMode;

cplusplus {{
#include "inet/common/INETUtils.h"
#include "SixLowPanDispatchCode.h"
}}


namespace inet::wirelesspan::sixlowpan;


class SixLowPanDispatch extends NetworkHeaderBase
{
   uint8_t dispatch = SixLowPanDispatchCode::LOWPAN_UNSUPPORTED;
   chunkLength = B(1);
   IpProtocolId protocolId = IP_PROT_NONE;
   //virtual B getSerializedSize() const;
   //virtual void adjustHeaderSize();
}

cplusplus(SixLowPanDispatch) {{
  public:
    virtual uint8_t getDispatchType() const {return SixLowPanDispatchCode::GetDispatchType(getDispatch());}
    virtual uint8_t getNhcDispatchType() const {return SixLowPanDispatchCode::GetNhcDispatchType(getDispatch());}
      
    virtual L3Address getSourceAddress() const override {throw cRuntimeError("Method not implemented"); return L3Address();}
    virtual void setSourceAddress(const L3Address& address)  override {throw cRuntimeError("Method not implemented");}
    virtual L3Address getDestinationAddress() const override {throw cRuntimeError("Method not implemented"); return L3Address();}
    virtual void setDestinationAddress(const L3Address& address)  override {throw cRuntimeError("Method not implemented");}
    virtual bool isFragment() const override {throw cRuntimeError("Method not implemented"); return false; }
    virtual const Protocol *getProtocol() const override { return ProtocolGroup::getIpProtocolGroup()->findProtocol(getProtocolId()); }
    virtual void setProtocol(const Protocol *protocol) override { setProtocolId(static_cast<IpProtocolId>(ProtocolGroup::getIpProtocolGroup()->getProtocolNumber(protocol))); }
    
    virtual B getSerializedSize() const {return B(1);}
    virtual void adjustHeaderSize() {this->setChunkLength(this->getSerializedSize());}
}}

enum LowPanHc1Addr_e
{
    HC1_PIII = 0x00;
    HC1_PIIC = 0x01;
    HC1_PCII = 0x02;
    HC1_PCIC = 0x03;
}
  
enum LowPanHc1NextHeader_e
{
    HC1_NC = 0x00;
    HC1_UDP = 0x01;
    HC1_ICMP = 0x02;
    HC1_TCP = 0x03;
}
    

class SixLowPanHc1 extends SixLowPanDispatch {
  chunkLength = B(-1);
  dispatch = SixLowPanDispatchCode::LOWPAN_HC1;
  uint8_t hopLimit = 0;         //!< Hop Limit.
  L3Address srcPrefix;       //!< Source prefix.
  L3Address srcInterface;    //!< Source interface.
  L3Address dstPrefix;       //!< Destination prefix.
  L3Address dstInterface;    //!< Destination interface.
  uint8_t trafficClass;       //!< Traffic Class.
  uint32_t flowLabel;       //!< Flow Label.
  //uint8_t nextHeader;       //!< Next header.
  LowPanHc1Addr_e srcCompression; //!< Source compression type.
  LowPanHc1Addr_e dstCompression; //!< Destination compression type.
  bool tcflCompression;       //!< Is TC and FL compressed.
  LowPanHc1NextHeader_e nextHeaderCompression; //!< Next header compression.
  bool hc2HeaderPresent;      //!< Is next header HC2 compressed.
}

cplusplus(SixLowPanHc1) {{
  public:
    virtual B getSerializedSize() const override;
}}

class SixLowPanFrag1 extends SixLowPanDispatch {
  chunkLength = B(4);
  dispatch = SixLowPanDispatchCode::LOWPAN_FRAG1;
  uint16_t datagramSize = 0; //!< Datagram size.
  uint16_t datagramTag = 0;  //!< Datagram tag.
}

cplusplus(SixLowPanFrag1) {{
  virtual B getSerializedSize() const override;
}}



class SixLowPanFragN extends SixLowPanDispatch {
  chunkLength = B(4);
  dispatch = SixLowPanDispatchCode::LOWPAN_FRAGN;
  uint16_t datagramSize = 0; //!< Datagram size.
  uint16_t datagramTag = 0;  //!< Datagram tag.
  uint8_t datagramOffset = 0; //!< Datagram offset.
}

cplusplus(SixLowPanFragN) {{
public:    
  virtual B getSerializedSize() const override;
}}

class SixLowPanIpv6 extends SixLowPanDispatch
{
     dispatch = SixLowPanDispatchCode::LOWPAN_IPv6;
}


cplusplus {{
class SixlowpanExtensionHeader;
std::ostream& operator<<(std::ostream& os, Ipv6ExtensionHeader eh);
}}

class SixLowPanIphc extends SixLowPanDispatch {
  dispatch = SixLowPanDispatchCode::LOWPAN_IPHC;
  uint16_t baseFormat = 0x6000;       //!< Dispatch + encoding fields.
  uint8_t srcdstContextId = 0;   //!< Src and Dst Context ID.
  uint8_t ecn;           //!< ECN bits.
  uint8_t dscp;          //!< DSCP bits.
  uint32_t flowLabel;   //!< Flow Label bits.
  uint8_t hopLimit;          //!< Hop Limit.
  // uint8_t nextHeader;       //!< Next header.
  uint8_t srcInlinePart[16]; //!< source address inline part.
  uint8_t dstInlinePart[16]; //!< destination address inline part.
  Ipv6ExtensionHeader *extensionHeader[] @owned; 
}


cplusplus(SixLowPanIphc) {{
  public:
    enum Hlim_e {
        HLIM_INLINE = 0,
        HLIM_COMPR_1,
        HLIM_COMPR_64,
        HLIM_COMPR_255
     };
     
     enum TrafficClassFlowLabel_e
     {
        TF_FULL = 0,
        TF_DSCP_ELIDED,
        TF_FL_ELIDED,
        TF_ELIDED
     };
     
  enum HeaderCompression_e
  {
    HC_INLINE = 0,
    HC_COMPR_64,
    HC_COMPR_16,
    HC_COMPR_0
  };     
     /**
     * Returns the extension header of the specified type,
     * or nullptr. If index is 0, then the first, if 1 then the
     * second extension is returned. (The datagram might
     * contain two Destination Options extension.)
     */
    virtual Ipv6ExtensionHeader *findExtensionHeaderByTypeForUpdate(IpProtocolId extensionType, int index = 0);
    virtual const Ipv6ExtensionHeader *findExtensionHeaderByType(IpProtocolId extensionType, int index = 0) const;

    /**
     * Adds an extension header to the datagram.
     * The extension headers are stored in the order specified in RFC 2460 4.1.
     */
    virtual void addExtensionHeader(Ipv6ExtensionHeader *eh);

    /**
     * Calculates the length of the Ipv6 header plus the extension
     * headers.
     */
    virtual B calculateHeaderByteLength() const;

    /**
     * Calculates the length of the unfragmentable part of Ipv6 header
     * plus the extension headers.
     */
    virtual B calculateUnfragmentableHeaderByteLength() const;

    /**
     * Calculates the length of the payload and extension headers
     * after the Fragment Header.
     */
    virtual B calculateFragmentLength() const;

    /**
     * Removes and returns the first extension header of this datagram
     */
    virtual Ipv6ExtensionHeader *removeFirstExtensionHeader();

    /**
     * Removes and returns the first extension header with the given type.
     */
    virtual Ipv6ExtensionHeader *removeExtensionHeader(IpProtocolId extensionType);
    // implements NetworkHeaderBase functions:
  
#if (OMNETPP_BUILDNUM < 1530)
    Ipv6ExtensionHeader * removeExtensionHeader(size_t k) { return dropExtensionHeader(k); }
    void appendExtensionHeader(Ipv6ExtensionHeader * extensionHeader) { insertExtensionHeader(extensionHeader); }
#endif

    virtual B getSerializedSize() const override;
    
    
    void setSrcInlinePart (uint8_t srcInlinePart[16], uint8_t size);
    const uint8_t* getSrcInlinePart (void) const;
    
    void setDstInlinePart (uint8_t srcInlinePart[16], uint8_t size);
    const uint8_t* getDstInlinePart (void) const;
    
    void setSrcContextId (uint8_t srcContextId);
    uint8_t getSrcContextId (void) const;
    void setDstContextId (uint8_t dstContextId);
    uint8_t getDstContextId (void) const;
    
    void setTf (TrafficClassFlowLabel_e tfField);
    TrafficClassFlowLabel_e getTf (void) const;
    void setNh (bool nhField);
    bool getNh (void) const;
    
    void setHlim (Hlim_e hlimField);
    Hlim_e getHlim (void) const;
    void setCid (bool cidField);
    bool getCid (void) const;
    void setSac (bool sacField);
    bool getSac (void) const;
    void setSam (HeaderCompression_e samField);
    HeaderCompression_e getSam (void) const;
    
    void setM (bool mField);
    bool getM (void) const;
    void setDac (bool dacField);
    bool getDac (void) const;
    void setDam (HeaderCompression_e damField);
    HeaderCompression_e getDam (void) const;
}}


class SixLowPanUdpNhcExtension extends FieldsChunk 
{
    uint8_t  baseFormat = 0xF0; //!< Dispatch + encoding fields.    
    uint16_t srcPort = 0;   //!< Source port.
    uint16_t dstPort = 0;   //!< Destination port.    
    uint16_t checksum = 0 @toString(utils::hex($)) @fromString(utils::uhex($));
    ChecksumMode checksumMode = CHECKSUM_MODE_UNDEFINED;
}

cplusplus(SixLowPanUdpNhcExtension) {{
  public:
    enum Ports_e {
       PORTS_INLINE = 0,
       PORTS_ALL_SRC_LAST_DST,
       PORTS_LAST_SRC_ALL_DST,
       PORTS_LAST_SRC_LAST_DST
    };
    void setPorts (Ports_e ports);
    Ports_e getPorts (void) const;
    void setC(bool cField);
    bool getC (void) const;
    virtual B getSerializedSize() const;
    virtual void adjustHeaderSize() {this->setChunkLength(this->getSerializedSize());}
}}

class SixLowPanBc0 extends SixLowPanDispatch
{
      chunkLength = B(2);
      dispatch = SixLowPanDispatchCode::LOWPAN_BC0;
      uint8_t seqNumber = 66;    //!< Sequence number.
}

cplusplus(SixLowPanBc0) {{
public:
  virtual B getSerializedSize() const override;
}}


class SixLowPanMesh extends SixLowPanDispatch {
    dispatch = SixLowPanDispatchCode::LOWPAN_MESH;
  uint8_t hopsLeft = 0; //!< Hops left.
  bool v = false;           //!< True if Originator address is 16 bit
  bool f = false;           //!< True if Destination address is 16 bit
  L3Address originator;      //!< Originator (source) address.
  L3Address destination;      //!< Destination (final) address.
}

cplusplus(SixLowPanMesh) {{
public:
  virtual B getSerializedSize() const override;
}}








