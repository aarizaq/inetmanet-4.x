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

package inet.wirelesspan.wakeup.packetlevelradio;
import inet.physicallayer.wireless.common.medium.RadioMedium;

module WakeUpScalarRadioMedium extends RadioMedium
{
    parameters:
        backgroundNoise.typename = default("IsotropicScalarBackgroundNoise");

        mediumLimitCache.centerFrequency = 433 MHz;

        // 802.15.4-2006, page 266
        pathLoss.typename = default("BreakpointPathLoss");
        pathLoss.breakpointDistance = 8 m;
        pathLoss.l01 = 40.2dB;
        pathLoss.alpha1 = 2;
        pathLoss.l02 = 58.5dB;
        pathLoss.alpha2 = 3.3;

        // Reflects the thermal noise for the receiver sensitivity
        // TODO couple with sensitivity
        backgroundNoise.power = default(-96.616dBm);
        backgroundNoise.dimensions = default("time");
        mediumLimitCache.maxTransmissionDuration = default(34 ms);
}


