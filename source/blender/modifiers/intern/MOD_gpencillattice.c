/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 * The Original Code is Copyright (C) 2005 by the Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Antonio Vazquez
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 */

/** \file blender/modifiers/intern/MOD_gpencillattice.c
 *  \ingroup modifiers
 */

#include <stdio.h>

#include "DNA_scene_types.h"
#include "DNA_object_types.h"
#include "DNA_gpencil_types.h"

#include "BLI_utildefines.h"

#include "BKE_context.h"
#include "BKE_DerivedMesh.h"
#include "BKE_gpencil.h"
#include "BKE_lattice.h"
#include "BKE_library_query.h"
#include "BKE_scene.h"
#include "BKE_main.h"

#include "MEM_guardedalloc.h"

#include "MOD_modifiertypes.h"

#include "DEG_depsgraph.h"

static void initData(ModifierData *md)
{
	GpencilLatticeModifierData *gpmd = (GpencilLatticeModifierData *)md;
	gpmd->pass_index = 0;
	gpmd->layername[0] = '\0';
	gpmd->vgname[0] = '\0';
	gpmd->object = NULL;
	gpmd->cache_data = NULL;
	gpmd->strength = 1.0f;
}

static void copyData(ModifierData *md, ModifierData *target)
{
	modifier_copyData_generic(md, target);
}

static DerivedMesh *applyModifier(ModifierData *md, const struct EvaluationContext *eval_ctx, Object *ob,
	DerivedMesh *UNUSED(dm),
	ModifierApplyFlag UNUSED(flag))
{
	GpencilLatticeModifierData *mmd = (GpencilLatticeModifierData *)md;
	LatticeDeformData *ldata = NULL;
	Scene *scene = md->scene;
	Main *bmain = CTX_data_main(mmd->C);
	bGPdata *gpd;
	Object *latob = NULL;
	int oldframe = CFRA;

	if ((!ob) || (!ob->gpd)) {
		return NULL;
	}
	gpd = ob->gpd;
	latob = mmd->object;
	if ((!latob) || (latob->type != OB_LATTICE)) {
		return NULL;
	}

	struct EvaluationContext eval_ctx_copy = *eval_ctx;

	for (bGPDlayer *gpl = gpd->layers.first; gpl; gpl = gpl->next) {
		for (bGPDframe *gpf = gpl->frames.first; gpf; gpf = gpf->next) {
			for (bGPDstroke *gps = gpf->strokes.first; gps; gps = gps->next) {
				CFRA = gpf->framenum;
				BKE_scene_update_for_newframe(&eval_ctx_copy, bmain, scene);
				/* recalculate lattice data */
				BKE_gpencil_lattice_init(ob);

				BKE_gpencil_lattice_modifier(-1, (GpencilLatticeModifierData *)md, ob, gpl, gps);
			}
		}
	}

	ldata = (LatticeDeformData *)mmd->cache_data;
	if (ldata) {
		end_latt_deform(ldata);
		mmd->cache_data = NULL;
	}

	CFRA = oldframe;
	return NULL;
}

static void freeData(ModifierData *md)
{
	GpencilLatticeModifierData *mmd = (GpencilLatticeModifierData *)md;
	LatticeDeformData *ldata = (LatticeDeformData *)mmd->cache_data;
	/* free deform data */
	if (ldata) {
		end_latt_deform(ldata);
	}
}

static bool isDisabled(ModifierData *md, int UNUSED(userRenderParams))
{
	GpencilLatticeModifierData *mmd = (GpencilLatticeModifierData *)md;

	return !mmd->object;
}

static void foreachObjectLink(
	ModifierData *md, Object *ob,
	ObjectWalkFunc walk, void *userData)
{
	GpencilLatticeModifierData *mmd = (GpencilLatticeModifierData *)md;

	walk(userData, ob, &mmd->object, IDWALK_CB_NOP);
}

ModifierTypeInfo modifierType_GpencilLattice = {
	/* name */              "Lattice",
	/* structName */        "GpencilLatticeModifierData",
	/* structSize */        sizeof(GpencilLatticeModifierData),
	/* type */              eModifierTypeType_Gpencil,
	/* flags */             eModifierTypeFlag_GpencilMod | eModifierTypeFlag_Single | eModifierTypeFlag_SupportsEditmode,

	/* copyData */          copyData,
	/* deformVerts */       NULL,
	/* deformMatrices */    NULL,
	/* deformVertsEM */     NULL,
	/* deformMatricesEM */  NULL,
	/* applyModifier */     applyModifier,
	/* applyModifierEM */   NULL,
	/* initData */          initData,
	/* requiredDataMask */  NULL,
	/* freeData */          freeData,
	/* isDisabled */        isDisabled,
	/* updateDepsgraph */   NULL,
	/* dependsOnTime */     NULL,
	/* dependsOnNormals */	NULL,
	/* foreachObjectLink */ foreachObjectLink,
	/* foreachIDLink */     NULL,
	/* foreachTexLink */    NULL,
};