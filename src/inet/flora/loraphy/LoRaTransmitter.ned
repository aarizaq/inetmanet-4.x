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

package inet.flora.loraphy;

import inet.physicallayer.wireless.common.base.packetlevel.NarrowbandTransmitterBase;

module LoRaTransmitter extends NarrowbandTransmitterBase
{
    parameters:
        headerLength = default(0b); // 
        string dataRate @enum("DR0", "DR1","DR2","DR3","DR4","DR5","DR6","DR7","DR8","DR9","DR10","DR11","DR12","DR13") = default("DR6");
        @signal[LoRaTransmissionCreated](type=bool); // optional
        @statistic[LoRaTransmissionCreated](source=LoRaTransmissionCreated; record=count);
        modulation = default("LoRaModulation");
        @class(LoRaTransmitter);
}