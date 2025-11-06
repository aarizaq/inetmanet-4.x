# Q-Learning Routing Implementation for INET/inetmanet-4.x

## Summary

This implementation adds a Q-Learning based routing protocol to the INET framework. Q-Learning is a reinforcement learning algorithm that enables network nodes to learn optimal routing paths through experience rather than using predefined metrics.

## What is Q-Learning Routing?

Q-Learning routing applies reinforcement learning principles to network routing:
- Nodes maintain a **Q-table** mapping (destination, next-hop) pairs to quality values
- Routes are selected using an **epsilon-greedy** strategy (exploration vs exploitation)
- Q-values are updated based on routing success/failure using rewards and penalties
- The algorithm learns optimal paths over time through trial and error

## Implementation Components

### 1. Core Protocol Implementation
Located in: `src/inet/routing/qlearning/`

#### Files Created:
- **QLearn.h** - Header file with class definition, Q-table structure, and method declarations
- **QLearn.cc** - Main protocol implementation with Q-learning algorithm
- **QLearn.ned** - NED module definition with configurable parameters
- **QLearningPacket.msg** - Message definitions (Hello, Update packets)
- **QLearningPacketSerializer.h/cc** - Packet serialization for network transmission
- **README.md** - Comprehensive documentation

#### Key Features:
- Q-table data structure for learning route qualities
- Epsilon-greedy route selection (exploration/exploitation)
- Hello message-based neighbor discovery
- Q-value updates based on transmission success/failure
- Configurable learning parameters (alpha, gamma, epsilon)
- Statistics collection for analysis

### 2. Node Module
Located in: `src/inet/node/qlearning/`

#### Files Created:
- **QLearningRouter.ned** - Node definition extending AdhocHost with Q-Learning routing

The QLearningRouter is a complete wireless node with Q-Learning routing integrated.

### 3. Example Configurations
Located in: `examples/qlearning/`

#### Files Created:
- **QLearningNetwork.ned** - Network topologies for testing
- **omnetpp.ini** - Three test configurations:
  - **Static**: 10 stationary nodes testing basic functionality
  - **SlowMobility**: Mobile nodes testing route adaptation
  - **SimpleTest**: 3-node test for verification
- **README.md** - Example documentation and usage guide

### 4. Test Suite
Located in: `tests/module/`

#### Files Created:
- **QLearningSimpleTest.test** - Module test for Q-Learning routing

### 5. Build Configuration
Modified file: `.oppfeatures`

Added two features:
- **QLearning**: The routing protocol itself
- **QLearningExamples**: Example configurations

## Algorithm Details

### Q-Learning Update Equation
```
Q(destination, next_hop) ← Q(destination, next_hop) + 
    α × [reward + γ × max_Q(destination) - Q(destination, next_hop)]
```

Where:
- **α (alpha)**: Learning rate (default: 0.5)
- **γ (gamma)**: Discount factor (default: 0.9)
- **reward**: +1.0 for success, -1.0 for failure, -0.1 per hop

### Route Selection Strategy
**Epsilon-Greedy**:
- With probability **ε** (default: 0.1): Select random neighbor (explore)
- With probability **1-ε** (default: 0.9): Select neighbor with highest Q-value (exploit)

### Protocol Operations

1. **Initialization**: Node starts with empty Q-table
2. **Neighbor Discovery**: Periodic hello messages (default: every 2s)
3. **Route Selection**: Epsilon-greedy selection when forwarding packets
4. **Q-Value Update**: Update Q-values based on transmission outcomes
5. **Aging**: Old Q-values decay if routes are not used
6. **Cleanup**: Remove neighbors not heard from within timeout (default: 10s)

## Configuration Parameters

### Q-Learning Parameters
| Parameter | Default | Range | Description |
|-----------|---------|-------|-------------|
| `learningRate` | 0.5 | 0.0-1.0 | How quickly to learn from new information |
| `discountFactor` | 0.9 | 0.0-1.0 | Importance of future rewards |
| `explorationRate` | 0.1 | 0.0-1.0 | Probability of exploring vs exploiting |
| `initialQValue` | 0.0 | any | Starting Q-value for new routes |

### Protocol Parameters
| Parameter | Default | Description |
|-----------|---------|-------------|
| `udpPort` | 655 | UDP port for Q-learning packets |
| `helloInterval` | 2s | Hello message broadcast interval |
| `routeTimeout` | 10s | Route expiration time |
| `qUpdateInterval` | 5s | Q-value update interval |

### Reward/Penalty Values
| Parameter | Default | Description |
|-----------|---------|-------------|
| `successReward` | 1.0 | Reward for successful delivery |
| `failurePenalty` | -1.0 | Penalty for failed transmission |
| `hopPenalty` | -0.1 | Penalty per hop (favors shorter paths) |

