#include "Conn3gram.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <fstream>
#include <limits>

#include "exception.h"
#include "file_utils.h"

namespace dict {
    DEFINE_CLASS_LOGGER(Conn3gram);

    namespace {
        constexpr std::array<char, 8> Magic = { 'D', 'Y', 'M', 'Z', 'C', '3', 'G', '1' };
        constexpr uint32_t Version = 1;
        constexpr int SpecialPenaltyCost = 5000;
        constexpr int MissingKeyCost = 10000;

#pragma pack(push, 1)
        struct FileHeader {
            char magic[8];
            uint32_t version;
            uint32_t idSize;
            uint64_t entryCount;
            uint64_t offsetCount;
        };
#pragma pack(pop)

        static_assert(sizeof(FileHeader) == 32);

        template<class T>
        void readExact(std::ifstream& input, T* data, size_t count, StringRef filepath, StringRef section) {
            input.read(reinterpret_cast<char*>(data), static_cast<std::streamsize>(sizeof(T) * count));
            if (!input) {
                throw util::RuntimeException(std::format(L"Conn3gram: cannot read {} from {}", section, filepath), __FILE__, __LINE__);
            }
        }
    }

    Conn3gram::Conn3gram(StringRef filepath) {
        load(filepath);
    }

    uint16_t Conn3gram::normalizeId(uint16_t id) {
        return id == 1317 ? 1310 : id;
    }

    bool Conn3gram::isPenaltyId(uint16_t id) {
        return id == 1316 || id == 1318;
    }

    void Conn3gram::load(StringRef filepath) {
        LOG_INFOH(L"ENTER: filepath={}", filepath);
        std::ifstream input(filepath, std::ios::binary);
        if (!input) {
            LOG_ERROR_AND_THROW_RTE(L"Conn3gram: cannot open {}", filepath);
        }

        FileHeader header{};
        readExact(input, &header, 1, filepath, L"header");

        if (!std::equal(Magic.begin(), Magic.end(), header.magic)) {
            LOG_ERROR_AND_THROW_RTE(L"Conn3gram: invalid magic: {}", filepath);
        }
        if (header.version != Version) {
            LOG_ERROR_AND_THROW_RTE(L"Conn3gram: invalid version {}: {}", header.version, filepath);
        }
        const uint64_t expectedOffsetCount = static_cast<uint64_t>(header.idSize) * header.idSize + 1;
        if (header.idSize == 0 || header.offsetCount != expectedOffsetCount) {
            LOG_ERROR_AND_THROW_RTE(L"Conn3gram: invalid header: {}", filepath);
        }
        if (header.entryCount > static_cast<uint64_t>(std::numeric_limits<size_t>::max()) ||
            header.offsetCount > static_cast<uint64_t>(std::numeric_limits<size_t>::max())) {
            LOG_ERROR_AND_THROW_RTE(L"Conn3gram: too large: {}", filepath);
        }

        Vector<uint32_t> offsets(static_cast<size_t>(header.offsetCount));
        Vector<Entry> entries(static_cast<size_t>(header.entryCount));
        readExact(input, offsets.data(), offsets.size(), filepath, L"offsets");
        readExact(input, entries.data(), entries.size(), filepath, L"entries");

        uint32_t prev = 0;
        for (size_t i = 0; i < offsets.size(); ++i) {
            uint32_t current = offsets[i];
            if (current < prev || current > header.entryCount) {
                LOG_ERROR_AND_THROW_RTE(L"Conn3gram: invalid offset at {}: {}", i, filepath);
            }
            prev = current;
        }
        if (offsets.back() != header.entryCount) {
            LOG_ERROR_AND_THROW_RTE(L"Conn3gram: last offset does not match entry count: {}", filepath);
        }

        idSize_ = header.idSize;
        entryCount_ = header.entryCount;
        offsetCount_ = header.offsetCount;
        offsets_.swap(offsets);
        entries_.swap(entries);
        LOG_INFOH(L"LEAVE: idSize={}, entryCount={}", idSize_, entryCount_);
    }

    bool Conn3gram::loaded() const {
        return idSize_ > 0 && !offsets_.empty();
    }

    int Conn3gram::cost(uint16_t x1, uint16_t x2, uint16_t x3) const {
        if (!loaded()) return 0;
        if (isPenaltyId(x1) || isPenaltyId(x2) || isPenaltyId(x3)) return SpecialPenaltyCost;

        x1 = normalizeId(x1);
        x2 = normalizeId(x2);
        x3 = normalizeId(x3);

        if (x1 >= idSize_ || x2 >= idSize_ || x3 >= idSize_) return MissingKeyCost;

        const uint64_t context = static_cast<uint64_t>(x1) * idSize_ + x2;
        const uint32_t begin = offsets_[static_cast<size_t>(context)];
        const uint32_t end = offsets_[static_cast<size_t>(context + 1)];
        const auto first = entries_.begin() + begin;
        const auto last = entries_.begin() + end;
        const auto iter = std::lower_bound(first, last, x3, [](const Entry& entry, uint16_t next) {
            return entry.next < next;
        });

        if (iter == last || iter->next != x3) return MissingKeyCost;
        return iter->cost;
    }

} // namespace dict
