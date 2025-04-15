#include <QFile>

#include "Resources/ShaderBundle.h"

#include <QDirIterator>

ShaderBundle *ShaderBundle::getInstance() {
    static ShaderBundle instance;
    return &instance;
}

QShader ShaderBundle::loadShader(QString path) {
    QFile f(path);
    if (!f.exists()) {
        qWarning() << "[Shader] File not found:" << path;
        return QShader();
    }
    if (f.open(QIODevice::ReadOnly)) {
        return QShader::fromSerialized(f.readAll());
    }
    return QShader();
}

bool ShaderBundle::loadShader(const QString &name, const QVector<ShaderPath> &shaderPaths) {
    QVector<ShaderInfo> shaderPack;
    qDebug() << "Want to load shader" << name;
    for (auto &shaderPath: shaderPaths) {
        QShader shader = loadShader(shaderPath.path);
        if (shader.isValid()) {
            shaderPack.push_back({shader, shaderPath.type});
        }
    }
    qDebug() << "Load shader" << name;
    shaderMap.insert(name, shaderPack);
    return shaderPack.isEmpty();
}

QRhiShaderStage ShaderBundle::getShaderStage(const QString &name, QRhiShaderStage::Type type) {
    const QShader *shader = nullptr;
    if (shaderMap.contains(name)) {
        QVector<ShaderInfo> &shaderList = shaderMap[name];
        for (const ShaderInfo &shaderInfo: shaderList) {
            if (shaderInfo.type == type) {
                shader = &shaderInfo.shader;
                break;
            }
        }
    }

    if (!shader || !shader->isValid()) {
        qWarning() << "Invalid shader stage:" << name << type;
        return QRhiShaderStage();
    }

    return QRhiShaderStage(type, *shader);
}
