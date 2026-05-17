#include "misc_utils.h"
#include "my_utils.h"
#include "path_utils.h"
#include "constants/Constants.h"
#include "Logger.h"
#include "Connector.h"
#include "exception.h"
#include "xsv_parser.h"

namespace {
    DEFINE_LOCAL_LOGGER(Connector);

    int parseMatrixCsvInt(StringRef value, StringRef filepath, size_t lineNumber, size_t columnNumber) {
        auto text = utils::strip(value);
        size_t pos = 0;
        try {
            int result = std::stoi(text, &pos);
            if (pos != text.size()) {
                THROW_RTE(L"{} contains non-numeric value at row {}, column {}: {}", filepath, lineNumber, columnNumber, value);
            }
            return result;
        } catch (const util::RuntimeException&) {
            throw;
        } catch (...) {
            THROW_RTE(L"{} contains non-numeric value at row {}, column {}: {}", filepath, lineNumber, columnNumber, value);
        }
    }

}

namespace analyzer {
    DECLARE_LOGGER;
    DEFINE_CLASS_LOGGER(Connector);

    // 属性間接続コスト行列
    struct Connector::Matrix {
        std::vector<short> matrix;  // 行列本体
        size_t lr_size = 0;         // 左側ノードの右接続属性の数
        size_t rl_size = 0;         // 右側ノードの左接続属性の数

        // 左側ノードのlr番目の右接続属性と、右側ノードのrl番目の左接続属性の間の接続コストを取得する
        short getConnectionCost(size_t lr, size_t rl) const {
            size_t idx = lr + lr_size * rl;
            return idx < matrix.size() ? matrix[idx] : 0;
        }

        // 左側ノードのlr番目の右接続属性と、右側ノードのrl番目の左接続属性の間の接続コストを設定する
        void setConnectionCost(size_t lr, size_t rl, int c) {
            size_t idx = lr + lr_size * rl;
            size_t ceilSize = lr_size * (rl + 1);
            if (matrix.size() < ceilSize) matrix.resize(ceilSize);
            //short sc = c < SHRT_MIN ? SHRT_MIN : (c > SHRT_MAX ? SHRT_MAX : (short)c);
            matrix[lr + lr_size * rl] = (short)c;
        }

        // EOS への接続コストを 0 にする
        void zeroClearEOSconnectionCost() {
            for (size_t i = 0; i < lr_size; ++i) {
                setConnectionCost(i, 0, 0);
            }
        }

        bool checkSize() {
            return matrix.size() == lr_size * rl_size;
        }

        void deserialize(StringRef filepath) {
            LOG_INFOH(L"ENTER: filepath={}", filepath);
            utils::IfstreamReader reader(filepath, true);
            CHECK_OR_THROW(reader.success(), L"can't open category matrix table binary file for read: {}", filepath);

            reader.read(lr_size);
            reader.read(rl_size);
            reader.read(matrix);
            LOG_INFOH(L"LEAVE: lr_size={}, rl_size={}", lr_size, rl_size);
        }

        void serialize(StringRef filepath) {
            LOG_INFOH(L"ENTER: filepath={}: lr_size={}, rl_size={}", filepath, lr_size, rl_size);
            utils::OfstreamWriter writer(filepath, true, false);
            CHECK_OR_THROW(writer.success(), L"can't open category matrix table binary file for write: {}", filepath);

            writer.write(lr_size);
            writer.write(rl_size);
            writer.write(matrix);
            LOG_INFOH(L"LEAVE");
        }

        bool equalsTo(const Matrix& m) const {
            return lr_size == m.lr_size && rl_size == m.rl_size && matrix == m.matrix;
        }
    };

    Connector::Connector() : pMatrix(MakeUniq<Matrix>()) {
        LOG_INFOH(L"CALLED: default ctor");
    }

    Connector::Connector(size_t lr_size, size_t rl_size)
        : pMatrix(new Matrix{ std::vector<short>(lr_size * rl_size), lr_size, rl_size }) {
        LOG_INFOH(L"CALLED: lr_size={}, rl_size={}", lr_size, rl_size);
    }

