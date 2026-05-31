// Builds a binary trigram connection-cost dictionary from key-sorted TSV.
//
// Usage:
//   conn_3gram_bin_build <input.3gram.tsv> <output.3.bin>

#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace {

constexpr std::array<char, 8> kMagic = { 'D', 'Y', 'M', 'Z', 'C', '3', 'G', '1' };
constexpr uint32_t kVersion = 1;
constexpr uint32_t kMaxId = 65535;

#pragma pack(push, 1)
struct FileHeader {
    char magic[8];
    uint32_t version;
    uint32_t idSize;
    uint64_t entryCount;
    uint64_t offsetCount;
};

struct TrigramCostEntry {
    uint16_t next;
    int16_t cost;
};
#pragma pack(pop)

static_assert(sizeof(FileHeader) == 32);
static_assert(sizeof(TrigramCostEntry) == 4);

struct Row {
    uint16_t x1 = 0;
    uint16_t x2 = 0;
    uint16_t next = 0;
    int16_t cost = 0;
};

struct Stats {
    uint64_t entryCount = 0;
    uint32_t maxId = 0;
};

struct ParseResult {
    bool ok = true;
    std::string message;
};

void printUsage(const char* program) {
    std::cerr << "Usage: " << program << " <input.3gram.tsv> <output.3.bin>\n";
}

bool parseUnsignedField(const std::string& text, size_t begin, size_t end, uint32_t& value) {
    if (begin >= end) return false;
    uint64_t parsed = 0;
    for (size_t i = begin; i < end; ++i) {
        const char ch = text[i];
        if (ch < '0' || ch > '9') return false;
        parsed = parsed * 10 + static_cast<uint32_t>(ch - '0');
        if (parsed > kMaxId) return false;
    }
    value = static_cast<uint32_t>(parsed);
    return true;
}

bool parseUnsigned64Field(const std::string& text, size_t begin, size_t end) {
    if (begin >= end) return false;
    uint64_t parsed = 0;
    for (size_t i = begin; i < end; ++i) {
        const char ch = text[i];
        if (ch < '0' || ch > '9') return false;
        const uint64_t digit = static_cast<uint64_t>(ch - '0');
        if (parsed > (std::numeric_limits<uint64_t>::max() - digit) / 10) return false;
        parsed = parsed * 10 + digit;
    }
    return true;
}

bool parseCostField(const std::string& text, size_t begin, size_t end, int16_t& value) {
    if (begin >= end) return false;

    bool negative = false;
    if (text[begin] == '-') {
        negative = true;
        ++begin;
        if (begin >= end) return false;
    }

    int64_t parsed = 0;
    for (size_t i = begin; i < end; ++i) {
        const char ch = text[i];
        if (ch < '0' || ch > '9') return false;
        parsed = parsed * 10 + static_cast<int32_t>(ch - '0');
        if (parsed > static_cast<int64_t>(std::numeric_limits<int16_t>::max()) + 1) return false;
    }
    if (negative) parsed = -parsed;
    if (parsed < std::numeric_limits<int16_t>::min() || parsed > std::numeric_limits<int16_t>::max()) return false;

    value = static_cast<int16_t>(parsed);
    return true;
}

