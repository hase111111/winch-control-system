
#ifdef MATH_UTIL_HPP
#define MATH_UTIL_HPP

namespace winch {

template <typename T>
T Clamp(T value, T minVal, T maxVal)
{
    if (value < minVal) return minVal;
    if (value > maxVal) return maxVal;
    return value;
}

}  // namespace winch

#endif // MATH_UTIL_HPP