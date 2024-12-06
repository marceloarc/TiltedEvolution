#include <Messages/NotifyRes.h>

void NotifyRes::SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept
{
    Serialization::WriteVarInt(aWriter, TargetId);
    Serialization::WriteVarInt(aWriter, PlayerId);
}

void NotifyRes::DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept
{
    ServerMessage::DeserializeRaw(aReader);

    TargetId = Serialization::ReadVarInt(aReader) & 0xFFFFFFFF;
    PlayerId = Serialization::ReadVarInt(aReader) & 0xFFFFFFFF;
}
