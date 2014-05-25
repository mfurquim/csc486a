#include "ng/framework/uvsphere.hpp"

#include "ng/engine/renderer.hpp"
#include "ng/engine/staticmesh.hpp"
#include "ng/engine/constants.hpp"

#include "ng/framework/renderobjectnode.hpp"

#include <vector>

namespace ng
{

UVSphere::UVSphere(std::shared_ptr<IRenderer> renderer)
    : mMesh(renderer->CreateStaticMesh())
{ }

void UVSphere::Init(int numRings, int numSegments, float radius)
{
    if (numRings < 1 || numSegments < 1 || radius < 0)
    {
        throw std::logic_error("Invalid dimensions for UVSphere.");
    }

    std::vector<vec3> sphereVertices;

    float radiansPerRing = 2 * pi<float>::value / numRings;
    float radiansPerSegment = pi<float>::value / numSegments;

    // add all vertices in the sphere
    for (int segment = 0; segment < numSegments + 1; segment++)
    {
        float theta = radiansPerSegment * segment;

        for (int ring = 0; ring < numRings + 1; ring++)
        {
            float phi = radiansPerRing * ring;

            sphereVertices.push_back(radius * vec3(std::sin(theta) * std::cos(phi),
                                                   std::cos(theta),
                                                   std::sin(theta) * std::sin(phi)));
        }
    }

    std::vector<std::uint32_t> sphereIndices;

    // add all the indices
    for (int segment = 0; segment < numSegments; segment++)
    {
        for (int ring = 0; ring < numRings + 1; ring++)
        {
            sphereIndices.push_back(segment * (numRings + 1) + ring);
            sphereIndices.push_back((segment + 1) * (numRings + 1) + ring);
        }

        // every segment except the last one needs degenerate triangles to tie them together
        if (segment < numSegments - 1)
        {
            sphereIndices.push_back((segment + 1) * (numRings + 1) + numRings);
            sphereIndices.push_back((segment + 1) * (numRings + 1));
        }
    }

    std::size_t indexBufferSize = sphereIndices.size() * sizeof(sphereIndices[0]);
    std::size_t indexCount = sphereIndices.size();

    auto pSphereIndices = std::make_shared<std::vector<std::uint32_t>>(std::move(sphereIndices));
    std::shared_ptr<const void> indexBuffer(pSphereIndices->data(), [pSphereIndices](const void*){});

    auto pSphereVertices= std::make_shared<std::vector<vec3>>(std::move(sphereVertices));
    std::shared_ptr<const void> vertexBuffer(pSphereVertices->data(), [pSphereVertices](const void*){});

    VertexFormat gridFormat({
            { VertexAttributeName::Position, VertexAttribute(3, ArithmeticType::Float, false, 0, 0) }
        }, ArithmeticType::UInt32);

    mMesh->Init(gridFormat, {
                    { VertexAttributeName::Position, { vertexBuffer, pSphereVertices->size() * sizeof(vec3) } }
                }, indexBuffer, indexBufferSize, indexCount);

    mNumRings = numRings;
    mNumSegments = numSegments;
    mRadius = radius;
}

RenderObjectPass UVSphere::PreUpdate(std::chrono::milliseconds,
                           RenderObjectNode& node)
{
    node.SetLocalBoundingBox(AxisAlignedBoundingBox<float>(
                                 -vec3(mRadius / 2),
                                  vec3(mRadius / 2)));

    return RenderObjectPass::Continue;
}

RenderObjectPass UVSphere::Draw(
        const std::shared_ptr<IShaderProgram>& program,
        const std::map<std::string, UniformValue>& uniforms,
        const RenderState& renderState)
{
    auto extraUniforms = uniforms;
    extraUniforms.emplace("uTint", vec4(1,0,0,1));

    mMesh->Draw(program, extraUniforms, renderState,
                PrimitiveType::TriangleStrip, 0, mMesh->GetVertexCount());

    return RenderObjectPass::Continue;
}

} // end namespace ng