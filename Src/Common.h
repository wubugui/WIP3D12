#pragma once
#include <cstdint>

#ifdef assert
#undef assert
#endif

#ifdef _DEBUG
#define assert(a)\
    if (!(a)) {\
        std::string str = "assertion failed(" + std::string(#a) + ")\nFile " + __FILE__ + ", line " + std::to_string(__LINE__);\
        LOG_ERROR(str.c_str());\
    }

#define should_not_get_here() assert(false);

#else // _DEBUG

#ifdef _AUTOTESTING
#define assert(a) if (!(a)) throw std::exception("Assertion Failure");
#else // _AUTOTESTING
#define assert(a) ((void)(a))
#endif // _AUTOTESTING

#ifdef _MSC_VER
#define should_not_get_here() __assume(0)
#else // _MSC_VER
#define should_not_get_here() __builtin_unreachable()
#endif // _MSC_VER

#endif // _DEBUG

#define safe_delete(_a) {delete _a; _a = nullptr;}
#define safe_delete_array(_a) {delete[] _a; _a = nullptr;}
#define stringize(a) #a
#define concat_strings_(a, b) a##b
#define concat_strings(a, b) concat_strings_(a, b)

#define enum_class_operators(e_) \
    inline e_ operator& (e_ a, e_ b) { return static_cast<e_>(static_cast<int>(a)& static_cast<int>(b)); } \
    inline e_ operator| (e_ a, e_ b) { return static_cast<e_>(static_cast<int>(a)| static_cast<int>(b)); } \
    inline e_& operator|= (e_& a, e_ b) { a = a | b; return a; }; \
    inline e_& operator&= (e_& a, e_ b) { a = a & b; return a; }; \
    inline e_  operator~ (e_ a) { return static_cast<e_>(~static_cast<int>(a)); } \
    inline bool is_set(e_ val, e_ flag) { return (val & flag) != static_cast<e_>(0); } \
    inline void flip_bit(e_& val, e_ flag) { val = is_set(val, flag) ? (val & (~flag)) : (val | flag); }

//获得第一个高位1的索引（从低位起的索引）
extern uint32_t bitScanReverse(uint32_t a);

#define align_to(_alignment, _val) ((((_val) + (_alignment) - 1) / (_alignment)) * (_alignment))