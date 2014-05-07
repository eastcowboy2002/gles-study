#ifndef DI_MAT_H_INCLUDED
#define DI_MAT_H_INCLUDED

#include "di_vec.h"

namespace di
{
    template <typename T, size_t ROWS, size_t COLS>
    struct Matrix
    {
        typedef T value_type;
        static const size_t rows = ROWS;
        static const size_t cols = COLS;

        T m_data[ROWS * COLS];

//         T* GetColumn(size_t col)
//         {
//             return &m_data[col * ROWS];
//         }

        // ====================================================================
        //   提取数据
        // ====================================================================

        T& operator() (size_t row, size_t col)
        {
            return m_data[col * ROWS + row];
        }

        const T& operator() (size_t row, size_t col) const
        {
            return m_data[col * ROWS + row];
        }

        // ====================================================================
        //   一元运算
        // ====================================================================

        const Matrix& operator +() const
        {
            return *this;
        }

        const Matrix operator -() const
        {
            Matrix result;
            ConstLoop<ROWS * COLS>::transform_1(m_data, result.m_data, std::negate<T>());
            return result;
        }

        // ====================================================================
        //   矩阵乘以矩阵
        // ====================================================================

        template <size_t COLS2>
        const Matrix<T, ROWS, COLS2> operator *(
            const Matrix<T, COLS, COLS2>& rhs) const
        {
            Matrix<T, ROWS, COLS2> result;

            size_t i, j, k;
            for (i = 0; i < ROWS; ++i)
            {
                for (j = 0; j < COLS2; ++j)
                {
                    T sum(0);
                    for (k = 0; k < COLS; ++k)
                    {
                        sum += (*this)(i, k) * rhs(k, j);
                    }

                    result(i, j) = sum;
                }
            }

            return result;
        }

        Matrix& operator *=(const Matrix& rhs)
        {
            *this = (*this) * rhs;
            return *this;
        }

        // ====================================================================
        //   矩阵乘以向量
        // ====================================================================

        const Vec<T, ROWS> operator *(const Vec<T, ROWS>& rhs) const
        {
            Vec<T, ROWS> result;

            size_t i, k;
            for (i = 0; i < ROWS; ++i)
            {
                T sum(0);
                for (k = 0; k < COLS; ++k)
                {
                    sum += (*this)(i, k) * rhs[k];
                }

                result[i] = sum;
            }

            return result;
        }

    };

    // ====================================================================
    //   单位矩阵
    // ====================================================================

    template <typename T, size_t N>
    const Matrix<T, N, N> matrix_identity()
    {
        Matrix<T, N, N> m;
        T zero(0);
        for (size_t i = 0; i < N * N; ++i)
        {
            m.m_data[i] = zero;
        }

        T one(1);
        for (size_t k = 0; k < N * N; k += (N + 1))
        {
            m.m_data[k] = one;
        }

        return m;
    }

    // ====================================================================
    //   逆矩阵，目前仅支持4x4的矩阵求逆
    //   代码从MESA 7.6复制，修改以适应接口
    // ====================================================================

