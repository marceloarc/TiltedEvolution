#include <Structs/Vector3_NetQuantize.h>
#include <TiltedCore/Serialization.hpp>

using TiltedPhoques::Serialization;

bool Vector3_NetQuantize::operator==(const Vector3_NetQuantize& acRhs) const noexcept
{
    return Pack() == acRhs.Pack();
}

bool Vector3_NetQuantize::operator!=(const Vector3_NetQuantize& acRhs) const noexcept
{
    return !this->operator==(acRhs);
}

Vector3_NetQuantize& Vector3_NetQuantize::operator=(const glm::vec3& acRhs) noexcept
{
    glm::vec3::operator=(acRhs);
    return *this;
}

void Vector3_NetQuantize::Serialize(TiltedPhoques::Buffer::Writer& aWriter) const noexcept
{
    // Empacota os dados
    auto [dataLow, dataHigh] = Pack();

    // Escreve os primeiros 64 bits
    aWriter.WriteBits(dataLow, 64);

    // Escreve os 32 bits restantes
    aWriter.WriteBits(dataHigh, 32);
}

void Vector3_NetQuantize::Deserialize(TiltedPhoques::Buffer::Reader& aReader) noexcept
{
    uint64_t dataLow = 0;
    uint32_t dataHigh = 0;

    // Lê os primeiros 64 bits
    aReader.ReadBits(dataLow, 64);

    // Lê os 32 bits restantes usando uma variável temporária de 64 bits
    uint64_t tempHigh = 0;
    aReader.ReadBits(tempHigh, 32);
    dataHigh = static_cast<uint32_t>(tempHigh); // Converte para uint32_t

    // Desempacota os dados
    Unpack(dataLow, dataHigh);
}

void Vector3_NetQuantize::Unpack(uint64_t dataLow, uint32_t dataHigh) noexcept
{
    const float scale = 1000.0f;

    // Extrai os sinais
    int32_t xSign = (dataLow & 1) != 0;
    int32_t ySign = (dataLow & 2) != 0;
    int32_t zSign = (dataLow & 4) != 0;

    // Extrai os valores
    auto xValue = static_cast<float>((dataLow >> 3) & ((1ULL << 32) - 1));
    auto yValue = static_cast<float>((dataLow >> 35) & ((1ULL << 32) - 1));
    auto zValue = static_cast<float>(dataHigh & ((1ULL << 32) - 1));

    // Aplica os sinais e escala
    x = xSign ? -(xValue / scale) : (xValue / scale);
    y = ySign ? -(yValue / scale) : (yValue / scale);
    z = zSign ? -(zValue / scale) : (zValue / scale);
}
std::pair<uint64_t, uint32_t> Vector3_NetQuantize::Pack() const noexcept
{
    uint64_t dataLow = 0;
    uint32_t dataHigh = 0;

    const float scale = 1000.0f; // Escala para precisão decimal

    int32_t ix = static_cast<int32_t>(x * scale);
    int32_t iy = static_cast<int32_t>(y * scale);
    int32_t iz = static_cast<int32_t>(z * scale);

    // Armazena os sinais nos bits mais baixos
    dataLow |= (ix < 0) ? 1 : 0;
    dataLow |= (iy < 0) ? 2 : 0;
    dataLow |= (iz < 0) ? 4 : 0;

    // Extrai os valores absolutos e aplica as máscaras
    const uint64_t xVal = std::abs(ix) & ((1ULL << 32) - 1); // 32 bits
    const uint64_t yVal = std::abs(iy) & ((1ULL << 32) - 1); // 32 bits
    const uint64_t zVal = std::abs(iz) & ((1ULL << 32) - 1); // 32 bits

    // Empacota x e y em dataLow
    dataLow |= xVal << 3;  // Bits 3 a 34
    dataLow |= yVal << 35; // Bits 35 a 66

    // Armazena z em dataHigh
    dataHigh = static_cast<uint32_t>(zVal); // Bits 67 a 98

    return {dataLow, dataHigh};
}
