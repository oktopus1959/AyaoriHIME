// Builds 1..4-gram counts and negative log-likelihood costs from connection ID sequences.
//
// Usage:
//   conn_ngram_train <input.conn.txt> <output.tsv> [order] [alpha] [cost_factor]

#include <algorithm>
#include <array>
#include <cerrno>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace {

constexpr int kMaxOrder = 4;
constexpr uint32_t kMaxId = 65535;

using Count = uint64_t;
using CountMap = std::unordered_map<uint64_t, Count>;

struct Options {
    std::string inputPath;
    std::string outputPath;
    int order = 4;
    double alpha = 0.1;
    double costFactor = 700.0;
};

struct ParseResult {
    bool ok = true;
    std::string message;
};

void printUsage(const char* program) {
    std::cerr
        << "Usage: " << program
        << " <input.conn.txt> <output.tsv> [order] [alpha] [cost_factor]\n";
}

bool parseIntArg(const char* text, int& value) {
    char* end = nullptr;
    errno = 0;
    const long parsed = std::strtol(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0') return false;
    if (parsed < std::numeric_limits<int>::min() || parsed > std::numeric_limits<int>::max()) return false;
    value = static_cast<int>(parsed);
    return true;
}

bool parseDoubleArg(const char* text, double& value) {
    char* end = nullptr;
    errno = 0;
    const double parsed = std::strtod(text, &end);
    if (errno != 0 || end == text || *end != '\0' || !std::isfinite(parsed)) return false;
    value = parsed;
    return true;
}

bool parseOptions(int argc, char** argv, Options& opts) {
    if (argc < 3 || argc > 6) {
        printUsage(argv[0]);
        return false;
    }

    opts.inputPath = argv[1];
    opts.outputPath = argv[2];

    if (argc >= 4 && !parseIntArg(argv[3], opts.order)) {
        std::cerr << "failed to parse order: " << argv[3] << "\n";
        return false;
    }
    if (argc >= 5 && !parseDoubleArg(argv[4], opts.alpha)) {
        std::cerr << "failed to parse alpha: " << argv[4] << "\n";
        return false;
    }
    if (argc >= 6 && !parseDoubleArg(argv[5], opts.costFactor)) {
        std::cerr << "failed to parse cost_factor: " << argv[5] << "\n";
        return false;
    }

    if (opts.order < 1 || opts.order > kMaxOrder) {
        std::cerr << "order must be in 1.." << kMaxOrder << ": " << opts.order << "\n";
        return false;
    }
    if (opts.alpha <= 0.0) {
        std::cerr << "alpha must be positive: " << opts.alpha << "\n";
        return false;
    }
    if (opts.costFactor <= 0.0) {
        std::cerr << "cost_factor must be positive: " << opts.costFactor << "\n";
        return false;
    }

    return true;
}

bool isSpace(char ch) {
    return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n';
}

ParseResult parseIdLine(const std::string& line, uint64_t lineNumber, std::vector<uint16_t>& ids) {
    ids.clear();

    size_t pos = 0;
    while (pos < line.size() && isSpace(line[pos])) ++pos;
    if (pos == line.size()) {
        return { false, "line " + std::to_string(lineNumber) + " is empty" };
    }

    bool expectValue = true;
    while (pos < line.size()) {
        while (pos < line.size() && isSpace(line[pos])) ++pos;

        if (pos == line.size()) {
            if (expectValue) {
                return { false, "line " + std::to_string(lineNumber) + " has an empty trailing column" };
            }
            break;
        }

        if (!expectValue) {
            if (line[pos] != ',') {
                return { false, "line " + std::to_string(lineNumber) + " has a non-comma delimiter" };
            }
            ++pos;
            expectValue = true;
            continue;
        }

        if (line[pos] < '0' || line[pos] > '9') {
            return { false, "line " + std::to_string(lineNumber) + " has a non-numeric column" };
        }

        uint32_t value = 0;
        while (pos < line.size() && line[pos] >= '0' && line[pos] <= '9') {
            value = value * 10 + static_cast<uint32_t>(line[pos] - '0');
            if (value > kMaxId) {
                return { false, "line " + std::to_string(lineNumber) + " has an ID larger than 65535" };
            }
            ++pos;
        }

        ids.push_back(static_cast<uint16_t>(value));
        expectValue = false;

        while (pos < line.size() && isSpace(line[pos])) ++pos;
        if (pos < line.size() && line[pos] != ',') {
            return { false, "line " + std::to_string(lineNumber) + " has an invalid character" };
        }
    }

    if (expectValue) {
        return { false, "line " + std::to_string(lineNumber) + " has an empty trailing column" };
    }
    if (ids.empty()) {
        return { false, "line " + std::to_string(lineNumber) + " is empty" };
    }

    return {};
}

uint64_t packIds(const std::vector<uint16_t>& ids, size_t begin, int length) {
    uint64_t key = 0;
    for (int i = 0; i < length; ++i) {
        key = (key << 16) | ids[begin + static_cast<size_t>(i)];
    }
    return key;
}

std::vector<uint16_t> unpackIds(uint64_t key, int length) {
    std::vector<uint16_t> ids(static_cast<size_t>(length));
    for (int i = length - 1; i >= 0; --i) {
        ids[static_cast<size_t>(i)] = static_cast<uint16_t>(key & 0xffff);
        key >>= 16;
    }
    return ids;
}

std::string historyToString(uint64_t contextKey, int historyLength) {
    if (historyLength == 0) return "<BOS>";

    const auto ids = unpackIds(contextKey, historyLength);
    std::string text;
    for (size_t i = 0; i < ids.size(); ++i) {
        if (i > 0) text += ",";
        text += std::to_string(ids[i]);
    }
    return text;
}

void increment(CountMap& map, uint64_t key) {
    auto iter = map.find(key);
    if (iter == map.end()) {
        map.emplace(key, 1);
    } else {
        ++iter->second;
    }
}

void countLine(
    const std::vector<uint16_t>& ids,
    int order,
    std::array<CountMap, kMaxOrder + 1>& gramCounts,
    std::array<CountMap, kMaxOrder + 1>& contextCounts,
    std::unordered_set<uint16_t>& vocabulary) {

    for (uint16_t id : ids) {
        vocabulary.insert(id);
    }

    for (size_t i = 0; i < ids.size(); ++i) {
        for (int n = 1; n <= order; ++n) {
            if (i + 1 < static_cast<size_t>(n)) break;

            const size_t begin = i + 1 - static_cast<size_t>(n);
            const uint64_t gramKey = packIds(ids, begin, n);
            const uint64_t contextKey = (n == 1) ? 0 : packIds(ids, begin, n - 1);

            increment(gramCounts[n], gramKey);
            increment(contextCounts[n], contextKey);
        }
    }
}

uint16_t nextIdFromGram(uint64_t gramKey) {
    return static_cast<uint16_t>(gramKey & 0xffff);
}

uint64_t contextFromGram(uint64_t gramKey, int n) {
    return n == 1 ? 0 : (gramKey >> 16);
}

int toCost(Count count, Count contextCount, size_t vocabSize, double alpha, double costFactor) {
    const double numerator = static_cast<double>(count) + alpha;
    const double denominator = static_cast<double>(contextCount) + alpha * static_cast<double>(vocabSize);
    const double probability = numerator / denominator;
    return static_cast<int>(std::llround(-std::log(probability) * costFactor));
}

bool writeModel(
    const Options& opts,
    const std::array<CountMap, kMaxOrder + 1>& gramCounts,
    const std::array<CountMap, kMaxOrder + 1>& contextCounts,
    size_t vocabSize) {

    std::ofstream output(opts.outputPath, std::ios::binary);
    if (!output) {
        std::cerr << "cannot open output file: " << opts.outputPath << "\n";
        return false;
    }

    output << "# dymazin-conn-ngram-model-v1\n";
    output << "# order=" << opts.order << "\n";
    output << "# alpha=" << opts.alpha << "\n";
    output << "# cost_factor=" << opts.costFactor << "\n";
    output << "# vocab_size=" << vocabSize << "\n";

    for (int n = 1; n <= opts.order; ++n) {
        std::vector<std::pair<uint64_t, Count>> rows;
        rows.reserve(gramCounts[n].size());
        for (const auto& item : gramCounts[n]) {
            rows.push_back(item);
        }
        std::sort(rows.begin(), rows.end(), [](const auto& a, const auto& b) {
            return a.first < b.first;
        });

        for (const auto& [gramKey, count] : rows) {
            const uint64_t contextKey = contextFromGram(gramKey, n);
            const auto contextIter = contextCounts[n].find(contextKey);
            if (contextIter == contextCounts[n].end()) {
                std::cerr << "internal error: context_count is missing\n";
                return false;
            }

            const Count contextCount = contextIter->second;
            const int cost = toCost(count, contextCount, vocabSize, opts.alpha, opts.costFactor);
            output
                << n << '\t'
                << historyToString(contextKey, n - 1) << '\t'
                << nextIdFromGram(gramKey) << '\t'
                << count << '\t'
                << contextCount << '\t'
                << cost << '\n';
        }
    }

    return true;
}

} // namespace

