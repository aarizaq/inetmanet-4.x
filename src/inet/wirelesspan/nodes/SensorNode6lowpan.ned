//
// Copyright (C) 2020 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


package inet.wirelesspan.nodes;
import inet.node.base.ApplicationLayerNodeBase;
//
// This module implements a wireless sensor node. It has one 802.15.4 wireless interface
// and an energy storage module by default.
//# TODO and energyconsumer in the radio
//# TODO there is no management; the IdealEnergyStorage never depletes...
//
module SensorNode6lowpan extends ApplicationLayerNodeBase
{
    parameters:
        @display("i=misc/sensor2");
        hasIpv4 = default(false);
        hasIpv6 = default(true);
        ipv6.typename = default("SixLowPanNetworkLayer");
        *.routingTableModule = default("^.ipv6.routingTable");
        
        @figure[submodules];
        numWlanInterfaces = default(1);
        energyStorage.typename = default("IdealEpEnergyStorage");
        wlan[*].typename = default("Ieee802154NarrowbandInterface");
        wlan[*].radio.energyConsumer.typename = default("SensorStateBasedEpEnergyConsumer"); //FIXME does this consumer exist???
}
