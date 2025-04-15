#pragma once

#include <QHash>

#include "ECSCore.h"
#include "Interface/IComponentArray.h"

template<typename T>
class ComponentArray : public IComponentArray {
public:
    void insert(EntityID entity, T component) {
        if (mEntityToIndex.count(entity)) {
            mComponents[mEntityToIndex[entity]] = component;
            return;
        }
        qint32 newIndex = mSize;
        mEntityToIndex[entity] = newIndex;
        mIndexToEntity[newIndex] = entity;
        if (newIndex >= mComponents.size()) {
            mComponents.resize(newIndex + 1);
        }
        mComponents[newIndex] = component;
        mSize++;
    }

    void remove(EntityID entity) {
        if (!mEntityToIndex.count(entity)) return;

        qint32 indexOfRemoved = mEntityToIndex[entity];
        qint32 indexOfLast = mSize - 1;
        mComponents[indexOfRemoved] = mComponents[indexOfLast];

        EntityID entityOfLast = mIndexToEntity[indexOfLast];
        mEntityToIndex[entityOfLast] = indexOfRemoved;
        mIndexToEntity[indexOfRemoved] = entityOfLast;

        mEntityToIndex.remove(entity);
        mIndexToEntity.remove(indexOfLast);
        mSize--;
    }

    void removeEntity(EntityID entity) override {
        remove(entity);
    }


    T *get(EntityID entity) {
        if (!mEntityToIndex.count(entity)) return nullptr;
        return &mComponents[mEntityToIndex[entity]];
    }

    const T *get(EntityID entity) const {
        if (!mEntityToIndex.count(entity)) return nullptr;
        return &mComponents[mEntityToIndex[entity]];
    }

    std::vector<T> &data() { return mComponents; }
    qint32 size() const { return mSize; }

    EntityID getEntity(const qint32 index) const {
        if (mIndexToEntity.count(index)) return mIndexToEntity[index];
        return INVALID_ENTITY;
    }

    bool hasEntity(EntityID entity) const {
        return mEntityToIndex.count(entity);
    }

private:
    QVector<T> mComponents;
    QHash<EntityID, qint32> mEntityToIndex;
    QHash<qint32, EntityID> mIndexToEntity;
    qint32 mSize = 0;
};
