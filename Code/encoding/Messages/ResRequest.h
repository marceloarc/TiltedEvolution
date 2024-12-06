#pragma once

#include "Message.h"

struct ResRequest final : ClientMessage
{
    static constexpr ClientOpcode Opcode = kResRequest;

    ResRequest()
        : ClientMessage(Opcode)
    {
    }

    void SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept override;
    void DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept override;

    bool operator==(const ResRequest& acRhs) const noexcept { return GetOpcode() == acRhs.GetOpcode() && TargetId == acRhs.TargetId && PlayerId == acRhs.PlayerId; }

    uint32_t TargetId;
    uint32_t PlayerId;
};
