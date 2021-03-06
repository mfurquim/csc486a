#include "ng/framework/loaders/md5loader.hpp"
#include "ng/framework/models/md5model.hpp"
#include "ng/engine/filesystem/readfile.hpp"

#include <sstream>

namespace ng
{

class MD5ParserBase
{
protected:
    std::string& mError;
    std::stringstream mInputStream;

    void EatWhitespace()
    {
        while (std::isspace(mInputStream.peek()))
        {
            mInputStream.get();
        }
    }

    bool AcceptIdentifier(std::string& id)
    {
        if (!(mInputStream >> id))
        {
            mError = "Expected identifier";
            return false;
        }

        if (!std::isalpha(id[0]))
        {
            mError = "Expected identifier "
                     "(identifiers begin with alpha character)";
            return false;
        }

        for (std::size_t i = 1; i < id.size(); i++)
        {
            if (!std::isalnum(id[i]))
            {
                mError =
                        "Expected identifier "
                        "(identifiers must be made of alphanumeric characters)";
            }
        }

        return true;
    }

    bool RequireIdentifier(const std::string& required)
    {
        std::string id;
        if (!AcceptIdentifier(id) || id != required)
        {
            mError = "Expected " + required;
            return false;
        }
        return true;
    }

    bool AcceptDoubleQuotedString(std::string& s)
    {
        if (RequireChar('"'))
        {
            s.clear();
            bool escaped = false;
            while (escaped || mInputStream.peek() != '"')
            {
                char peeked = mInputStream.peek();

                if (escaped)
                {
                    if (peeked == '\\' || peeked == '"')
                    {
                        s += peeked;
                    }
                    else if (peeked == 'n')
                    {
                        s += '\n';
                    }
                    else if (peeked == 't')
                    {
                        s += '\t';
                    }
                    else if (peeked == 'r')
                    {
                        s += '\r';
                    }
                    else
                    {
                        mError = "Unescapable character: ";
                        mError += peeked;
                        return false;
                    }

                    escaped = false;
                }
                else
                {
                    if (peeked == '\\')
                    {
                        escaped = true;
                    }
                    else
                    {
                        s += peeked;
                    }
                }

                mInputStream.get();
            }

            if (RequireChar('"'))
            {
                return true;
            }
        }

        return false;
    }

    bool AcceptChar(char& ch)
    {
        if (!(mInputStream >> ch))
        {
            mError = "Expected char";
            return false;
        }
        return true;
    }

    bool RequireChar(char required)
    {
        char ch;
        if (!AcceptChar(ch) || ch != required)
        {
            mError = "Expected ";
            mError += required;
            return false;
        }
        return true;
    }

    bool AcceptInt(int& i)
    {
        if (!(mInputStream >> i))
        {
            mError = "Expected int";
            return false;
        }
        return true;
    }

    bool RequireInt(int required)
    {
        int i;
        if (!AcceptInt(i) || i != required)
        {
            mError = "Expected " + std::to_string(i);
            return false;
        }
        return true;
    }

    bool AcceptFloat(float& f)
    {
        if (!(mInputStream >> f))
        {
            mError = "Expected float";
            return false;
        }
        return true;
    }

    bool RequireFloat(float required)
    {
        float f;
        if (!AcceptFloat(f) || f != required)
        {
            mError = "Expected " + std::to_string(f);
            return false;
        }
        return true;
    }

    bool AcceptVersion(int& version)
    {
        if (RequireIdentifier("MD5Version") && RequireInt(10))
        {
            version = 10;
            return true;
        }

        return false;
    }

    bool AcceptCommandLine(std::string& commandline)
    {
        std::string cmd;
        if (RequireIdentifier("commandline") &&
            AcceptDoubleQuotedString(cmd))
        {
            commandline = std::move(cmd);
            return true;
        }

        return false;
    }

public:
    MD5ParserBase(std::string& error)
        : mError(error)
    { }

