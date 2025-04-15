#pragma once
#include "Component/ComponentArray.h"
#include "Resources/ShaderBundle.h"

class World {
private:
    // --- ECS ---
    template<typename T>
    QSharedPointer<ComponentArray<T> > getComponentArray() {
        auto typeId = std::type_index(typeid(T));
        if (!mComponentArrays.contains(typeId)) {
            QSharedPointer<ComponentArray<T> > newArray = QSharedPointer<ComponentArray<T> >::create();
            mComponentArrays.insert(typeId, newArray);
        }
        return qSharedPointerCast<ComponentArray<T> >(mComponentArrays[typeId]);
    }

    template<typename T>
    QSharedPointer<const ComponentArray<T>> getComponentArray() const {
        const auto typeId = std::type_index(typeid(T));
        if (!mComponentArrays.contains(typeId)) {
            return nullptr;
        }
        return qSharedPointerCast<const ComponentArray<T>>(mComponentArrays[typeId]);
    }

public:
    EntityID createEntity() {
        EntityID id = mNextEntityId++;
        mEntityComponentTypes[id] = {};
        return id;
    }

    void destroyEntity(EntityID entity) {
        if (mEntityComponentTypes.contains(entity)) {
            for (const auto &typeId: mEntityComponentTypes[entity]) {
                if (mComponentArrays.count(typeId)) {
                    mComponentArrays[typeId]->removeEntity(entity);
                }
            }
            mEntityComponentTypes.remove(entity);
        }
    }

    template<typename T>
    void addComponent(EntityID entity, T component) {
        static_assert(std::is_base_of_v<Component, T>, "T must inherit from Component (for concept check)");
        static_assert(std::is_trivial_v<T> || std::is_standard_layout_v<T>,
                      "Component should be POD-like for performance");

        getComponentArray<T>()->insert(entity, component);

        if (mEntityComponentTypes.contains(entity)) {
            auto &types = mEntityComponentTypes[entity];
            auto typeId = std::type_index(typeid(T));
            if (types.indexOf(typeId) == -1) {
                types.push_back(typeId);
            }
        }
    }

    template<typename T>
    void removeComponent(EntityID entity) {
        getComponentArray<T>()->remove(entity);
        if (mEntityComponentTypes.count(entity)) {
            auto &types = mEntityComponentTypes[entity];
            auto typeId = std::type_index(typeid(T));
            types.removeAll(typeId);
        }
    }

    template<typename T>
    T *getComponent(EntityID entity) {
        return getComponentArray<T>()->get(entity);
    }

    template<typename T>
    const T *getComponent(EntityID entity) const {
        auto array = getComponentArray<T>();
        return array ? array->get(entity) : nullptr;
    }


    template<typename T>
    bool hasComponent(EntityID entity) const {
        auto array = getComponentArray<T>();
        return array && array->hasEntity(entity);
    }

    // --- View/Query System ---
    template<typename T>
    QPair<QVector<T> *, QVector<EntityID> > view() {
        auto array = getComponentArray<T>();
        QVector<EntityID> entities;
        if (!array) return {nullptr, entities};

        entities.reserve(array->size());
        for (size_t i = 0; i < array->size(); ++i) {
            entities.push_back(array->getEntity(i));
        }
        return {&(array->data()), entities};
    }

    // More complex view (entities having ALL specified components)
    // This requires iterating one component array and checking for others.
    template<typename... ComponentTypes>
    class ViewIterator {
        World *mWorld;
        QVector<EntityID> mEntities;
        qint32 mCurrentIndex;

        template<typename First, typename... Rest>
        void filterEntities() {
            auto array = mWorld->getComponentArray<First>();
            if (!array) {
                mEntities.clear();
                return;
            }

            if (mEntities.empty()) {
                // 第一次：从组件数组中提取所有符合条件的实体
                mEntities.reserve(array->size());
                for (size_t i = 0; i < array->size(); ++i) {
                    if (EntityID e = array->getEntity(i); e != INVALID_ENTITY && checkHasComponents<Rest...>(e)) {
                        mEntities.push_back(e);
                    }
                }
            } else {
                // 从已有实体中过滤掉不符合条件的
                QVector<EntityID> filtered;
                filtered.reserve(mEntities.size());
                for (const EntityID &e: std::as_const(mEntities)) {
                    if (mWorld->hasComponent<First>(e) && checkHasComponents<Rest...>(e)) {
                        filtered.append(e);
                    }
                }
                mEntities = std::move(filtered);
            }
        }

        // Base case for template recursion
        template<typename T>
        bool checkHasComponents(EntityID e) const {
            return mWorld->hasComponent<T>(e);
        }

        // Recursive step
        template<typename First, typename Second, typename... Rest>
        bool checkHasComponents(EntityID e) {
            return mWorld->hasComponent<First>(e) && checkHasComponents<Second, Rest...>(e);
        }

    public:
        ViewIterator(World *world) : mWorld(world), mCurrentIndex(0) {
            filterEntities<ComponentTypes...>();
        }

        bool hasNext() const {
            return mCurrentIndex < mEntities.size();
        }

        EntityID next() {
            return mEntities[mCurrentIndex++];
        }

        // Allow range-based for loop (basic implementation)
        struct Sentinel {
        };

        struct Iterator {
            ViewIterator *m_viewIter;
            EntityID operator*() const { return m_viewIter->mEntities[m_viewIter->mCurrentIndex]; }

            Iterator &operator++() {
                ++m_viewIter->mCurrentIndex;
                return *this;
            }

            bool operator!=(const Sentinel &) const { return m_viewIter->hasNext(); }
        };

        Iterator begin() { return Iterator{this}; }
        Sentinel end() { return Sentinel{}; }
    };

    template<typename... ComponentTypes>
    ViewIterator<ComponentTypes...> view() {
        return ViewIterator<ComponentTypes...>(this);
    }

    QVector<QSharedPointer<EntityID> > mEntities;

    EntityID mNextEntityId = 1;
    QHash<std::type_index, QSharedPointer<IComponentArray> > mComponentArrays;
    QHash<EntityID, QVector<std::type_index> > mEntityComponentTypes;
};
