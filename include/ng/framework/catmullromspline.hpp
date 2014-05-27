#ifndef NG_CATMULLROMSPLINE_HPP
#define NG_CATMULLROMSPLINE_HPP

#include "ng/engine/linearalgebra.hpp"

#include <vector>

namespace ng
{

template<class T, class InterpolationType = T>
class CatmullRomSpline
{
    void ValidateSegment(std::size_t segmentIndex) const
    {
        if (ControlPoints.size() < 4)
        {
            throw std::logic_error("Not enough points for Catmull-Rom (need at least 4)");
        }

        if (segmentIndex > ControlPoints.size() - 4)
        {
            throw std::logic_error("Not enough points for Catmull-Rom (need at least 2 control points before and after each segment)");
        }
    }

public:
    std::vector<vec<T,3>> ControlPoints;

    vec3 CalculatePoint(std::size_t segmentIndex, InterpolationType t) const
    {
        ValidateSegment(segmentIndex);

        return ( ( T(2) * ControlPoints[segmentIndex + 1] ) +
                 ( - ControlPoints[segmentIndex] + ControlPoints[segmentIndex + 2]) * t +
                 ( T(2) * ControlPoints[segmentIndex] - T(5) * ControlPoints[segmentIndex + 1] + T(4) * ControlPoints[segmentIndex + 2] - ControlPoints[segmentIndex + 3]) * t * t +
                 ( - ControlPoints[segmentIndex] + T(3) * ControlPoints[segmentIndex + 1] - T(3) * ControlPoints[segmentIndex + 2] + ControlPoints[segmentIndex + 3]) * t * t * t
                 ) / T(2);
    }

    // returns derivative of spline with respect to t
    vec3 CalculateDerivative(std::size_t segmentIndex, InterpolationType t) const
    {
        ValidateSegment(segmentIndex);

        return ( ( - ControlPoints[segmentIndex] + ControlPoints[segmentIndex + 2]) +
                 T(2) * ( T(2) * ControlPoints[segmentIndex] - T(5) * ControlPoints[segmentIndex + 1] + T(4) * ControlPoints[segmentIndex + 2] - ControlPoints[segmentIndex + 3]) * t +
                 T(3) * ( - ControlPoints[segmentIndex] + T(3) * ControlPoints[segmentIndex + 1] - T(3) * ControlPoints[segmentIndex + 2] + ControlPoints[segmentIndex + 3]) * t * t
                 ) / T(2);
    }
};

} // end namespace ng

#endif // NG_CATMULLROMSPLINE_HPP
