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
// along with this program.  If not, see http://www.gnu.org/licenses/.
// 


package inet.flora.loranode;

import inet.networklayer.common.InterfaceTable;
import inet.flora.lorabase.LoRaNic;
import inet.flora.loraapp.SimpleLoRaApp;
import inet.mobility.contract.IMobility;


module LoRaNode
{
    parameters:
        string deploymentType = default("");
        double maxGatewayDistance = default(320.0);
        double gatewayX = default(320.0);
        double gatewayY = default(320.0);
        @networkNode();
        *.interfaceTableModule = default(absPath(".interfaceTable"));
        @display("bgb=297,313;i=device/accesspoint;is=vs");
    gates:
        input radioIn[1] @directIn;
    submodules:
        interfaceTable: InterfaceTable {
            @display("p=30,26");
        }
        mobility: <default("StationaryMobility")> like IMobility {
             @display("p=24,88");
        }
        LoRaNic: LoRaNic {
            @display("p=137,239");
        }
        SimpleLoRaApp: SimpleLoRaApp {
            @display("p=137,32");
        }
    connections allowunconnected:
        SimpleLoRaApp.appOut --> LoRaNic.upperLayerIn;
        LoRaNic.upperLayerOut --> SimpleLoRaApp.appIn;
        radioIn[0] --> LoRaNic.radioIn;
}
