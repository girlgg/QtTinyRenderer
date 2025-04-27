#pragma once
#include <QObject>
#include <QSharedPointer>
#include <assimp/matrix4x4.h>

#include "ECSCore.h"

class World;
class ResourceManager;
class aiMesh;
class aiNode;
class aiScene;

class ModelImporter : public QObject {
    Q_OBJECT

public:
    ModelImporter(QSharedPointer<World> world, QSharedPointer<ResourceManager> resourceManager,
                  QObject *parent = nullptr);

    EntityID importModel(const QString &filePath);

    QMatrix4x4 aiMatrix4x4ToQMatrix4x4(const aiMatrix4x4 &from);

signals:
    void modelImportedSuccessfully();

    void importFailed(const QString &error);

private:
    void processNode(aiNode *node, const aiScene *scene, const QString &modelDir, const QMatrix4x4 &parentTransform,
                     EntityID parentEntity);

    EntityID processMesh(aiMesh *mesh, const aiScene *scene, const QString &modelDir, const QMatrix4x4 &nodeTransform,
                         const QString &baseName);

    QSharedPointer<World> mWorld;
    QSharedPointer<ResourceManager> mResourceManager;
    QString mCurrentModelBasePath;
};
