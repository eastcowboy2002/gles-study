#ifndef DI_VEC_H_INCLUDED
#define DI_VEC_H_INCLUDED

#include <cmath>
#include <cstddef>
#include <numeric>
#include <functional>

namespace di
{
    const float PI = 3.141592654f;

    template <size_t N>
    struct ConstLoop
    {
        template <typename IT1, typename IT_OUT, typename F>
        static void transform_1(IT1 it1, IT_OUT it_out, const F& f)
        {
            (*it_out) = f(*it1);
            ++it1;
            ++it_out;
            ConstLoop<N-1>::transform_1(it1, it_out, f);
        }

        template <typename IT1, typename IT2, typename IT_OUT, typename F>
        static void transform_2(IT1 it1, IT2 it2, IT_OUT it_out, const F& f)
        {
            (*it_out) = f(*it1, *it2);
            ++it1;
            ++it2;
            ++it_out;
            ConstLoop<N-1>::transform_2(it1, it2, it_out, f);
        }
    };

    template <>
    struct ConstLoop<0>
    {
        template <typename IT1, typename IT_OUT, typename F>
        static void transform_1(IT1 it1, IT_OUT it_out, const F& f)
        {
            // empty
        }

        template <typename IT1, typename IT2, typename IT_OUT, typename F>
        static void transform_2(IT1 it1, IT2 it2, IT_OUT it_out, const F& f)
        {
            // empty
        }
    };

    template <typename T, size_t N>
    class Vec
    {
    public:
        T m_data[N];

        T& operator [] (size_t index) { return m_data[index]; }
        const T& operator [] (size_t index) const { return m_data[index]; }

        // 正负号
        const Vec& operator +() const { return *this; }
        Vec operator - () const { Vec tmp; ConstLoop<N>::transform_1(m_data, tmp.m_data, std::negate<T>()); return tmp; }

        // 向量与标量的运算，结果保存到*this
        Vec& operator += (const T& other) { ConstLoop<N>::transform_1(m_data, m_data, std::bind2nd(std::plus<T>(),       std::cref(other)));    return *this; }
        Vec& operator -= (const T& other) { ConstLoop<N>::transform_1(m_data, m_data, std::bind2nd(std::minus<T>(),      std::cref(other)));    return *this; }
        Vec& operator *= (const T& other) { ConstLoop<N>::transform_1(m_data, m_data, std::bind2nd(std::multiplies<T>(), std::cref(other)));    return *this; }
        Vec& operator /= (const T& other) { ConstLoop<N>::transform_1(m_data, m_data, std::bind2nd(std::divides<T>(),    std::cref(other)));    return *this; }
        Vec& operator %= (const T& other) { ConstLoop<N>::transform_1(m_data, m_data, std::bind2nd(std::modulus<T>(),    std::cref(other)));    return *this; }

        // 向量与向量的运算，结果保存到*this
        Vec& operator += (const Vec<T, N>& other) { ConstLoop<N>::transform_2(m_data, other.m_data, m_data, std::plus<T>()      );  return *this; }
        Vec& operator -= (const Vec<T, N>& other) { ConstLoop<N>::transform_2(m_data, other.m_data, m_data, std::minus<T>()     );  return *this; }
        Vec& operator *= (const Vec<T, N>& other) { ConstLoop<N>::transform_2(m_data, other.m_data, m_data, std::multiplies<T>());  return *this; }
        Vec& operator /= (const Vec<T, N>& other) { ConstLoop<N>::transform_2(m_data, other.m_data, m_data, std::divides<T>()   );  return *this; }
        Vec& operator %= (const Vec<T, N>& other) { ConstLoop<N>::transform_2(m_data, other.m_data, m_data, std::modulus<T>()   );  return *this; }

        // 向量与（向量或者标量）的运算，结果保存到返回值
        template <typename X>
        Vec operator + (const X& other) const { Vec tmp = *this; tmp += other; return tmp; }
        template <typename X>
        Vec operator - (const X& other) const { Vec tmp = *this; tmp -= other; return tmp; }
        template <typename X>
        Vec operator * (const X& other) const { Vec tmp = *this; tmp *= other; return tmp; }
        template <typename X>
        Vec operator / (const X& other) const { Vec tmp = *this; tmp /= other; return tmp; }
        template <typename X>
        Vec operator % (const X& other) const { Vec tmp = *this; tmp %= other; return tmp; }
    };

