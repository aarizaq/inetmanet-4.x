//
// Copyright (C) 2024
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/routing/qlearning/QLearningPacketSerializer.h"

#include "inet/common/packet/serializer/ChunkSerializerRegistry.h"
#include "inet/routing/qlearning/QLearningPacket_m.h"

namespace inet {

Register_Serializer(QLearningHello, QLearningHelloSerializer);
Register_Serializer(QLearningUpdate, QLearningUpdateSerializer);

void QLearningHelloSerializer::serialize(MemoryOutputStream& stream, const Ptr<const Chunk>& chunk) const
{
    const auto& hello = staticPtrCast<const QLearningHello>(chunk);
    stream.writeUint8(hello->getPacketType());
    stream.writeIpv4Address(hello->getSourceAddr());
    stream.writeUint32Be(hello->getSequenceNumber());
    stream.writeDouble(hello->getQValue());
}

const Ptr<Chunk> QLearningHelloSerializer::deserialize(MemoryInputStream& stream) const
{
    auto hello = makeShared<QLearningHello>();
    hello->setPacketType((QLearningPacketType)stream.readUint8());
    hello->setSourceAddr(stream.readIpv4Address());
    hello->setSequenceNumber(stream.readUint32Be());
    hello->setQValue(stream.readDouble());
    hello->setChunkLength(B(17));  // 1 + 4 + 4 + 8 = 17 bytes
    return hello;
}

void QLearningUpdateSerializer::serialize(MemoryOutputStream& stream, const Ptr<const Chunk>& chunk) const
{
    const auto& update = staticPtrCast<const QLearningUpdate>(chunk);
    stream.writeUint8(update->getPacketType());
    stream.writeIpv4Address(update->getSourceAddr());
    stream.writeIpv4Address(update->getDestAddr());
    stream.writeDouble(update->getQValue());
    stream.writeUint32Be(update->getHopCount());
}

const Ptr<Chunk> QLearningUpdateSerializer::deserialize(MemoryInputStream& stream) const
{
    auto update = makeShared<QLearningUpdate>();
    update->setPacketType((QLearningPacketType)stream.readUint8());
    update->setSourceAddr(stream.readIpv4Address());
    update->setDestAddr(stream.readIpv4Address());
    update->setQValue(stream.readDouble());
    update->setHopCount(stream.readUint32Be());
    update->setChunkLength(B(21));  // 1 + 4 + 4 + 8 + 4 = 21 bytes
    return update;
}

} // namespace inet
