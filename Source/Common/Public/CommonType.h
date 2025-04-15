#pragma once

struct alignas(16) QVector3DAlign {
    QVector3D v;
    float pad = 0.0f;

    constexpr QVector3DAlign() noexcept {
    }

    constexpr QVector3DAlign(float xpos, float ypos, float zpos) noexcept : v{xpos, ypos, zpos} {
    }

    constexpr QVector3DAlign(QVector3D vector) noexcept : v{vector} {
    };

    operator QVector3D &() { return v; }
    operator const QVector3D &() const { return v; }

    float length() const { return v.length(); }
    QVector3DAlign normalized() const { return QVector3DAlign{v.normalized()}; }

    QVector3DAlign &operator+=(const QVector3D &rhs) {
        v += rhs;
        return *this;
    }

    QVector3DAlign &operator-=(const QVector3D &rhs) {
        v -= rhs;
        return *this;
    }

    // 等等……你可以添加更多你常用的 QVector3D 方法
};
