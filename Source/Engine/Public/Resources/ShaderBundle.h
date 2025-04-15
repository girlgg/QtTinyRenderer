#pragma once

#include <QString>
#include <QVector>
#include <rhi/qrhi.h>
#include <rhi/qshader.h>

struct ShaderPath {
    QString path;
    QRhiShaderStage::Type type;
};

struct ShaderInfo {
    QShader shader;
    QRhiShaderStage::Type type;
};

class ShaderBundle {
public:
    static ShaderBundle *getInstance();

    QShader loadShader(QString path);

    bool loadShader(const QString &name, const QVector<ShaderPath> &shaderPaths);

    QRhiShaderStage getShaderStage(const QString &name, QRhiShaderStage::Type type);

private:
    QHash<QString, QVector<ShaderInfo> > shaderMap;
};
