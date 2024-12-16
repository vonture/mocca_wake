#include "persistent_data.hpp"

namespace mocca {
    namespace {
        uint32_t compute_peristen_data_crc(const persistent_data* data) {
            constexpr uint32_t crc_table[16] = {
                0x00000000, 0x1db71064, 0x3b6e20c8, 0x26d930ac, 0x76dc4190, 0x6b6b51f4, 0x4db26158, 0x5005713c,
                0xedb88320, 0xf00f9344, 0xd6d6a3e8, 0xcb61b38c, 0x9b64c2b0, 0x86d3d2d4, 0xa00ae278, 0xbdbdf21c,
            };

            const uint8_t* raw_data = reinterpret_cast<const uint8_t*>(data);

            uint32_t crc = ~0L;
            for (const uint8_t* byte = raw_data + sizeof(uint32_t); byte < raw_data + sizeof(persistent_data); byte++) {
                crc = crc_table[(crc ^ (*byte)) & 0x0f] ^ (crc >> 4);
                crc = crc_table[(crc ^ ((*byte) >> 4)) & 0x0f] ^ (crc >> 4);
                crc = ~crc;
            }

            return crc;
        }
    } // namespace

    bool persistent_data::crc_is_valid() const {
        return compute_peristen_data_crc(this) == crc;
    }
    void persistent_data::update_crc() {
        crc = compute_peristen_data_crc(this);
    }
} // namespace mocca
