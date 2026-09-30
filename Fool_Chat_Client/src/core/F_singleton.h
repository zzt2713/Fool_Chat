#ifndef F_SINGLETON_H
#define F_SINGLETON_H

#include "global.h"
/******************************************************************************
*
* @file       f_singleton.h
* @brief      单例模式模板
*
* @author     Fool
* @date       2026/02/04
* @history
*****************************************************************************/

template <typename T>
class F_Singleton {
protected:
    static std::shared_ptr<T> _instance;

    F_Singleton() = default;
    F_Singleton(const F_Singleton<T>& ) = delete;
    F_Singleton& operator = (const F_Singleton<T>& s) = delete;

public:
    static std::shared_ptr<T> GetInstance(){
        static std::once_flag flag;
        std::call_once(flag,[&](){
            _instance = std::shared_ptr<T>(new T);
        });
        return _instance;
    }

    void PrintAddress(){
        std::cout << _instance.get() << std::endl;
    }

    ~F_Singleton(){
        std::cout << "this is singleton destruct" << std::endl;
    }
};

template <typename T>
std::shared_ptr<T> F_Singleton<T>::_instance = nullptr;

#endif // F_SINGLETON_H
