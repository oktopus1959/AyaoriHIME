#pragma once

#include "DoubleArray.h"

namespace darts
{
    class DoubleArrayTestHelper {
    public:
        static std::shared_ptr<DoubleArrayTestHelper> createHelper();

    public:
        //DoubleArrayTestHelper(std::shared_ptr<DoubleArray> dblAry) : dblArray(dblAry) { }
        virtual std::shared_ptr<DoubleArray> createDoubleArray(
            const std::vector<String>& keys,
            const std::vector<int>& values) = 0;

        virtual const std::vector<int>& baseArray() = 0;
        virtual const std::vector<int>& checkArray() = 0;
        virtual int error() = 0;
        virtual int errorIndex() = 0;
    };
}

