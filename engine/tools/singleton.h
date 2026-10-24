#pragma once

namespace elysia::tools
{
template<typename T>
class Singleton
{
public:
    static T* instance()
    {
        static T singleton_instance;
        return &singleton_instance;
    }

protected:
    Singleton() = default;
    ~Singleton() = default;
    Singleton(const Singleton&) = delete;
    Singleton& operator=(const Singleton&) = delete;
};

}
