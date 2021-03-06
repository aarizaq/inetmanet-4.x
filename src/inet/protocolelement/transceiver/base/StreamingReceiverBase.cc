//
// Copyright (C) 2020 OpenSim Ltd.
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
// along with this program.  If not, see <https://www.gnu.org/licenses/>.
//

#include "inet/protocolelement/transceiver/base/StreamingReceiverBase.h"

namespace inet {

void StreamingReceiverBase::initialize(int stage)
{
    PacketReceiverBase::initialize(stage);
    if (stage == INITSTAGE_LOCAL) {
        transmissionChannel = inputGate->findTransmissionChannel();
        if (transmissionChannel != nullptr) {
            transmissionChannel->subscribe(PRE_MODEL_CHANGE, this);
            transmissionChannel->subscribe(POST_MODEL_CHANGE, this);
        }
        subscribe(PRE_MODEL_CHANGE, this);
        subscribe(POST_MODEL_CHANGE, this);
    }
}

void StreamingReceiverBase::receiveSignal(cComponent *source, simsignal_t signal, cObject *object, cObject *details)
{
    Enter_Method("%s", cComponent::getSignalName(signal));

    // TODO handle if the channel is cut at the receiver
}

} // namespace inet