ParseResult parseLine(const std::string& line, uint64_t lineNumber, Row& row) {
    std::array<size_t, 6> tab = {};
    size_t tabCount = 0;
    for (size_t i = 0; i < line.size(); ++i) {
        if (line[i] == '\t') {
            if (tabCount >= tab.size()) {
                return { false, "line " + std::to_string(lineNumber) + " has too many columns" };
            }
            tab[tabCount++] = i;
        }
    }
    if (tabCount != 5) {
        return { false, "line " + std::to_string(lineNumber) + " must have 6 TSV columns" };
    }

    if (tab[0] != 1 || line[0] != '3') {
        return { false, "line " + std::to_string(lineNumber) + " is not a 3gram row" };
    }

    const size_t historyBegin = tab[0] + 1;
    const size_t historyEnd = tab[1];
    const size_t comma = line.find(',', historyBegin);
    if (comma == std::string::npos || comma >= historyEnd) {
        return { false, "line " + std::to_string(lineNumber) + " has invalid history" };
    }

    uint32_t x1 = 0;
    uint32_t x2 = 0;
    uint32_t next = 0;
    int16_t cost = 0;

    if (!parseUnsignedField(line, historyBegin, comma, x1) ||
        !parseUnsignedField(line, comma + 1, historyEnd, x2) ||
        !parseUnsignedField(line, tab[1] + 1, tab[2], next)) {
        return { false, "line " + std::to_string(lineNumber) + " has an invalid ID" };
    }

    if (!parseCostField(line, tab[4] + 1, line.size(), cost)) {
        return { false, "line " + std::to_string(lineNumber) + " has an invalid cost" };
    }
    if (!parseUnsigned64Field(line, tab[2] + 1, tab[3]) ||
        !parseUnsigned64Field(line, tab[3] + 1, tab[4])) {
        return { false, "line " + std::to_string(lineNumber) + " has an invalid count" };
    }

    row.x1 = static_cast<uint16_t>(x1);
    row.x2 = static_cast<uint16_t>(x2);
    row.next = static_cast<uint16_t>(next);
    row.cost = cost;
    return {};
}

uint64_t sortKey(const Row& row) {
    return (static_cast<uint64_t>(row.x1) << 32) |
        (static_cast<uint64_t>(row.x2) << 16) |
        static_cast<uint64_t>(row.next);
}

bool analyzeInput(const std::string& inputPath, Stats& stats) {
    std::ifstream input(inputPath, std::ios::binary);
    if (!input) {
        std::cerr << "cannot open input file: " << inputPath << "\n";
        return false;
    }

    std::string line;
    Row row;
    uint64_t lineNumber = 0;
    uint64_t prevKey = 0;
    bool hasPrev = false;

    while (std::getline(input, line)) {
        ++lineNumber;
        if (!line.empty() && line.back() == '\r') line.pop_back();

        const auto parsed = parseLine(line, lineNumber, row);
        if (!parsed.ok) {
            std::cerr << parsed.message << "\n";
            return false;
        }

        const uint64_t key = sortKey(row);
        if (hasPrev) {
            if (key < prevKey) {
                std::cerr << "line " << lineNumber << " is not sorted by key\n";
                return false;
            }
            if (key == prevKey) {
                std::cerr << "line " << lineNumber << " has a duplicated key\n";
                return false;
            }
        }
        hasPrev = true;
        prevKey = key;

        stats.maxId = std::max<uint32_t>(stats.maxId, row.x1);
        stats.maxId = std::max<uint32_t>(stats.maxId, row.x2);
        stats.maxId = std::max<uint32_t>(stats.maxId, row.next);
        ++stats.entryCount;
    }

    if (stats.entryCount == 0) {
        std::cerr << "input file has no entries: " << inputPath << "\n";
        return false;
    }

    return true;
}

uint64_t contextIndex(const Row& row, uint32_t idSize) {
    return static_cast<uint64_t>(row.x1) * idSize + row.x2;
}

