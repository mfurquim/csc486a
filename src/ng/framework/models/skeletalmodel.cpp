#include "ng/framework/models/skeletalmodel.hpp"

#include "ng/framework/models/md5model.hpp"

namespace ng
{

void SkeletonFromMD5Model(
        const MD5Model& model,
        Skeleton& skeleton)
{
    Skeleton newSkeleton;

    skeleton = std::move(newSkeleton);
}

mat4 PoseToMat4(const SkeletonJointPose& pose)
{
    mat3 R(pose.Rotation);
    mat3 S(scale3x3(pose.Scale));
    mat4 P(R * S);
    P[3][0] = pose.Translation[0];
    P[3][1] = pose.Translation[1];
    P[3][2] = pose.Translation[2];
    return P;
}

void CalculateInverseBindPose(
        const SkeletonJointPose* bindPoseJointPoses,
        SkeletonJoint* joints,
        std::size_t numJoints)
{
    for (std::size_t j = 0; j < numJoints; j++)
    {
        const SkeletonJoint* joint = &joints[j];
        const SkeletonJointPose* pose = &bindPoseJointPoses[j];

        mat4 Pj_M = PoseToMat4(*pose);
        while (joint->Parent != SkeletonJoint::RootJointIndex)
        {
            int parent = joint->Parent;
            joint = &joints[parent];
            pose = &bindPoseJointPoses[parent];

            Pj_M = PoseToMat4(*pose) * Pj_M;
        }

        joints[j].InverseBindPose = inverse(Pj_M);
    }
}

void LocalPosesToGlobalPoses(
        const SkeletonJoint* joints,
        const SkeletonJointPose* localPoses,
        std::size_t numJoints,
        mat4* globalPoses)
{
    for (std::size_t j = 0; j < numJoints; j++)
    {
        const SkeletonJoint* joint = &joints[j];
        const SkeletonJointPose* pose = &localPoses[j];

        mat4 Pj_M = PoseToMat4(*pose);
        while (joint->Parent != SkeletonJoint::RootJointIndex)
        {
            int parent = joint->Parent;
            joint = &joints[parent];
            pose = &localPoses[parent];

            Pj_M = PoseToMat4(*pose) * Pj_M;
        }

        globalPoses[j] = Pj_M;
    }
}

void GlobalPosesToSkinningMatrices(
        const SkeletonJoint* joints,
        const mat4* NG_RESTRICT globalPoses,
        std::size_t numJoints,
        mat4* NG_RESTRICT skinningMatrices)
{
    for (std::size_t j = 0; j < numJoints; j++)
    {
        skinningMatrices[j] = globalPoses[j]
                            * joints[j].InverseBindPose;
    }
}

} // end namespace ng
