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

package inet.physicallayer.wireless.common.base.packetlevel;

import inet.physicallayer.wireless.common.base.packetlevel.NarrowbandReceiverBase;

//
// TODO auto-generated module
//
module LoRaReceiver extends NarrowbandReceiverBase
{
        parameters:
        
            
        @signal[LoRaReceptionCollision](type=bool); // optional
        @statistic[LoRaReceptionCollision](source=LoRaReceptionCollision; record=count);
        //double energyDetection @unit(dBm); // no signal is detected at all below this reception power threshold (idle state)
        //double sensitivity @unit(dBm);     // reception is not possible if the signal power is below sensitivity (idle or busy states)
        //double snirThreshold @unit(dB);    // reception is not successful if the SNIR is below this threshold (unsuccessful reception)
        //double centerFrequency @unit(Hz); // center frequency of the band where this receiver listens on the medium
        //double bandwidth @unit(Hz);        // bandwidth of the band where this receiver listens on the medium
        //string modulation;                 // one of "BPSK", "16-QAM", "256-QAM"
        errorModel.typename = default("");
        modulation = default("BPSK"); // not used for the lora module 
        bool alohaChannelModel = default(false);
        @class(LoRaReceiver);
        @display("i=block/wrx");
    
}
