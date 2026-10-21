//
// Copyright (C) 2024
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

package inet.routing.qlearning;

import inet.common.SimpleModule;
import inet.applications.contract.IApp;

//
// Q-Learning Routing Protocol module.
//
// Q-Learning is a reinforcement learning based routing protocol that
// learns optimal routes through experience. Each node maintains a Q-table
// that stores quality values for each destination through each neighbor.
// The protocol selects routes based on these Q-values and updates them
// based on the success/failure of packet transmissions.
//
// Key features:
// - Adaptive routing based on network conditions
// - No pre-defined routes, learns through experience
// - Uses reward/penalty mechanism for route quality
// - Periodic hello messages for neighbor discovery
//
simple QLearn extends SimpleModule like IApp
{
    parameters:
        @class(qlearning::QLearn);
        @display("i=block/network2");
        
        // Module references
        string routingTableModule = default("^.ipv4.routingTable");
        string interfaceTableModule = default("^.interfaceTable");
        string networkProtocolModule = default("^.ipv4.ip");
        
        // Q-Learning parameters
        double learningRate = default(0.5);        // Alpha: learning rate (0-1)
        double discountFactor = default(0.9);      // Gamma: discount factor (0-1)
        double explorationRate = default(0.1);     // Epsilon: exploration rate (0-1)
        double initialQValue = default(0.0);       // Initial Q-value for new routes
        
        // Protocol parameters
        int udpPort = default(655);
        string interface = default("wlan0");
        double helloInterval @unit(s) = default(2s);  // Interval for hello messages
        double routeTimeout @unit(s) = default(10s);   // Route expiry time
        double qUpdateInterval @unit(s) = default(5s); // Q-value update interval
        
        // Reward/penalty values
        double successReward = default(1.0);       // Reward for successful transmission
        double failurePenalty = default(-1.0);     // Penalty for failed transmission
        double hopPenalty = default(-0.1);         // Penalty per hop
        
        // Jitter parameters
        double maxJitter @unit(s) = default(5ms);
        volatile double jitter @unit(s) = default(uniform(0ms, maxJitter));
        
    gates:
        input socketIn;
        output socketOut;
}