    virtual bool Parse() = 0;
};

class MD5MeshParser : public MD5ParserBase
{
    MD5Model& mModel;

    int mNumExpectedJoints;
    int mNumExpectedMeshes;

    int mNumExpectedVertices;
    int mNumExpectedTriangles;
    int mNumExpectedWeights;

    // used to check that the same index isn't
    // used by two vertices/triangles/weights
    std::vector<bool> mExpectedIndices;

    bool AcceptNumJoints()
    {
        int numJoints;
        if (RequireIdentifier("numJoints") && AcceptInt(numJoints))
        {
            if (numJoints < 0)
            {
                mError = "numJoints < 0";
                return false;
            }

            mModel.BindPoseJoints.reserve(numJoints);
            mNumExpectedJoints = numJoints;
            return true;
        }

        return false;
    }

    bool AcceptNumMeshes()
    {
        int numMeshes;
        if (RequireIdentifier("numMeshes") && AcceptInt(numMeshes))
        {
            if (numMeshes < 0)
            {
                mError = "numMeshes < 0";
                return false;
            }

            mModel.Meshes.reserve(numMeshes);
            mNumExpectedMeshes = numMeshes;

            return true;
        }

        return false;
    }

    bool AcceptJoint()
    {
        MD5Joint joint;
        if (AcceptDoubleQuotedString(joint.Name) &&
            AcceptInt(joint.ParentIndex) &&
            RequireChar('(') &&
                AcceptFloat(joint.Position[0]) &&
                AcceptFloat(joint.Position[1]) &&
                AcceptFloat(joint.Position[2]) &&
            RequireChar(')') &&
            RequireChar('(') &&
                AcceptFloat(joint.Orientation[0]) &&
                AcceptFloat(joint.Orientation[1]) &&
                AcceptFloat(joint.Orientation[2]) &&
            RequireChar(')'))
        {
            if (joint.ParentIndex < -1 ||
                joint.ParentIndex >= mNumExpectedJoints ||
                joint.ParentIndex == (int) mModel.BindPoseJoints.size())
            {
                mError = "Parent index out of range, or self-referential";
                return false;
            }

            if (joint.ParentIndex > (int) mModel.BindPoseJoints.size())
            {
                mError = "Parent joints must appear before their children.";
                return false;
            }

            mModel.BindPoseJoints.push_back(std::move(joint));

            return true;
        }

        return false;
    }

    bool AcceptJoints()
    {
        int numAcceptedJoints = 0;

        if (RequireIdentifier("joints") && RequireChar('{'))
        {
            while (true)
            {
                EatWhitespace();
                if (mInputStream.peek() == '}')
                {
                    mInputStream.get();
                    break;
                }

                if (!AcceptJoint())
                {
                    return false;
                }

                numAcceptedJoints++;
            }

            if (numAcceptedJoints != mNumExpectedJoints)
            {
                mError = "Expected " + std::to_string(mNumExpectedJoints)
                       + " joints, but got "
                       + std::to_string(numAcceptedJoints);
                return false;
            }

            return true;
        }

        return false;
    }

    bool AcceptNumVertices()
    {
        int numverts;
        if (RequireIdentifier("numverts") && AcceptInt(numverts))
        {
            if (numverts < 0)
            {
                mError = "numverts < 0";
                return false;
            }

            mModel.Meshes.back().Vertices.resize(numverts);
            mNumExpectedVertices = numverts;

            mExpectedIndices.clear();
            mExpectedIndices.resize(mNumExpectedVertices);

            return true;
        }

        return false;
    }

