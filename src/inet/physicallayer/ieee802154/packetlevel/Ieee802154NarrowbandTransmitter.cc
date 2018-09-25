//
// Copyright (C) 2014 Florian Meier
// Copyright (C) 2013 OpenSim Ltd.
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program; if not, see <http://www.gnu.org/licenses/>.
//

#include "inet/mobility/contract/IMobility.h"
#include "inet/physicallayer/analogmodel/packetlevel/DimensionalTransmission.h"
#include "inet/physicallayer/analogmodel/packetlevel/ScalarTransmission.h"
#include "inet/physicallayer/contract/packetlevel/RadioControlInfo_m.h"
#include "inet/physicallayer/ieee802154/packetlevel/Ieee802154NarrowbandTransmitter.h"

namespace inet {

namespace physicallayer {

Define_Module(Ieee802154NarrowbandTransmitter);

Ieee802154NarrowbandTransmitter::Ieee802154NarrowbandTransmitter() :
    FlatTransmitterBase(),
    DimensionalTransmitterBase()
{
}

void Ieee802154NarrowbandTransmitter::initialize(int stage)
{
    FlatTransmitterBase::initialize(stage);
    DimensionalTransmitterBase::initialize(stage);
}

std::ostream& Ieee802154NarrowbandTransmitter::printToStream(std::ostream& stream, int level) const
{
    stream << "Ieee802154NarrowbandTransmitter";
    DimensionalTransmitterBase::printToStream(stream, level);
    return DimensionalTransmitterBase::printToStream(stream, level);
}

const ITransmission *Ieee802154NarrowbandTransmitter::createTransmission(const IRadio *transmitter, const Packet *packet, const simtime_t startTime) const
{
    W transmissionPower = computeTransmissionPower(packet);
    bps transmissionBitrate = computeTransmissionDataBitrate(packet);
    const simtime_t headerDuration = b(headerLength).get() / bps(transmissionBitrate).get();
    const simtime_t dataDuration = b(packet->getTotalLength()).get() / bps(transmissionBitrate).get();
    const simtime_t duration = preambleDuration + headerDuration + dataDuration;
    const simtime_t endTime = startTime + duration;
    IMobility *mobility = transmitter->getAntenna()->getMobility();
    const Coord startPosition = mobility->getCurrentPosition();
    const Coord endPosition = mobility->getCurrentPosition();
    const Quaternion startOrientation = mobility->getCurrentAngularPosition();
    const Quaternion endOrientation = mobility->getCurrentAngularPosition();
    if (dimensions.size() == 0)
        return new ScalarTransmission(transmitter, packet, startTime, endTime, preambleDuration, headerDuration, dataDuration, startPosition, endPosition, startOrientation, endOrientation, modulation, headerLength, packet->getTotalLength(), carrierFrequency, bandwidth, transmissionBitrate, transmissionPower);
    else {
        const ConstMapping *powerMapping = createPowerMapping(startTime, endTime, carrierFrequency, bandwidth, transmissionPower);
        return new DimensionalTransmission(transmitter, packet, startTime, endTime, preambleDuration, headerDuration, dataDuration, startPosition, endPosition, startOrientation, endOrientation, modulation, headerLength, packet->getTotalLength(), carrierFrequency, bandwidth, transmissionBitrate, powerMapping);
    }
}

} // namespace physicallayer

} // namespace inet

