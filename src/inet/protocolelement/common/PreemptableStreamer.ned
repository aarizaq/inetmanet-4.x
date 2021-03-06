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

package inet.protocolelement.common;

import inet.queueing.base.PacketProcessorBase;
import inet.queueing.contract.IPacketFlow;


//
// This module takes packets passed to its input and streams them to its output.
//
simple PreemptableStreamer extends PacketProcessorBase like IPacketFlow
{
    parameters:
        string clockModule = default(""); // relative path of a module that implements IClock; optional
        double datarate @unit(bps) = default(nan bps);
        int minPacketLength @unit(b);
        int roundingLength @unit(b) = default(1B);
        @class(PreemptableStreamer);
        @display("i=block/cogwheel");
    gates:
        input in @labels(send,push,pull,pass);
        output out @labels(send,push,pull,stream);
}
