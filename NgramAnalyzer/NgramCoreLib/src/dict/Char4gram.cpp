#include "Char4gram.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cmath>
#include <cwchar>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <limits>
#include <set>
#include <unordered_map>

#include "constants/Constants.h"
#include "exception.h"
#include "file_utils.h"
#include "string_utils.h"

#include "NgramDebugLog.h"

namespace dict {
    DEFINE_CLASS_LOGGER(Char4gram);

    namespace {
        constexpr std::array<char, 8> Magic = { 'N', 'G', 'C', '4', 'G', '0', '0', '1' };
        constexpr uint32_t Version = 1;
        constexpr double Alpha = 0.1;
        constexpr double CostScale = 1000.0;

#pragma pack(push, 1)
        struct FileHeader {
            char magic[8];
            uint32_t version;
            uint32_t headerSize;
            uint32_t characterCount;
            uint32_t contextCount;
            uint64_t entryCount;
            int32_t uniformCost;
            uint32_t reserved;
        };
#pragma pack(pop)

        static_assert(sizeof(FileHeader) == 40);

        constexpr wchar_t DigitBlockChar = L'■';
        constexpr wchar_t WideTildeChar = L'～';

        struct SourceRow {
            wchar_t first = 0;
            wchar_t second = 0;
            wchar_t third = 0;
            wchar_t next = 0;
            uint64_t count = 0;
        };

        template<class T>
        void readExact(std::ifstream& input, T* data, size_t count, StringRef filepath, StringRef section) {
            if (count == 0) return;
            input.read(reinterpret_cast<char*>(data), static_cast<std::streamsize>(sizeof(T) * count));
            if (!input) {
                throw util::RuntimeException(std::format(L"Char4gram: cannot read {} from {}", section, filepath), __FILE__, __LINE__);
            }
        }

        template<class T>
        void writeExact(std::ofstream& output, const T* data, size_t count, StringRef filepath, StringRef section) {
            if (count == 0) return;
            output.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(sizeof(T) * count));
            if (!output) {
                throw util::RuntimeException(std::format(L"Char4gram: cannot write {} to {}", section, filepath), __FILE__, __LINE__);
            }
        }

        uint64_t quadgramKey(wchar_t first, wchar_t second, wchar_t third, wchar_t next) {
            return (static_cast<uint64_t>(static_cast<uint16_t>(first)) << 48) |
                (static_cast<uint64_t>(static_cast<uint16_t>(second)) << 32) |
                (static_cast<uint64_t>(static_cast<uint16_t>(third)) << 16) |
                static_cast<uint16_t>(next);
        }

        uint64_t contextKey(wchar_t first, wchar_t second, wchar_t third) {
            return (static_cast<uint64_t>(static_cast<uint16_t>(first)) << 32) |
                (static_cast<uint64_t>(static_cast<uint16_t>(second)) << 16) |
                static_cast<uint16_t>(third);
        }

        uint64_t contextKey(uint16_t first, uint16_t second, uint16_t third) {
            return (static_cast<uint64_t>(first) << 32) |
                (static_cast<uint64_t>(second) << 16) |
                third;
        }

        int32_t probabilityCost(double probability) {
            const double value = std::round(-std::log(probability) * CostScale);
            if (value < 0.0 || value > static_cast<double>(std::numeric_limits<int32_t>::max())) {
                throw util::RuntimeException(L"Char4gram: calculated cost is out of range", __FILE__, __LINE__);
            }
            return static_cast<int32_t>(value);
        }

        uint64_t parseCount(StringRef value, uint64_t lineNumber) {
            if (value.empty() || std::any_of(value.begin(), value.end(), [](wchar_t ch) { return ch < L'0' || ch > L'9'; })) {
                throw util::RuntimeException(std::format(L"Char4gram: invalid count at line {}", lineNumber), __FILE__, __LINE__);
            }
            errno = 0;
            wchar_t* end = nullptr;
            const uint64_t count = std::wcstoull(value.c_str(), &end, 10);
            if (errno == ERANGE || end != value.c_str() + value.size() || count == 0) {
                throw util::RuntimeException(std::format(L"Char4gram: invalid count at line {}", lineNumber), __FILE__, __LINE__);
            }
            return count;
        }

