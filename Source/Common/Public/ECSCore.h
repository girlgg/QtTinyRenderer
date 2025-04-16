#pragma once

#include <typeindex>
#include <type_traits>
#include <QtTypes>

class Component;
class System;

using EntityID = quint32;
constexpr EntityID INVALID_ENTITY = 0;

using SystemTypeID = std::type_index;
using ComponentTypeID = std::type_index;

namespace Internal {
    template<typename T>
    struct SystemTypeIDGenerator {
        static SystemTypeID get() noexcept {
            static_assert(std::is_base_of_v<System, T>,
                          "T must inherit from System");
            return typeid(T);
        }
    };
}

template<typename T>
inline ComponentTypeID getComponentTypeID() noexcept {
    static_assert(std::is_base_of_v<Component, T>,
                  "T must inherit from Component");
    return typeid(T);
}

template<typename T>
inline SystemTypeID getSystemTypeID() noexcept {
    return Internal::SystemTypeIDGenerator<T>::get();
}
