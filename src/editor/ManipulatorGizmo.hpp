//
//  UISystems.hpp
//
//  Created by Carl Johan Gribel on 2023-11-04.
//  Copyright © 2023 Carl Johan Gribel. All rights reserved.
//

#ifndef EditorUISystems_hpp
#define EditorUISystems_hpp

//#include <stdio.h>
//#include "vec.h"
#include "SceneAPI.hpp"
#include "ImPrimitiveRenderer.hpp"

using namespace linalg;
using namespace ImPrimitiveRendererNS;

// TODO: Aux calsses. WHERE?

struct LineIntersectionResult
{
    float s = 0.0f, t = 0.0f, dist = 0.0f;
    v3f c0, c1;
};

struct Plane
{
    v3f p, n;
};

namespace EditorUI {

enum class WidgetState
{
    Default, Hovered, Engaged, Passive
};

class TransformWidgetBase
{
public:
    const Color4u color;
    const float screenspace_size;
    
    virtual bool hover(Ray& ray, const Transform& tfm, float scale) const = 0;
    virtual void engage(Ray& ray, const Transform& tfm, float scale) = 0;
    virtual void update(Ray& ray, Transform& tfm, float scale) = 0;
    virtual void render(Scene& scene,
                        std::shared_ptr<ImPrimitiveRenderer> renderer,
                        const Transform& tfm,
                        float scale,
                        WidgetState state) const = 0;
    
    TransformWidgetBase(Color4u color, float screenspace_size)
    : color(color), screenspace_size(screenspace_size) {}
};

class AxisTranslateSubWidget : public TransformWidgetBase
{
    const v3f dir;
    const float radius;
    const float length;

    // OR store point + dir at the point of engagement
    float t_ofs = 0.0f;
    float t_current = 0.0f;
    
public:
    AxisTranslateSubWidget(const v3f& dir, 
               float radius,
               float length,
               Color4u color,
               float screenspace_size) :
    dir(dir),
    radius(radius),
    length(length),
    TransformWidgetBase(color, screenspace_size) {}
    
    virtual bool hover(Ray& ray, const Transform& tfm, float scale) const override;
    virtual void engage(Ray& ray, const Transform& tfm, float scale) override;
    virtual void update(Ray& ray, Transform& tfm, float scale) override;
    virtual void render(Scene& scene,
                        std::shared_ptr<ImPrimitiveRenderer> renderer,
                        const Transform& tfm,
                        float scale,
                        WidgetState state) const override;
};

class ScaleSubWidget : public TransformWidgetBase
{
    const v3f dir;
    const float radius;

    float t_ofs = 0.0f;
    float t_current = 0.0f;
    v3f scaling_engaged;
    
public:
    ScaleSubWidget(const v3f& dir,
                float radius,
                Color4u color,
                float screenspace_size) :
    dir(dir),
    radius(radius),
    TransformWidgetBase(color, screenspace_size) {}
    
    virtual bool hover(Ray& ray, const Transform& tfm, float scale) const override;
    virtual void engage(Ray& ray, const Transform& tfm, float scale) override;
    virtual void update(Ray& ray, Transform& tfm, float scale) override;
    virtual void render(Scene& scene,
                        std::shared_ptr<ImPrimitiveRenderer> renderer,
                        const Transform& tfm,
                        float scale,
                        WidgetState state) const override;
};

class PlaneTranslateSubWidget : public TransformWidgetBase
{
    const v3f puv[3]; // position and two axes
    
    Plane plane_engaged;
    v3f p_engaged;
    
    void get_transformed_quad(v3f points[4], 
                              const Transform& tfm,
                              float scale) const;
public:
    PlaneTranslateSubWidget(const v3f& p,
                const v3f& u,
                const v3f& v,
                Color4u color,
                float screenspace_size) :
    puv {p, u, v},
    TransformWidgetBase(color, screenspace_size) {}
    
    virtual bool hover(Ray& ray,  const Transform& tfm, float scale) const override;
    virtual void engage(Ray& ray,  const Transform& tfm, float scale) override;
    virtual void update(Ray& ray, Transform& tfm, float scale) override;
    virtual void render(Scene& scene,
                        std::shared_ptr<ImPrimitiveRenderer> renderer,
                        const Transform& tfm,
                        float scale,
                        WidgetState state) const override;
};

class RotationSubWidget : public TransformWidgetBase
{
    const v3f u, v;
    const float radius_outer, radius_inner;
    
    // Engaged data
    Plane plane_engaged;
    v3f p_engaged;
    v3f rot_engaged;
    
    // Engaged data, for drawing
    v3f axis0_enagaged;
    v3f axis1_enagaged;
public:
    RotationSubWidget(const v3f& u,
                   const v3f& v,
                   const float radius_outer,
                   const float radius_inner,
                   Color4u color,
                   float screenspace_size) :
    u(u), v(v),
    radius_outer(radius_outer), radius_inner(radius_inner),
    TransformWidgetBase(color, screenspace_size) {}
    
    virtual bool hover(Ray& ray, const Transform& tfm, float scale) const override;
    virtual void engage(Ray& ray, const Transform& tfm, float scale) override;
    virtual void update(Ray& ray, Transform& tfm, float scale) override;
    virtual void render(Scene& scene,
                        std::shared_ptr<ImPrimitiveRenderer> renderer,
                        const Transform& tfm,
                        float scale,
                        WidgetState state) const override;
};

struct TransformWidgetComponent
{
    PrimaryEntity target_entity;
    using WidgetPtr = std::shared_ptr<TransformWidgetBase>;
    
    TransformWidgetComponent();
    
    std::vector<WidgetPtr> widgets;
    WidgetPtr hovered_widget = nullptr;
    WidgetPtr engaged_widget = nullptr;
    float linear_snap = 1.0f;
    float angular_snap = 10.0f;
//    float scale_snap = 10.0f;
    
    void set_target_entity(PrimaryEntity entity)
    {
        if (target_entity != entity) {
            WidgetPtr hovered_widget = nullptr;
            WidgetPtr engaged_widget = nullptr;
        }
        target_entity = entity;
    }
    inline bool any_engaged() { return (bool)engaged_widget; }
    inline bool is_engaged(WidgetPtr w) { return (w == engaged_widget); }
    inline bool any_hovered() { return (bool)hovered_widget; }
    inline bool is_hovered(WidgetPtr w) { return (w == hovered_widget); }
    inline void engage(WidgetPtr w) { engaged_widget = w; }
    inline void disengage() { engaged_widget = nullptr; }
};

class TransformWidgetSystem
{
public:
    static void init(Scene& scene,
                     entt::dispatcher& dispatcher);
    
    static void update(Scene& scene,
                       entt::dispatcher& dispatcher,
                       float dt);
    
    static void primitive_render(Scene& scene,
                                 std::shared_ptr<ImPrimitiveRenderer> renderer);
};

} // namespace EditorUI

#endif /* UISystems_hpp */