    bool AcceptVertex()
    {
        MD5Vertex vertex;
        int vertexIndex;
        if (RequireIdentifier("vert") &&
            AcceptInt(vertexIndex) &&
            RequireChar('(') &&
                AcceptFloat(vertex.Texcoords[0]) &&
                AcceptFloat(vertex.Texcoords[1]) &&
            RequireChar(')') &&
            AcceptInt(vertex.StartWeight) &&
            AcceptInt(vertex.WeightCount))
        {
            if (vertexIndex < 0 || vertexIndex >= mNumExpectedVertices)
            {
                mError = "vertexIndex out of bounds";
                return false;
            }

            if (vertex.StartWeight < 0)
            {
                mError = "StartWeight < 0";
                return false;
            }

            if (vertex.WeightCount < 0)
            {
                mError = "WeightCount < 0";
                return false;
            }

            if (mExpectedIndices[vertexIndex] == true)
            {
                mError = "Duplicate vertexIndex";
                return false;
            }

            mExpectedIndices[vertexIndex] = true;

            mModel.Meshes.back().Vertices[vertexIndex] = std::move(vertex);
            return true;
        }

        return false;
    }

    bool AcceptVertices()
    {
        int numAcceptedVertices = 0;

        while (true)
        {
            EatWhitespace();

            if (mInputStream.peek() != 'v')
            {
                break;
            }

            if (!AcceptVertex())
            {
                return false;
            }

            numAcceptedVertices++;
        }

        if (numAcceptedVertices != mNumExpectedVertices)
        {
            mError = "Expected " + std::to_string(mNumExpectedVertices)
                   + " vertices, but got " + std::to_string(numAcceptedVertices);
            return false;
        }

        return true;
    }

    bool AcceptNumTriangles()
    {
        int numtris;
        if (RequireIdentifier("numtris") && AcceptInt(numtris))
        {
            if (numtris < 0)
            {
                mError = "numtris < 0";
                return false;
            }

            mModel.Meshes.back().Triangles.resize(numtris);
            mNumExpectedTriangles = numtris;

            mExpectedIndices.clear();
            mExpectedIndices.resize(mNumExpectedTriangles);

            return true;
        }

        return false;
    }

    bool AcceptTriangle()
    {
        MD5Triangle triangle;
        int triangleIndex;
        if (RequireIdentifier("tri") &&
            AcceptInt(triangleIndex) &&
            AcceptInt(triangle.VertexIndices[0]) &&
            AcceptInt(triangle.VertexIndices[1]) &&
            AcceptInt(triangle.VertexIndices[2]))
        {
            if (triangleIndex < 0 ||
                triangleIndex >= mNumExpectedTriangles)
            {
                mError = "triangleIndex out of bounds";
                return false;
            }

            if (triangle.VertexIndices[0] < 0 ||
                triangle.VertexIndices[0] >= mNumExpectedVertices)
            {
                mError = "VertexIndices[0] out of bounds";
                return false;
            }

            if (triangle.VertexIndices[1] < 0 ||
                triangle.VertexIndices[1] >= mNumExpectedVertices)
            {
                mError = "VertexIndices[1] out of bounds";
                return false;
            }

            if (triangle.VertexIndices[2] < 0 ||
                triangle.VertexIndices[2] >= mNumExpectedVertices)
            {
                mError = "VertexIndices[2] out of bounds";
                return false;
            }

            if (mExpectedIndices[triangleIndex] == true)
            {
                mError = "duplicate triangleIndex";
                return false;
            }

            mExpectedIndices[triangleIndex] = true;

            mModel.Meshes.back().Triangles[triangleIndex] = std::move(triangle);

            return true;
        }

        return false;
    }

    bool AcceptTriangles()
    {
        int numAcceptedTriangles = 0;

        while (true)
        {
            EatWhitespace();

            if (mInputStream.peek() != 't')
            {
                break;
            }

            if (!AcceptTriangle())
            {
                return false;
            }

            numAcceptedTriangles++;
        }

        if (numAcceptedTriangles != mNumExpectedTriangles)
        {
            mError = "Expected " + std::to_string(mNumExpectedTriangles)
                   + " triangles, but got " + std::to_string(numAcceptedTriangles);
            return false;
        }

        return true;
    }

