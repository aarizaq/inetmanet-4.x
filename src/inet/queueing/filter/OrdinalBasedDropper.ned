//
// Copyright (C) 2020 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


package inet.queueing.filter;

import inet.queueing.base.PacketFilterBase;

//
// Drops packets based on the ordinal number of the packet in the
// received stream of packets.
//
simple OrdinalBasedDropper extends PacketFilterBase
{
    parameters:
        string dropsVector = default(""); // The dropsVector format is "<packetNumber>;<packetNumber>;...;"
                                          // Packet numbers in ascending order e.g. "5;6;12;13;"
        @class(OrdinalBasedDropper);
}

