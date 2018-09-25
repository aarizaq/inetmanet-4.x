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

#include "inet/physicallayer/ieee802154/packetlevel/Ieee802154NarrowbandReceiver.h"

namespace inet {

namespace physicallayer {

Define_Module(Ieee802154NarrowbandReceiver);

Ieee802154NarrowbandReceiver::Ieee802154NarrowbandReceiver() :
    FlatReceiverBase()
{
}

void Ieee802154NarrowbandReceiver::initialize(int stage)
{
    FlatReceiverBase::initialize(stage);
    if (stage == INITSTAGE_LOCAL) {
        minInterferencePower = mW(math::dBm2mW(par("minInterferencePower")));
    }
}

std::ostream& Ieee802154NarrowbandReceiver::printToStream(std::ostream& stream, int level) const
{
    stream << "Ieee802154NarrowbandReceiver";
    return FlatReceiverBase::printToStream(stream, level);
}

} // namespace physicallayer

} // namespace inet