    bool AcceptNumWeights()
    {
        int numweights;
        if (RequireIdentifier("numweights") && AcceptInt(numweights))
        {
            if (numweights < 0)
            {
                mError = "numweights < 0";
                return false;
            }

            mModel.Meshes.back().Weights.resize(numweights);
            mNumExpectedWeights = numweights;

            mExpectedIndices.clear();
            mExpectedIndices.resize(mNumExpectedWeights);

            return true;
        }

        return false;
    }

    bool AcceptWeight()
    {
        MD5Weight weight;
        int weightIndex;
        if (RequireIdentifier("weight") &&
            AcceptInt(weightIndex) &&
            AcceptInt(weight.JointIndex) &&
            AcceptFloat(weight.WeightBias) &&
            RequireChar('(') &&
                AcceptFloat(weight.WeightPosition[0]) &&
                AcceptFloat(weight.WeightPosition[1]) &&
                AcceptFloat(weight.WeightPosition[2]) &&
            RequireChar(')'))
        {
            if (weightIndex < 0 ||
                weightIndex >= mNumExpectedWeights)
            {
                mError = "weightIndex out of bounds";
                return false;
            }

            if (weight.JointIndex < 0 ||
                weight.JointIndex >= mNumExpectedJoints)
            {
                mError = "JointIndex out of bounds";
                return false;
            }

            if (mExpectedIndices[weightIndex] == true)
            {
                mError = "duplicate weightIndex";
                return false;
            }

            mExpectedIndices[weightIndex] = true;

            mModel.Meshes.back().Weights[weightIndex] = std::move(weight);

            return true;
        }

        return false;
    }

    bool AcceptWeights()
    {
        int numAcceptedWeights = 0;

        while (true)
        {
            EatWhitespace();

            if (mInputStream.peek() != 'w')
            {
                break;
            }

            if (!AcceptWeight())
            {
                return false;
            }

            numAcceptedWeights++;
        }

        if (numAcceptedWeights != mNumExpectedWeights)
        {
            mError = "Expected " + std::to_string(mNumExpectedWeights)
                   + " weights, but got " + std::to_string(numAcceptedWeights);
            return false;
        }

        // bounds-check the weights in the vertices

        for (const MD5Vertex& vert : mModel.Meshes.back().Vertices)
        {
            if (vert.StartWeight >= mNumExpectedWeights ||
                vert.StartWeight + vert.WeightCount > mNumExpectedWeights)
            {
                mError = "StartWeight/WeightCount out of bounds";
                return false;
            }
        }

        return true;
    }

    bool AcceptMesh()
    {
        if (RequireIdentifier("mesh") &&
            RequireChar('{'))
        {
            mModel.Meshes.emplace_back();

            return RequireIdentifier("shader") &&
                   AcceptDoubleQuotedString(mModel.Meshes.back().Shader) &&
                   AcceptNumVertices() &&
                   AcceptVertices() &&
                   AcceptNumTriangles() &&
                   AcceptTriangles() &&
                   AcceptNumWeights() &&
                   AcceptWeights() &&
                   RequireChar('}');
        }

        return false;
    }

    bool AcceptMeshes()
    {
        int numAcceptedMeshes = 0;

        while (true)
        {
            EatWhitespace();

            if (mInputStream.peek() == std::stringstream::traits_type::eof())
            {
                break;
            }

            if (!AcceptMesh())
            {
                return false;
            }

            numAcceptedMeshes++;
        }

        if (numAcceptedMeshes != mNumExpectedMeshes)
        {
            mError = "Expected " + std::to_string(mNumExpectedMeshes)
                   + " meshes, but got " + std::to_string(numAcceptedMeshes);
            return false;
        }

        return true;
    }

public:
    MD5MeshParser(
            MD5Model& model,
            IReadFile& md5meshFile,
            std::string& error)
        : MD5ParserBase(error)
        , mModel(model)
    {
        std::string line;
        while (getline(line, md5meshFile))
        {
            line = line.substr(0, line.find("//"));
            mInputStream << line << '\n';
        }
    }

