#ifndef NG_SQUAREMESH_HPP
#define NG_SQUAREMESH_HPP

#include "ng/engine/rendering/mesh.hpp"

namespace ng
{

class SquareMesh : public IMesh
{
    float mSideLength;

public:
    SquareMesh(float sideLength);

    VertexFormat GetVertexFormat() const override;

    std::size_t GetMaxVertexBufferSize() const override;
    std::size_t GetMaxElementBufferSize() const override;

    std::size_t WriteVertices(void* buffer) const override;
    std::size_t WriteIndices(void* buffer) const override;
};

} // end namespace ng

#endif // NG_SQUAREMESH_HPP
