#include "Char3gram.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cmath>
#include <cwchar>
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
    DEFINE_CLASS_LOGGER(Char3gram);

    namespace {
        constexpr std::array<char, 8> Magic = { 'N', 'G', 'C', '3', 'G', '0', '0', '1' };
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

        struct SourceRow {
            wchar_t first = 0;
            wchar_t second = 0;
            wchar_t next = 0;
            uint64_t count = 0;
        };

        template<class T>
        void readExact(std::ifstream& input, T* data, size_t count, StringRef filepath, StringRef section) {
            if (count == 0) return;
            input.read(reinterpret_cast<char*>(data), static_cast<std::streamsize>(sizeof(T) * count));
            if (!input) {
                throw util::RuntimeException(std::format(L"Char3gram: cannot read {} from {}", section, filepath), __FILE__, __LINE__);
            }
        }

        template<class T>
        void writeExact(std::ofstream& output, const T* data, size_t count, StringRef filepath, StringRef section) {
            if (count == 0) return;
            output.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(sizeof(T) * count));
            if (!output) {
                throw util::RuntimeException(std::format(L"Char3gram: cannot write {} to {}", section, filepath), __FILE__, __LINE__);
            }
        }

        uint64_t trigramKey(wchar_t first, wchar_t second, wchar_t next) {
            return (static_cast<uint64_t>(static_cast<uint16_t>(first)) << 32) |
                (static_cast<uint64_t>(static_cast<uint16_t>(second)) << 16) |
                static_cast<uint16_t>(next);
        }

        uint32_t contextKey(wchar_t first, wchar_t second) {
            return (static_cast<uint32_t>(static_cast<uint16_t>(first)) << 16) |
                static_cast<uint16_t>(second);
        }

        int32_t probabilityCost(double probability) {
            const double value = std::round(-std::log(probability) * CostScale);
            if (value < 0.0 || value > static_cast<double>(std::numeric_limits<int32_t>::max())) {
                throw util::RuntimeException(L"Char3gram: calculated cost is out of range", __FILE__, __LINE__);
            }
            return static_cast<int32_t>(value);
        }

        uint64_t parseCount(StringRef value, uint64_t lineNumber) {
            if (value.empty() || std::any_of(value.begin(), value.end(), [](wchar_t ch) { return ch < L'0' || ch > L'9'; })) {
                throw util::RuntimeException(std::format(L"Char3gram: invalid count at line {}", lineNumber), __FILE__, __LINE__);
            }
            errno = 0;
            wchar_t* end = nullptr;
            const uint64_t count = std::wcstoull(value.c_str(), &end, 10);
            if (errno == ERANGE || end != value.c_str() + value.size() || count == 0) {
                throw util::RuntimeException(std::format(L"Char3gram: invalid count at line {}", lineNumber), __FILE__, __LINE__);
            }
            return count;
        }

        bool isGetaTriple(StringRef gram) {
            return gram.size() == 3 && gram[0] == GETA_CHAR && gram[1] == GETA_CHAR && gram[2] == GETA_CHAR;
        }
    }

    Char3gram::Char3gram(StringRef filepath) {
        load(filepath);
    }

    void Char3gram::load(StringRef filepath) {
        LOG_INFOH(L"ENTER: filepath={}", filepath);

        std::ifstream input(filepath, std::ios::binary);
        if (!input) {
            LOG_ERROR_AND_THROW_RTE(L"Char3gram: cannot open {}", filepath);
        }

        FileHeader header{};
        readExact(input, &header, 1, filepath, L"header");
        if (!std::equal(Magic.begin(), Magic.end(), header.magic)) {
            LOG_ERROR_AND_THROW_RTE(L"Char3gram: invalid magic: {}", filepath);
        }
        if (header.version != Version || header.headerSize != sizeof(FileHeader)) {
            LOG_ERROR_AND_THROW_RTE(L"Char3gram: unsupported format: {}", filepath);
        }
        if (header.characterCount == 0 || header.characterCount > std::numeric_limits<uint16_t>::max() ||
            header.contextCount == 0 || header.entryCount == 0 || header.reserved != 0 ||
            header.entryCount > std::numeric_limits<uint32_t>::max() ||
            header.entryCount > static_cast<uint64_t>(std::numeric_limits<size_t>::max()) ||
            header.uniformCost < 0) {
            LOG_ERROR_AND_THROW_RTE(L"Char3gram: invalid header: {}", filepath);
        }

        const uint64_t expectedSize = sizeof(FileHeader) +
            static_cast<uint64_t>(header.characterCount) * sizeof(wchar_t) +
            static_cast<uint64_t>(header.contextCount) * sizeof(Context) +
            header.entryCount * sizeof(Entry);
        std::error_code ec;
        const uint64_t actualSize = std::filesystem::file_size(filepath, ec);
        if (ec || actualSize != expectedSize) {
            LOG_ERROR_AND_THROW_RTE(L"Char3gram: invalid file size: {}", filepath);
        }

        Vector<wchar_t> characters(header.characterCount);
        Vector<Context> contexts(header.contextCount);
        Vector<Entry> entries(static_cast<size_t>(header.entryCount));
        readExact(input, characters.data(), characters.size(), filepath, L"characters");
        readExact(input, contexts.data(), contexts.size(), filepath, L"contexts");
        readExact(input, entries.data(), entries.size(), filepath, L"entries");

        if (!std::is_sorted(characters.begin(), characters.end()) ||
            std::adjacent_find(characters.begin(), characters.end()) != characters.end()) {
            LOG_ERROR_AND_THROW_RTE(L"Char3gram: character table is not strictly sorted: {}", filepath);
        }

        uint32_t expectedEntryBegin = 0;
        uint32_t previousContextKey = 0;
        bool hasPreviousContext = false;
        for (const auto& context : contexts) {
            if (context.first >= characters.size() || context.second >= characters.size() ||
                context.entryBegin != expectedEntryBegin || context.entryCount == 0 ||
                context.entryCount > entries.size() - context.entryBegin || context.missingCost < 0) {
                LOG_ERROR_AND_THROW_RTE(L"Char3gram: invalid context table: {}", filepath);
            }
            const uint32_t key = (static_cast<uint32_t>(context.first) << 16) | context.second;
            if (hasPreviousContext && key <= previousContextKey) {
                LOG_ERROR_AND_THROW_RTE(L"Char3gram: context table is not strictly sorted: {}", filepath);
            }
            hasPreviousContext = true;
            previousContextKey = key;

            uint16_t previousNext = 0;
            bool hasPreviousNext = false;
            for (uint32_t i = 0; i < context.entryCount; ++i) {
                const auto& entry = entries[context.entryBegin + i];
                if (entry.next >= characters.size() || entry.reserved != 0 || entry.cost < 0 ||
                    (hasPreviousNext && entry.next <= previousNext)) {
                    LOG_ERROR_AND_THROW_RTE(L"Char3gram: invalid entry table: {}", filepath);
                }
                hasPreviousNext = true;
                previousNext = entry.next;
            }
            expectedEntryBegin += context.entryCount;
        }
        if (expectedEntryBegin != entries.size()) {
            LOG_ERROR_AND_THROW_RTE(L"Char3gram: entry count mismatch: {}", filepath);
        }

        characters_.swap(characters);
        contexts_.swap(contexts);
        entries_.swap(entries);
        uniformCost_ = header.uniformCost;
        LOG_INFOH(L"LEAVE: characters={}, contexts={}, entries={}, uniformCost={}",
            characters_.size(), contexts_.size(), entries_.size(), uniformCost_);
    }

    bool Char3gram::loaded() const {
        return !characters_.empty() && !contexts_.empty() && !entries_.empty();
    }

    int Char3gram::findCharacterId(wchar_t ch) const {
        const auto it = std::lower_bound(characters_.begin(), characters_.end(), ch);
        return it == characters_.end() || *it != ch ? -1 : static_cast<int>(it - characters_.begin());
    }

    int Char3gram::cost(wchar_t first, wchar_t second, wchar_t next) const {
        const int firstId = findCharacterId(first);
        const int secondId = findCharacterId(second);
        if (firstId < 0 || secondId < 0) return uniformCost_;

        const uint32_t key = (static_cast<uint32_t>(firstId) << 16) | static_cast<uint16_t>(secondId);
        const auto contextIt = std::lower_bound(contexts_.begin(), contexts_.end(), key, [](const Context& context, uint32_t value) {
            return ((static_cast<uint32_t>(context.first) << 16) | context.second) < value;
        });
        if (contextIt == contexts_.end() ||
            ((static_cast<uint32_t>(contextIt->first) << 16) | contextIt->second) != key) {
            return uniformCost_;
        }

        const int nextId = findCharacterId(next);
        if (nextId < 0) return contextIt->missingCost;
        const auto firstEntry = entries_.begin() + contextIt->entryBegin;
        const auto lastEntry = firstEntry + contextIt->entryCount;
        const auto entryIt = std::lower_bound(firstEntry, lastEntry, static_cast<uint16_t>(nextId), [](const Entry& entry, uint16_t value) {
            return entry.next < value;
        });
        return entryIt == lastEntry || entryIt->next != nextId ? contextIt->missingCost : entryIt->cost;
    }

    Char3gram::ScoreResult Char3gram::score(StringRef sentence) const {
        ScoreResult result;
        if (!loaded()) return result;

        result.normalized.reserve(sentence.size());
        for (size_t i = 0; i < sentence.size(); ++i) {
            const wchar_t ch = sentence[i];
            if (utils::is_hiragana(ch) || utils::is_kanji(ch) || ch == GETA_CHAR) {
                result.normalized.push_back(ch);
            } else {
                result.normalized.push_back(GETA_CHAR);
                if (is_high_surrogate(ch) && i + 1 < sentence.size() && is_low_surrogate(sentence[i + 1])) {
                    ++i;
                }
            }
        }

        int64_t sum = 0;
        for (size_t i = 0; i + 2 < result.normalized.size(); ++i) {
            if (result.normalized[i] == GETA_CHAR &&
                result.normalized[i + 1] == GETA_CHAR &&
                result.normalized[i + 2] == GETA_CHAR) {
                continue;
            }
            sum += cost(result.normalized[i], result.normalized[i + 1], result.normalized[i + 2]);
            ++result.validWindowCount;
        }
        LOG_DEBUGH(L"validWindowCount={}, sum={}", result.validWindowCount, sum);
        if (result.validWindowCount > 0) {
            result.averageCost = static_cast<int>(std::llround(
                static_cast<double>(sum) / result.validWindowCount));
        }
        return result;
    }

    void Char3gram::build(StringRef inputPath, StringRef outputPath) {
        LOG_INFOH(L"ENTER: input={}, output={}", inputPath, outputPath);
        utils::IfstreamReader reader(inputPath);
        if (!reader.success()) {
            LOG_ERROR_AND_THROW_RTE(L"Char3gram: cannot open input {}", inputPath);
        }

        std::unordered_map<uint64_t, uint64_t> counts;
        std::unordered_map<uint32_t, uint64_t> contextTotals;
        std::set<wchar_t> characterSet;
        uint64_t lineNumber = 0;
        while (true) {
            auto [line, eof] = reader.getLine();
            if (eof) break;
            ++lineNumber;

            const size_t tab = line.find(L'\t');
            if (tab == String::npos || line.find(L'\t', tab + 1) != String::npos) {
                LOG_ERROR_AND_THROW_RTE(L"Char3gram: line {} must have two TSV columns", lineNumber);
            }
            const String gram = line.substr(0, tab);
            if (gram.size() != 3) {
                LOG_ERROR_AND_THROW_RTE(L"Char3gram: line {} does not contain three UTF-16 characters", lineNumber);
            }
            const uint64_t count = parseCount(line.substr(tab + 1), lineNumber);
            if (isGetaTriple(gram)) continue;

            const uint64_t key = trigramKey(gram[0], gram[1], gram[2]);
            if (!counts.emplace(key, count).second) {
                LOG_ERROR_AND_THROW_RTE(L"Char3gram: duplicate trigram at line {}", lineNumber);
            }
            const uint32_t ckey = contextKey(gram[0], gram[1]);
            auto& total = contextTotals[ckey];
            if (count > std::numeric_limits<uint64_t>::max() - total) {
                LOG_ERROR_AND_THROW_RTE(L"Char3gram: context count overflow at line {}", lineNumber);
            }
            total += count;
            characterSet.insert(gram[0]);
            characterSet.insert(gram[1]);
            characterSet.insert(gram[2]);
        }
        if (counts.empty() || characterSet.empty() || characterSet.size() > std::numeric_limits<uint16_t>::max()) {
            LOG_ERROR_AND_THROW_RTE(L"Char3gram: input has no usable entries or too many characters: {}", inputPath);
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
                static_cast<wchar_t>((key >> 32) & 0xffff),
                static_cast<wchar_t>((key >> 16) & 0xffff),
                static_cast<wchar_t>(key & 0xffff),
                count
            });
        }
        std::sort(rows.begin(), rows.end(), [](const SourceRow& lhs, const SourceRow& rhs) {
            return std::tie(lhs.first, lhs.second, lhs.next) < std::tie(rhs.first, rhs.second, rhs.next);
        });

        Vector<Context> contexts;
        Vector<Entry> entries;
        contexts.reserve(contextTotals.size());
        entries.reserve(rows.size());
        const double vocabularySize = static_cast<double>(characters.size());
        for (size_t pos = 0; pos < rows.size();) {
            const wchar_t first = rows[pos].first;
            const wchar_t second = rows[pos].second;
            const uint64_t total = contextTotals.at(contextKey(first, second));
            const double denominator = static_cast<double>(total) + Alpha * vocabularySize;

            Context context;
            context.first = characterIds.at(first);
            context.second = characterIds.at(second);
            context.entryBegin = static_cast<uint32_t>(entries.size());
            context.missingCost = probabilityCost(Alpha / denominator);

            while (pos < rows.size() && rows[pos].first == first && rows[pos].second == second) {
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
                LOG_ERROR_AND_THROW_RTE(L"Char3gram: cannot create output directory: {}", outputPath);
            }
        }
        std::ofstream output(outputPath, std::ios::binary | std::ios::trunc);
        if (!output) {
            LOG_ERROR_AND_THROW_RTE(L"Char3gram: cannot open output {}", outputPath);
        }
        writeExact(output, &header, 1, outputPath, L"header");
        writeExact(output, characters.data(), characters.size(), outputPath, L"characters");
        writeExact(output, contexts.data(), contexts.size(), outputPath, L"contexts");
        writeExact(output, entries.data(), entries.size(), outputPath, L"entries");
        output.close();
        if (!output) {
            LOG_ERROR_AND_THROW_RTE(L"Char3gram: cannot finish output {}", outputPath);
        }
        LOG_INFOH(L"LEAVE: characters={}, contexts={}, entries={}", characters.size(), contexts.size(), entries.size());
    }

} // namespace dict
