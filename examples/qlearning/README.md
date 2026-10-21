# Q-Learning Routing Protocol Example

This example demonstrates the Q-Learning routing protocol implementation in INET.

## Overview

Q-Learning is a reinforcement learning based routing protocol that learns optimal routes through experience. Each node maintains a Q-table that stores quality values for each destination through each neighbor. The protocol selects routes based on these Q-values and updates them based on the success/failure of packet transmissions.

## Key Features

- **Adaptive Routing**: Routes adapt based on network conditions and past experience
- **Reinforcement Learning**: Uses Q-learning algorithm to learn optimal paths
- **Exploration vs Exploitation**: Balances trying new routes vs using known good routes
- **Neighbor Discovery**: Periodic hello messages for maintaining neighbor information

## Q-Learning Parameters

- **learningRate (alpha)**: Controls how much new information overrides old information (0-1)
- **discountFactor (gamma)**: Importance of future rewards (0-1)
- **explorationRate (epsilon)**: Probability of exploring new routes vs exploiting known routes (0-1)
- **successReward**: Reward value for successful packet transmission
- **failurePenalty**: Penalty value for failed transmission
- **hopPenalty**: Penalty per hop (encourages shorter paths)

## Configurations

### Static
Basic Q-learning test with 10 stationary nodes. Node 0 pings node 9.

### SlowMobility
Q-learning with mobile nodes moving at low speed to test route adaptation.

### SimpleTest
Simple 3-node test (sender-relay-receiver) for basic functionality verification.

## Running the Examples

1. Build the INET framework:
   ```
   make makefiles
   make
   ```

2. Run a configuration:
   ```
   cd examples/qlearning
   ../../src/run_inet -u Cmdenv -c Static
   ```

## How It Works

1. **Neighbor Discovery**: Nodes periodically broadcast hello messages to discover neighbors
2. **Route Learning**: When forwarding packets, nodes update Q-values based on success/failure
3. **Route Selection**: For each packet, select next hop using epsilon-greedy strategy:
   - With probability epsilon: explore by choosing random neighbor
   - With probability 1-epsilon: exploit by choosing neighbor with highest Q-value
4. **Q-Value Updates**: Update Q-values using the formula:
   ```
   Q(s,a) = Q(s,a) + α * [R + γ * max(Q(s',a')) - Q(s,a)]
   ```
   where:
   - Q(s,a) = Q-value for state s and action a
   - α = learning rate
   - R = reward
   - γ = discount factor
   - max(Q(s',a')) = maximum Q-value for next state

## Expected Behavior

- Initially, nodes explore various routes as Q-values are not established
- Over time, nodes converge to better routes as Q-values improve
- The protocol adapts to topology changes by updating Q-values
- Statistics show learning progress through packet success rates
