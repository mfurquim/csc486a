#include "ng/framework/scenegraph/scenegraph.hpp"

#include "ng/engine/rendering/uniform.hpp"

#include "ng/framework/scenegraph/camera.hpp"
#include "ng/framework/scenegraph/light.hpp"

#include "ng/framework/scenegraph/renderobject.hpp"
#include "ng/framework/scenegraph/renderobjectnode.hpp"

#include "ng/engine/rendering/renderstate.hpp"
#include "ng/engine/util/scopeguard.hpp"

#include <stack>
#include <algorithm>

namespace ng
{

void SceneGraph::SetRoot(std::shared_ptr<RenderObjectNode> root)
{
    mRoot = std::move(root);
}

void SceneGraph::SetCamera(std::shared_ptr<CameraNode> camera)
{
    mCamera = std::move(camera);
}

void SceneGraph::AddLight(std::weak_ptr<LightNode> light)
{
    mLights.push_back(std::move(light));
}

static void UpdateDepthFirst(
        std::chrono::milliseconds deltaTime,
        const std::shared_ptr<RenderObjectNode>& node)
{
    if (node)
    {
        RenderObjectPass pass = RenderObjectPass::Continue;

        if (node->GetRenderObject())
        {
            pass = node->GetRenderObject()->PreUpdate(deltaTime, *node);
        }

        if (pass != RenderObjectPass::SkipChildren)
        {
            for (const std::shared_ptr<RenderObjectNode>& child : node->GetChildren())
            {
                UpdateDepthFirst(deltaTime, child);
            }
        }

        if (node->GetRenderObject())
        {
            node->GetRenderObject()->PostUpdate(deltaTime, *node);
        }
    }
}

void SceneGraph::Update(std::chrono::milliseconds deltaTime)
{
    UpdateDepthFirst(deltaTime, mRoot);
}

using MatrixStack = std::stack<mat4, std::vector<mat4>>;

static void DrawMultiPassDepthFirst(
        const mat4& projection,
        const mat4& worldView,
        MatrixStack& modelViewStack,
        const std::shared_ptr<IShaderProgram>& program,
        const RenderState& renderState,
        const std::vector<std::weak_ptr<LightNode>>& lights,
        const std::shared_ptr<RenderObjectNode>& node)
{
    if (node)
    {
        struct MatrixStackScope
        {
            MatrixStack& mMatStack;

            MatrixStackScope(MatrixStack& matStack, mat4 m)
                : mMatStack(matStack)
            {
                mat4 newtop = mMatStack.top() * m;
                mMatStack.push(newtop);
            }

            ~MatrixStackScope()
            {
                mMatStack.pop();
            }
        };

        MatrixStackScope modelViewScope(modelViewStack, node->GetLocalTransform());

        RenderObjectPass pass = RenderObjectPass::Continue;

        if (node->GetRenderObject() && !node->IsHidden())
        {
            mat4 modelView = modelViewStack.top();

            std::map<std::string,UniformValue> uniforms{
                { "uProjection", projection },
                { "uModelView", modelView }
            };

            for (const std::weak_ptr<LightNode>& wpLight : lights)
            {
                if (wpLight.expired())
                {
                    continue;
                }

                std::shared_ptr<LightNode> light = wpLight.lock();

                // check if the light intersects with the node
                if (!AABBoxIntersect(light->GetWorldBoundingBox(), node->GetWorldBoundingBox()))
                {
                    // continue;
                }

                vec4 lightViewPos = worldView * light->GetWorldTransform() * vec4(0,0,0,1);
                vec4 lightModelPos = inverse(modelView) * lightViewPos;

                uniforms["uLight.Position"] = UniformValue(vec3(lightModelPos));
                uniforms["uLight.Radius"] = UniformValue(vec1(light->GetLight()->GetRadius()));
                uniforms["uLight.Color"] = UniformValue(light->GetLight()->GetColor());

                pass = node->GetRenderObject()->Draw(program, uniforms, renderState);
            }
        }

        if (pass != RenderObjectPass::SkipChildren)
        {
            for (const std::shared_ptr<RenderObjectNode>& child : node->GetChildren())
            {
                DrawMultiPassDepthFirst(projection, worldView, modelViewStack,
                                        program, renderState, lights, child);
            }
        }
    }
}

void SceneGraph::DrawMultiPass(
        const std::shared_ptr<IShaderProgram>& program,
        const RenderState& renderState) const
{
    const std::shared_ptr<CameraNode>& camera = mCamera;

    RenderState decoratedState = renderState;
    decoratedState.Viewport = camera->GetViewport();
    decoratedState.ActivatedParameters.set(RenderState::Activate_Viewport);

    if (decoratedState.Viewport == ng::ivec4(0,0,0,1))
    {
        throw std::logic_error("Woops, you probably forgot to initialize the viewport with meaningful values.");
    }

    // create the viewWorld matrix
    mat4 worldView = camera->GetWorldView();

    MatrixStack modelViewStack;
    modelViewStack.push(worldView);

    DrawMultiPassDepthFirst(camera->GetProjection(), worldView, modelViewStack,
                            program, decoratedState, mLights, mRoot);
}

} // end namespace ng
