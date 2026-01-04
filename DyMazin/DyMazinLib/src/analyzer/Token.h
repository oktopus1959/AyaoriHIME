#pragma once

#include "std_utils.h"
#include "util/file_utils.h"

using namespace util;

namespace analyzer {
    /**
     * Dictonary Entry Token
     */
    struct Token {
        short lcAttr;       // left connection attribute
        short rcAttr;       // right connection attribute
        //short posId,      // POS-ID
        int wcost;          // word cost
        size_t featurePtr;  // pointer to the feature string

        String debugString() const {
            return std::format(_T("lcAttr:{}, rcAttr:{}, wcost:{}, featurePtr:{}"),
                lcAttr, rcAttr, wcost, featurePtr);
        }

        Token() : lcAttr(0), rcAttr(0), wcost(0), featurePtr(0) { }

        Token(short lcAttr, short rcAttr, int wcost, size_t featPtr)
            : lcAttr(lcAttr), rcAttr(rcAttr), wcost(wcost), featurePtr(featPtr)
        { }

        void serialize(utils::OfstreamWriter& writer) const {
            writer.write(lcAttr);
            writer.write(rcAttr);
            writer.write(wcost);
            writer.write(featurePtr);
        }

        void deserialize(utils::IfstreamReader& reader) {
            reader.read(lcAttr);
            reader.read(rcAttr);
            reader.read(wcost);
            reader.read(featurePtr);
        }
    };

} // namespace analyzer