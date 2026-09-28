//
// Copyright (C) 2024
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

import inet.common.INETDefs;
import inet.common.packet.chunk.Chunk;
import inet.networklayer.contract.ipv4.Ipv4Address;

namespace inet;

//
// Q-Learning routing packet types
//
enum QLearningPacketType
{
    QLEARN_HELLO = 1;      // Hello/neighbor discovery
    QLEARN_DATA = 2;       // Data packet with Q-learning info
    QLEARN_UPDATE = 3;     // Q-value update message
}

//
// Q-Learning Hello packet
//
class QLearningHello extends FieldsChunk
{
    QLearningPacketType packetType = QLEARN_HELLO;
    Ipv4Address sourceAddr;
    unsigned int sequenceNumber;
    double qValue = 0.0;  // Q-value information for neighbors
}

//
// Q-Learning Update packet
//
class QLearningUpdate extends FieldsChunk
{
    QLearningPacketType packetType = QLEARN_UPDATE;
    Ipv4Address sourceAddr;
    Ipv4Address destAddr;
    double qValue;
    unsigned int hopCount;
}
