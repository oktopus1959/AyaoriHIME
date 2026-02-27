#pragma once

#include "std_utils.h"
#include "node/Node.h"
using NodePtr = node::NodePtr;

namespace analyzer
{
    class NBestGenerator {
    public:
        static SharedPtr<NBestGenerator> Create(NodePtr eos_node);

        virtual int next() = 0;
    };

} // namespace analyzer