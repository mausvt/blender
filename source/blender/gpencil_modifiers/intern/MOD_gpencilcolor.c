/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software  Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2017, Blender Foundation
 * This is a new part of Blender
 */

/** \file
 * \ingroup modifiers
 */

#include <stdio.h>

#include "BLI_utildefines.h"

#include "BLI_blenlib.h"
#include "BLI_math_color.h"
#include "BLI_math_vector.h"

#include "DNA_scene_types.h"
#include "DNA_object_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_gpencil_modifier_types.h"

#include "BKE_gpencil.h"
#include "BKE_gpencil_modifier.h"
#include "BKE_main.h"
#include "BKE_material.h"

#include "DEG_depsgraph.h"

#include "MOD_gpencil_util.h"
#include "MOD_gpencil_modifiertypes.h"

static void initData(GpencilModifierData *md)
{
  ColorGpencilModifierData *gpmd = (ColorGpencilModifierData *)md;
  gpmd->pass_index = 0;
  ARRAY_SET_ITEMS(gpmd->hsv, 0.5f, 1.0f, 1.0f);
  gpmd->layername[0] = '\0';
  gpmd->materialname[0] = '\0';
  gpmd->modify_color = GP_MODIFY_COLOR_BOTH;
}

static void copyData(const GpencilModifierData *md, GpencilModifierData *target)
{
  BKE_gpencil_modifier_copyData_generic(md, target);
}

/* color correction strokes */
static void deformStroke(GpencilModifierData *md,
                         Depsgraph *UNUSED(depsgraph),
                         Object *ob,
                         bGPDlayer *gpl,
                         bGPDframe *UNUSED(gpf),
                         bGPDstroke *gps)
{

  ColorGpencilModifierData *mmd = (ColorGpencilModifierData *)md;
  float hsv[3], factor[3];

  if (!is_stroke_affected_by_modifier(ob,
                                      mmd->layername,
                                      mmd->materialname,
                                      mmd->pass_index,
                                      mmd->layer_pass,
                                      1,
                                      gpl,
                                      gps,
                                      mmd->flag & GP_COLOR_INVERT_LAYER,
                                      mmd->flag & GP_COLOR_INVERT_PASS,
                                      mmd->flag & GP_COLOR_INVERT_LAYERPASS,
                                      mmd->flag & GP_COLOR_INVERT_MATERIAL)) {
    return;
  }

  copy_v3_v3(factor, mmd->hsv);
  /* keep initial values unchanged, subtracting the default values. */
  factor[0] -= 0.5f;
  factor[1] -= 1.0f;
  factor[2] -= 1.0f;

  MaterialGPencilStyle *gp_style = BKE_material_gpencil_settings_get(ob, gps->mat_nr + 1);

  /* Apply to Vertex Color. */
  /* Fill */
  if (mmd->modify_color != GP_MODIFY_COLOR_STROKE) {
    /* If not using Vertex Color, use the material color. */
    if ((gp_style != NULL) && (gps->vert_color_fill[3] == 0.0f) &&
        (gp_style->fill_rgba[3] > 0.0f)) {
      copy_v4_v4(gps->vert_color_fill, gp_style->fill_rgba);
      gps->vert_color_fill[3] = 1.0f;
    }

    rgb_to_hsv_v(gps->vert_color_fill, hsv);
    add_v3_v3(hsv, factor);
    CLAMP3(hsv, 0.0f, 1.0f);
    hsv_to_rgb_v(hsv, gps->vert_color_fill);
  }

  /* Stroke */
  if (mmd->modify_color != GP_MODIFY_COLOR_FILL) {

    for (int i = 0; i < gps->totpoints; i++) {
      bGPDspoint *pt = &gps->points[i];
      /* If not using Vertex Color, use the material color. */
      if ((gp_style != NULL) && (pt->vert_color[3] == 0.0f) && (gp_style->stroke_rgba[3] > 0.0f)) {
        copy_v4_v4(pt->vert_color, gp_style->stroke_rgba);
        pt->vert_color[3] = 1.0f;
      }

      rgb_to_hsv_v(pt->vert_color, hsv);
      add_v3_v3(hsv, factor);
      CLAMP3(hsv, 0.0f, 1.0f);
      hsv_to_rgb_v(hsv, pt->vert_color);
    }
  }
}

static void bakeModifier(Main *bmain, Depsgraph *depsgraph, GpencilModifierData *md, Object *ob)
{
  bGPdata *gpd = ob->data;

  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    LISTBASE_FOREACH (bGPDframe *, gpf, &gpl->frames) {
      LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
        deformStroke(md, depsgraph, ob, gpl, gpf, gps);
      }
    }
  }
}

GpencilModifierTypeInfo modifierType_Gpencil_Color = {
    /* name */ "Hue/Saturation",
    /* structName */ "ColorGpencilModifierData",
    /* structSize */ sizeof(ColorGpencilModifierData),
    /* type */ eGpencilModifierTypeType_Gpencil,
    /* flags */ eGpencilModifierTypeFlag_SupportsEditmode,

    /* copyData */ copyData,

    /* deformStroke */ deformStroke,
    /* generateStrokes */ NULL,
    /* bakeModifier */ bakeModifier,
    /* remapTime */ NULL,

    /* initData */ initData,
    /* freeData */ NULL,
    /* isDisabled */ NULL,
    /* updateDepsgraph */ NULL,
    /* dependsOnTime */ NULL,
    /* foreachObjectLink */ NULL,
    /* foreachIDLink */ NULL,
    /* foreachTexLink */ NULL,
};
