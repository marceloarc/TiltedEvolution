#include <Messages/ResRequest.h>

void ResRequest::SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept
{
    Serialization::WriteVarInt(aWriter, TargetId);
    Serialization::WriteVarInt(aWriter, PlayerId);
}

void ResRequest::DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept
{
    ClientMessage::DeserializeRaw(aReader);

    TargetId = Serialization::ReadVarInt(aReader) & 0xFFFFFFFF;
    PlayerId = Serialization::ReadVarInt(aReader) & 0xFFFFFFFF;
}
