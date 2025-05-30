//
// Copyright (C) 2020 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


package inet.linklayer.ethernet.basic;

import inet.common.SimpleModule;
import inet.linklayer.contract.IEthernetLayer;

//
// Performs Ethernet II or Ethernet with LLC/SNAP encapsulation/decapsulation.
//
// Expected environment:
// - `lowerLayerIn`, `lowerLayerOut` gates should be connected to an ~EthernetCsmaMacPhy
// - `upperLayerIn`, `upperLayerOut` gates should be connected to a higher layer
//   protocol or application
//
// Functionality:
// - packets coming from upper layers will be encapsulated in an `EthernetII` frame
//   (~EthernetMacHeader) or Ethernet SNAP frame (~Ieee8022LlcSnapHeader)
//   depending on the `useSNAP` module parameter,
//   and sent to the MAC. The ~EtherType is taken from the ~PacketProtocolTag tag
//   attached to the packet. The source address will be filled in by ~EthernetCsmaMacPhy.
// - also accepts SENDPAUSE command (message kind=IEEE802CTRL_SENDPAUSE) from
//   the higher layer: it will send a PAUSE frame, as described in
//   <a href="ether-pause.html">PAUSE frames</a>.
// - Ethernet frames arriving from MAC will be decapsulated and sent up to the
//   higher layers. Accepts ~EthernetMacHeader, ~Ieee8022LlcHeader, and
//   ~Ieee8022LlcSnapHeader too.
//
simple EthernetEncapsulation extends SimpleModule like IEthernetLayer
{
    parameters:
        @class(EthernetEncapsulation);
        bool registerProtocol = default(false);     //FIXME //KLUDGE should redesign place of EthernetEncapsulation and LLC modules
        string interfaceTableModule;   // The path to the InterfaceTable module
        string fcsMode @enum("declared","computed") = default("declared");
        @lifecycleSupport;
        double stopOperationExtraTime @unit(s) = default(-1s);    // Extra time after the lifecycle stop operation has finished
        double stopOperationTimeout @unit(s) = default(2s);    // Timeout value for the lifecycle stop operation
        @class(EthernetEncapsulation);
        @display("i=block/layer");
        @signal[encapPk](type=cPacket);
        @signal[decapPk](type=cPacket);
        @signal[pauseSent](type=long); // Pause units
        @signal[packetDropped](type=inet::Packet);
        @statistic[encapPk](title="packets encapsulated"; source=encapPk; record=count,"sum(packetBytes)","vector(packetBytes)"; interpolationmode=none);
        @statistic[decapPk](title="packets decapsulated"; source=decapPk; record=count,"sum(packetBytes)","vector(packetBytes)"; interpolationmode=none);
        @statistic[pauseSent](title="pause sent"; record=count,sum,vector);
    gates:
        input upperLayerIn; // TODO accepts packets and socket commands with  @messageKinds(inet::SocketCommandCode), too
        output upperLayerOut; // TODO sends packets and socket indications with @messageKinds(inet::SocketStatusInd), too
        input lowerLayerIn;
        output lowerLayerOut;
}
