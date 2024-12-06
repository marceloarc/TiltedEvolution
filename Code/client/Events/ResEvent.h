#pragma once

struct ResEvent
{
    ResEvent() = delete;
    ResEvent(uint32_t TargetId, uint32_t PlayerId)
        : TargetId(TargetId)
        , PlayerId(PlayerId)
    {
    }

    uint32_t TargetId;
    uint32_t PlayerId;
};
