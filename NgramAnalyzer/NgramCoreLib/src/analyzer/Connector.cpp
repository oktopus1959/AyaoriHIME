#include "misc_utils.h"
#include "my_utils.h"
#include "path_utils.h"
#include "constants/Constants.h"
#include "Logger.h"
#include "Connector.h"
#include "exception.h"

namespace {


}

#if 0
namespace analyzer {
    DECLARE_LOGGER;
    DEFINE_CLASS_LOGGER(Connector);

    // 属性間接続コスト行列
    struct Connector::Matrix {
        std::vector<short> matrix;  // 行列本体
        size_t lr_size = 0;         // 左側ノードの右接続属性の数
        size_t rl_size = 0;         // 右側ノードの左接続属性の数

        // 左側ノードのlr番目の右接続属性と、右側ノードのrl番目の左接続属性の間の接続コストを取得する
        short getConnectionCost(node::WordType leftWordType, node::WordType rigthWordType) const {
            // TODO: word type に基づく接続コスト取得処理を実装する
            return 0;
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

#if 0
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
        if (!reader.success()) {
            LOG_WARN(L"{} is not found. minimum setting is used", ifile);
            reader.setDummyLines(utils::split(MATRIX_DEF_DEFAULT, '\n'));
        }

        size_t lr_size = 0;
        size_t rl_size = 0;
        auto reDelim = std::wregex(L"[\\t ]+");
        {
            auto [line, eof] = reader.getLine();
            if (eof) THROW_RTE(L"no line in file: {}", ifile);

            auto columns = utils::reSplit(line, reDelim);
            if (columns.size() != 2) THROW_RTE(L"format error: {}", line);

            lr_size = utils::strToInt(columns[0]);
            rl_size = utils::strToInt(columns[1]);
            if (lr_size == 0 || rl_size == 0) THROW_RTE(L"format error: {}", line);
        }

        //pConn->pMatrix.reset(new Matrix{ std::vector<short>(lr_size * rl_size), lr_size, rl_size });
        //auto pConn = MakeShared<Connector>(lr_size, rl_size);
        auto pConn = SharedPtr<Connector>(new Connector(lr_size, rl_size));
        Matrix& matrix = *pConn->pMatrix;

        String msg = std::format(L"reading {} ... {}x{}", ifile, lr_size, rl_size);
        LOG_INFOH(msg);
        std::wcout << msg << std::endl;

        size_t percent = 0;

        while (true) {
            auto [ln, eof] = reader.getLine();
            if (eof) break;

            auto line = utils::strip(ln);
            if (line.empty() || line[0] == '#') continue;

            auto cols = utils::reSplit(line, reDelim);
            if (cols.size() != 3) THROW_RTE(L"format error: {}", line);

            size_t lr = utils::strToInt(cols[0]);
            size_t rl = utils::strToInt(cols[1]);
            int c = utils::strToInt(cols[2]);
            if (rl == 0 && leftAttrsWithEOSConnPenalty.contains(lr)) {
                LOG_INFO(L"eof penalty attr={}, penalty={}", lr, penalty);
                c += penalty;
            }
            CHECK_OR_THROW(lr < lr_size && rl < rl_size, L"index values are out of range: lr={}, rl={}", lr, rl);

            size_t pcnt = (lr + 1) * 100 / lr_size;
            if (pcnt > percent) {
                LOG_INFOH(L"{}% done", pcnt);
                percent = pcnt;
            }
            util::progress_bar(L"emitting matrix      ", lr + 1, lr_size);
            matrix.setConnectionCost(lr, rl, c);
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
#endif

    void Connector::deserialize(StringRef filepath) {
        pMatrix->deserialize(filepath);
    }

    void Connector::serialize(StringRef filepath) {
        pMatrix->serialize(filepath);
    }

    std::tuple<size_t, size_t> Connector::readMatrixSize(StringRef filename) {
        if (!utils::isFileExistent(filename)) {
            //std::wcerr << L"no such file or directory: " << filename << std::endl;
            LOG_ERROR(L"no such file or directory: {}", filename);
            return { 0, 0 };
        }
        auto line = utils::readFirstLine(filename);
        auto columns = utils::reSplit(utils::strip(line), L"[\\t ]");
        if (columns.size() != 2) THROW_RTE(L"format error: {}", line);
        return { utils::strToInt(columns[0]), utils::strToInt(columns[1]) };
    }

    // 左側ノードの右接続属性の数を取得
    size_t Connector::left_size() const {
        return pMatrix->lr_size;
    }

    // 右側ノードの左接続属性の数を取得
    size_t Connector::right_size() const {
        return pMatrix->rl_size;
    }

    // 接続コストを取得
    int Connector::connectionCost(node::WordType leftWordType, node::WordType rigthWordType) const {
        return pMatrix->getConnectionCost(leftWordType, rigthWordType);
    }

    // 接続コストを取得(lNode: 左側ノード, rNode: 右側ノード)
    int Connector::cost(const Node& lNode, const Node& rNode) const {
        return connectionCost(lNode.wordType(), rNode.wordType());
    }

    bool Connector::is_valid(int lid, int rid) const {
        return (lid >= 0 && (size_t)lid < pMatrix->rl_size&& rid >= 0 && (size_t)rid < pMatrix->lr_size);
    }
} // namespace analyzer
#endif
