//
// Copyright (C) 2020 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#include "inet/protocolelement/shaper/EligibilityTimeGate.h"

#include "inet/protocolelement/shaper/EligibilityTimeTag_m.h"

namespace inet {

Define_Module(EligibilityTimeGate);

simsignal_t EligibilityTimeGate::remainingEligibilityTimeChangedSignal = cComponent::registerSignal("remainingEligibilityTimeChanged");

void EligibilityTimeGate::initialize(int stage)
{
    ClockUserModuleMixin::initialize(stage);
    if (stage == INITSTAGE_LOCAL) {
        eligibilityTimer = new ClockEvent("EligibilityTimer");
        lastRemainingEligibilityTimeSignalTime = simTime();
#if OMNETPP_VERSION >= 0x0602
        WATCH_EXPR("remainingEligibilityTime", getRemainingEligibilityTime());
#endif
    }
    else if (stage == INITSTAGE_QUEUEING) {
        updateOpen();
        emitEligibilityTimeChangedSignal();
    }
}

void EligibilityTimeGate::handleMessage(cMessage *message)
{
    if (message == eligibilityTimer)
        updateOpen();
    else
        throw cRuntimeError("Unknown message");
}

void EligibilityTimeGate::finish()
{
    emitEligibilityTimeChangedSignal();
}

void EligibilityTimeGate::updateOpen()
{
    auto packet = provider.canPullPacket();
    if (packet == nullptr || packet->getTag<EligibilityTimeTag>()->getEligibilityTime() <= getClockTime()) {
        if (isClosed())
            open();
    }
    else {
        if (isOpen())
            close();
    }
    packet = provider.canPullPacket();
    if (packet != nullptr) {
        clocktime_t eligibilityTime = packet->getTag<EligibilityTimeTag>()->getEligibilityTime();
        if (eligibilityTime > getClockTime())
            rescheduleClockEventAt(eligibilityTime, eligibilityTimer);
    }
}

void EligibilityTimeGate::emitEligibilityTimeChangedSignal()
{
    simtime_t now = simTime();
    simtime_t signalValue;
    if (lastRemainingEligibilityTimeSignalTime == now) {
        auto packet = provider.canPullPacket();
        signalValue = packet == nullptr ? 0 : CLOCKTIME_AS_SIMTIME(packet->getTag<EligibilityTimeTag>()->getEligibilityTime() - getClockTime());
        lastRemainingEligibilityTimePacket = packet;
    }
    else
        signalValue = lastRemainingEligibilityTimePacket == nullptr ? 0 : lastRemainingEligibilityTimeSignalValue - (now - lastRemainingEligibilityTimeSignalTime);
    emit(remainingEligibilityTimeChangedSignal, signalValue);
    lastRemainingEligibilityTimeSignalValue = signalValue;
    lastRemainingEligibilityTimeSignalTime = now;
}

Packet *EligibilityTimeGate::pullPacket(const cGate *gate)
{
    Enter_Method("pullPacket");
    emitEligibilityTimeChangedSignal();
    auto packet = PacketGateBase::pullPacket(gate);
    updateOpen();
    emitEligibilityTimeChangedSignal();
    return packet;
}

void EligibilityTimeGate::handleCanPullPacketChanged(const cGate *gate)
{
    Enter_Method("handleCanPullPacketChanged");
    emitEligibilityTimeChangedSignal();
    updateOpen();
    emitEligibilityTimeChangedSignal();
    PacketGateBase::handleCanPullPacketChanged(gate);
}

simtime_t EligibilityTimeGate::getRemainingEligibilityTime() const
{
    auto packet = provider.canPullPacket();
    auto remainingEligibilityTime = packet == nullptr ? 0 : CLOCKTIME_AS_SIMTIME(packet->getTag<EligibilityTimeTag>()->getEligibilityTime() - getClockTime());
    return remainingEligibilityTime < 0 ? 0 : remainingEligibilityTime;
}

#if OMNETPP_VERSION < 0x0602
std::string EligibilityTimeGate::resolveExpression(const char *expression) const
{
    if (!strcmp(expression, "remainingEligibilityTime"))
        return getRemainingEligibilityTime().ustr();
    else
        return ClockUserModuleMixin::resolveExpression(expression);
}
#endif

} // namespace inet

