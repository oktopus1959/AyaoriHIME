#pragma once

#include <cstdint>

#include "std_utils.h"
#include "Logger.h"

namespace dict {

    class Conn3gram {
        DECLARE_CLASS_LOGGER;

    private:
        struct Entry {
            uint16_t next = 0;
            int16_t cost = 0;
        };

        uint32_t idSize_ = 0;
        uint64_t entryCount_ = 0;
        uint64_t offsetCount_ = 0;
        Vector<uint32_t> offsets_;
        Vector<Entry> entries_;

        static uint16_t normalizeId(uint16_t id);
        static bool isPenaltyId(uint16_t id);

    public:
        Conn3gram() = default;
        explicit Conn3gram(StringRef filepath);

        void load(StringRef filepath);
        bool loaded() const;
        int cost(uint16_t x1, uint16_t x2, uint16_t x3) const;

        uint32_t idSize() const { return idSize_; }
        uint64_t entryCount() const { return entryCount_; }
    };

} // namespace dict

using Conn3gramPtr = SharedPtr<dict::Conn3gram>;
