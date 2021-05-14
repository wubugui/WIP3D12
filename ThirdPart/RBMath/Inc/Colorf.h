#pragma once

#include "./Platform/RBBasedata.h"
#include "RBMath.h"


class RBColor32;
class RBVector4;

class RBColorf
{
public:
	//0.0f-1.0f
	f32 r, g, b, a;
	RBColorf(f32 val) :r(val), g(val), b(val), a(1.f) {}
	RBColorf();
	RBColorf(f32 r, f32 g, f32 b, f32 a);
	RBColorf(f32 r, f32 g, f32 b);
	RBColorf(const RBVector4& v);
	bool equal(const RBColorf& other) const;
	f32 get_grayscale() const;
	void to_linear();
	//[Kinds of operators(+,-,*,/)] 
	inline f32 avg() const { return (r + g + b)*0.3333f; }
	inline f32 average() const { return avg(); }
	RBColorf get_fix_neg()
	{
		if (r < 0) r = 0.f;
		if (g < 0) g = 0.f;
		if (b < 0) b = 0.f;
		if (r > 1) r = 1.f;
		if (g > 1) g = 1.f;
		if (b > 1) b = 1.f;
		return *this;
	}
	RBColorf get_exp() const
	{
		RBColorf ret;
		ret.r = RBMath::exp(r);
		ret.g = RBMath::exp(g);
		ret.b = RBMath::exp(b);
		return ret;
	}
	//ignore a
	RBColorf get_sqrt() const
	{
		RBColorf ret;
		ret.r = RBMath::sqrt(r);
		ret.g = RBMath::sqrt(g);
		ret.b = RBMath::sqrt(b);
		return ret;
	}
	void rand()
	{
		r = RBMath::get_rand_range_f(0.f, 1.f);
		g = RBMath::get_rand_range_f(0.f, 1.f);
		b = RBMath::get_rand_range_f(0.f, 1.f);
	}
	void clamp_to_0_1()
	{
		r = RBMath::clamp(r, 0.f, 1.f);
		g = RBMath::clamp(g, 0.f, 1.f);
		b = RBMath::clamp(b, 0.f, 1.f);
	}

	bool operator==(const RBColorf& o) const
	{
		return r == o.r&&g == o.g&&b == o.g&&a == o.a;
	}

	FORCEINLINE RBColorf& operator=(const RBColorf& b)
	{
		this->r = b.r;
		this->g = b.g;
		this->b = b.b;
		this->a = b.a;
		return *this;
	}

	FORCEINLINE RBColorf operator+(f32 a) const
	{
		return RBColorf(r + a, g + a, b + a, this->a + a);
	}

	FORCEINLINE RBColorf operator+(const RBColorf& o) const
	{
		return RBColorf(r + o.r, g + o.g, b + o.b, a + o.a);
	}

	FORCEINLINE RBColorf operator-(f32 a) const
	{
		return RBColorf(r - a, g - a, b - a, this->a - a);
	}

	FORCEINLINE RBColorf operator-(const RBColorf& o) const
	{
		return RBColorf(r - o.r, g - o.g, b - o.b, a - o.a);
	}

	FORCEINLINE RBColorf operator*(f32 a) const
	{
		return RBColorf(r * a, g * a, b * a, this->a * a);
	}

	FORCEINLINE RBColorf operator*(const RBColorf& o) const
	{
		return RBColorf(r * o.r, g * o.g, b * o.b, a * o.a);
	}

	FORCEINLINE RBColorf operator+=(f32 a)
	{
		r += a; g += a; b += a; this->a += a;
		return *this;
	}

	FORCEINLINE RBColorf operator+=(const RBColorf& o)
	{
		r += o.r; g += o.g; b += o.b; this->a += o.a;
		return *this;
	}

	FORCEINLINE RBColorf operator-=(f32 a)
	{
		r -= a; g -= a; b -= a; this->a -= a;
		return *this;
	}

	FORCEINLINE RBColorf operator-=(const RBColorf& o)
	{
		r -= o.r; g -= o.g; b -= o.b; this->a -= o.a;
		return *this;
	}

	FORCEINLINE RBColorf operator*=(f32 a)
	{
		r *= a; g *= a; b *= a; this->a *= a;
		return *this;
	}

	FORCEINLINE RBColorf operator*=(const RBColorf& o)
	{
		r *= o.r; g *= o.g; b *= o.b; this->a *= o.a;
		return *this;
	}

	RBColorf operator/=(f32 c);

	FORCEINLINE RBColorf operator/(const RBColorf& o) const
	{
		RBColorf c = *this;
		c.r /= o.r; c.g /= o.g; c.b /= o.b; c.a /= o.a;
		return c;
	}

	FORCEINLINE f32 operator[](i32 i) const
	{
		return *((f32*)this + i);
	}

	FORCEINLINE f32& operator[](i32 i)
	{
		return *((f32*)this + i);
	}

	FORCEINLINE f32 y() const
	{
		const float YWeight[3] = { 0.212671f, 0.715160f, 0.072169f };
		return YWeight[0] * r + YWeight[1] * g + YWeight[2] * b;
	}

	FORCEINLINE f32 power_y(f32 times) const
	{
		return y()*times;
	}

	bool is_black(f32 d = 0.00001f) const
	{
		return RBMath::is_nearly_zero(r, d) && RBMath::is_nearly_zero(g, d) && RBMath::is_nearly_zero(b, d);
	}

	bool is_avaliable() const
	{
		bool b1 = RBMath::is_NaN_f32(r);
		bool b2 = RBMath::is_NaN_f32(g);
		bool b3 = RBMath::is_NaN_f32(b);
		bool b4 = RBMath::is_NaN_f32(a);
		return !(b1 || b2 || b3 || b4);
	}

	bool is_avaliable_rgb() const
	{
		bool b1 = RBMath::is_NaN_f32(r);
		bool b2 = RBMath::is_NaN_f32(g);
		bool b3 = RBMath::is_NaN_f32(b);
		return !(b1 || b2 || b3);
	}

	bool isValid() const
	{
		return is_avaliable();
	}

	RBColorf(const RBColor32& color32);


	static const RBColorf
		red,
		green,
		blue,
		white,
		black,
		yellow,
		cyan,
		magenta,
		gray,
		grey,
		blank;


	void out_cyan();

	void out() const;
};

FORCEINLINE RBColorf operator*(f32 scale, const RBColorf& v)
{
	return v.operator*(scale);
}

/*
RBColorf::green(0.f, 1.f, 0.f, 1.f),
RBColorf::blue(0.f, 0.f, 1.f, 1.f),
RBColorf::white(1.f, 1.f, 1.f, 1.f),
RBColorf::black(0.f, 0.f, 0.f, 1.f),
RBColorf::yellow(1.f, 0.9215686f, 0.01568628f, 1.f),
//Çà
RBColorf::cyan(0.f, 1.f, 1.f, 1.f),
//Ñóºì
RBColorf::magenta(1.f, 0.f, 1.f, 1.f),
//»Ò
RBColorf::grey(0.5f, 0.5f, 0.5f, 1.f),
//»Ò
RBColorf::gray(0.5f, 0.5f, 0.5f, 1.f),
//clear
RBColorf::blank(0.f, 0.f, 0.f, 0.f);
*/
template <> struct TIsPODType < RBColorf > { enum { v = true }; };