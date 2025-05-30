//
// Copyright (C) 2020 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


package inet.queueing.server;

import inet.queueing.base.PacketServerBase;
import inet.queueing.contract.IPacketServer;

//
// Repeatedly pulls packets from the connected packet provider and
// after a processing delay, it pushes the packet into the connected packet
// consumer. The processing delay is `processingTime` + `packetLength` / `processingBitrate`.
//
simple PacketServer extends PacketServerBase like IPacketServer
{
    parameters:
        displayStringTextFormat = default("served %p pk (%l)\n%s");
        string clockModule = default(""); // Relative path of a module that implements IClock; optional
        int serveSchedulingPriority = default(-1); // Specifies the FES scheduling priority for the extra event that is pulling the packet, -1 means no extra event
        volatile double processingTime @unit(s); // Determines additional processing time per packet
        volatile double processingBitrate @unit(bps) = default(inf bps); // Determines additional processing time per bit
        @class(PacketServer);
}
