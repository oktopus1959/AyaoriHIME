#include "ContextIDMapper.h"

#include "file_utils.h"
#include "path_utils.h"
#include "my_utils.h"
#include "exception.h"

using namespace util;
using namespace utils;

namespace analyzer {
    DEFINE_CLASS_LOGGER(ContextIDMapper);

    bool ContextIDMapper::open_map(StringRef filename, ContextMap& cmap) {

        CHECK_OR_THROW(exists_path(filename), L"no such file or directory: {}", filename);

        cmap.clear();

        for (const auto& ln : readAllLines(filename)) {
            auto line = strip(ln);
            auto items = tokenizeN(line, L" \t", 2);
            CHECK_OR_THROW(items.size() == 2, L"format error: {}", line);
            auto pos = items[1];
            cmap[items[1]] = utils::parseInt(items[0]);
        }
        return true;
    }

    void ContextIDMapper::addContext(ContextMap& cmap, StringRef key) {
        auto iter = cmap.find(key);
        if (iter == cmap.end()) {
            cmap[key] = (int)cmap.size();
        }
    }

    bool ContextIDMapper::build(ContextMap& cmap, StringRef bos) {
        int id = 1;
        // cmap.toSeq.map(_._1).sorted.foreach{ k => cmap(k) = id; id += 1 }
        for (auto iter = cmap.begin(); iter != cmap.end(); ++iter) {
            iter->second = id++;
        }
        cmap[bos] = 0;
        return true;
    }

    bool ContextIDMapper::save(StringRef filename, const ContextMap& cmap) {
        utils::OfstreamWriter writer(filename);
        CHECK_OR_THROW(writer.success(), L"can't open ContextIDMap file for write: {}", filename);

        //wr.write(
        //  cmap.toSeq.sortBy(_._2).map{case (k,v) => "%d %s\n".format(v,k)}.mkString("")
        //)
        for (const auto& pair : reverseMap(cmap)) {
            writer.writeLine(utils::wconcatAny(pair.first, L" ", pair.second));
        }
        return true;
    }

    std::map<int, String> ContextIDMapper::reverseMap(const ContextMap& cmap) {
        std::map<int, String> rmap;
        for (auto iter = cmap.begin(); iter != cmap.end(); ++iter) {
            rmap[iter->second] = iter->first;
        }
        return rmap;
    }

    short ContextIDMapper::findId(const ContextMap& cmap, StringRef s, const wchar_t* errMsg) {
        auto iter = cmap.find(s);
        if (iter != cmap.end()) {
            return (short)iter->second;
        } else {
            THROW_RTE(L"{}{}", errMsg, s);
            return 0; // dummy
        }
    }
} // namespace analyzer
