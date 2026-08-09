#pragma once

#include <cstdint>

#include "std_utils.h"
#include "Logger.h"

namespace dict {

    class Char4gram {
        DECLARE_CLASS_LOGGER;

    public:
        struct ScoreResult {
            String normalized;
            int targetWindowCount = 0;
            int matchedWindowCount = 0;
            int64_t costSum = 0;
            int averageCost = 0;
        };

    private:
#pragma pack(push, 1)
        struct Context {
            uint16_t first = 0;
            uint16_t second = 0;
            uint16_t third = 0;
            uint16_t reserved = 0;
            uint32_t entryBegin = 0;
            uint32_t entryCount = 0;
            int32_t missingCost = 0;
        };

        struct Entry {
            uint16_t next = 0;
            uint16_t reserved = 0;
            int32_t cost = 0;
        };
#pragma pack(pop)

        static_assert(sizeof(Context) == 20);
        static_assert(sizeof(Entry) == 8);

        Vector<wchar_t> characters_;
        Vector<Context> contexts_;
        Vector<Entry> entries_;
        int32_t uniformCost_ = 0;

        int findCharacterId(wchar_t ch) const;
        int cost(wchar_t first, wchar_t second, wchar_t third, wchar_t next, bool& matched) const;

    public:
        Char4gram() = default;
        explicit Char4gram(StringRef filepath);

        void load(StringRef filepath);
        bool loaded() const;
        ScoreResult score(StringRef sentence) const;

        static void build(StringRef inputPath, StringRef outputPath);
    };

} // namespace dict

using Char4gramPtr = SharedPtr<dict::Char4gram>;