    // 标量与向量的运算
    template <typename T, size_t N>
    Vec<T, N> operator + (const T& t, const Vec<T, N>& vec) { Vec<T, N> tmp; ConstLoop<N>::transform_1(vec.m_data, tmp.m_data, std::bind1st(std::plus<T>(),       std::cref(t))); return tmp; }
    template <typename T, size_t N>
    Vec<T, N> operator - (const T& t, const Vec<T, N>& vec) { Vec<T, N> tmp; ConstLoop<N>::transform_1(vec.m_data, tmp.m_data, std::bind1st(std::minus<T>(),      std::cref(t))); return tmp; }
    template <typename T, size_t N>
    Vec<T, N> operator * (const T& t, const Vec<T, N>& vec) { Vec<T, N> tmp; ConstLoop<N>::transform_1(vec.m_data, tmp.m_data, std::bind1st(std::multiplies<T>(), std::cref(t))); return tmp; }
    template <typename T, size_t N>
    Vec<T, N> operator / (const T& t, const Vec<T, N>& vec) { Vec<T, N> tmp; ConstLoop<N>::transform_1(vec.m_data, tmp.m_data, std::bind1st(std::divides<T>(),    std::cref(t))); return tmp; }
    template <typename T, size_t N>
    Vec<T, N> operator % (const T& t, const Vec<T, N>& vec) { Vec<T, N> tmp; ConstLoop<N>::transform_1(vec.m_data, tmp.m_data, std::bind1st(std::modulus<T>(),    std::cref(t))); return tmp; }

    // 点乘
    template <typename T>
    struct DotHelpIterator
    {
        T& m_t;

        DotHelpIterator& operator*() { return *this; }
        DotHelpIterator& operator=(const T& t) { m_t += t; return *this; }
        DotHelpIterator& operator ++() { return *this; }
    };

    template <typename T, size_t N>
    T vec_dot(const Vec<T, N>& v1, const Vec<T, N>& v2)
    {
        T t(0);
        DotHelpIterator<T> accIter = {t};
        ConstLoop<N>::transform_2(v1.m_data, v2.m_data, accIter, std::multiplies<T>());
        return t;
    }

    // 叉乘
    template <typename T>
    Vec<T, 3> vec_cross(const Vec<T, 3>& a, const Vec<T, 3>& b)
    {
        Vec<T, 3> tmp = {
            a[1]*b[2] - a[2]*b[1],
            a[2]*b[0] - a[0]*b[2],
            a[0]*b[1] - a[1]*b[0]
        };

        return tmp;
    }

    // 安全叉乘
    template <typename T>
    Vec<T, 3> vec_cross_safe(const Vec<T, 3>& a, const Vec<T, 3>& b)
    {
        Vec<T, 3> tmp = vec_cross(a, b);

        if (vec_abs_square(tmp) < 0.0001f)
        {
            Vec<T, 3> x = {T(1), T(0), T(0)};
            tmp = vec_cross(a, x);
            if (vec_abs_square(tmp) < 0.0001f)
            {
                Vec<T, 3> y = {T(0), T(1), T(0)};
                tmp = vec_cross(a, y);
                if (vec_abs_square(tmp) < 0.0001f)
                {
                    Vec<T, 3> z = {T(0), T(0), T(1)};
                    tmp = vec_cross(a, z);
                }
            }
        }

        return tmp;
    }

    // 求模
    template <typename T, size_t N>
    T vec_abs_square(const Vec<T, N>& v)
    {
        return vec_dot(v, v);
    }

    template <typename T, size_t N>
    T vec_abs(const Vec<T, N>& v)
    {
        return sqrt(vec_dot(v, v));
    }

    // 归一化
    template <typename T, size_t N>
    void vec_normalize_self(Vec<T, N>& v)
    {
        T l = vec_abs(v);
        if (l != T(0))
        {
            v /= l;
        }
    }

    template <typename T, size_t N>
    Vec<T, N> vec_normalize(const Vec<T, N>& v)
    {
        Vec<T, N> tmp = v;
        vec_normalize_self(tmp);
        return tmp;
    }

    template <typename T>
    Vec<T, 4> quaternion_inverse(const Vec<T, 4>& q)
    {
        return MakeVec4(-q[0], -q[1], -q[2], q[3]);
    }

    template <typename T>
    Vec<T, 4> quaternion_ln(const Vec<T, 4>& q)
    {
        // see http://blog.csdn.net/zgce/article/details/8856968
        if (abs(q[3]) < T(1))
        {
            T theta = acos(q[3]);
            T sinTheta = sin(theta);
            if (abs(sinTheta) > T(0))
            {
                T coeff = theta / sinTheta;
                return MakeVec4(coeff * q[0], coeff * q[1], coeff * q[2], T(0));
            }
        }

        return MakeVec4(q[0], q[1], q[2], T(0));
    }

    template <typename T>
    Vec<T, 4> quaternion_exp(const Vec<T, 4>& q)
    {
        // see http://blog.csdn.net/zgce/article/details/8856968
        T theta = vec_abs_square(q);
        if (theta != T(0))
        {
            theta = sqrt(theta);
            T sinTheta = sin(theta);
            return MakeVec4(sinTheta * q[0] / theta, sinTheta * q[1] / theta, sinTheta * q[2] / theta, cosf(theta));
        }
        else
        {
            return MakeVec4(T(0), T(0), T(0), T(1));
        }
    }