int main(int argc, char** argv) {
    Options opts;
    if (!parseOptions(argc, argv, opts)) return 1;

    std::ifstream input(opts.inputPath, std::ios::binary);
    if (!input) {
        std::cerr << "cannot open input file: " << opts.inputPath << "\n";
        return 1;
    }

    std::array<CountMap, kMaxOrder + 1> gramCounts;
    std::array<CountMap, kMaxOrder + 1> contextCounts;
    std::unordered_set<uint16_t> vocabulary;

    std::string line;
    std::vector<uint16_t> ids;
    uint64_t lineNumber = 0;

    while (std::getline(input, line)) {
        ++lineNumber;
        const auto parseResult = parseIdLine(line, lineNumber, ids);
        if (!parseResult.ok) {
            std::cerr << parseResult.message << "\n";
            return 1;
        }

        countLine(ids, opts.order, gramCounts, contextCounts, vocabulary);

        if (lineNumber % 1000000 == 0) {
            std::cerr << "processed_lines=" << lineNumber << "\n";
        }
    }

    if (lineNumber == 0) {
        std::cerr << "input file is empty: " << opts.inputPath << "\n";
        return 1;
    }

    if (!writeModel(opts, gramCounts, contextCounts, vocabulary.size())) return 1;

    std::cerr
        << "done: lines=" << lineNumber
        << ", vocab_size=" << vocabulary.size()
        << ", order=" << opts.order
        << "\n";
    return 0;
}
