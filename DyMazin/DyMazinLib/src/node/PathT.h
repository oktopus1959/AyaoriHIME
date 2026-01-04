#pragma once

#include "std_utils.h"
#include "Logger.h"

namespace node
{
    /**
     * 基本的なパス情報を含むトレイト
     */
    struct PathBase {
        /**
         * marginal probability
         */
        double prob;
    };

    //template<class N, class P>
    //class PathT : public PathBase {
    //public:
    //    // pointer to the right node
    //    N* rnode;

    //    // pointer to the next right path
    //    P* rnext;

    //    // pointer to the left node
    //    N* lnode;

    //    // pointer to the next left path
    //    P* lnext;
    //};

    ///**
    // * Path のファクトリ
    // */
    //template<class P>
    //class PathFactoryT {
    //    virtual SharedPtr<P> newNode() = 0;
    //};

} // namespace node