    template <typename T>
    Vec<T, 4> quaternion_multiply(const Vec<T, 4>& q1, const Vec<T, 4>& q2)
    {
        // see http://www.winehq.org/pipermail/wine-cvs/2007-November/038183.html
        return MakeVec4(
            q2[3] * q1[0] + q2[0] * q1[3] + q2[1] * q1[2] - q2[2] * q1[1],
            q2[3] * q1[1] - q2[0] * q1[2] + q2[1] * q1[3] + q2[2] * q1[0],
            q2[3] * q1[2] + q2[0] * q1[1] - q2[1] * q1[0] + q2[2] * q1[3],
            q2[3] * q1[3] - q2[0] * q1[0] - q2[1] * q1[1] - q2[2] * q1[2]
            );
    }

    template <typename T>
    Vec<T, 4> quaternion_slerp(const Vec<T, 4>& q1, const Vec<T, 4>& q2, T factor)
    {
        // 《3D数学基础 图形与游戏开发.pdf》 page 156, 157
        T cosOmega = vec_dot(q1, q2);
        Vec<T, 4> cq2 = q2;
        if (cosOmega < T(0))
        {
            cq2 = -cq2;
            cosOmega = -cosOmega;
        }

        T k0, k1;
        if (cosOmega > T(0.9999))
        {
            k0 = 1.0f - factor;
            k1 = factor;
        }
        else
        {
            T sinOmega = sqrt(T(1) - cosOmega * cosOmega);
            T omega = atan2(sinOmega, cosOmega);
            T oneOverSinOmega = T(1) / sinOmega;
            k0 = sin((T(1) - factor) * omega) * oneOverSinOmega;
            k1 = sin( factor         * omega) * oneOverSinOmega;
        }

        return k0 * q1 + k1 * cq2;
    }

    template <typename T>
    Vec<T, 4> quaternion_squad(const Vec<T, 4>& q1, const Vec<T, 4>& q2, const Vec<T, 4>& q3, const Vec<T, 4>& q4, T factor)
    {
        return quaternion_slerp(quaternion_slerp(q1, q4, factor), quaternion_slerp(q2, q3, factor), T(2) * factor * (T(1) - factor));
    }

    template <typename T>
    void quaternion_squad_setup(Vec<T, 4>* out1, Vec<T, 4>* out2, Vec<T, 4>* out3,
                                const Vec<T, 4>& q1, const Vec<T, 4>& q2, const Vec<T, 4>& q3, const Vec<T, 4>& q4)
    {
        Vec<T, 4> tmp1, tmp2, tmp3;

        *out3 = (vec_dot(q2, q3) >= T(0))    ? q3 : -q3;
        tmp2  = (vec_dot(q1, q2) >= T(0))    ? q1 : -q1;
        tmp3  = (vec_dot(*out3, q4) >= T(0)) ? q4 : -q4;

        tmp1 = quaternion_inverse(q2);
        tmp1 = quaternion_ln(quaternion_multiply(tmp1, tmp2)) + quaternion_ln(quaternion_multiply(tmp1, *out3));
        tmp1 = quaternion_exp(tmp1 * T(-0.25));
        *out1 = quaternion_multiply(q2, tmp1);

        tmp1 = quaternion_inverse(*out3);
        tmp1 = quaternion_ln(quaternion_multiply(tmp1, q2)) + quaternion_ln(quaternion_multiply(tmp1, tmp3));
        tmp1 = quaternion_exp(tmp1 * T(-0.25));
        *out2 = quaternion_multiply(*out3, tmp1);
    }

    // 常用类型
    typedef Vec<float, 2> Vec2;
    typedef Vec<float, 3> Vec3;
    typedef Vec<float, 4> Vec4;
    typedef Vec<int, 2> IntVec2;
    typedef Vec<int, 3> IntVec3;
    typedef Vec<int, 4> IntVec4;

    template <typename T>
    Vec<T, 2> MakeVec2(const T& a, const T& b)
    {
        Vec<T, 2> tmp = {a, b};
        return tmp;
    }

    template <typename T>
    Vec<T, 3> MakeVec3(const T& a, const T& b, const T& c)
    {
        Vec<T, 3> tmp = {a, b, c};
        return tmp;
    }

    template <typename T>
    Vec<T, 4> MakeVec4(const T& a, const T& b, const T& c, const T& d)
    {
        Vec<T, 4> tmp = {a, b, c, d};
        return tmp;
    }
}

#endif // DI_VEC_H_INCLUDED
