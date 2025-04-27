#pragma once

#include <QWidget>

#include "ECSCore.h"


class World;
class QPushButton;
enum class TextureType;
class QLineEdit;
class ResourceManager;
QT_BEGIN_NAMESPACE

namespace Ui {
    class TextureEditor;
}

QT_END_NAMESPACE

class TextureEditor : public QWidget {
    Q_OBJECT

public:
    explicit TextureEditor(QWidget *parent = nullptr);

    ~TextureEditor() override;

    void setWorld(QSharedPointer<World> world);

    void setResourceManager(QSharedPointer<ResourceManager> resourceManager);

public slots:
    void setCurrentObject(EntityID objId);

    void updateUI();

private slots:
    void onBrowseAlbedo();

    void onBrowseNormal();

    void onBrowseMetallicRoughness();

    void onBrowseAO();

    void onBrowseEmissive();

signals:
    void textureChanged(EntityID entityId, TextureType type, const QString &newPath);

private:
    void setupConnections();

    void blockUIUpdates(bool block);

    void browseForTexture(TextureType type, QLineEdit *displayLineEdit);


    Ui::TextureEditor *ui;
    QSharedPointer<World> mWorld;
    QSharedPointer<ResourceManager> mResourceManager;
    EntityID mCurrentObjId = INVALID_ENTITY;
    bool mUpdatingUI = false;
};
