#pragma once

#include "Message.h"

struct NotifyRes final : ServerMessage
{
    static constexpr ServerOpcode Opcode = kNotifyRes;

    NotifyRes()
        : ServerMessage(Opcode)
    {
    }

    void SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept override;
    void DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept override;

    bool operator==(const NotifyRes& acRhs) const noexcept { return GetOpcode() == acRhs.GetOpcode() && TargetId == acRhs.TargetId && PlayerId == acRhs.PlayerId; }

    uint32_t TargetId;
    uint32_t PlayerId;
};
