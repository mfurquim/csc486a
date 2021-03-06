#include "ng/engine/app.hpp"

#include "ng/engine/window/windowmanager.hpp"
#include "ng/engine/window/window.hpp"
#include "ng/engine/window/windowevent.hpp"

#include "ng/engine/filesystem/filesystem.hpp"

#include "ng/engine/rendering/renderer.hpp"
#include "ng/engine/rendering/scenegraph.hpp"
#include "ng/engine/rendering/material.hpp"

#include "ng/engine/math/constants.hpp"

#include "ng/engine/util/memory.hpp"
#include "ng/engine/util/scopeguard.hpp"
#include "ng/engine/util/debug.hpp"

#include "ng/framework/loaders/md5loader.hpp"

#include "ng/framework/meshes/skeletalmesh.hpp"
#include "ng/framework/meshes/md5mesh.hpp"
#include "ng/framework/meshes/basismesh.hpp"
#include "ng/framework/meshes/skeletonwireframemesh.hpp"

#include "ng/framework/models/skeletalmodel.hpp"
#include "ng/framework/models/md5model.hpp"

#include "ng/framework/textures/checkerboardtexture.hpp"

#include "ng/framework/util/fixedstepupdate.hpp"

#include <vector>
#include <chrono>

namespace a4
{

class A4 : public ng::IApp
{
    std::shared_ptr<ng::IWindowManager> mWindowManager;
    std::shared_ptr<ng::IWindow> mWindow;
    std::shared_ptr<ng::IRenderer> mRenderer;
    std::shared_ptr<ng::IFileSystem> mFileSystem;

    ng::SceneGraph mScene;
    std::shared_ptr<ng::SceneGraphCameraNode> mMainCamera;
    std::shared_ptr<ng::SceneGraphCameraNode> mOverlayCamera;

    std::shared_ptr<ng::SceneGraphNode> mAnimationNode;
    std::shared_ptr<ng::immutable<ng::Skeleton>> mAnimationSkeleton;
    std::shared_ptr<ng::IMesh> mAnimationBindPoseMesh;
    std::shared_ptr<ng::SceneGraphNode> mSkeletonNode;
    ng::MD5Anim mAnimationAnim;
    float mCurrentAnimationFrame = 0.0f;
    bool mInBindPose = false;

    std::vector<std::pair<std::string,ng::Material>> mModes;
    std::size_t mCurrentModeIndex = 0;

    ng::FixedStepUpdate mFixedStepUpdate{std::chrono::milliseconds(1000/60)};

public:
    void Init() override
    {
        mWindowManager = ng::CreateWindowManager();

        mWindow = mWindowManager->CreateWindow(
            "a4", 640, 480, 0, 0, ng::VideoFlags());

        mRenderer = ng::CreateRenderer(mWindowManager, mWindow);

        mFileSystem = ng::CreateFileSystem();

        // setup materials
        mModes.emplace_back("NormalColored", ng::MaterialType::NormalColored);

        mModes.emplace_back("Skeleton", ng::MaterialType::Null);

        mModes.emplace_back("Wireframe", ng::MaterialType::Wireframe);

        mModes.emplace_back("Checkered", ng::MaterialType::Textured);

        ng::Material& checkeredMaterial = mModes.back().second;
        checkeredMaterial.Texture0 =
            std::make_shared<ng::CheckerboardTexture>(
                10, 10, 1, ng::vec4(1), ng::vec4(0));
        checkeredMaterial.Sampler0.MinFilter = ng::TextureFilter::Nearest;
        checkeredMaterial.Sampler0.MagFilter = ng::TextureFilter::Nearest;
        checkeredMaterial.Sampler0.WrapX = ng::TextureWrap::ClampToEdge;
        checkeredMaterial.Sampler0.WrapY = ng::TextureWrap::ClampToEdge;

        // setup scene
        std::shared_ptr<ng::SceneGraphNode> rootNode =
                std::make_shared<ng::SceneGraphNode>();
        mScene.Root = rootNode;

        mAnimationNode = std::make_shared<ng::SceneGraphNode>();

        {
            std::shared_ptr<ng::IReadFile> robotMD5MeshFile =
                    mFileSystem->GetReadFile("bob_lamp_update_export.md5mesh",
                                             ng::FileReadMode::Text);

            ng::MD5Model animationModel;
            ng::LoadMD5Mesh(animationModel, *robotMD5MeshFile);

            ng::Skeleton animationSkeleton(ng::Skeleton::FromMD5Model(animationModel));

            mAnimationSkeleton =
                    std::make_shared<ng::immutable<ng::Skeleton>>(
                        std::move(animationSkeleton));

            mAnimationBindPoseMesh =
                    std::make_shared<ng::MD5Mesh>(
                        std::move(animationModel));
        }

        {
            std::shared_ptr<ng::IReadFile> robotMD5AnimFile =
                    mFileSystem->GetReadFile("bob_lamp_update_export.md5anim",
                                             ng::FileReadMode::Text);

            ng::LoadMD5Anim(mAnimationAnim, *robotMD5AnimFile);
        }

        mAnimationNode->Material = checkeredMaterial;
        mAnimationNode->Transform = ng::mat4(
                                            1,0,0,0,
                                            0,0,-1,0,
                                            0,1,0,0,
                                            0,0,0,1);
        rootNode->Children.push_back(mAnimationNode);

        ng::Material vertexColoredMaterial(ng::MaterialType::VertexColored);
        mSkeletonNode = std::make_shared<ng::SceneGraphCameraNode>();
        mSkeletonNode->Material = vertexColoredMaterial;
        mAnimationNode->Children.push_back(mSkeletonNode);

        mMainCamera = std::make_shared<ng::SceneGraphCameraNode>();
        rootNode->Children.push_back(mMainCamera);
        mScene.ActiveCameras.push_back(mMainCamera);

        // setup overlay
        std::shared_ptr<ng::SceneGraphNode> overlayRootNode =
                std::make_shared<ng::SceneGraphNode>();
        mScene.OverlayRoot = overlayRootNode;

        mOverlayCamera = std::make_shared<ng::SceneGraphCameraNode>();
        overlayRootNode->Children.push_back(mOverlayCamera);
        mScene.OverlayActiveCameras.push_back(mOverlayCamera);

        Update(std::chrono::milliseconds(0));
    }