bool buildBinary(const std::string& inputPath, const std::string& outputPath, const Stats& stats) {
    const uint32_t idSize = stats.maxId + 1;
    const uint64_t offsetCount64 = static_cast<uint64_t>(idSize) * idSize + 1;
    if (offsetCount64 > static_cast<uint64_t>(std::numeric_limits<size_t>::max())) {
        std::cerr << "offset table is too large\n";
        return false;
    }
    if (stats.entryCount > static_cast<uint64_t>(std::numeric_limits<uint32_t>::max())) {
        std::cerr << "entry_count exceeds uint32 offset range\n";
        return false;
    }

    std::vector<uint32_t> offsets(static_cast<size_t>(offsetCount64), 0);
    std::vector<TrigramCostEntry> entries;
    entries.reserve(static_cast<size_t>(stats.entryCount));

    std::ifstream input(inputPath, std::ios::binary);
    if (!input) {
        std::cerr << "cannot reopen input file: " << inputPath << "\n";
        return false;
    }

    std::string line;
    Row row;
    uint64_t lineNumber = 0;
    uint64_t nextOffset = 0;
    uint32_t entryIndex = 0;

    while (std::getline(input, line)) {
        ++lineNumber;
        if (!line.empty() && line.back() == '\r') line.pop_back();

        const auto parsed = parseLine(line, lineNumber, row);
        if (!parsed.ok) {
            std::cerr << parsed.message << "\n";
            return false;
        }

        const uint64_t context = contextIndex(row, idSize);
        while (nextOffset <= context) {
            offsets[static_cast<size_t>(nextOffset)] = entryIndex;
            ++nextOffset;
        }

        entries.push_back(TrigramCostEntry{ row.next, row.cost });
        ++entryIndex;
    }

    while (nextOffset < offsetCount64) {
        offsets[static_cast<size_t>(nextOffset)] = entryIndex;
        ++nextOffset;
    }

    FileHeader header{};
    std::memcpy(header.magic, kMagic.data(), kMagic.size());
    header.version = kVersion;
    header.idSize = idSize;
    header.entryCount = stats.entryCount;
    header.offsetCount = offsetCount64;

    std::ofstream output(outputPath, std::ios::binary);
    if (!output) {
        std::cerr << "cannot open output file: " << outputPath << "\n";
        return false;
    }

    output.write(reinterpret_cast<const char*>(&header), sizeof(header));
    output.write(reinterpret_cast<const char*>(offsets.data()), static_cast<std::streamsize>(offsets.size() * sizeof(uint32_t)));
    output.write(reinterpret_cast<const char*>(entries.data()), static_cast<std::streamsize>(entries.size() * sizeof(TrigramCostEntry)));

    if (!output) {
        std::cerr << "failed to write output file: " << outputPath << "\n";
        return false;
    }

    return true;
}

bool verifyBinary(const std::string& outputPath, const Stats& stats) {
    std::ifstream input(outputPath, std::ios::binary | std::ios::ate);
    if (!input) {
        std::cerr << "cannot open generated file: " << outputPath << "\n";
        return false;
    }

    const auto fileSize = static_cast<uint64_t>(input.tellg());
    input.seekg(0);

    FileHeader header{};
    input.read(reinterpret_cast<char*>(&header), sizeof(header));
    if (!input) {
        std::cerr << "generated file is too short\n";
        return false;
    }

    if (std::memcmp(header.magic, kMagic.data(), kMagic.size()) != 0 ||
        header.version != kVersion ||
        header.idSize != stats.maxId + 1 ||
        header.entryCount != stats.entryCount) {
        std::cerr << "generated header is invalid\n";
        return false;
    }

    const uint64_t expectedOffsetCount = static_cast<uint64_t>(header.idSize) * header.idSize + 1;
    if (header.offsetCount != expectedOffsetCount) {
        std::cerr << "generated offset_count is invalid\n";
        return false;
    }

    const uint64_t expectedSize =
        sizeof(FileHeader) +
        header.offsetCount * sizeof(uint32_t) +
        header.entryCount * sizeof(TrigramCostEntry);
    if (fileSize != expectedSize) {
        std::cerr << "generated file size is invalid: actual=" << fileSize
            << ", expected=" << expectedSize << "\n";
        return false;
    }

    uint32_t prev = 0;
    for (uint64_t i = 0; i < header.offsetCount; ++i) {
        uint32_t current = 0;
        input.read(reinterpret_cast<char*>(&current), sizeof(current));
        if (!input) {
            std::cerr << "failed to read offsets\n";
            return false;
        }
        if (current < prev || current > header.entryCount) {
            std::cerr << "offset table is invalid at index " << i << "\n";
            return false;
        }
        prev = current;
    }

    if (prev != header.entryCount) {
        std::cerr << "last offset does not match entry_count\n";
        return false;
    }

    return true;
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 3) {
        printUsage(argv[0]);
        return 1;
    }

    const std::string inputPath = argv[1];
    const std::string outputPath = argv[2];

    Stats stats;
    if (!analyzeInput(inputPath, stats)) return 1;
    if (!buildBinary(inputPath, outputPath, stats)) return 1;
    if (!verifyBinary(outputPath, stats)) return 1;

    std::cerr
        << "done: entries=" << stats.entryCount
        << ", max_id=" << stats.maxId
        << ", id_size=" << (stats.maxId + 1)
        << "\n";
    return 0;
}
