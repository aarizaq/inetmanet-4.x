

// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License version 2 as
// published by the Free Software Foundation;
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
// Copyright (c) 2022 Universidad de Malaga, Spain
// Author: Alfonso Ariza aarizaq@uma.es
//

// Disable warnings about unused variables, empty switch stmts, etc:

#include "inet/wirelesspan/networklayer/sixlowpan/SixLowPanDispatchCode.h"
#include "inet/wirelesspan/networklayer/sixlowpan/SixLowPanHeader_m.h"

namespace inet {
namespace wirelesspan {
namespace sixlowpan {
/*
uint8_t SixLowPanDispatch::getDispatchType() const
{
    return SixLowPanDispatchCode::GetDispatchType(dispatch);
}

uint8_t SixLowPanDispatch::getNhcDispatchType() const
{
    return SixLowPanDispatchCode::GetNhcDispatchType(dispatch);
}
*/

B SixLowPanHc1::getSerializedSize () const
{
  B serializedSize = B(3);

  switch (srcCompression)
    {
    case HC1_PIII:
      serializedSize += B(16);
      break;
    case HC1_PIIC:
      serializedSize += B(8);
      break;
    case HC1_PCII:
      serializedSize += B(8);
      break;
    case HC1_PCIC:
      break;
    }
  switch (dstCompression)
    {
    case HC1_PIII:
      serializedSize += B(16);
      break;
    case HC1_PIIC:
      serializedSize += B(8);
      break;
    case HC1_PCII:
      serializedSize += B(8);
      break;
    case HC1_PCIC:
      break;
    }

  if (tcflCompression == false )
      serializedSize += B(4);

  if (nextHeaderCompression == HC1_NC )
      serializedSize++;

  return serializedSize;
}

B SixLowPanFrag1::getSerializedSize () const
{
  return B(4);
}

B SixLowPanFragN::getSerializedSize () const
{
  return B(5);
}

const uint8_t* SixLowPanIphc::getSrcInlinePart (void) const
{
  return dstInlinePart;
}

void SixLowPanIphc::setSrcInlinePart (uint8_t srcInlinePartAux[16], uint8_t size)
{
    ASSERT2(size <= 16,  "Src inline part too large");
    memcpy (srcInlinePart, srcInlinePartAux, size);
    return;
}

const uint8_t* SixLowPanIphc::getDstInlinePart (void) const
{
  return dstInlinePart;
}

void SixLowPanIphc::setDstInlinePart (uint8_t dstInlinePartAux[16], uint8_t size)
{
    ASSERT2(size <= 16,  "Src inline part too large");
    memcpy (dstInlinePart, dstInlinePartAux, size);
    return;
}

B SixLowPanIphc::getSerializedSize() const {
    B serializedSize = B(2);

    if (getCid())
        serializedSize++;

    switch (getTf()) {
    case TF_FULL:
        serializedSize += B(4);
        break;
    case TF_DSCP_ELIDED:
        serializedSize += B(3);
        break;
    case TF_FL_ELIDED:
        serializedSize++;
        break;
    default:
        break;
    }
    if (getNh() == false)
        serializedSize++;
    if (getHlim() == HLIM_INLINE)
        serializedSize++;
    switch (getSam()) {
    case HC_INLINE:
        if (getSac() == false) {
            serializedSize += B(16);
        }
        break;
    case HC_COMPR_64:
        serializedSize += B(8);
        break;
    case HC_COMPR_16:
        serializedSize += B(2);
        break;
    case HC_COMPR_0:
    default:
        break;
    }
    if (getM() == false) {
        switch (getDam()) {
        case HC_INLINE:
            if (getDac() == false) {
                serializedSize += B(16);
            }
            break;
        case HC_COMPR_64:
            serializedSize += B(8);
            break;
        case HC_COMPR_16:
            serializedSize += B(2);
            break;
        case HC_COMPR_0:
        default:
            break;
        }
    }
    else {
        switch (getDam()) {
        case HC_INLINE:
            if (getDac() == false) {
                serializedSize += B(16);
            }
            else {
                serializedSize += B(6);
            }
            break;
        case HC_COMPR_64:
            if (getDac() == false) {
                serializedSize += B(6);
            }
            break;
        case HC_COMPR_16:
            if (getDac() == false) {
                serializedSize += B(4);
            }
            break;
        case HC_COMPR_0:
        default:
            if (getDac() == false)
                serializedSize++;
            break;
        }
    }

    return serializedSize;
}

void SixLowPanIphc::setSrcContextId (uint8_t srcContextId)
{
    if (srcContextId > 15)
        throw cRuntimeError("Src Context ID too large");
    srcdstContextId |= srcContextId << 4;
}

uint8_t SixLowPanIphc::getSrcContextId (void) const
{
    return (srcdstContextId >> 4);
}

void SixLowPanIphc::setDstContextId (uint8_t dstContextId)
{
    if (dstContextId > 15)
        throw cRuntimeError(dstContextId < 16,  "Dst Context ID too large");
    srcdstContextId |= (dstContextId & 0xF);
}

uint8_t SixLowPanIphc::getDstContextId (void) const
{
    return (srcdstContextId & 0xF);
}


void SixLowPanIphc::setTf (TrafficClassFlowLabel_e tfField)
{
  uint16_t field = tfField;
  baseFormat |= (field << 11);
}

SixLowPanIphc::TrafficClassFlowLabel_e SixLowPanIphc::getTf (void) const
{
  return TrafficClassFlowLabel_e ((baseFormat >> 11) & 0x3);
}

void SixLowPanIphc::setNh (bool nhField)
{
  uint16_t field = nhField;
  baseFormat |= (field << 10);
}

bool SixLowPanIphc::getNh (void) const
{
  return ((baseFormat >> 10) & 0x1);
}

void SixLowPanIphc::setHlim (SixLowPanIphc::Hlim_e hlimField)
{
  uint16_t field = hlimField;
  baseFormat |= (field << 8);
}

SixLowPanIphc::Hlim_e SixLowPanIphc::getHlim (void) const
{
  return Hlim_e ((baseFormat >> 8) & 0x3);
}

void SixLowPanIphc::setCid (bool cidField)
{
  uint16_t field = cidField;
  baseFormat |= (field << 7);
}

bool SixLowPanIphc::getCid (void) const
{
  return ((baseFormat >> 7) & 0x1);
}

void SixLowPanIphc::setSac (bool sacField)
{
  uint16_t field = sacField;
  baseFormat |= (field << 6);
}

bool SixLowPanIphc::getSac (void) const
{
  return ((baseFormat >> 6) & 0x1);
}

void SixLowPanIphc::setSam (HeaderCompression_e samField)
{
  uint16_t field = samField;
  baseFormat |= (field << 4);
}

SixLowPanIphc::HeaderCompression_e SixLowPanIphc::getSam (void) const
{
  return HeaderCompression_e ((baseFormat >> 4) & 0x3);
}

void SixLowPanIphc::setM (bool mField)
{
  uint16_t field = mField;
  baseFormat |= (field << 3);
}

bool SixLowPanIphc::getM (void) const
{
  return ((baseFormat >> 3) & 0x1);
}

void SixLowPanIphc::setDac (bool dacField)
{
  uint16_t field = dacField;
  baseFormat |= (field << 2);
}

bool SixLowPanIphc::getDac (void) const
{
  return ((baseFormat >> 2) & 0x1);
}

void SixLowPanIphc::setDam (SixLowPanIphc::HeaderCompression_e damField)
{
  uint16_t field = damField;
  baseFormat |= field;
}

SixLowPanIphc::HeaderCompression_e SixLowPanIphc::getDam (void) const
{
  return HeaderCompression_e (baseFormat & 0x3);
}


void SixLowPanUdpNhcExtension::setPorts (Ports_e ports)
{
  uint16_t field = ports;
  baseFormat |= field;
}

SixLowPanUdpNhcExtension::Ports_e SixLowPanUdpNhcExtension::getPorts (void) const
{
  return Ports_e (baseFormat & 0x3);
}

void SixLowPanUdpNhcExtension::setC (bool cField)
{
  uint16_t field = cField;
  baseFormat |= (field << 2);
}

bool SixLowPanUdpNhcExtension::getC (void) const
{
  return ((baseFormat >> 2) & 0x1);
}

B SixLowPanUdpNhcExtension::getSerializedSize () const
{
  B serializedSize = B(1);
  if (!getC()) {
      serializedSize += B(2);
  }
  switch (getPorts ())
    {
    case PORTS_INLINE:
      serializedSize += B(4);
      break;
    case PORTS_ALL_SRC_LAST_DST:
    case PORTS_LAST_SRC_ALL_DST:
      serializedSize += B(3);
      break;
    case PORTS_LAST_SRC_LAST_DST:
      serializedSize += B(1);
      break;
    default:
      break;
    }
  return serializedSize;
}

B SixLowPanMesh::getSerializedSize () const
{
  B serializedSize = B(1);

  if (hopsLeft >= 0xF)
      serializedSize++;

  if (v)
      serializedSize += B(2);
  else
      serializedSize += B(8);

  if (f)
      serializedSize += B(2);
  else
      serializedSize += B(8);

  return serializedSize;
}

B SixLowPanBc0::getSerializedSize() const {
   return B(2);
}

}  // namespace sixlowpan
}
} // namespace inet
