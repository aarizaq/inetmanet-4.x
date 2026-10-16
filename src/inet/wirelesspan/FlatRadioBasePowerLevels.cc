//
// Copyright (C) 2013 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#include "inet/wirelesspan/FlatRadioBasePowerLevels.h"
#include "inet/physicallayer/wireless/common/base/packetlevel/FlatReceiverBase.h"
#include "inet/physicallayer/wireless/common/base/packetlevel/FlatTransmitterBase.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/RadioControlInfo_m.h"

namespace inet {

namespace physicallayer {

Define_Module(FlatRadioBasePowerLevels);


FlatRadioBasePowerLevels::FlatRadioBasePowerLevels() : NarrowbandRadioBase()
{
}

void FlatRadioBasePowerLevels::parseLevels()
{
    const char *energy = par("enegyLevels");

    auto tokens = cStringTokenizer(energy).asVector();

    int nunLevels = tokens.size()/2;

    for (int i = 0; i < nunLevels; i++) {
        EnergyData data;
        auto powerOutput = tokens[i];
        auto consumption = tokens[i+1];
        auto posUnits = powerOutput.find("W");
        auto posUnits2 = consumption.find("A");
        if (posUnits == std::string::npos)
            throw cRuntimeError("Units not found power");
        powerOutput.pop_back();
        data.power = W(std::stod(powerOutput));
        if (posUnits2 == std::string::npos)
            throw cRuntimeError("Units not found consumption");
        consumption.pop_back();
        data.consumption = A(std::stod(consumption));
        powerVector.push_back(data);
    }
}

void FlatRadioBasePowerLevels::initialize(int stage)
{
    NarrowbandRadioBase::initialize(stage);
    if (stage == INITSTAGE_LOCAL) {
        parseLevels();
        energyConsumerCc = dynamic_cast<StateBasedCcEnergyConsumerLevel *>(getSubmodule("energyConsumer"));
        energyConsumerEp = dynamic_cast<StateBasedEpEnergyConsumerLevel *>(getSubmodule("energyConsumer"));
    }
}

void FlatRadioBasePowerLevels::setPowerLevel(int level)
{
    FlatTransmitterBase *flatTransmitter = const_cast<FlatTransmitterBase *>(check_and_cast<const FlatTransmitterBase *>(transmitter));
    flatTransmitter->setPower(powerVector[level].power);
    if (energyConsumerCc != nullptr)
        energyConsumerCc->setTransmissionConsumption(powerVector[level].consumption);
    else if (energyConsumerEp != nullptr)
        energyConsumerEp->setTransmissionConsumption(powerVector[level].consumptionEp);
}


void FlatRadioBasePowerLevels::setPower(W newPower)
{
    // select the level with the closet value to newPower
    W power(std::numeric_limits<double>::max());
    int index = -1;
    for (unsigned int i = 0 ; i < powerVector.size(); i++) {
        W diference;
        if (newPower > powerVector[i].power)
            diference = newPower - powerVector[i].power;
        else
            diference = powerVector[i].power - newPower;
        if (power > diference) {
            power = diference;
            index = (int)i;
        }
    }
    if (index != -1)
        setPowerLevel(index);
}

void FlatRadioBasePowerLevels::setBitrate(bps newBitrate)
{
    FlatTransmitterBase *flatTransmitter = const_cast<FlatTransmitterBase *>(check_and_cast<const FlatTransmitterBase *>(transmitter));
    flatTransmitter->setBitrate(newBitrate);

    while (!attemptedReceptionTimers.empty())
        abortReception(attemptedReceptionTimers.front());
}

void FlatRadioBasePowerLevels::handleUpperCommand(cMessage *message)
{
    if (message->getKind() == RADIO_C_CONFIGURE) {
        ConfigureRadioCommand *configureCommand = check_and_cast<ConfigureRadioCommand *>(message->getControlInfo());
        W newPower = configureCommand->getPower();
        if (!std::isnan(newPower.get())) {// find the nearest
            unsigned int level = 0;
            double distance = std::numeric_limits<double>::infinity();
            for (unsigned int i = 0; i < powerVector.size(); i++) {
                W d = newPower - powerVector[i].power;
                if (distance > std::abs(d.get()))
                    level = i;
            }
            setPowerLevel(level);
        }
        bps newBitrate = configureCommand->getBitrate();
        if (!std::isnan(newBitrate.get()))
            setBitrate(newBitrate);
    }
    NarrowbandRadioBase::handleUpperCommand(message);
}


} // namespace physicallayer

} // namespace inet