    template <typename T>
    const Matrix<T, 4, 4> matrix_invert(const Matrix<T, 4, 4>& mat)
    {
        const T* m = mat.m_data;
        T inv[16], det;

        inv[0] =   m[5]*m[10]*m[15] - m[5]*m[11]*m[14] - m[9]*m[6]*m[15]
                 + m[9]*m[7]*m[14] + m[13]*m[6]*m[11] - m[13]*m[7]*m[10];
        inv[4] =  -m[4]*m[10]*m[15] + m[4]*m[11]*m[14] + m[8]*m[6]*m[15]
                 - m[8]*m[7]*m[14] - m[12]*m[6]*m[11] + m[12]*m[7]*m[10];
        inv[8] =   m[4]*m[9]*m[15] - m[4]*m[11]*m[13] - m[8]*m[5]*m[15]
                 + m[8]*m[7]*m[13] + m[12]*m[5]*m[11] - m[12]*m[7]*m[9];
        inv[12] = -m[4]*m[9]*m[14] + m[4]*m[10]*m[13] + m[8]*m[5]*m[14]
                 - m[8]*m[6]*m[13] - m[12]*m[5]*m[10] + m[12]*m[6]*m[9];
        inv[1] =  -m[1]*m[10]*m[15] + m[1]*m[11]*m[14] + m[9]*m[2]*m[15]
                 - m[9]*m[3]*m[14] - m[13]*m[2]*m[11] + m[13]*m[3]*m[10];
        inv[5] =   m[0]*m[10]*m[15] - m[0]*m[11]*m[14] - m[8]*m[2]*m[15]
                 + m[8]*m[3]*m[14] + m[12]*m[2]*m[11] - m[12]*m[3]*m[10];
        inv[9] =  -m[0]*m[9]*m[15] + m[0]*m[11]*m[13] + m[8]*m[1]*m[15]
                 - m[8]*m[3]*m[13] - m[12]*m[1]*m[11] + m[12]*m[3]*m[9];
        inv[13] =  m[0]*m[9]*m[14] - m[0]*m[10]*m[13] - m[8]*m[1]*m[14]
                 + m[8]*m[2]*m[13] + m[12]*m[1]*m[10] - m[12]*m[2]*m[9];
        inv[2] =   m[1]*m[6]*m[15] - m[1]*m[7]*m[14] - m[5]*m[2]*m[15]
                 + m[5]*m[3]*m[14] + m[13]*m[2]*m[7] - m[13]*m[3]*m[6];
        inv[6] =  -m[0]*m[6]*m[15] + m[0]*m[7]*m[14] + m[4]*m[2]*m[15]
                 - m[4]*m[3]*m[14] - m[12]*m[2]*m[7] + m[12]*m[3]*m[6];
        inv[10] =  m[0]*m[5]*m[15] - m[0]*m[7]*m[13] - m[4]*m[1]*m[15]
                 + m[4]*m[3]*m[13] + m[12]*m[1]*m[7] - m[12]*m[3]*m[5];
        inv[14] = -m[0]*m[5]*m[14] + m[0]*m[6]*m[13] + m[4]*m[1]*m[14]
                 - m[4]*m[2]*m[13] - m[12]*m[1]*m[6] + m[12]*m[2]*m[5];
        inv[3] =  -m[1]*m[6]*m[11] + m[1]*m[7]*m[10] + m[5]*m[2]*m[11]
                 - m[5]*m[3]*m[10] - m[9]*m[2]*m[7] + m[9]*m[3]*m[6];
        inv[7] =   m[0]*m[6]*m[11] - m[0]*m[7]*m[10] - m[4]*m[2]*m[11]
                 + m[4]*m[3]*m[10] + m[8]*m[2]*m[7] - m[8]*m[3]*m[6];
        inv[11] = -m[0]*m[5]*m[11] + m[0]*m[7]*m[9] + m[4]*m[1]*m[11]
                 - m[4]*m[3]*m[9] - m[8]*m[1]*m[7] + m[8]*m[3]*m[5];
        inv[15] =  m[0]*m[5]*m[10] - m[0]*m[6]*m[9] - m[4]*m[1]*m[10]
                 + m[4]*m[2]*m[9] + m[8]*m[1]*m[6] - m[8]*m[2]*m[5];

        det = m[0]*inv[0] + m[1]*inv[4] + m[2]*inv[8] + m[3]*inv[12];

        if (det == 0)
        {
            return matrix_identity<T, 4>();
        }

        det = T(1) / det;

        Matrix<T, 4, 4> result;
        for (int i = 0; i < 16; i++)
            result.m_data[i] = inv[i] * det;
        return result;
    }

    template <typename T, size_t ROWS, size_t COLS>
    const Matrix<T, COLS, ROWS> matrix_transpose(const Matrix<T, ROWS, COLS>& mat)
    {
        Matrix<T, COLS, ROWS> ret;
        for (size_t i = 0; i < ROWS; ++i)
        {
            for (size_t j = 0; j < COLS; ++j)
            {
                ret(j, i) = mat(i, j);
            }
        }

        return ret;
    }

    // ====================================================================
    //   兼容glTranslate*
    // ====================================================================