    bool Parse() override
    {
        return AcceptVersion(mModel.MD5Version) &&
               AcceptCommandLine(mModel.CommandLine) &&
               AcceptNumJoints() &&
               AcceptNumMeshes() &&
               AcceptJoints() &&
               AcceptMeshes();
    }
};

class MD5AnimParser : public MD5ParserBase
{
    MD5Anim& mAnim;

    int mNumExpectedFrames;
    int mNumExpectedJoints;
    int mNumExpectedAnimatedComponents;

    bool AcceptNumFrames()
    {
        int numFrames;
        if (RequireIdentifier("numFrames") && AcceptInt(numFrames))
        {
            if (numFrames < 0)
            {
                mError = "numFrames < 0";
                return false;
            }

            mAnim.FrameBounds.reserve(numFrames);
            mAnim.Frames.reserve(numFrames);
            mNumExpectedFrames = numFrames;

            return true;
        }

        return false;
    }

    bool AcceptNumJoints()
    {
        int numJoints;
        if (RequireIdentifier("numJoints") && AcceptInt(numJoints))
        {
            if (numJoints < 0)
            {
                mError = "numJoints < 0";
                return false;
            }

            mAnim.Joints.reserve(numJoints);
            mAnim.BaseFrame.reserve(numJoints);
            mNumExpectedJoints = numJoints;

            return true;
        }

        return false;
    }

    bool AcceptFrameRate()
    {
        int frameRate;
        if (RequireIdentifier("frameRate") && AcceptInt(frameRate))
        {
            if (frameRate < 0)
            {
                mError = "frameRate < 0";
                return false;
            }

            mAnim.FrameRate = frameRate;

            return true;
        }

        return false;
    }

    bool AcceptNumAnimatedComponents()
    {
        int numAnimatedComponents;
        if (RequireIdentifier("numAnimatedComponents") &&
            AcceptInt(numAnimatedComponents))
        {
            if (numAnimatedComponents < 0)
            {
                mError = "numAnimatedComponents < 0";
                return false;
            }

            mNumExpectedAnimatedComponents = numAnimatedComponents;

            return true;
        }

        return false;
    }

    bool AcceptHierarchyJoint()
    {
        MD5AnimationJoint joint;
        int flags;
        if (AcceptDoubleQuotedString(joint.Name) &&
            AcceptInt(joint.ParentIndex) &&
            AcceptInt(flags) &&
            AcceptInt(joint.StartIndex))
        {
            if (joint.ParentIndex < -1 ||
                joint.ParentIndex >= mNumExpectedJoints ||
                joint.ParentIndex == (int) mAnim.Joints.size())
            {
                mError = "parentIndex out of bounds or self-referential";
                return false;
            }

            if (joint.ParentIndex > (int) mAnim.Joints.size())
            {
                mError = "Parent joints must appear before their children.";
                return false;
            }

            if (joint.StartIndex < 0 ||
                joint.StartIndex >= mNumExpectedAnimatedComponents)
            {
                mError = "joint's startIndex is out of bounds";
                return false;
            }

            if (flags < 0)
            {
                mError = "flags must be a positive number";
                return false;
            }

            joint.Flags = flags;

            if ((joint.Flags | 0x3F) != 0x3F)
            {
                mError = "flags may only have 6 least significant bits set "
                         "(numbers from 0 to 63 inclusive.)";
                return false;
            }

            mAnim.Joints.push_back(std::move(joint));
            return true;
        }

        return false;
    }

