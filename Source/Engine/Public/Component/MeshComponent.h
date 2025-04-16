#pragma once

#include "Component.h"
#include "CommonRender.h"

struct MeshComponent : Component {
    QVector<VertexData> vertices;
    QVector<quint16> indices;
    bool rhiDataDirty = true;

    QString meshResourceId = BUILTIN_CUBE_MESH_ID;
};
