//
// Copyright (C) 2024
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

package inet.node.qlearning;

import inet.node.inet.AdhocHost;
import inet.routing.qlearning.QLearn;

//
// A mobile node with the Q-Learning routing protocol.
// Extends AdhocHost by adding the Q-Learning routing protocol module.
//
// Q-Learning is a reinforcement learning based routing protocol where nodes
// learn optimal routes through experience. Each node maintains a Q-table that
// stores quality values for routes through different neighbors to various
// destinations.
//
// Key features:
// - Adaptive routing based on network conditions and past experience
// - Uses reinforcement learning (Q-learning algorithm) to learn optimal paths
// - Balances exploration (trying new routes) vs exploitation (using known good routes)
// - Maintains Q-table with route quality values
// - Inherits wireless capabilities and mobility support from AdhocHost
//
// @see ~AdhocHost, ~AodvRouter, ~DsdvRouter
//
module QLearningRouter extends AdhocHost
{
    submodules:
        qlearn: QLearn {
            @display("p=825,226");
        }
    connections:
        qlearn.socketOut --> at.in++;
        qlearn.socketIn <-- at.out++;
}