    ng::AppStepAction Step() override
    {
        mFixedStepUpdate.QueuePendingSteps();

        while (mFixedStepUpdate.GetNumPendingSteps() > 0)
        {
            ng::WindowEvent we;
            while (mWindowManager->PollEvent(we))
            {
                if (we.Type == ng::WindowEventType::Quit)
                {
                    return ng::AppStepAction::Quit;
                }
                else
                {
                    HandleEvent(we);
                }
            }

            Update(mFixedStepUpdate.GetStepDuration());
            mFixedStepUpdate.Step();
        }

        {
            const ng::vec3 cornflowerBlue(
                        100.0f / 255.0f,
                        149.0f / 255.0f,
                        237.0f / 255.0f);

            mRenderer->BeginFrame(cornflowerBlue);
            auto endFrameScope = ng::make_scope_guard([&]{
                mRenderer->EndFrame();
            });

            mRenderer->Render(mScene);
        }

        return ng::AppStepAction::Continue;
    }

private:
    ng::vec3 mCameraPosition{6.0f};
    ng::vec3 mCameraTarget{0.0f,3.0f,0.0f};
    float mPendingScrolling= 0.0f;
    bool mIsRightClickHeld = false;
    float mPendingRotation = 0.0f;

    void HandleEvent(const ng::WindowEvent& we)
    {
        if (we.Type == ng::WindowEventType::MouseScroll)
        {
            mPendingScrolling += we.Scroll.Delta;
        }
        else if (we.Type == ng::WindowEventType::MouseButton)
        {
            if (we.Button.Button == ng::MouseButton::Right)
            {
                mIsRightClickHeld = we.Button.State == ng::ButtonState::Pressed;
            }
        }
        else if (we.Type == ng::WindowEventType::MouseMotion)
        {
            if (mIsRightClickHeld)
            {
                int dX = we.Motion.X - we.Motion.OldX;
                mPendingRotation -= (float) dX / mWindow->GetWidth()
                                  * ng::pi<float>::value * 4;
            }
        }
        else if (we.Type == ng::WindowEventType::KeyPress)
        {
            ng::Scancode code = we.KeyPress.Scancode;

            int modeDelta =
                code == ng::Scancode::LeftArrow ? -1
              : code == ng::Scancode::RightArrow ? 1
              : 0;

            if (modeDelta != 0)
            {
                int newModeIndex = mCurrentModeIndex;
                newModeIndex += modeDelta;

                if (newModeIndex < 0)
                {
                    newModeIndex = (int) mModes.size() - 1;
                }

                newModeIndex = newModeIndex % mModes.size();

                mCurrentModeIndex = newModeIndex;
            }

            if (code == ng::Scancode::Space)
            {
                mInBindPose = !mInBindPose;
            }
        }
    }

    void UpdateCameraToWindow()
    {
        mMainCamera->Projection =
                ng::perspective(
                    ng::Radiansf(ng::Degreesf(70.0f)),
                    mWindow->GetAspect(),
                    0.1f, 1000.0f);

        mMainCamera->ViewportTopLeft = ng::ivec2(0,0);

        mMainCamera->ViewportSize = ng::ivec2(
                    mWindow->GetWidth(), mWindow->GetHeight());

        mOverlayCamera->Projection =
                ng::ortho2D(0.0f, (float) mWindow->GetWidth(),
                            0.0f, (float) mWindow->GetHeight());
        mOverlayCamera->ViewportTopLeft - ng::ivec2(0,0);
        mOverlayCamera->ViewportSize = ng::ivec2(
                    mWindow->GetWidth(), mWindow->GetHeight());
    }

