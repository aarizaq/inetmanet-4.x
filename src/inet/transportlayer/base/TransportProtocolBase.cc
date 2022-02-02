//
// Copyright (C) 2013 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#include "inet/transportlayer/base/TransportProtocolBase.h"

namespace inet {

bool TransportProtocolBase::isUpperMessage(cMessage *msg)
{
    return msg->arrivedOn("appIn");
}

bool TransportProtocolBase::isLowerMessage(cMessage *msg)
{
    return msg->arrivedOn("ipIn");
}

bool TransportProtocolBase::isInitializeStage(int stage)
{
    return stage == INITSTAGE_TRANSPORT_LAYER;
}

bool TransportProtocolBase::isModuleStartStage(int stage)
{
    return stage == ModuleStartOperation::STAGE_TRANSPORT_LAYER;
}

bool TransportProtocolBase::isModuleStopStage(int stage)
{
    return stage == ModuleStopOperation::STAGE_TRANSPORT_LAYER;
}

} // namespace inet