        bool isChar4gramDigit(wchar_t ch) {
            return (ch >= L'0' && ch <= L'9') || (ch >= L'０' && ch <= L'９');
        }

        bool isChar4gramAllowed(wchar_t ch) {
            return utils::is_hiragana(ch) ||
                ch == DigitBlockChar ||
                ch == CHOON ||
                ch == WideTildeChar ||
                ch == NAKAGURO ||
                ch == 0x3005 /* 々 */ ||
                ch == TOTEN ||
                ch == GETA_CHAR;
        }

        bool isChar4gramAllowedOnly(StringRef gram) {
            return std::all_of(gram.begin(), gram.end(), isChar4gramAllowed);
        }

        void appendGetaRun(String& normalized, bool& inOtherRun) {
            if (!inOtherRun) {
                normalized.push_back(GETA_CHAR);
                normalized.push_back(GETA_CHAR);
                inOtherRun = true;
            }
        }
    }

    Char4gram::Char4gram(StringRef filepath) {
        load(filepath);
    }

    void Char4gram::load(StringRef filepath) {
        LOG_INFOH(L"ENTER: filepath={}", filepath);

        std::ifstream input(filepath, std::ios::binary);
        if (!input) {
            LOG_ERROR_AND_THROW_RTE(L"Char4gram: cannot open {}", filepath);
        }

        FileHeader header{};
        readExact(input, &header, 1, filepath, L"header");
        if (!std::equal(Magic.begin(), Magic.end(), header.magic)) {
            LOG_ERROR_AND_THROW_RTE(L"Char4gram: invalid magic: {}", filepath);
        }
        if (header.version != Version || header.headerSize != sizeof(FileHeader)) {
            LOG_ERROR_AND_THROW_RTE(L"Char4gram: unsupported format: {}", filepath);
        }
        if (header.characterCount == 0 || header.characterCount > std::numeric_limits<uint16_t>::max() ||
            header.contextCount == 0 || header.entryCount == 0 || header.reserved != 0 ||
            header.entryCount > std::numeric_limits<uint32_t>::max() ||
            header.entryCount > static_cast<uint64_t>(std::numeric_limits<size_t>::max()) ||
            header.uniformCost < 0) {
            LOG_ERROR_AND_THROW_RTE(L"Char4gram: invalid header: {}", filepath);
        }

        const uint64_t expectedSize = sizeof(FileHeader) +
            static_cast<uint64_t>(header.characterCount) * sizeof(wchar_t) +
            static_cast<uint64_t>(header.contextCount) * sizeof(Context) +
            header.entryCount * sizeof(Entry);
        std::error_code ec;
        const uint64_t actualSize = std::filesystem::file_size(filepath, ec);
        if (ec || actualSize != expectedSize) {
            LOG_ERROR_AND_THROW_RTE(L"Char4gram: invalid file size: {}", filepath);
        }

        Vector<wchar_t> characters(header.characterCount);
        Vector<Context> contexts(header.contextCount);
        Vector<Entry> entries(static_cast<size_t>(header.entryCount));
        readExact(input, characters.data(), characters.size(), filepath, L"characters");
        readExact(input, contexts.data(), contexts.size(), filepath, L"contexts");
        readExact(input, entries.data(), entries.size(), filepath, L"entries");

        if (!std::is_sorted(characters.begin(), characters.end()) ||
            std::adjacent_find(characters.begin(), characters.end()) != characters.end()) {
            LOG_ERROR_AND_THROW_RTE(L"Char4gram: character table is not strictly sorted: {}", filepath);
        }

        uint32_t expectedEntryBegin = 0;
        uint64_t previousContextKey = 0;
        bool hasPreviousContext = false;
        for (const auto& context : contexts) {
            if (context.first >= characters.size() || context.second >= characters.size() ||
                context.third >= characters.size() || context.reserved != 0 ||
                context.entryBegin != expectedEntryBegin || context.entryCount == 0 ||
                context.entryCount > entries.size() - context.entryBegin || context.missingCost < 0) {
                LOG_ERROR_AND_THROW_RTE(L"Char4gram: invalid context table: {}", filepath);
            }
            const uint64_t key = contextKey(context.first, context.second, context.third);
            if (hasPreviousContext && key <= previousContextKey) {
                LOG_ERROR_AND_THROW_RTE(L"Char4gram: context table is not strictly sorted: {}", filepath);
            }
            hasPreviousContext = true;
            previousContextKey = key;

            uint16_t previousNext = 0;
            bool hasPreviousNext = false;
            for (uint32_t i = 0; i < context.entryCount; ++i) {
                const auto& entry = entries[context.entryBegin + i];
                if (entry.next >= characters.size() || entry.reserved != 0 || entry.cost < 0 ||
                    (hasPreviousNext && entry.next <= previousNext)) {
                    LOG_ERROR_AND_THROW_RTE(L"Char4gram: invalid entry table: {}", filepath);
                }
                hasPreviousNext = true;
                previousNext = entry.next;
            }
            expectedEntryBegin += context.entryCount;
        }
        if (expectedEntryBegin != entries.size()) {
            LOG_ERROR_AND_THROW_RTE(L"Char4gram: entry count mismatch: {}", filepath);
        }

        characters_.swap(characters);
        contexts_.swap(contexts);
        entries_.swap(entries);
        uniformCost_ = header.uniformCost;
        LOG_INFOH(L"LEAVE: characters={}, contexts={}, entries={}, uniformCost={}",
            characters_.size(), contexts_.size(), entries_.size(), uniformCost_);
    }

    bool Char4gram::loaded() const {
        return !characters_.empty() && !contexts_.empty() && !entries_.empty();
    }

    int Char4gram::findCharacterId(wchar_t ch) const {
        const auto it = std::lower_bound(characters_.begin(), characters_.end(), ch);
        return it == characters_.end() || *it != ch ? -1 : static_cast<int>(it - characters_.begin());
    }

    int Char4gram::cost(wchar_t first, wchar_t second, wchar_t third, wchar_t next, bool& matched) const {
        matched = false;
        const int firstId = findCharacterId(first);
        const int secondId = findCharacterId(second);
        const int thirdId = findCharacterId(third);
        if (firstId < 0 || secondId < 0 || thirdId < 0) return uniformCost_;

        const uint64_t key = contextKey(
            static_cast<uint16_t>(firstId),
            static_cast<uint16_t>(secondId),
            static_cast<uint16_t>(thirdId));
        const auto contextIt = std::lower_bound(contexts_.begin(), contexts_.end(), key, [](const Context& context, uint64_t value) {
            return contextKey(context.first, context.second, context.third) < value;
        });
        if (contextIt == contexts_.end() ||
            contextKey(contextIt->first, contextIt->second, contextIt->third) != key) {
            return uniformCost_;
        }

        const int nextId = findCharacterId(next);
        if (nextId < 0) return contextIt->missingCost;
        const auto firstEntry = entries_.begin() + contextIt->entryBegin;
        const auto lastEntry = firstEntry + contextIt->entryCount;
        const auto entryIt = std::lower_bound(firstEntry, lastEntry, static_cast<uint16_t>(nextId), [](const Entry& entry, uint16_t value) {
            return entry.next < value;
        });
        if (entryIt == lastEntry || entryIt->next != nextId) return contextIt->missingCost;

        matched = true;
        return entryIt->cost;
    }

    Char4gram::ScoreResult Char4gram::score(StringRef sentence) const {
        ScoreResult result;
        if (!loaded()) return result;

        result.normalized.reserve(sentence.size() + 1);
        bool inOtherRun = false;
        for (size_t i = 0; i <= sentence.size(); ++i) {
            const wchar_t ch = i == 0 ? GETA_CHAR : sentence[i - 1];
            if (std::iswspace(ch)) {
                continue;
            }
            if (isChar4gramDigit(ch)) {
                result.normalized.push_back(DigitBlockChar);
                inOtherRun = false;
                while (i < sentence.size() && isChar4gramDigit(sentence[i])) {
                    ++i;
                }
            } else if (isChar4gramAllowed(ch)) {
                result.normalized.push_back(ch);
                inOtherRun = false;
            } else {
                appendGetaRun(result.normalized, inOtherRun);
                const size_t sentenceIndex = i - 1;
                if (i > 0 && is_high_surrogate(ch) && sentenceIndex + 1 < sentence.size() && is_low_surrogate(sentence[sentenceIndex + 1])) {
                    ++i;
                }
            }
        }

        for (size_t i = 0; i + 3 < result.normalized.size(); ++i) {
            const StringRef normalized = result.normalized;
            bool matched = false;
            result.costSum += cost(normalized[i], normalized[i + 1], normalized[i + 2], normalized[i + 3], matched);
            ++result.targetWindowCount;
            if (matched) {
                ++result.matchedWindowCount;
            }
        }
        LOG_DEBUGH(L"targetWindowCount={}, matchedWindowCount={}, costSum={}",
            result.targetWindowCount, result.matchedWindowCount, result.costSum);
        if (result.targetWindowCount > 0) {
            result.averageCost = static_cast<int>(std::llround(
                static_cast<double>(result.costSum) / result.targetWindowCount));
        }
        return result;
    }

    void Char4gram::build(StringRef inputPath, StringRef outputPath) {
        LOG_INFOH(L"ENTER: input={}, output={}", inputPath, outputPath);
        utils::IfstreamReader reader(inputPath);
        if (!reader.success()) {
            LOG_ERROR_AND_THROW_RTE(L"Char4gram: cannot open input {}", inputPath);
        }

        std::unordered_map<uint64_t, uint64_t> counts;
        std::unordered_map<uint64_t, uint64_t> contextTotals;
        std::set<wchar_t> characterSet;
        uint64_t lineNumber = 0;
        while (true) {
            auto [line, eof] = reader.getLine();
            if (eof) break;
            ++lineNumber;

            const size_t tab = line.find(L'\t');
            if (tab == String::npos || line.find(L'\t', tab + 1) != String::npos) {
                LOG_ERROR_AND_THROW_RTE(L"Char4gram: line {} must have two TSV columns", lineNumber);
            }
            const String gram = line.substr(0, tab);
            if (gram.size() != 4) {
                LOG_ERROR_AND_THROW_RTE(L"Char4gram: line {} does not contain four UTF-16 characters", lineNumber);
            }
            const uint64_t count = parseCount(line.substr(tab + 1), lineNumber);
            if (!isChar4gramAllowedOnly(gram)) continue;

            const uint64_t key = quadgramKey(gram[0], gram[1], gram[2], gram[3]);
            if (!counts.emplace(key, count).second) {
                LOG_ERROR_AND_THROW_RTE(L"Char4gram: duplicate quadgram at line {}", lineNumber);
            }
            const uint64_t ckey = contextKey(gram[0], gram[1], gram[2]);
            auto& total = contextTotals[ckey];
            if (count > std::numeric_limits<uint64_t>::max() - total) {
                LOG_ERROR_AND_THROW_RTE(L"Char4gram: context count overflow at line {}", lineNumber);
            }
            total += count;
            characterSet.insert(gram[0]);
            characterSet.insert(gram[1]);
            characterSet.insert(gram[2]);
            characterSet.insert(gram[3]);
        }
        if (counts.empty() || characterSet.empty() || characterSet.size() > std::numeric_limits<uint16_t>::max()) {
            LOG_ERROR_AND_THROW_RTE(L"Char4gram: input has no usable entries or too many characters: {}", inputPath);
        }

        Vector<wchar_t> characters(characterSet.begin(), characterSet.end());
        std::unordered_map<wchar_t, uint16_t> characterIds;
        characterIds.reserve(characters.size());
        for (size_t i = 0; i < characters.size(); ++i) {
            characterIds.emplace(characters[i], static_cast<uint16_t>(i));
        }

        Vector<SourceRow> rows;
        rows.reserve(counts.size());
        for (const auto& [key, count] : counts) {
            rows.push_back({
                static_cast<wchar_t>((key >> 48) & 0xffff),
                static_cast<wchar_t>((key >> 32) & 0xffff),
                static_cast<wchar_t>((key >> 16) & 0xffff),
                static_cast<wchar_t>(key & 0xffff),
                count
            });
        }
        std::sort(rows.begin(), rows.end(), [](const SourceRow& lhs, const SourceRow& rhs) {
            return std::tie(lhs.first, lhs.second, lhs.third, lhs.next) <
                std::tie(rhs.first, rhs.second, rhs.third, rhs.next);
        });

        Vector<Context> contexts;
        Vector<Entry> entries;
        contexts.reserve(contextTotals.size());
        entries.reserve(rows.size());
        const double vocabularySize = static_cast<double>(characters.size());
        for (size_t pos = 0; pos < rows.size();) {
            const wchar_t first = rows[pos].first;
            const wchar_t second = rows[pos].second;
            const wchar_t third = rows[pos].third;
            const uint64_t total = contextTotals.at(contextKey(first, second, third));
            const double denominator = static_cast<double>(total) + Alpha * vocabularySize;

            Context context;
            context.first = characterIds.at(first);
            context.second = characterIds.at(second);
            context.third = characterIds.at(third);
            context.entryBegin = static_cast<uint32_t>(entries.size());
            context.missingCost = probabilityCost(Alpha / denominator);

            while (pos < rows.size() &&
                rows[pos].first == first &&
                rows[pos].second == second &&
                rows[pos].third == third) {
                Entry entry;
                entry.next = characterIds.at(rows[pos].next);
                entry.cost = probabilityCost((static_cast<double>(rows[pos].count) + Alpha) / denominator);
                entries.push_back(entry);
                ++context.entryCount;
                ++pos;
            }
            contexts.push_back(context);
        }

        FileHeader header{};
        std::copy(Magic.begin(), Magic.end(), header.magic);
        header.version = Version;
        header.headerSize = sizeof(FileHeader);
        header.characterCount = static_cast<uint32_t>(characters.size());
        header.contextCount = static_cast<uint32_t>(contexts.size());
        header.entryCount = entries.size();
        header.uniformCost = probabilityCost(1.0 / vocabularySize);

        const std::filesystem::path outputFile(outputPath);
        if (outputFile.has_parent_path()) {
            std::error_code ec;
            std::filesystem::create_directories(outputFile.parent_path(), ec);
            if (ec) {
                LOG_ERROR_AND_THROW_RTE(L"Char4gram: cannot create output directory: {}", outputPath);
            }
        }
        std::ofstream output(outputPath, std::ios::binary | std::ios::trunc);
        if (!output) {
            LOG_ERROR_AND_THROW_RTE(L"Char4gram: cannot open output {}", outputPath);
        }
        writeExact(output, &header, 1, outputPath, L"header");
        writeExact(output, characters.data(), characters.size(), outputPath, L"characters");
        writeExact(output, contexts.data(), contexts.size(), outputPath, L"contexts");
        writeExact(output, entries.data(), entries.size(), outputPath, L"entries");
        output.close();
        if (!output) {
            LOG_ERROR_AND_THROW_RTE(L"Char4gram: cannot finish output {}", outputPath);
        }
        LOG_INFOH(L"LEAVE: characters={}, contexts={}, entries={}", characters.size(), contexts.size(), entries.size());
    }

} // namespace dict
