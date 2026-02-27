#pragma once

#include "std_utils.h"
#include "Logger.h"

namespace node
{
    class Path;
    typedef Path* PathPtr;

    class Node;

    /**
     * Path structure
     */
    class Path {
        DECLARE_CLASS_LOGGER;
    public:
        Path(int id);
        ~Path();

    private:
        // pointer to the right node
        Node* _rnode;

        // pointer to the next right path
        Path* _rnext;

        // pointer to the left node
        Node* _lnode;

        // pointer to the next left path
        Path* _lnext;

    private:
        /**
         * local cost
         */
        int _cost = 0;

        int _id;

        static int _serial;

    public:
        inline Node* rnode() const { return _rnode; }
        inline Node* lnode() const { return _lnode; }
        inline Path* rnext() const { return _rnext; }
        inline Path* lnext() const { return _lnext; }

        inline void setRnode(Node* node) { _rnode = node; }
        inline void setLnode(Node* node) { _lnode = node; }
        inline void setRnext(Path* path) { _rnext = path; }
        inline void setLnext(Path* path) { _lnext = path; }

        inline int cost() const { return _cost; }
        inline void cost(int cost) { _cost = cost; }

        inline int getId() const { return _id; }

        static void Reset();

        String debugString() const;

        // Path ファクトリ
        static SharedPtr<Path> Create();
    };

} // namespace node