    template <typename T>
    const Matrix<T, 4, 4> matrix_translate(const T& x, const T& y, const T& z)
    {
        Matrix<T, 4, 4> m = {
            1, 0, 0, 0,
            0, 1, 0, 0,
            0, 0, 1, 0,
            x, y, z, 1
        };

        return m;
    }

    template <typename T>
    const Matrix<T, 4, 4> matrix_translate(const Vec<T, 3>& vec)
    {
        return matrix_translate(vec[0], vec[1], vec[2]);
    }

    // ====================================================================
    //   兼容glScale*
    // ====================================================================

    template <typename T>
    const Matrix<T, 4, 4> matrix_scale(const T& x, const T& y, const T& z)
    {
        Matrix<T, 4, 4> m = {
            x, 0, 0, 0,
            0, y, 0, 0,
            0, 0, z, 0,
            0, 0, 0, 1
        };

        return m;
    }

    template <typename T>
    const Matrix<T, 4, 4> matrix_scale(const Vec<T, 3>& vec)
    {
        return matrix_scale(vec[0], vec[1], vec[2]);
    }

    // ====================================================================
    //   兼容glRotate*
    // ====================================================================

    template <typename T>
    const Matrix<T, 4, 4> matrix_rotate(const T& cos_theta, const T& sin_theta, const Vec<T, 3>& vn)
    {
        const T one_minus_cos_theta = T(1) - cos_theta;

        Matrix<T, 4, 4> result = {
            cos_theta + one_minus_cos_theta * vn[0] * vn[0],
            one_minus_cos_theta * vn[1] * vn[0] + sin_theta * vn[2],
            one_minus_cos_theta * vn[2] * vn[0] - sin_theta * vn[1],
            0,

            one_minus_cos_theta * vn[0] * vn[1] - sin_theta * vn[2],
            cos_theta + one_minus_cos_theta * vn[1] * vn[1],
            one_minus_cos_theta * vn[2] * vn[1] + sin_theta * vn[0],
            0,

            one_minus_cos_theta * vn[0] * vn[2] + sin_theta * vn[1],
            one_minus_cos_theta * vn[1] * vn[2] - sin_theta * vn[0],
            cos_theta + one_minus_cos_theta * vn[2] * vn[2],
            0,

            0,
            0,
            0,
            1
        };

        return result;
    }

    template <typename T>
    const Matrix<T, 4, 4> matrix_rotate(const T& theta, const Vec<T, 3>& vec)
    {
        const Vec<T, 3> vn = vec_normalize(vec);
        const T sin_theta = sin(theta);
        const T cos_theta = cos(theta);

        return matrix_rotate(cos_theta, sin_theta, vn);
    }

    template <typename T>
    const Matrix<T, 4, 4> matrix_rotate(
        const T& theta, const T& x, const T& y, const T& z)
    {
        Vec<T, 3> v = {x, y, z};
        return matrix_rotate(theta, v);
    }

    // ====================================================================
    //   兼容glOrtho
    // ====================================================================

    template <typename T>
    const Matrix<T, 4, 4> matrix_ortho(const T& left, const T& right,
        const T& bottom, const T& top, const T& zNear, const T& zFar)
    {
        const T tx(- (right + left) / (right - left));
        const T ty(- (top + bottom) / (top - bottom));
        const T tz(- (zFar + zNear) / (zFar - zNear));
        Matrix<T, 4, 4> m = {
            2 / (right - left), 0, 0, 0,
            0, 2 / (top - bottom), 0, 0,
            0, 0, -2 / (zFar - zNear), 0,
            tx, ty, tz, 1
        };

        return m;
    }

    // ====================================================================
    //   兼容glFrustum
    // ====================================================================

    template <typename T>
    const Matrix<T, 4, 4> matrix_frustum(const T& left, const T& right,
        const T& bottom, const T& top, const T& zNear, const T& zFar)
    {
        if (zNear <= 0 || zFar <= 0)
        {
            return matrix_identity<T, 4>();
        }

        const T a((right + left) / (right - left));
        const T b((top + bottom) / (top - bottom));
        const T c(- (zFar + zNear) / (zFar - zNear));
        const T d(-2 * zFar * zNear / (zFar - zNear));
        Matrix<T, 4, 4> m = {
            2 * zNear / (right - left), 0, 0, 0,
            0, 2 * zNear / (top - bottom), 0, 0,
            a, b, c, -1,
            0, 0, d, 0
        };

        return m;
    }

