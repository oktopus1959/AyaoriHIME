#pragma once

#include "std_utils.h"
#include "util/file_utils.h"

using namespace util;

namespace analyzer {
    /**
     * Dictonary Entry Token
     */
    struct Token {
        //short wtype;        // word type
        int wcost;        // word cost

        String debugString() const {
            //return std::format(_T("wtype:{}, wcost:{}"), wtype, wcost);
            return std::format(_T("wcost:{}"), wcost);
        }

        //Token() : wtype(0), wcost(0) { }
        Token() : wcost(0) { }

        //Token(short wtype, short wcost) : wtype(wtype), wcost(wcost) { }
        Token(int wcost) : wcost(wcost) { }

        void serialize(utils::OfstreamWriter& writer) const {
            //writer.write(wtype);
            writer.write(wcost);
        }

        void deserialize(utils::IfstreamReader& reader) {
            //reader.read(wtype);
            reader.read(wcost);
        }
    };

} // namespace analyzer