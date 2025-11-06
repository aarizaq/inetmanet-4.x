//
// Copyright (C) 2024
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_QLEARNINGPACKETSERIALIZER_H
#define __INET_QLEARNINGPACKETSERIALIZER_H

#include "inet/common/packet/serializer/FieldsChunkSerializer.h"

namespace inet {

/**
 * Converts between QLearningHello and binary network packet.
 */
class INET_API QLearningHelloSerializer : public FieldsChunkSerializer
{
  protected:
    virtual void serialize(MemoryOutputStream& stream, const Ptr<const Chunk>& chunk) const override;
    virtual const Ptr<Chunk> deserialize(MemoryInputStream& stream) const override;

  public:
    QLearningHelloSerializer() : FieldsChunkSerializer() {}
};

/**
 * Converts between QLearningUpdate and binary network packet.
 */
class INET_API QLearningUpdateSerializer : public FieldsChunkSerializer
{
  protected:
    virtual void serialize(MemoryOutputStream& stream, const Ptr<const Chunk>& chunk) const override;
    virtual const Ptr<Chunk> deserialize(MemoryInputStream& stream) const override;

  public:
    QLearningUpdateSerializer() : FieldsChunkSerializer() {}
};

} // namespace inet

#endif
