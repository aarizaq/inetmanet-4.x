//
// Copyright (C) 2020 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


package inet.common.misc;

//
// This channel adds support for throughput metering to the datarate channel.
// A `cDatarateChannel` extended with throughput calculation. Values
// are displayed on the link, using the connection's "t=" or "tt=" display
// string tag.
//
// The display can be customized with the `thruputDisplayFormat` parameter.
// In the format string, the following characters will get expanded:
//   - 'N': number of packets
//   - 'V': volume (in bytes)
//   - 'p': current packet/sec
//   - 'b': current bandwidth
//   - 'u': current channel utilization (%)
//   - 'P': average packet/sec on [0,now)
//   - 'B': average bandwidth on [0,now)
//   - 'U': average channel utilization (%) on [0,now)
//
// Other characters are copied verbatim.
//
// "Current" actually means the last measurement interval, which is
// 10 packets or 0.1s, whichever comes first.
//
channel ThruputMeteringChannel extends ned.DatarateChannel
{
    @class(ThruputMeteringChannel);
    bool displayAsTooltip = default(false);
    string thruputDisplayFormat = default("");  // The format string for display on the link. Leave empty to disable display
}

