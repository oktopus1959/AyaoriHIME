#pragma once

#include "std_utils.h"
#include "util/OptHandler.h"

#include "RangeString.h"
#include "node/Node.h"

#define TextWriterPtr SharedPtr<analyzer::TextWriter>

namespace analyzer {
    class Lattice;

    class TextWriter {
        DECLARE_CLASS_LOGGER;
    public:
        // Node情報を返す
        virtual String writeNode(RangeStringPtr sentence, StringRef format, const node::Node& node) = 0;

        // Node情報を返す
        virtual String writeNode(const Lattice& lattice, const node::Node& node) = 0;

        // Lattice 情報を返す
        virtual String write(const Lattice& lattice) = 0;

        // 交ぜ書き情報を返す
        virtual String writeMaze(const Lattice& lattice) = 0;
        //virtual String writeMaze(const Lattice& lattice, int mazeType) = 0;

        ~TextWriter();

    public:
        static TextWriterPtr CreateTextWriter(OptHandlerPtr opts = nullptr);
    };

} // namespace analyzer

