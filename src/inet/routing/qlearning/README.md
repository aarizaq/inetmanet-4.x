# Q-Learning Routing Protocol

## Overview

This directory contains the implementation of a Q-Learning based routing protocol for INET/OMNeT++. Q-Learning is a reinforcement learning algorithm that enables nodes to learn optimal routing paths through experience rather than using predefined routing metrics.

## Files

- **QLearn.h/cc**: Main protocol implementation
- **QLearn.ned**: NED module definition with configurable parameters
- **QLearningPacket.msg**: Message definitions for protocol packets
- **QLearningPacketSerializer.h/cc**: Serialization for network transmission
- **README.md**: This file

## How Q-Learning Routing Works

### Basic Concept

Q-Learning routing uses a reinforcement learning approach where:
1. Each node maintains a **Q-table** that stores quality values (Q-values) for routes
2. Q-values represent the expected quality of reaching a destination through a specific next hop
3. Nodes learn optimal routes by receiving rewards/penalties based on routing success/failure
4. The protocol balances **exploration** (trying new routes) vs **exploitation** (using known good routes)

### Q-Learning Algorithm

The core Q-learning update equation is:

```
Q(s,a) ← Q(s,a) + α[R + γ·max(Q(s',a')) - Q(s,a)]
```

Where:
- **Q(s,a)**: Q-value for state s and action a (destination and next hop)
- **α (alpha)**: Learning rate (0-1), controls how much new information overrides old
- **R**: Immediate reward (positive for success, negative for failure)
- **γ (gamma)**: Discount factor (0-1), importance of future rewards
- **max(Q(s',a'))**: Maximum Q-value for the next state

### Protocol Operations

#### 1. Neighbor Discovery
- Nodes periodically broadcast **Hello messages** to discover neighbors
- Maintains a neighbor table with last-seen timestamps
- Removes neighbors that haven't been heard from within the timeout period

#### 2. Route Selection
Uses **epsilon-greedy** strategy:
- With probability **ε (epsilon)**: **Explore** by selecting a random neighbor
- With probability **1-ε**: **Exploit** by selecting the neighbor with highest Q-value

#### 3. Q-Value Updates
- When packets are successfully forwarded: positive reward
- When packets fail or links break: negative penalty
- Additional penalty per hop to favor shorter paths

#### 4. Route Maintenance
- Q-values are updated based on packet transmission outcomes
- Old Q-values decay over time if routes are not used
- Link breakages trigger Q-value penalties for affected routes

## Configuration Parameters

### Q-Learning Parameters

| Parameter | Default | Description |
|-----------|---------|-------------|
| `learningRate` | 0.5 | α: How quickly the node learns (0-1) |
| `discountFactor` | 0.9 | γ: Importance of future rewards (0-1) |
| `explorationRate` | 0.1 | ε: Probability of exploring vs exploiting (0-1) |
| `initialQValue` | 0.0 | Initial Q-value for new routes |

### Protocol Parameters

| Parameter | Default | Description |
|-----------|---------|-------------|
| `udpPort` | 655 | UDP port for Q-learning messages |
| `helloInterval` | 2s | How often to send hello messages |
| `routeTimeout` | 10s | Time before a route expires |
| `qUpdateInterval` | 5s | How often to perform Q-value updates |

### Reward/Penalty Values

| Parameter | Default | Description |
|-----------|---------|-------------|
| `successReward` | 1.0 | Reward for successful packet delivery |
| `failurePenalty` | -1.0 | Penalty for failed transmission |
| `hopPenalty` | -0.1 | Penalty per hop (encourages shorter paths) |

## Usage Example

```ned
import inet.node.qlearning.QLearningRouter;

network MyNetwork
{
    submodules:
        node[10]: QLearningRouter {
            qlearn.learningRate = 0.5;
            qlearn.explorationRate = 0.1;
            qlearn.helloInterval = 2s;
        }
}
```

Or configure in omnetpp.ini:
```ini
**.qlearn.learningRate = 0.5
**.qlearn.discountFactor = 0.9
**.qlearn.explorationRate = 0.1
**.qlearn.helloInterval = 2s
```

## Comparison with Traditional Routing

### Advantages
- **Adaptive**: Learns and adapts to network conditions
- **No topology knowledge required**: Learns routes through experience
- **Can optimize for different metrics**: Configurable rewards allow optimization for delay, throughput, etc.
- **Handles dynamic topologies**: Continuously learning enables adaptation to changes

### Disadvantages
- **Convergence time**: Needs time to learn optimal routes
- **Memory overhead**: Must maintain Q-table entries
- **Exploration cost**: Random exploration may lead to suboptimal paths initially
- **Not standardized**: Experimental protocol, not widely deployed

## Statistics Collected

The protocol collects the following statistics:
- `numPacketsSent`: Total packets sent
- `numPacketsReceived`: Total packets received
- `numPacketsForwarded`: Total packets forwarded
- `numPacketsDropped`: Total packets dropped
- `qTableSize`: Current size of Q-table
- `numNeighbors`: Current number of active neighbors

## Implementation Notes

### Q-Table Structure
The Q-table is implemented as a nested map:
```cpp
std::map<Destination, std::map<NextHop, QTableEntry>>
```

Each QTableEntry contains:
- `qValue`: The learned Q-value
- `lastUpdate`: Timestamp of last update
- `hopCount`: Number of hops through this route

### Thread Safety
The implementation is single-threaded (OMNeT++ event-driven) and does not require locking.

### IPv6 Support
Currently supports IPv4. IPv6 support can be added by extending the packet definitions and address handling.

## Future Enhancements

Possible improvements:
1. **Deep Q-Learning**: Use neural networks instead of Q-table
2. **Multi-objective optimization**: Learn Q-values for multiple metrics (delay, energy, etc.)
3. **Cooperative learning**: Share Q-values between nodes
4. **Priority experience replay**: Learn more efficiently from past experiences
5. **IPv6 support**: Extend to support IPv6 addresses

## References

1. Watkins, C.J.C.H. (1989). "Learning from Delayed Rewards". PhD thesis, Cambridge University.
2. Boyan, J. A., & Littman, M. L. (1994). "Packet routing in dynamically changing networks: A reinforcement learning approach".
3. Sutton, R. S., & Barto, A. G. (2018). "Reinforcement Learning: An Introduction" (2nd ed.).

## License

SPDX-License-Identifier: LGPL-3.0-or-later