    bool AcceptHierarchy()
    {
        int numAcceptedJoints = 0;
        if (RequireIdentifier("hierarchy") && RequireChar('{'))
        {
            while (true)
            {
                EatWhitespace();

                if (mInputStream.peek() == '}')
                {
                    mInputStream.get();

                    break;
                }
                else
                {
                    if (!AcceptHierarchyJoint())
                    {
                        return false;
                    }

                    numAcceptedJoints++;
                }
            }

            if (numAcceptedJoints != mNumExpectedJoints)
            {
                mError = "Expected " + std::to_string(mNumExpectedJoints)
                        + " joints, but got " + std::to_string(numAcceptedJoints);
                return false;
            }

            return true;
        }

        return false;
    }

    bool AcceptBound()
    {
        MD5FrameBounds bounds;
        if (RequireChar('(') &&
                AcceptFloat(bounds.MinimumExtent[0]) &&
                AcceptFloat(bounds.MinimumExtent[1]) &&
                AcceptFloat(bounds.MinimumExtent[2]) &&
            RequireChar(')') &&
            RequireChar('(') &&
                AcceptFloat(bounds.MaximumExtent[0]) &&
                AcceptFloat(bounds.MaximumExtent[1]) &&
                AcceptFloat(bounds.MaximumExtent[2]) &&
            RequireChar(')'))
        {
            if (bounds.MinimumExtent[0] > bounds.MaximumExtent[0] ||
                bounds.MinimumExtent[1] > bounds.MaximumExtent[1] ||
                bounds.MinimumExtent[2] > bounds.MaximumExtent[2])
            {
                mError = "MinimumExtent > MaximumExtent";
                return false;
            }

            mAnim.FrameBounds.push_back(std::move(bounds));

            return true;
        }

        return false;
    }

    bool AcceptBounds()
    {
        int numAcceptedBounds = 0;
        if (RequireIdentifier("bounds") && RequireChar('{'))
        {
            while (true)
            {
                EatWhitespace();

                if (mInputStream.peek() == '}')
                {
                    mInputStream.get();
                    break;
                }

                if (!AcceptBound())
                {
                    return false;
                }

                numAcceptedBounds++;
            }

            if (numAcceptedBounds != mNumExpectedFrames)
            {
                mError = "Mismatch between number of bounds and number of frames";
                return false;
            }

            return true;
        }

        return false;
    }

    bool AcceptBaseFrameJointPose()
    {
        MD5JointPose pose;
        if (RequireChar('(') &&
                AcceptFloat(pose.Position[0]) &&
                AcceptFloat(pose.Position[1]) &&
                AcceptFloat(pose.Position[2]) &&
            RequireChar(')') &&
            RequireChar('(') &&
                AcceptFloat(pose.Orientation[0]) &&
                AcceptFloat(pose.Orientation[1]) &&
                AcceptFloat(pose.Orientation[2]) &&
            RequireChar(')'))
        {
            mAnim.BaseFrame.push_back(std::move(pose));

            return true;
        }

        return false;
    }

    bool AcceptBaseFrame()
    {
        int numAcceptedBaseFrameJointPoses = 0;
        if (RequireIdentifier("baseframe") && RequireChar('{'))
        {
            while (true)
            {
                EatWhitespace();

                if (mInputStream.peek() == '}')
                {
                    mInputStream.get();
                    break;
                }

                if (!AcceptBaseFrameJointPose())
                {
                    return false;
                }

                numAcceptedBaseFrameJointPoses++;
            }

            if (numAcceptedBaseFrameJointPoses != mNumExpectedJoints)
            {
                mError = "Mismatch between number of base frame joint poses "
                         "and the number of expected joints";
                return false;
            }

            return true;
        }

        return false;
    }

