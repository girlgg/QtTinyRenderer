#include <utility>

#include "Scene/SceneManager.h"

#include <QRandomGenerator>

#include "Component/TransformComponent.h"
#include "Component/CameraControllerComponent.h"
#include "Scene/Light.h"

SceneManager &SceneManager::get() {
    static SceneManager instance;
    return instance;
}

SceneManager::SceneManager() {
    for (int i = 0; i < 10; ++i) {
        addObject("Cube_" + QString::number(i));
        setObjectLocation(i, QVector3D(QRandomGenerator::global()->bounded(4),
                                       QRandomGenerator::global()->bounded(4),
                                       QRandomGenerator::global()->bounded(4)));
    }
    DirLight dirLight;
    dirLight.setDirection(QVector3D(1, 1, 1));
    dirLight.setAmbient(QVector3D(0.5, 0.5, 0.5));
    dirLight.setDiffuse(QVector3D(1, 1, 1));
    dirLight.setSpecular(QVector3D(1, 1, 1));
    setDirectionalLight(dirLight);

    mCamera.reset(new Camera);
    // mEntities.push_back(mCamera);
}

void SceneManager::tick(float deltaTime) {
    mCamera->controller.update(deltaTime, &mCamera->transform);
    // for (auto &entity: mEntities) {
    //     entity->update(deltaTime);
    // }
}

void SceneManager::addObject(QString name) {
    QSharedPointer<Primitive> primitive = QSharedPointer<Primitive>(new Primitive());
    mSceneObjects.append({std::move(name), primitive});

    emit objectLocationUpdated(mSceneObjects.count() - 1, QVector3D(0, 0, 0));
}

void SceneManager::setObjectLocation(int objIdx, QVector3D location) {
    if (objIdx < mSceneObjects.count()) {
        mSceneObjects[objIdx].objectPtr->setLocation(location);
        emit objectLocationUpdated(objIdx, QVector3D(location));
    }
}

QVector3D SceneManager::getObjectLocation(int objIdx) {
    if (objIdx < mSceneObjects.count()) {
        return mSceneObjects[objIdx].objectPtr->getLocation();
    }
    return QVector3D();
}

void SceneManager::getObjects(QVector<SceneObjectInfo> &objects) {
    objects.clear();
    objects.reserve(mSceneObjects.count());
    for (int objIdx = 0; objIdx < mSceneObjects.count(); ++objIdx) {
        objects.append({objIdx, mSceneObjects[objIdx].name});
    }
}

void SceneManager::setDirectionalLight(const DirLight &light) {
    mDirLight.reset(new DirLight(light));
}

void SceneManager::addPointLight() {
    if (mPointLights.size() < MAX_POINT_LIGHTS) {
        mPointLights.emplace_back(new PointLight());
    }
}

void SceneManager::addSpotLight() {
    if (mSpotLights.size() < MAX_SPOT_LIGHTS) {
        mSpotLights.emplace_back(new SpotLight());
    }
}

void SceneManager::clearPointLights() {
    mPointLights.clear();
}

void SceneManager::clearSpotLights() {
    mSpotLights.clear();
}

DirLight *SceneManager::getDirectionalLight() {
    return mDirLight.get();
}

PointLight *SceneManager::getPointLight(int idx) {
    if (mPointLights.size() > idx) return mPointLights[idx].get();
    return nullptr;
}

SpotLight *SceneManager::getSpotLight(int idx) {
    if (mSpotLights.size() > idx) return mSpotLights[idx].get();
    return nullptr;
}