    // ====================================================================
    //   兼容gluPerspective
    // ====================================================================

    template <typename T>
    const Matrix<T, 4, 4> matrix_perspective(const T& fovy, const T& aspect, const T& zNear, const T& zFar)
    {
        const T radians = fovy / 2 * T(PI) / 180;
        const T sine = sin(radians);
        const T deltaZ = zFar - zNear;
        if (deltaZ == 0 || sine == 0 || aspect == 0)
        {
            return matrix_identity<T, 4>();
        }

        const T cotangent = cos(radians) / sine;
        Matrix<T, 4, 4> m = {
            cotangent / aspect, 0, 0, 0,
            0, cotangent, 0, 0,
            0, 0, -(zFar + zNear) / deltaZ, -1,
            0, 0, -2 * zNear * zFar / deltaZ, 0
        };

        return m;
    }

    // ====================================================================
    //   兼容gluLookAt
    // ====================================================================

    template <typename T>
    const Matrix<T, 4, 4> matrix_lookat(const Vec<T, 3>& eye, const Vec<T, 3>& dst, const Vec<T, 3>& up)
    {
        const Vec<T, 3> forward = vec_normalize(dst - eye);
        const Vec<T, 3> side = vec_normalize(vec_cross(forward, up));
        const Vec<T, 3> up2 = vec_cross(side, forward);

        Matrix<T, 4, 4> m = {
            side[0], up2[0], -forward[0], 0,
            side[1], up2[1], -forward[1], 0,
            side[2], up2[2], -forward[2], 0,
            0, 0, 0, 1
        };

        return m * matrix_translate(-eye);
    }

    template <typename T>
    const Matrix<T, 4, 4> matrix_lookat(
        const T& eyex, const T& eyey, const T& eyez,
        const T& dstx, const T& dsty, const T& dstz,
        const T& upx, const T& upy, const T& upz)
    {
        const Vec<T, 3> eye = {eyex, eyey, eyez};
        const Vec<T, 3> dst = {dstx, dsty, dstz};
        const Vec<T, 3> up = {upx, upy, upz};
        return matrix_lookat(eye, dst, up);
    }

    // ====================================================================
    //   向量变换
    // ====================================================================

    typedef Matrix<float, 4, 4> Mat4;

    template <typename T>
    Vec<T, 3> matrix_transform(const Matrix<T, 4, 4>& m, const Vec<T, 3>& v)
    {
        Vec<T, 4> v4 = {v[0], v[1], v[2], T(1)};
        v4 = m * v4;
        return MakeVec3<T>(v4[0], v4[1], v4[2]);    // 不必写为MakeVec3<T>(v4[0] / v4[3], v4[1] / v4[3], v4[2] / v4[3])
    }

    // ====================================================================
    //   四元数
    // ====================================================================

    template <typename T>
    Matrix<T, 4, 4> quaternion_to_matrix(T x, T y, T z, T w)
    {
        // see http://www.linuxgraphics.cn/opengl/opengl_quaternion.html
        T x2 = x * x;
        T y2 = y * y;
        T z2 = z * z;
        T xy = x * y;
        T xz = x * z;
        T yz = y * z;
        T wx = w * x;
        T wy = w * y;
        T wz = w * z;

        Matrix<T, 4, 4> tmp = {
            T(1) - T(2) * (y2 + z2),    T(2) * (xy + wz),           T(2) * (xz - wy),           T(0),
            T(2) * (xy - wz),           T(1) - 2.0f * (x2 + z2),    T(2) * (yz + wx),           T(0),
            T(2) * (xz + wy),           T(2) * (yz - wx),           T(1) - 2.0f * (x2 + y2),    T(0),
            T(0),                       T(0),                       T(0),                       T(1)
        };

        return tmp;
    }

    template <typename T>
    Matrix<T, 4, 4> quaternion_to_matrix(const Vec<T, 4>& q)
    {
        return quaternion_to_matrix(q[0], q[1], q[2], q[3]);
    }
}

#endif // DI_MAT_H_INCLUDED