## Usage

### In NED file:
```ned
import inet.node.qlearning.QLearningRouter;

network MyNetwork {
    submodules:
        node[10]: QLearningRouter;
}
```

### In omnetpp.ini:
```ini
**.qlearn.learningRate = 0.5
**.qlearn.discountFactor = 0.9
**.qlearn.explorationRate = 0.1
**.qlearn.helloInterval = 2s
```

## Building and Testing

### Building:
```bash
make makefiles
make MODE=release
```

### Running Examples:
```bash
cd examples/qlearning
../../src/run_inet -u Cmdenv -c Static
../../src/run_inet -u Cmdenv -c SimpleTest
```

### Running Tests:
```bash
cd tests/module
./runtest QLearningSimpleTest
```

## Statistics Collected

The protocol collects these statistics:
- `numPacketsSent` - Total packets sent
- `numPacketsReceived` - Total packets received  
- `numPacketsForwarded` - Total packets forwarded
- `numPacketsDropped` - Total packets dropped
- `qTableSize` - Current Q-table size
- `numNeighbors` - Active neighbor count

## Technical Details

### Data Structures
```cpp
// Q-table: Map of destination -> (next_hop -> Q-value)
std::map<L3Address, std::map<L3Address, QTableEntry>> qTable;

// Neighbor table
std::map<L3Address, NeighborInfo> neighbors;
```

### Packet Types
1. **QLEARN_HELLO** (17 bytes): Neighbor discovery
   - Packet type (1 byte)
   - Source address (4 bytes)
   - Sequence number (4 bytes)
   - Q-value (8 bytes)

2. **QLEARN_UPDATE** (21 bytes): Q-value information
   - Packet type (1 byte)
   - Source address (4 bytes)
   - Destination address (4 bytes)
   - Q-value (8 bytes)
   - Hop count (4 bytes)

## Advantages

1. **Adaptive**: Learns and adapts to changing network conditions
2. **No topology knowledge needed**: Discovers routes through experience
3. **Flexible**: Configurable rewards allow optimization for different metrics
4. **Self-organizing**: No central control or pre-configuration required
5. **Handles mobility**: Continuously updates routes as topology changes

## Limitations

1. **Convergence time**: Needs time to learn optimal routes (exploration phase)
2. **Memory overhead**: Must maintain Q-table entries
3. **Exploration cost**: Random exploration can lead to initial packet loss
4. **Not standardized**: Experimental protocol, not for production use

## Comparison with Traditional Routing

| Aspect | Q-Learning | AODV | DSDV |
|--------|-----------|------|------|
| Type | Learning-based | Reactive | Proactive |
| Route Discovery | Experience | On-demand | Periodic |
| Adaptation | Continuous | On link break | Periodic updates |
| Memory | Q-table | Route cache | Full routing table |
| Overhead | Medium | Low (on-demand) | High (periodic) |
| Convergence | Gradual | Fast | Fast |

## Future Enhancements

Potential improvements:
1. **Deep Q-Learning**: Replace Q-table with neural network
2. **Multi-objective**: Learn for multiple metrics simultaneously
3. **Cooperative learning**: Share learned knowledge between nodes
4. **Priority replay**: Learn more efficiently from past experiences
5. **IPv6 support**: Extend to IPv6 addressing
6. **Energy awareness**: Include energy consumption in Q-values
7. **QoS support**: Different Q-tables for different traffic classes

## References

1. Watkins, C.J.C.H. (1989). "Learning from Delayed Rewards"
2. Boyan, J.A. & Littman, M.L. (1994). "Packet routing in dynamically changing networks"
3. Sutton & Barto (2018). "Reinforcement Learning: An Introduction"

## File Structure

```
inetmanet-4.x/
├── .oppfeatures                              # Build configuration
├── src/inet/
│   ├── routing/qlearning/                    # Protocol implementation
│   │   ├── QLearn.h
│   │   ├── QLearn.cc
│   │   ├── QLearn.ned
│   │   ├── QLearningPacket.msg
│   │   ├── QLearningPacketSerializer.h
│   │   ├── QLearningPacketSerializer.cc
│   │   └── README.md
│   └── node/qlearning/                       # Node module
│       └── QLearningRouter.ned
├── examples/qlearning/                       # Example configurations
│   ├── QLearningNetwork.ned
│   ├── omnetpp.ini
│   └── README.md
├── tests/module/                             # Tests
│   └── QLearningSimpleTest.test
└── Q-LEARNING-IMPLEMENTATION.md             # This file
```

## License

SPDX-License-Identifier: LGPL-3.0-or-later

## Authors

Implementation for INET/inetmanet-4.x framework (2024)
