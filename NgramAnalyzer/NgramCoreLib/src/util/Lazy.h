#pragma once

#include "std_utils.h"

namespace util {
    /** 具象クラス用(コンストラクタも静的ファクトリも使える) */
    template<class T>
    class Lazy {
        std::function<T* ()> _creator;

        std::function<SharedPtr<T> ()> _sharedCreator;

        mutable SharedPtr<T> _impl;

    public:
        // デフォルトコンストラクタ: T が抽象型でない場合にのみ有効
        template<typename U = T, typename = std::enable_if_t<!std::is_abstract<U>::value>>
        Lazy() : _creator([]() { return new T(); }) { }

        // 引数ありのコンストラクタ
        template<class... Args>
        Lazy(Args&&... args) : _creator([args...]() { return new T(args...); }) { }

        //// 1引数new用コンストラクタ
        //template<class A1>
        //Lazy(A1 arg) : _creator([arg]() {return new T(arg);}) { }

        //// 2引数new用コンストラクタ
        //template<class A1, class A2>
        //Lazy(A1 arg1, A2 arg2) : _creator([arg1, arg2]() {return new T(arg1, arg2);}) { }

        //// 3引数new用コンストラクタ
        //template<class A1, class A2, class A3>
        //Lazy(A1 arg1, A2 arg2, A3 arg3) : _creator([arg1, arg2, arg3]() {return new T(arg1, arg2, arg3);}) { }

        SharedPtr<T> operator()() const {
            if (!_impl) {
                if (_sharedCreator) {
                    _impl = _sharedCreator();
                } else if (_creator) {
                    _impl.reset(_creator());
                }
            }
            return _impl;
        }

        void setCreator(const std::function<T* ()>& creator) {
            _creator = creator;
        }

        void setCreator(const std::function<SharedPtr<T> ()>& sharedCreator) {
            _sharedCreator = sharedCreator;
        }
    };

    // 通常の型用（非抽象型） make_lazy
    template<class T, class... Args,
        std::enable_if_t<!std::is_abstract<T>::value, int> = 0>
    Lazy<T> make_lazy(Args&&... args) {
        return Lazy<T>(std::forward<Args>(args)...);
    }

    // 抽象型対応版： shared_ptr<T>() を返す関数を指定
    template<class T>
    Lazy<T> make_lazy(std::function<std::shared_ptr<T>()> creator) {
        Lazy<T> lazy;
        lazy.setCreator(creator);
        return lazy;
    }

    /** 抽象クラス用(静的ファクトリを使う) */
    template<class T>
    class LazyByCreator {
        std::function<T* ()> _creator;

        std::function<SharedPtr<T> ()> _sharedCreator;

        mutable SharedPtr<T> _impl;

    public:
        LazyByCreator() { }

        SharedPtr<T> operator()() const {
            if (!_impl) {
                if (_sharedCreator) {
                    _impl = _sharedCreator();
                } else {
                    _impl.reset(_creator());
                }
            }
            return _impl;
        }

        void setCreator(const std::function<T* ()>& creator) {
            _creator = creator;
        }

        void setCreator(const std::function<SharedPtr<T> ()>& sharedCreator) {
            _sharedCreator = sharedCreator;
        }
    };

}

#define LAZY_INITIALIZE(X, C)  X.setCreator([this]() { return C; })
