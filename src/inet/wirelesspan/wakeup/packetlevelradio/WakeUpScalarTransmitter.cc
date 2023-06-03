//
// Copyright (C) 2014 Florian Meier
// Copyright (C) 2013 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/wirelesspan/wakeup/packetlevelradio/WakeUpScalarTransmitter.h"
#include "inet/mobility/contract/IMobility.h"
#include "inet/physicallayer/wireless/common/analogmodel/scalar/ScalarTransmissionAnalogModel.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/RadioControlInfo_m.h"
#include "inet/physicallayer/wireless/common/modulation/BpskModulation.h"
#include "inet/wirelesspan/wakeup/packetlevelradio/WakeUpTransmission.h"

namespace inet {
namespace wirelesspan {
namespace physicallayer {

Define_Module(WakeUpScalarTransmitter);

using namespace inet::physicallayer;
WakeUpScalarTransmitter::WakeUpScalarTransmitter() :
    FlatTransmitterBase()
{
}

std::ostream& WakeUpScalarTransmitter::printToStream(std::ostream& stream, int level, int evFlags) const
{
    stream << "WakeUpScalarTransmitter";
    return FlatTransmitterBase::printToStream(stream, level);
}

const ITransmission *WakeUpScalarTransmitter::createTransmission(const IRadio *transmitter, const Packet *packet, const simtime_t startTime) const
{
    /*
    W transmissionPower = computeTransmissionPower(packet);
    bps transmissionBitrate = computeTransmissionDataBitrate(packet);
    const simtime_t duration = b(packet->getBitLength()).get()/transmissionBitrate.get();

    const simtime_t endTime = startTime + duration;
    IMobility *mobility = transmitter->getAntenna()->getMobility();
    const Coord& startPosition = mobility->getCurrentPosition();
    const Coord& endPosition = mobility->getCurrentPosition();
    const Quaternion& startOrientation = mobility->getCurrentAngularPosition();
    const Quaternion& endOrientation = mobility->getCurrentAngularPosition();
    return new ScalarTransmission(transmitter, packet, startTime, endTime, simtime_t::ZERO, simtime_t::ZERO, duration, startPosition, endPosition, startOrientation, endOrientation, &BpskModulation::singleton, b(0), packet->getTotalLength(), centerFrequency, bandwidth, transmissionBitrate, transmissionPower);
    */

    W transmissionPower = computeTransmissionPower(packet);
    bps transmissionBitrate = computeTransmissionDataBitrate(packet);
    const simtime_t headerDuration = b(headerLength).get() / bps(transmissionBitrate).get();
    const simtime_t dataDuration = b(packet->getTotalLength()).get() / bps(transmissionBitrate).get();
    const simtime_t duration = preambleDuration + headerDuration + dataDuration;
    const simtime_t endTime = startTime + duration;
    IMobility *mobility = transmitter->getAntenna()->getMobility();
    const Coord& startPosition = mobility->getCurrentPosition();
    const Coord& endPosition = mobility->getCurrentPosition();
    const Quaternion& startOrientation = mobility->getCurrentAngularPosition();
    const Quaternion& endOrientation = mobility->getCurrentAngularPosition();
    auto symbolTime = 0;
    auto analogModel = getAnalogModel()->createAnalogModel(preambleDuration, headerDuration, dataDuration, centerFrequency, bandwidth, transmissionPower);
    return new WakeUpTransmission(transmitter, packet, startTime, endTime, preambleDuration, headerDuration, dataDuration, startPosition, endPosition, startOrientation, endOrientation, nullptr, nullptr, nullptr, nullptr, analogModel, transmissionPower, centerFrequency, bandwidth);
}

} // namespace physicallayer
}
} // namespace inet

