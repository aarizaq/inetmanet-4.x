//
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

#include "inet/physicallayer/analogmodel/packetlevel/ScalarNoise.h"
#include "inet/physicallayer/analogmodel/packetlevel/ScalarReception.h"
#include "inet/physicallayer/analogmodel/packetlevel/ScalarSnir.h"
#include "inet/physicallayer/apskradio/packetlevel/ApskDimensionalTransmission.h"
#include "inet/physicallayer/apskradio/packetlevel/ApskReceiver.h"
#include "inet/physicallayer/apskradio/packetlevel/ApskScalarTransmission.h"
#include "inet/physicallayer/common/packetlevel/BandListening.h"

namespace inet {

namespace physicallayer {

Define_Module(ApskReceiver);

ApskReceiver::ApskReceiver() :
    FlatReceiverBase()
{
}

std::ostream& ApskReceiver::printToStream(std::ostream& stream, int level) const
{
    stream << "ApskReceiver";
    return FlatReceiverBase::printToStream(stream, level);
}

bool ApskReceiver::computeIsReceptionPossible(const IListening *listening, const ITransmission *transmission) const
{
    return (dynamic_cast<const ApskScalarTransmission *>(transmission) ||
            dynamic_cast<const ApskDimensionalTransmission *>(transmission)) &&
            NarrowbandReceiverBase::computeIsReceptionPossible(listening, transmission);
}

bool ApskReceiver::computeIsReceptionPossible(const IListening *listening, const IReception *reception, IRadioSignal::SignalPart part) const
{
    return (dynamic_cast<const ApskScalarTransmission *>(reception->getTransmission()) ||
            dynamic_cast<const ApskDimensionalTransmission *>(reception->getTransmission())) &&
            FlatReceiverBase::computeIsReceptionPossible(listening, reception, part);
}

} // namespace physicallayer

} // namespace inet

