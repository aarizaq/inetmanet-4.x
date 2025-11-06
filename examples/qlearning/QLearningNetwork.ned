//
// Copyright (C) 2024
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

package inet.examples.qlearning;

import inet.networklayer.configurator.ipv4.Ipv4NetworkConfigurator;
import inet.node.inet.AdhocHost;
import inet.physicallayer.wireless.common.contract.packetlevel.IRadioMedium;
import inet.visualizer.contract.IIntegratedVisualizer;


//
// Q-Learning routing test network with mobile nodes
//
network QLearningNetwork
{
    parameters:
        int numHosts;
        @display("bgb=600,600");
        
    submodules:
        visualizer: <default("IntegratedCanvasVisualizer")> like IIntegratedVisualizer if hasVisualizer() {
            @display("p=50,50");
        }
        
        configurator: Ipv4NetworkConfigurator {
            @display("p=50,100");
        }
        
        radioMedium: <default("UnitDiskRadioMedium")> like IRadioMedium {
            @display("p=50,150");
        }
        
        host[numHosts]: AdhocHost {
            @display("p=300,300");
        }
}

//
// Simple Q-Learning test with 3 nodes
//
network SimpleQLearning
{
    parameters:
        @display("bgb=600,400");
        
    submodules:
        configurator: Ipv4NetworkConfigurator {
            @display("p=50,50");
        }
        
        radioMedium: <default("UnitDiskRadioMedium")> like IRadioMedium {
            @display("p=50,100");
        }
        
        sender: AdhocHost {
            @display("p=100,200");
        }
        
        relay: AdhocHost {
            @display("p=300,200");
        }
        
        receiver: AdhocHost {
            @display("p=500,200");
        }
}
