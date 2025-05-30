//
// Copyright (C) 2019 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#ifndef __INET_PIM_H
#define __INET_PIM_H

#include "inet/common/SimpleModule.h"
#include "inet/networklayer/contract/INetfilter.h"
#include "inet/routing/base/RoutingProtocolBase.h"
#include "inet/routing/pim/PimPacket_m.h"
#include "inet/common/checksum/ChecksumMode_m.h"
#include "inet/transportlayer/common/TransportPseudoHeader_m.h"

namespace inet {

class INET_API PimChecksumInsertionHook : public SimpleModule, public NetfilterBase::HookBase
{
  public:
    virtual Result datagramPreRoutingHook(Packet *packet) override { return ACCEPT; }
    virtual Result datagramForwardHook(Packet *packet) override { return ACCEPT; }
    virtual Result datagramPostRoutingHook(Packet *packet) override;
    virtual Result datagramLocalInHook(Packet *packet) override { return ACCEPT; }
    virtual Result datagramLocalOutHook(Packet *packet) override { return ACCEPT; }
};

/**
 * Compound module for PIM protocol (RFC 4601).
 */
class INET_API Pim : public RoutingProtocolBase, protected cListener
{
  protected:
    // parameters
    ChecksumMode checksumMode = CHECKSUM_MODE_UNDEFINED;

  public:
    Pim() {}
    virtual ~Pim();
    ChecksumMode getChecksumMode() { return checksumMode; }
    static void insertChecksum(const Ptr<PimPacket>& pimPacket);
    static void insertChecksum(const Protocol *networkProtocol, const L3Address& srcAddress, const L3Address& destAddress, const Ptr<PimPacket>& pimPacket);
    static bool verifyChecksum(const Protocol *networkProtocol, const Ptr<const PimPacket>& pimPacket, Packet *packet);
    static uint16_t computeChecksum(const Protocol *networkProtocol, const L3Address& srcAddress, const L3Address& destAddress, const Ptr<const PimPacket>& pimPacket);

  protected:
    virtual int numInitStages() const override { return NUM_INIT_STAGES; }
    virtual void initialize(int stage) override;
    virtual void handleMessageWhenUp(cMessage *msg) override;
    virtual void handleStartOperation(LifecycleOperation *operation) override;
    virtual void handleStopOperation(LifecycleOperation *operation) override;
    virtual void handleCrashOperation(LifecycleOperation *operation) override;
};

} // namespace inet

#endif

