// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_DC_LAYER_OVERLAY_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_DC_LAYER_OVERLAY_H_

#include "base/containers/flat_map.h"
#include "base/memory/ref_counted.h"
#include "components/viz/common/quads/render_pass.h"
#include "components/viz/service/viz_service_export.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkMatrix44.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gl/dc_renderer_layer_params.h"

namespace viz {
class DisplayResourceProvider;
class OutputSurface;

// Holds all information necessary to construct a DCLayer from a DrawQuad.
class VIZ_SERVICE_EXPORT DCLayerOverlay {
 public:
  DCLayerOverlay();
  DCLayerOverlay(const DCLayerOverlay& other);
  DCLayerOverlay& operator=(const DCLayerOverlay& other);
  ~DCLayerOverlay();

  // TODO(magchen): Once software protected video is enabled for all GPUs and
  // all configurations, RequiresOverlay() will be true for all protected video.
  // Currently, we only force the overlay swap chain path (RequiresOverlay) for
  // hardware protected video and soon for Finch experiment on software
  // protected video.
  bool RequiresOverlay() const {
    return (protected_video_type == ui::ProtectedVideoType::kHardwareProtected);
  }

  // Resource ids for video Y and UV planes.  Can be the same resource.
  // See DirectCompositionSurfaceWin for details.
  ResourceId y_resource_id = 0;
  ResourceId uv_resource_id = 0;

  // Stacking order relative to backbuffer which has z-order 0.
  int z_order = 1;

  // What part of the content to display in pixels.
  gfx::Rect content_rect;

  // Bounds of the overlay in pre-transform space.
  gfx::Rect quad_rect;

  // 2D flattened transform that maps |quad_rect| to root target space,
  // after applying the |quad_rect.origin()| as an offset.
  gfx::Transform transform;

  // If |is_clipped| is true, then clip to |clip_rect| in root target space.
  bool is_clipped = false;
  gfx::Rect clip_rect;

  // This is the color-space the texture should be displayed as. If invalid,
  // then the default for the texture should be used. For YUV textures, that's
  // normally BT.709.
  gfx::ColorSpace color_space;

  ui::ProtectedVideoType protected_video_type = ui::ProtectedVideoType::kClear;
};

typedef std::vector<DCLayerOverlay> DCLayerOverlayList;

class DCLayerOverlayProcessor {
 public:
  // This is used for a histogram to determine why overlays are or aren't
  // used, so don't remove entries and make sure to update enums.xml if
  // it changes.
  enum DCLayerResult {
    DC_LAYER_SUCCESS,
    DC_LAYER_FAILED_UNSUPPORTED_QUAD,
    DC_LAYER_FAILED_QUAD_BLEND_MODE,
    DC_LAYER_FAILED_TEXTURE_NOT_CANDIDATE,
    DC_LAYER_FAILED_OCCLUDED,
    DC_LAYER_FAILED_COMPLEX_TRANSFORM,
    DC_LAYER_FAILED_TRANSPARENT,
    DC_LAYER_FAILED_NON_ROOT,
    DC_LAYER_FAILED_TOO_MANY_OVERLAYS,
    DC_LAYER_FAILED_NO_HW_OVERLAY_SUPPORT,
    kMaxValue = DC_LAYER_FAILED_NO_HW_OVERLAY_SUPPORT,
  };

  explicit DCLayerOverlayProcessor(OutputSurface* surface);
  ~DCLayerOverlayProcessor();

  void Process(DisplayResourceProvider* resource_provider,
               const gfx::RectF& display_rect,
               RenderPassList* render_passes,
               gfx::Rect* overlay_damage_rect,
               gfx::Rect* damage_rect,
               DCLayerOverlayList* dc_layer_overlays);
  void ClearOverlayState() {
    previous_frame_underlay_rect_ = gfx::Rect();
    previous_frame_underlay_occlusion_ = gfx::Rect();
  }
  void SetHasHwOverlaySupport() { has_hw_overlay_support_ = true; }

 private:
  DCLayerResult FromDrawQuad(DisplayResourceProvider* resource_provider,
                             QuadList::ConstIterator quad_list_begin,
                             QuadList::ConstIterator quad,
                             const gfx::Transform& transform_to_root_target,
                             DCLayerOverlay* dc_layer_overlay);
  // Returns an iterator to the element after |it|.
  QuadList::Iterator ProcessRenderPassDrawQuad(RenderPass* render_pass,
                                               gfx::Rect* damage_rect,
                                               QuadList::Iterator it);

  void ProcessRenderPass(DisplayResourceProvider* resource_provider,
                         const gfx::RectF& display_rect,
                         RenderPass* render_pass,
                         bool is_root,
                         gfx::Rect* overlay_damage_rect,
                         gfx::Rect* damage_rect,
                         DCLayerOverlayList* dc_layer_overlays);
  bool ProcessForOverlay(const gfx::RectF& display_rect,
                         QuadList* quad_list,
                         const gfx::Rect& quad_rectangle,
                         const gfx::RectF& occlusion_bounding_box,
                         QuadList::Iterator* it,
                         gfx::Rect* damage_rect);
  bool ProcessForUnderlay(const gfx::RectF& display_rect,
                          RenderPass* render_pass,
                          const gfx::Rect& quad_rectangle,
                          const gfx::RectF& occlusion_bounding_box,
                          const QuadList::Iterator& it,
                          bool is_root,
                          gfx::Rect* damage_rect,
                          gfx::Rect* this_frame_underlay_rect,
                          gfx::Rect* this_frame_underlay_occlusion,
                          DCLayerOverlay* dc_layer);

  gfx::Rect previous_frame_underlay_rect_;
  gfx::Rect previous_frame_underlay_occlusion_;
  gfx::RectF previous_display_rect_;
  bool processed_overlay_in_frame_ = false;
  bool has_hw_overlay_support_ = true;

  // Store information about clipped punch-through rects in target space for
  // non-root render passes. These rects are used to clear the corresponding
  // areas in parent render passes.
  base::flat_map<RenderPassId, std::vector<gfx::Rect>>
      pass_punch_through_rects_;

  DISALLOW_COPY_AND_ASSIGN(DCLayerOverlayProcessor);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_DC_LAYER_OVERLAY_H_
