//
// Copyright (C) 2016 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#include "inet/flora/lorabase/CsmaCaMacLora.h"
#include "inet/flora/lorabase/LoRaMacFrame_m.h"
#include "inet/common/ModuleAccess.h"
#include "inet/common/ProtocolGroup.h"
#include "inet/common/ProtocolTag_m.h"
#include "inet/common/packet/chunk/ByteCountChunk.h"
#include "inet/linklayer/common/InterfaceTag_m.h"
#include "inet/linklayer/common/MacAddressTag_m.h"
#include "inet/flora/lorabase/LoRaTagInfo_m.h"

namespace inet {
namespace flora {

using namespace inet::physicallayer;

Define_Module(CsmaCaMacLora);


/****************************************************************
 * Initialization functions.
 */

void CsmaCaMacLora::generateBackoffPeriod()
{
    ASSERT(0 <= retryCounter && retryCounter <= retryLimit);
    EV << "generating backoff slot number for retry: " << retryCounter << endl;
    int cw;
    if (getCurrentTransmission()->peekAtFront<LoRaMacFrame>()->getReceiverAddress().isMulticast())
        cw = cwMulticast;
    else
        cw = std::min(cwMax, (cwMin + 1) * (1 << retryCounter) - 1);
    int slots = intrand(cw + 1);
    EV << "generated backoff slot number: " << slots << " , cw: " << cw << endl;
    backoffPeriod = slots * slotTime;
    ASSERT(backoffPeriod >= 0);
    EV << "backoff period set to " << backoffPeriod << endl;
}

bool CsmaCaMacLora::isFcsOk(Packet *frame)
{
    if (frame->hasBitError() || !frame->peekData()->isCorrect())
        return false;
    else
        return true;
}


void CsmaCaMacLora::initialize(int stage)
{
    CsmaCaMac::initialize(stage);
    if (stage == INITSTAGE_LINK_LAYER) {
        // subscribe for the information of the carrier sense
       // search the bit app module to read the paramters.
    }
}

bps CsmaCaMacLora::computeBitRate(Packet *pkt) {
    auto tag = pkt->getTag<LoRaTag>();
    double rateCode = 4.0/(4.0+tag->getCodeRendundance());
    bps tbitrate =  bps(tag->getSpreadFactor() * rateCode / (pow(2,tag->getSpreadFactor())/tag->getBandwidth().get()));
    return tbitrate;
}

bool CsmaCaMacLora::isAck(Packet *frame)
{
    const auto& macHeader = frame->peekAtFront<LoRaMacFrame>();
    return macHeader->getType() == LORA_ACK;
}

bool CsmaCaMacLora::isBroadcast(Packet *frame)
{
    const auto& macHeader = frame->peekAtFront<LoRaMacFrame>();
    return macHeader->getReceiverAddress().isBroadcast();
}

bool CsmaCaMacLora::isForUs(Packet *frame)
{
    const auto& macHeader = frame->peekAtFront<LoRaMacFrame>();
    return macHeader->getReceiverAddress() == networkInterface->getMacAddress();
}

void CsmaCaMacLora::encapsulate(Packet *msg)
{
    auto frame = makeShared<LoRaMacFrame>();
    frame->setChunkLength(B(headerLength));
    auto tag = msg->getTag<LoRaTag>();

    frame->setLoRaTP(tag->getPower().get());
    frame->setLoRaCF(tag->getCenterFrequency());
    frame->setLoRaSF(tag->getSpreadFactor());
    frame->setLoRaBW(tag->getBandwidth());
    frame->setLoRaCR(tag->getCodeRendundance());
    frame->setSequenceNumber(sequenceNumber);
    frame->setReceiverAddress(msg->getTag<MacAddressReq>()->getDestAddress());
    frame->setTransmitterAddress(networkInterface->getMacAddress());

    ++sequenceNumber;
    //frame->setLoRaUseHeader(cInfo->getLoRaUseHeader());
    frame->setLoRaUseHeader(tag->getUseHeader());
    msg->insertAtFront(frame);
}

void CsmaCaMacLora::decapsulate(Packet *frame)
{
    auto loraHeader = frame->popAtFront<LoRaMacFrame>();
    frame->addTagIfAbsent<MacAddressInd>()->setSrcAddress(loraHeader->getTransmitterAddress());
    frame->addTagIfAbsent<MacAddressInd>()->setDestAddress(loraHeader->getReceiverAddress());
    frame->addTagIfAbsent<InterfaceInd>()->setInterfaceId(networkInterface->getInterfaceId());
    //    auto payloadProtocol = ProtocolGroup::ethertype.getProtocol(loraHeader->getNetworkProtocol());
    //    frame->addTagIfAbsent<DispatchProtocolReq>()->setProtocol(payloadProtocol);
    //    frame->addTagIfAbsent<PacketProtocolTag>()->setProtocol(payloadProtocol);
}


void CsmaCaMacLora::handleUpperPacket(Packet *packet)
{
    auto tagAddr = packet->findTagForUpdate<MacAddressReq>();
    if (tagAddr == nullptr) {
        tagAddr = packet->addTag<MacAddressReq>();
        tagAddr->setDestAddress(MacAddress::BROADCAST_ADDRESS);
    }
    if (tagAddr->getDestAddress().isUnspecified()) {
        tagAddr->setDestAddress(MacAddress::BROADCAST_ADDRESS);
    }

    EV << "frame " << packet << " received from higher layer, receiver = " << tagAddr->getDestAddress() << endl;
    encapsulate(packet);
    if (currentTxFrame != nullptr)
        throw cRuntimeError("Model error: incomplete transmission exists");
    currentTxFrame = packet;
    handleWithFsm(currentTxFrame);
}

}

} // namespace inet

