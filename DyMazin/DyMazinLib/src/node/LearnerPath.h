#pragma once

#include "std_utils.h"
#include "string_type.h"
#include "misc_utils.h"
#include "Logger.h"

#include "PathT.h"

namespace node
{
    class LearnerNode;
    class LearnerPath;

    using LearnerNodePtr = SharedPtr<LearnerNode>;
    using LearnerPathPtr = SharedPtr<LearnerPath>;

    class LearnerPath : public PathBase{
        DECLARE_CLASS_LOGGER;
    public:
        LearnerPath();
        ~LearnerPath();

    public:
        // pointer to the right node
        LearnerNodePtr rnode;

        // pointer to the next right path
        LearnerPathPtr rnext;

        // pointer to the left node
        LearnerNodePtr lnode;

        // pointer to the next left path
        LearnerPathPtr lnext;

    public:
        Vector<size_t> fvector;
        double dcost = 0.0;

        bool isEmpty() const;

        bool nonEmpty() {
            return !isEmpty();
        }

        static SharedPtr<LearnerPath> Create();
    };

} // namespace node

using LearnerPathPtr = SharedPtr<node::LearnerPath>;
//typedef node::LearnerPath* LearnerPathPtr;