    Connector::Connector(StringRef filename) : Connector() {
        LOG_INFOH(L"ENTER: filename={}", filename);
        if (!utils::isFileExistent(filename)) THROW_RTE(L"cannot open: {}", filename);
        pMatrix->deserialize(filename);
        if (pMatrix->matrix.empty()) THROW_RTE(L"matrix is NULL");
        if (!pMatrix->checkSize()) THROW_RTE(L"matrix size is invalid: {}", filename);
        LOG_INFOH(L"LEAVE");
    }

    Connector::Connector(OptHandlerRef opts)
        : Connector(utils::join_path(opts.getString(L"dicdir", L"."), MATRIX_FILE)) {
        LOG_INFOH(L"CALLED: opts");
        if (opts.getBoolean(L"ignore-eos")) {
            pMatrix->zeroClearEOSconnectionCost();
            LOG_INFOH(L"EOS connection cost cleared");
        }
    }

    Connector::~Connector() = default;

    bool Connector::equalsTo(const Connector& cc) const {
        return pMatrix->equalsTo(*(cc.pMatrix));
    }

    // pfile: EOS接続の場合のペナルティを設定するファイル
    void Connector::compile(StringRef ifile, StringRef pfile, StringRef ofile) {
        LOG_INFOH(L"ENTER: ifile={}, ofile={}", ifile, ofile);
        auto pConn = _compile(ifile, pfile);
        if (pConn) {
            // serialize
            std::wcout << L"serializing matrix: " << ofile << L" ... " << std::flush;
            pConn->serialize(ofile);
            std::wcout << "done" << std::endl;
        }
        LOG_INFOH(L"LEAVE");
    }

    SharedPtr<Connector> Connector::_compile(StringRef ifile, StringRef pfile) {
        LOG_INFOH(L"ENTER: ifile={}, pfile={}", ifile, pfile);

        std::set<size_t> leftAttrsWithEOSConnPenalty;
        
        int penalty = _getLeftAttributesWithEOSConnectionPenalty(pfile, leftAttrsWithEOSConnPenalty);

        utils::IfstreamReader reader(ifile);
        CHECK_OR_THROW(reader.success(), L"matrix.def.csv is not found: {}", ifile);

        size_t rl_size = 0;
        Vector<Vector<int>> rows;

        while (true) {
            auto [ln, eof] = reader.getLine();
            if (eof) break;

            auto line = utils::strip(ln);
            if (line.empty()) continue;

            auto cols = utils::parseCSV(line);
            CHECK_OR_THROW(!cols.empty(), L"matrix.def.csv has no columns at row {}: {}", rows.size() + 1, ifile);

            if (rl_size == 0) {
                rl_size = cols.size();
                CHECK_OR_THROW(rl_size > 0, L"matrix.def.csv has no columns: {}", ifile);
            } else {
                CHECK_OR_THROW(cols.size() == rl_size, L"matrix.def.csv row width is inconsistent at row {}: expected {}, actual {}", rows.size() + 1, rl_size, cols.size());
            }

            Vector<int> row;
            row.reserve(rl_size);
            for (size_t rl = 0; rl < rl_size; ++rl) {
                row.push_back(parseMatrixCsvInt(cols[rl], ifile, rows.size() + 1, rl + 1));
            }
            rows.push_back(std::move(row));
        }

        CHECK_OR_THROW(!rows.empty(), L"matrix.def.csv has no rows: {}", ifile);
        CHECK_OR_THROW(rl_size > 0, L"matrix.def.csv has no columns: {}", ifile);
        size_t lr_size = rows.size();

        auto pConn = SharedPtr<Connector>(new Connector(lr_size, rl_size));
        Matrix& matrix = *pConn->pMatrix;

        String msg = std::format(L"reading {} ... {}x{}", ifile, lr_size, rl_size);
        LOG_INFOH(msg);
        std::wcout << msg << std::endl;

        for (size_t lr = 0; lr < lr_size; ++lr) {
            util::progress_bar(L"emitting matrix      ", lr + 1, lr_size);
            for (size_t rl = 0; rl < rl_size; ++rl) {
                int c = rows[lr][rl];
                if (rl == 0 && leftAttrsWithEOSConnPenalty.contains(lr)) {
                    LOG_INFO(L"eof penalty attr={}, penalty={}", lr, penalty);
                    c += penalty;
                }
                matrix.setConnectionCost(lr, rl, c);
            }
        }

        LOG_INFOH(L"LEAVE");
        return pConn;
    }

