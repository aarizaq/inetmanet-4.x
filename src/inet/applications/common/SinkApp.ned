//
// Copyright (C) 2004 OpenSim Ltd.
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

package inet.applications.common;

import inet.applications.contract.IApp;
import inet.queueing.contract.IPassivePacketSink;

module SinkApp like IApp
{
    parameters:
        @display("i=block/app");
    gates:
        input socketIn;
        output socketOut;
    submodules:
        sink: <default("PassivePacketSink")> like IPassivePacketSink {
            @display("p=150,150");
        }
    connections allowunconnected:
        socketIn --> sink.in;
}
