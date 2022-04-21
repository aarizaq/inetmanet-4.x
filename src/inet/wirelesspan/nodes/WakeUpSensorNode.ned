//
// Copyright (C) 2020 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


package inet.wirelesspan.nodes;
import inet.node.base.ApplicationLayerNodeBase;
import inet.wirelesspan.wakeup.packetlevelradio.WakeUpIeee802154NarrowbandInterface;
import inet.linklayer.contract.INetworkInterface;
//
// This module implements a wireless sensor node. It has one 802.15.4 wireless interface
// and an energy storage module by default.
//# TODO and energyconsumer in the radio
//# TODO there is no management; the IdealEnergyStorage never depletes...
//
module WakeUpSensorNode extends ApplicationLayerNodeBase
{
    parameters:
        @display("i=misc/sensor2");
        hasIpv4 = default(true);
        hasIpv6 = default(false);
        *.routingTableModule = default("^.ipv4.routingTable");
        mobility.typename = default("StationaryMobility");
        *.interfaceTableModule = default(absPath(".interfaceTable"));
        wlanWake.radio.antenna.mobilityModule = default("^.^.^.mobility");
        wlanWake.controlledRadio.antenna.mobilityModule = default("^.^.^.mobility");
        @figure[submodules];
        numWlanInterfaces = default(0);
        energyStorage.typename = default("IdealEpEnergyStorage");
        wlanWake.radio.energyConsumer.typename = default("WakeUpStateBasedEpEnergyConsumer"); //FIXME does this consumer exist???
        wlanWake.controlledRadio.energyConsumer.typename = default("SensorStateBasedEpEnergyConsumer"); //FIXME does this consumer exist???
    gates:
        input radioInW[2] @directIn;
    submodules:
        wlanWake: WakeUpIeee802154NarrowbandInterface {
            @display("p=106.12601,973.19806,row,150;q=queue");
        }
    connections allowunconnected:
        radioInW[0] --> { @display("m=s"); } --> wlanWake.radioIn[0];
        radioInW[1] --> { @display("m=s"); } --> wlanWake.radioIn[1];

        wlanWake.upperLayerOut --> li.in++;
        wlanWake.upperLayerIn <-- li.out++;
}