    bool AcceptAnimationComponents()
    {
        int numAcceptedAnimationComponents = 0;

        MD5Frame& frame = mAnim.Frames.back();

        while (true)
        {
            EatWhitespace();
            if (mInputStream.peek() == '}')
            {
                break;
            }

            float value;
            if (!AcceptFloat(value))
            {
                return false;
            }

            frame.AnimationComponents.push_back(value);

            numAcceptedAnimationComponents++;
        }

        if (numAcceptedAnimationComponents != mNumExpectedAnimatedComponents)
        {
            mError = "mismatch between number of frame components "
                     "and numAnimatedComponents";
            return false;
        }

        for (std::size_t j = 0; j < mAnim.Joints.size(); j++)
        {
            int start = mAnim.Joints[j].StartIndex;
            int componentCount = 0;
            for (unsigned int i = 0; i < 6; i++)
            {
                if (mAnim.Joints[j].Flags & (1 << i))
                {
                    componentCount++;
                }
            }

            if (start >= numAcceptedAnimationComponents ||
                start + componentCount > numAcceptedAnimationComponents)
            {
                mError = "joint's frame data is out of range";
                return false;
            }
        }

        return true;
    }

    bool AcceptFrames()
    {
        int numAcceptedFrames = 0;

        while (true)
        {
            EatWhitespace();

            if (mInputStream.peek() == std::stringstream::traits_type::eof())
            {
                break;
            }

            int frameNumber;
            if (RequireIdentifier("frame") &&
                AcceptInt(frameNumber) &&
                RequireChar('{'))
            {
                if (frameNumber != (int) mAnim.Frames.size())
                {
                    mError = "Incorrect frame index";
                    return false;
                }

                mAnim.Frames.emplace_back();

                if (!AcceptAnimationComponents())
                {
                    return false;
                }

                numAcceptedFrames++;

                if (!RequireChar('}'))
                {
                    return false;
                }
            }
            else
            {
                return false;
            }
        }

        if (numAcceptedFrames != mNumExpectedFrames)
        {
            mError = "mismatch between number of frames and number of "
                     "expected frames";
            return false;
        }

        return true;
    }

public:
    MD5AnimParser(
            MD5Anim& anim,
            IReadFile& md5animFile,
            std::string& error)
        : MD5ParserBase(error)
        , mAnim(anim)
    {
        std::string line;
        while (getline(line, md5animFile))
        {
            line = line.substr(0, line.find("//"));
            mInputStream << line << '\n';
        }
    }

    bool Parse() override
    {
        return AcceptVersion(mAnim.MD5Version) &&
               AcceptCommandLine(mAnim.CommandLine) &&
               AcceptNumFrames() &&
               AcceptNumJoints() &&
               AcceptFrameRate() &&
               AcceptNumAnimatedComponents() &&
               AcceptHierarchy() &&
               AcceptBounds() &&
               AcceptBaseFrame() &&
               AcceptFrames();
    }
};

bool TryLoadMD5Mesh(
        MD5Model& model,
        IReadFile& md5meshFile,
        std::string& error)
{
    MD5Model newModel;
    error.clear();
    MD5MeshParser parser(newModel, md5meshFile, error);

    if (!parser.Parse())
    {
        return false;
    }
    else
    {
        model = std::move(newModel);
        return true;
    }
}

void LoadMD5Mesh(
        MD5Model& model,
        IReadFile& md5meshFile)
{
    std::string error;
    if (!TryLoadMD5Mesh(model, md5meshFile, error))
    {
        throw std::runtime_error(error);
    }
}

bool TryLoadMD5Anim(
        MD5Anim& anim,
        IReadFile& md5animFile,
        std::string& error)
{
    MD5Anim newAnim;
    error.clear();
    MD5AnimParser parser(newAnim, md5animFile, error);

    if (!parser.Parse())
    {
        return false;
    }
    else
    {
        anim = std::move(newAnim);
        return true;
    }
}

void LoadMD5Anim(
        MD5Anim& anim,
        IReadFile& md5animFile)
{
    std::string error;
    if (!TryLoadMD5Anim(anim, md5animFile, error))
    {
        throw std::runtime_error(error);
    }
}

} // end namespace ng