    void UpdateCameraZoom()
    {
        ng::vec3 toCamera = mCameraPosition - mCameraTarget;

        ng::vec3 toCameraDelta = normalize(toCamera)
                               * mPendingScrolling
                               * ng::vec3(0.1f);

        if (dot(toCamera + toCameraDelta, toCamera) > 0)
        {
            mCameraPosition = mCameraTarget + toCamera + toCameraDelta;
            mPendingScrolling -= 0.1f * mPendingScrolling;
        }
        else
        {
            mPendingScrolling = 0.0f;
        }
    }

    void UpdateCameraRotation()
    {
        ng::vec3 toCamera = mCameraPosition - mCameraTarget;

        toCamera = ng::rotate3x3(
                    ng::Radiansf(mPendingRotation * 0.1f),
                    ng::vec3(0,1,0)) * toCamera;

        mCameraPosition = mCameraTarget + toCamera;

        mPendingRotation -= mPendingRotation * 0.1f;
    }

    void UpdateCameraTransform(std::chrono::milliseconds)
    {
        UpdateCameraZoom();
        UpdateCameraRotation();

        mMainCamera->Transform =
            inverse(ng::lookAt(mCameraPosition,
                               mCameraTarget,
                               ng::vec3(0.0f,1.0f,0.0f)));
    }

    void Update(std::chrono::milliseconds dt)
    {
        UpdateCameraToWindow();
        UpdateCameraTransform(dt);

        mCurrentAnimationFrame += dt.count() / 1000.0f
                                * mAnimationAnim.FrameRate;
        mCurrentAnimationFrame = std::fmod(mCurrentAnimationFrame,
                                           mAnimationAnim.Frames.size());

        int startFrame = (int) mCurrentAnimationFrame;
        int endFrame = (int) (mCurrentAnimationFrame + 1.0f);
        if (endFrame >= (int) mAnimationAnim.Frames.size())
        {
            // loop over
            endFrame = 0;
        }

        ng::SkeletonLocalPose startLocalPose(
                    ng::SkeletonLocalPose::FromMD5AnimFrame(
                        mAnimationSkeleton->get(), mAnimationAnim,
                        startFrame));

        ng::SkeletonLocalPose endLocalPose(
                    ng::SkeletonLocalPose::FromMD5AnimFrame(
                        mAnimationSkeleton->get(), mAnimationAnim,
                        endFrame));

        ng::SkeletonLocalPose interpolatedPose(
                    ng::SkeletonLocalPose::FromLERPedPoses(
                        startLocalPose, endLocalPose,
                        std::fmod(mCurrentAnimationFrame, 1.0f)));

        ng::SkeletonGlobalPose globalAnimationPose(
                    ng::SkeletonGlobalPose::FromLocalPose(
                        mAnimationSkeleton->get(), interpolatedPose));

        ng::SkinningMatrixPalette animationSkinningPalette =
                ng::SkinningMatrixPalette::FromGlobalPose(
                    mAnimationSkeleton->get(), globalAnimationPose);

        std::shared_ptr<ng::immutable<ng::SkinningMatrixPalette>>
                animationSkinningPalettePtr =
                    std::make_shared<ng::immutable<ng::SkinningMatrixPalette>>(
                        std::move(animationSkinningPalette));

        const std::string& currentModeName =
            mModes.at(mCurrentModeIndex).first;
        const ng::Material& currentMaterial =
            mModes.at(mCurrentModeIndex).second;

        mAnimationNode->Material = currentMaterial;

        if (mInBindPose)
        {
            mAnimationNode->Mesh = mAnimationBindPoseMesh;
        }
        else
        {
            mAnimationNode->Mesh =
                    std::make_shared<ng::SkeletalMesh>(
                        mAnimationBindPoseMesh,
                        animationSkinningPalettePtr);
        }

        if ((currentModeName == "Wireframe" ||
            currentModeName == "Skeleton") && !mInBindPose)
        {
            mSkeletonNode->Mesh =
                    std::make_shared<ng::SkeletonWireframeMesh>(
                        mAnimationSkeleton, animationSkinningPalettePtr);
        }
        else
        {
            mSkeletonNode->Mesh = nullptr;
        }
    }
};

} // end namespace a4

namespace ng
{

std::shared_ptr<IApp> CreateApp()
{
    return std::shared_ptr<IApp>(new a4::A4());
}

} // end namespace ng
