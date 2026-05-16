namespace std {
float sqrt(float);
}

class Vec3 {
public:
    float x;
    float y;
    float z;

    Vec3 operator+(const Vec3 &v)
    {
        Vec3 ret;
        ret.x = x + v.x;
        ret.y = y + v.y;
        ret.z = z + v.z;
        return ret;
    }

    Vec3 operator-(const Vec3 &v)
    {
        Vec3 ret;
        ret.x = x - v.x;
        ret.y = y - v.y;
        ret.z = z - v.z;
        return ret;
    }

    Vec3 operator*(float s)
    {
        Vec3 ret;
        ret.x = x * s;
        ret.y = y * s;
        ret.z = z * s;
        return ret;
    }

    Vec3 operator*(const Vec3 &v)
    {
        Vec3 ret;
        ret.x = x * v.x;
        ret.y = y * v.y;
        ret.z = z * v.z;
        return ret;
    }

    Vec3 operator/(float s)
    {
        Vec3 ret;
        ret.x = x / s;
        ret.y = y / s;
        ret.z = z / s;
        return ret;
    }

    Vec3 operator/(const Vec3 &v)
    {
        Vec3 ret;
        ret.x = x / v.x;
        ret.y = y / v.y;
        ret.z = z / v.z;
        return ret;
    }

    float dot(const Vec3 &v)
    {
        return x * v.x + y * v.y + z * v.z;
    }

    Vec3 cross(const Vec3 &v)
    {
        Vec3 ret;
        ret.x = y * v.z - z * v.y;
        ret.y = z * v.x - x * v.z;
        ret.z = x * v.y - y * v.x;
        return ret;
    }

    float len()
    {
        return std::sqrt(this->dot(*this));
    }

    Vec3 normalized()
    {
        return *this / this->len();
    }

    Vec3 &operator+=(const Vec3 &v)
    {
        return *this = *this + v;
    }

    Vec3 &operator-=(const Vec3 &v)
    {
        return *this = *this - v;
    }

    Vec3 &operator*=(float s)
    {
        return *this = *this * s;
    }

    Vec3 &operator*=(const Vec3 &v)
    {
        return *this = *this * v;
    }

    Vec3 &operator/=(float s)
    {
        return *this = *this / s;
    }

    Vec3 &operator/=(const Vec3 &v)
    {
        return *this = *this / v;
    }
};

int main()
{
    Vec3 a;
    Vec3 b;
    Vec3 c;

    c = a + b;     // calls operator+(const Vec3 &)
    c = a - b;     // calls operator-(const Vec3 &)
    c = a * 123;   // calls operator*(float)
    c = a * b;     // calls operator*(const Vec3 &)
    c = a / 123.f; // calls operator/(float)
    c = a / b;     // calls operator/(const Vec3 &)

    c += a;      // calls operator+=(const Vec3 &)
    c -= a;      // calls operator-=(const Vec3 &)
    c *= 123ULL; // calls operator*=(float)
    c *= a;      // calls operator*=(const Vec3 &)
    c /= 123.L;  // calls operator/=(float)
    c /= a;      // calls operator/=(const Vec3 &)
}
