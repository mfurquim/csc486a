#ifndef NG_SKELETALMESH_HPP
#define NG_SKELETALMESH_HPP

#include "ng/engine/rendering/mesh.hpp"

#include <memory>

namespace ng
{

class ImmutableSkinningMatrixPalette;

class SkeletalMesh : public IMesh
{
    const std::shared_ptr<IMesh> mBindPoseMesh;
    const std::shared_ptr<ImmutableSkinningMatrixPalette> mSkinningPalette;

public:
    SkeletalMesh(
            std::shared_ptr<IMesh> bindPoseMesh,
            std::shared_ptr<ImmutableSkinningMatrixPalette> skinningPalette);

    VertexFormat GetVertexFormat() const override;

    std::size_t GetMaxVertexBufferSize() const override;
    std::size_t GetMaxIndexBufferSize() const override;

    std::size_t WriteVertices(void* buffer) const override;
    std::size_t WriteIndices(void* buffer) const override;
};

} // end namespace ng

#endif // NG_SKELETALMESH_HPP
