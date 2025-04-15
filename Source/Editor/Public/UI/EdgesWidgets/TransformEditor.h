#pragma once

#include <QWidget>
#include <QVector3D>

QT_BEGIN_NAMESPACE

namespace Ui {
    class TransformEditor;
}

QT_END_NAMESPACE

class TransformEditor : public QWidget {
    Q_OBJECT

public:
    explicit TransformEditor(QWidget *parent = nullptr);

    ~TransformEditor() override;

    void setCurrentObject(int objId);

    int currentObjectId() const { return m_currentObjId; }
signals:
    void transformChanged(int objId, QVector3D newLocation);

    public slots:
        void updateFromSceneManager();

    private slots:
        void updateTransform();

private:
    void setupConnections();

    void blockUIUpdates(bool block);

    Ui::TransformEditor *ui;
    int m_currentObjId = -1;
};
