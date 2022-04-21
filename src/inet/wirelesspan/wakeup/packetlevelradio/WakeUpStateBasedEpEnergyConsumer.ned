//
// Copyright (C) 2013 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

package inet.wirelesspan.wakeup.packetlevelradio;
import inet.physicallayer.wireless.common.energyconsumer.StateBasedEpEnergyConsumer;

//
// This module provides a radio power consumer model. The power consumption is
// determined by the radio mode, the transmitter state and the receiver state
// using constant parameters.
//
// This module provides default values
// for low power wireless sensor nodes. Roughly based on
//

simple WakeUpStateBasedEpEnergyConsumer extends StateBasedEpEnergyConsumer
{
    parameters:
        offPowerConsumption = default(0mW);
        sleepPowerConsumption = default(0.1uW);
        switchingPowerConsumption = default(5mW);
        receiverIdlePowerConsumption = default(5mW);
        receiverBusyPowerConsumption = default(5mW);
        receiverReceivingPowerConsumption = default(5mW);
        transmitterIdlePowerConsumption = default(5mW);
        transmitterTransmittingPowerConsumption = default(5mW);
}

