#pragma once

#include "MeshSync/MeshSync.h"

namespace ms {

class Entity;
class Transform;
class Camera;
class Light;
class Mesh;
class Points;
class Animation;
class AnimationCurve;
class AnimationClip;
class InstanceInfo;

class EntityConverter
{
public:
    virtual ~EntityConverter() {}
    virtual void convert(Entity& v);
    virtual void convertTransform(Transform& v) = 0;
    virtual void convertCamera(Camera& v) = 0;
    virtual void convertLight(Light& v) = 0;
    virtual void convertMesh(Mesh& v) = 0;
    virtual void convertPoints(Points& v) = 0;
    virtual void convertCurve(Curve& v) = 0;

    virtual void convert(AnimationClip& v);
    virtual void convert(Animation& v);
    virtual void convertAnimationCurve(AnimationCurve& v);
};


class ScaleConverter : public EntityConverter
{
using super = EntityConverter;
public:
    static std::shared_ptr<ScaleConverter> create(float scale);

    ScaleConverter(float scale);

    void convertTransform(Transform& v) override;
    void convertCamera(Camera& v) override;
    void convertLight(Light& v) override;
    void convertMesh(Mesh& v) override;
    void convertPoints(Points& v) override;
    void convertCurve(Curve& v) override;

    void convertAnimationCurve(AnimationCurve& v) override;

    void convertInstanceInfos(InstanceInfo& v);
private:
    float m_scale;
};


class FlipX_HandednessCorrector : public EntityConverter
{
using super = EntityConverter;
public:
    static std::shared_ptr<FlipX_HandednessCorrector> create();

    void convertTransform(Transform& v) override;
    void convertCamera(Camera& v) override;
    void convertLight(Light& v) override;
    void convertMesh(Mesh& v) override;
    void convertPoints(Points& v) override;
    void convertCurve(Curve& v) override;

    void convertAnimationCurve(AnimationCurve& v) override;
};


class FlipYZ_ZUpCorrector : public EntityConverter
{
using super = EntityConverter;
public:
    static const int UndoIterations = 4;
    static std::shared_ptr<FlipYZ_ZUpCorrector> create();

    void convertTransform(Transform& v) override;
    void convertCamera(Camera& v) override;
    void convertLight(Light& v) override;
    void convertMesh(Mesh& v) override;
    void convertPoints(Points& v) override;
    void convertCurve(Curve& v) override;

    void convert(Animation &anim) override;
};


// fbx-conpatible Z-up to Y-up
// simply set X angle of root object to -90
class RotateX_ZUpCorrector : public EntityConverter
{
using super = EntityConverter;
public:
    static const int UndoIterations = 8;
    static std::shared_ptr<RotateX_ZUpCorrector> create();

    void convertTransform(Transform& v) override;
    void convertCamera(Camera& v) override;
    void convertLight(Light& v) override;
    void convertMesh(Mesh& v) override;
    void convertPoints(Points& v) override;
    void convertCurve(Curve& v) override;

    void convert(Animation& v)override;
    void convertAnimationCurve(AnimationCurve& v) override;
};


} // namespace ms
