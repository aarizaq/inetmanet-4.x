//
// Copyright (C) 2013 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


package inet.physicallayer.wireless.common.radio.packetlevel;

import inet.physicallayer.wireless.common.base.packetlevel.PhysicalLayerBase;
import inet.physicallayer.wireless.common.contract.packetlevel.IAntenna;
import inet.physicallayer.wireless.common.contract.packetlevel.IRadio;
import inet.physicallayer.wireless.common.contract.packetlevel.IReceiver;
import inet.physicallayer.wireless.common.contract.packetlevel.ITransmitter;
import inet.power.contract.IEnergyConsumer;

//
// The radio model describes the physical device that is capable of transmitting
// and receiving signals on the medium. It contains an antenna model, a
// transmitter model, a receiver model, and an energy consumer model.
//
// The radio model supports changing radio mode, transmission power, or bitrate
// via ~ConfigureRadioCommand, or with direct C++ function calls.
//
// `switchingTimes` parameter should be regarded as a 5x5 matrix:
// <pre>
//               OFF SLEEP RECEIVER TRANSMITTER TRANSCEIVER
// OFF           0 0 0 0 0
// SLEEP         0 0 0 0 0
// RECEIVER      0 0 0 0 0
// TRANSMITTER   0 0 0 0 0
// TRANSCEIVER   0 0 0 0 0
// </pre>
// The corresponding `RadioMode` pairs contain the time needed to
// switch from one state to another.
// For example: the 6th number identifies the time needed to switch
// from SLEEP to OFF.
//
// The first entry of the `switchingTimes` string indicates the
// time-related metric prefix and it MUST be s, ms or ns.
//
module Radio extends PhysicalLayerBase like IRadio
{
    parameters:
        string radioMediumModule = default("radioMedium"); // Module path of the medium module where this radio communicates
        string energySourceModule = default("");           // Module path of the energy source module which provides energy to the radio

        string initialRadioMode @enum("off","sleep","receiver","transmitter","transceiver") = default("off");
        string switchingTimes = default("ms 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0"); // Time parameters to switch between radio modes

        bool sendRawBytes = default(false); // When true packets are serialized into a sequence of bytes before sending out

        bool separateTransmissionParts = default(false); // When enabled the transmission of preamble, header and data part are simulated separately
        bool separateReceptionParts = default(false);    // When enabled the reception of preamble, header and data part are simulated separately

        bool displayCommunicationRange = default(false); // If true communication range is displayed as a blue circle around the node
        bool displayInterferenceRange = default(false);  // If true interference range is displayed as a gray circle around the node

        string signalAnalogRepresentation @enum("unitDisk", "scalar", "dimensional","unitDiskLoss") = default("scalar");

        *.signalAnalogRepresentation = default(this.signalAnalogRepresentation);
        *.energySourceModule = default(absPath(this.energySourceModule));

        @class(Radio);

        @signal[packetSentToUpper](type=cPacket);
        @signal[packetReceivedFromUpper](type=cPacket);
        @signal[radioModeChanged](type=long);
        @signal[listeningChanged];
        @signal[receptionStateChanged](type=long);
        @signal[transmissionStateChanged](type=long);
        @signal[receivedSignalPartChanged](type=long);
        @signal[transmittedSignalPartChanged](type=long);
        @signal[transmissionStarted];
        @signal[transmissionEnded];
        @signal[receptionStarted];
        @signal[receptionEnded];

        @statistic[radioMode](title="Radio mode"; type=enum; enum=OFF, SLEEP, RECEIVER, TRANSMITTER, TRANSCEIVER, SWITCHING; source=radioModeChanged; record=count,vector; interpolationmode=sample-hold);
        @statistic[receptionState](title="Radio reception state"; type=enum; enum=UNDEFINED, IDLE, BUSY, RECEIVING; source=receptionStateChanged; record=count,vector; interpolationmode=sample-hold);
        @statistic[transmissionState](title="Radio transmission state"; type=enum; enum=UNDEFINED, IDLE, TRANSMITTING; source=transmissionStateChanged; record=count,vector; interpolationmode=sample-hold);

        @statistic[minSnir](title="Min SNIR"; source=minimumSnir(packetSentToUpper); record=histogram);
        @statistic[packetErrorRate](title="Packet error rate"; source=packetErrorRate(packetSentToUpper); record=histogram);
        @statistic[bitErrorRate](title="Bit error rate"; source=bitErrorRate(packetSentToUpper); record=histogram);
        @statistic[symbolErrorRate](title="Symbol error rate"; source=symbolErrorRate(packetSentToUpper); record=histogram);

    submodules:
        antenna: <default("IsotropicAntenna")> like IAntenna {
            parameters:
                @display("p=100,100");
        }
        transmitter: <> like ITransmitter {
            parameters:
                @display("p=100,200");
        }
        receiver: <> like IReceiver {
            parameters:
                @display("p=100,300");
        }
        energyConsumer: <default("")> like IEnergyConsumer if typename != "" {
            parameters:
                @display("p=100,400");
        }
}

