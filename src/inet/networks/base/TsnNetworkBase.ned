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

package inet.networks.base;

import inet.linklayer.configurator.gatescheduling.contract.IGateSchedulingConfigurator;
import inet.networklayer.configurator.contract.INetworkConfigurator;

//
// This module serves as a network base module for Time-Sensitive Networking (TSN).
//
network TsnNetworkBase extends WiredNetworkBase
{
    parameters:
        **.clock.defaultOverdueClockEventHandlingMode = default("execute");
    submodules:
        gateSchedulingConfigurator: <default("")> like IGateSchedulingConfigurator if typename != "" {
            @display("p=100,500;is=s");
        }
        streamRedundancyConfigurator: <default("")> like INetworkConfigurator if typename != "" {
            @display("p=100,600;is=s");
        }
        tsnConfigurator: <default("")> like INetworkConfigurator if typename != "" {
            @display("p=100,700;is=s");
        }
}