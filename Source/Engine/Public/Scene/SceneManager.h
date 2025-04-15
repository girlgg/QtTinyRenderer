#pragma once

#include "Resources/ShaderBundle.h"
#include "Camera.h"
#include "Primitive.h"
#include "Component/ComponentArray.h"

class SpotLight;
class PointLight;
class DirLight;
class Primitive;

struct SceneObjectInfo {
    int id;
    QString name;
};

struct SceneObjectDefine {
    QString name;
    QSharedPointer<Primitive> objectPtr;
};

static constexpr int MAX_POINT_LIGHTS = 4;
static constexpr int MAX_SPOT_LIGHTS = 4;

class SceneManager : public QObject {
    Q_OBJECT

    friend class RasterizeRenderSystem;

public:
    static SceneManager &get();

    SceneManager();

signals:
    void objectLocationUpdated(int id, QVector3D loc);

public:
    void tick(float deltaTime);

    // --- 对象管理 ---
    void addObject(QString name);

    void setObjectLocation(int objIdx, QVector3D location);

    QVector3D getObjectLocation(int objIdx);

    void getObjects(QVector<SceneObjectInfo> &objects);

    inline QSharedPointer<Camera> getCamera() { return mCamera; }

    // --- 光源管理 ---
    void setDirectionalLight(const DirLight &light);

    void addPointLight();

    void addSpotLight();

    void clearPointLights();

    void clearSpotLights();

    DirLight *getDirectionalLight();

    PointLight *getPointLight(int idx);

    SpotLight *getSpotLight(int idx);

    QVector<SceneObjectDefine> mSceneObjects;

    QSharedPointer<DirLight> mDirLight;
    QVector<QSharedPointer<PointLight> > mPointLights;
    QVector<QSharedPointer<SpotLight> > mSpotLights;

    QSharedPointer<Camera> mCamera;
};