    // 先頭行はペナルティ値、その後の行は左側ノードの右接続属性を1列ずつ
    int Connector::_getLeftAttributesWithEOSConnectionPenalty(StringRef pfile, std::set<size_t>& attrs) {
        LOG_INFOH(L"ENTER: pfile={}", pfile);

        int penalty = 0;

        utils::IfstreamReader reader(pfile);
        if (!reader.success()) {
            LOG_WARN(L"{} is not found.", pfile);
            return penalty;
        }

        auto reDelim = std::wregex(L"[\\t ]+");
        while (true) {
            auto [ln, eof] = reader.getLine();
            if (eof) return penalty;

            auto line = utils::strip(ln);
            if (line.empty() || line[0] == '#') continue;

            auto columns = utils::reSplit(line, reDelim);
            if (columns.size() < 1) continue;

            penalty = utils::strToInt(columns[0]);
            break;
        }

        size_t lr_size = 0;
        size_t rl_size = 0;
        while (true) {
            auto [ln, eof] = reader.getLine();
            if (eof) break;

            auto line = utils::strip(ln);
            if (line.empty() || line[0] == '#') continue;

            auto cols = utils::reSplit(line, reDelim);
            if (cols.size() >= 1) {
                attrs.insert(utils::strToInt(cols[0]));
            }
        }

        LOG_INFOH(L"LEAVE: penalty={}", penalty);
        return penalty;
    }

    void Connector::deserialize(StringRef filepath) {
        pMatrix->deserialize(filepath);
    }

    void Connector::serialize(StringRef filepath) {
        pMatrix->serialize(filepath);
    }

    std::tuple<size_t, size_t> Connector::readMatrixSize(StringRef filename) {
        LOG_INFOH(L"ENTER: filename={}", filename);
        if (!utils::isFileExistent(filename)) {
            //std::wcerr << L"no such file or directory: " << filename << std::endl;
            LOG_ERROR(L"no such file or directory: {}", filename);
            return { 0, 0 };
        }
        auto line = utils::readFirstLine(filename);
        auto columns = utils::reSplit(utils::strip(line), L"[\\t ]");
        if (columns.size() != 2) THROW_RTE(L"format error: {}", line);
        LOG_INFOH(L"LEAVE: lsize={}, rsize={}", columns[0], columns[1]);
        return { utils::strToInt(columns[0]), utils::strToInt(columns[1]) };
    }

    std::tuple<size_t, size_t> Connector::readMatrixBinSize(StringRef filename) {
        LOG_INFOH(L"ENTER: filename={}", filename);
        CHECK_OR_THROW(utils::isFileExistent(filename), L"matrix.bin is not found: {}", filename);

        utils::IfstreamReader reader(filename, true);
        CHECK_OR_THROW(reader.success(), L"can't open matrix.bin for read: {}", filename);

        size_t lr_size = 0;
        size_t rl_size = 0;
        reader.read(lr_size);
        reader.read(rl_size);

        CHECK_OR_THROW(lr_size > 0 && rl_size > 0, L"matrix.bin header is invalid: {} (left={}, right={})", filename, lr_size, rl_size);
        LOG_INFOH(L"LEAVE: lr_size={}, rl_size={}", lr_size, rl_size);
        return { lr_size, rl_size };
    }

    // 左側ノードの右接続属性の数を取得
    size_t Connector::left_size() const {
        return pMatrix->lr_size;
    }

    // 右側ノードの左接続属性の数を取得
    size_t Connector::right_size() const {
        return pMatrix->rl_size;
    }

    // 接続コストを取得(lrcAttr: 左側ノードの右接続属性, rlcAttr: 右側ノードの左接続属性)
    int Connector::connectionCost(size_t lrcAttr, size_t rlcAttr) const {
        return pMatrix->getConnectionCost(lrcAttr, rlcAttr);
    }

    // 接続コストを取得(lNode: 左側ノード, rNode: 右側ノード)
    int Connector::cost(const Node& lNode, const Node& rNode) const {
        return connectionCost(lNode.rcAttr(), rNode.lcAttr()) + // rNode.wcost +
            (lNode.isUnknown() && rNode.isUnknown() ? UNK_CONNECT_COST : 0);
    }

    bool Connector::is_valid(int lid, int rid) const {
        return (lid >= 0 && (size_t)lid < pMatrix->rl_size&& rid >= 0 && (size_t)rid < pMatrix->lr_size);
    }
} // namespace analyzer
