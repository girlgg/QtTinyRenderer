#pragma once

#include <QVector3D>

/*!
 * 灯光基类
 * @note 不要出现虚函数，否则结构体首部出现虚函数指针，导致对象偏移
 */
class alignas(16) LightBase {
public:
    void setAmbient(const QVector3D &color) { ambient = color; }
    void setDiffuse(const QVector3D &color) { diffuse = color; }
    void setSpecular(const QVector3D &color) { specular = color; }

    const QVector3D &getAmbient() const { return ambient; }
    const QVector3D &getDiffuse() const { return diffuse; }
    const QVector3D &getSpecular() const { return specular; }

protected:
    QVector3D ambient{0.1f, 0.1f, 0.1f};
    QVector3D diffuse{0.8f, 0.8f, 0.8f};
    QVector3D specular{1.0f, 1.0f, 1.0f};
};

class alignas(16) DirLight : public LightBase {
public:
    void setDirection(const QVector3D &dir) { direction = dir.normalized(); }
    const QVector3D &getDirection() const { return direction; }

private:
    QVector3D direction{0.0f, -1.0f, 0.0f};
};

class PointLight : public LightBase {
public:
    void setPosition(const QVector3D &pos) { position = pos; }
    const QVector3D &getPosition() const { return position; }

    void setAttenuation(float constant, float linear, float quadratic) {
        this->constant = constant;
        this->linear = linear;
        this->quadratic = quadratic;
    }

    float getConstant() const { return constant; }
    float getLinear() const { return linear; }
    float getQuadratic() const { return quadratic; }

private:
    QVector3D position{0.0f, 0.0f, 0.0f};

    // 衰减因子
    float constant{1.0f}; // 常数项
    float linear{0.09f}; // 一次项
    float quadratic{0.032f}; // 二次项
};

class SpotLight : public LightBase {
public:
    void setPosition(const QVector3D &pos) { position = pos; }
    void setDirection(const QVector3D &dir) { direction = dir.normalized(); }

    const QVector3D &getPosition() const { return position; }
    const QVector3D &getDirection() const { return direction; }

    void setCutOffAngles(float innerDegrees, float outerDegrees) {
        cutOff = innerDegrees;
        outerCutOff = outerDegrees;
    }

    void setAttenuation(float constant, float linear, float quadratic) {
        this->constant = constant;
        this->linear = linear;
        this->quadratic = quadratic;
    }

    float getCutOff() const { return cutOff; }
    float getOuterCutOff() const { return outerCutOff; }
    float getConstant() const { return constant; }
    float getLinear() const { return linear; }
    float getQuadratic() const { return quadratic; }

private:
    QVector3D position{0.0f, 0.0f, 0.0f};
    QVector3D direction{0.0f, -1.0f, 0.0f};

    float cutOff{12.5f}; // 内切光角（度）
    float outerCutOff{17.5f}; // 外切光角（度）

    // 衰减因子
    float constant{1.0f};
    float linear{0.09f};
    float quadratic{0.032f};
};
