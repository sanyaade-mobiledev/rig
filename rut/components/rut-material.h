/*
 * Rut
 *
 * Copyright (C) 2012  Intel Corporation
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __RUT_MATERIAL_H__
#define __RUT_MATERIAL_H__

#include "rut-entity.h"
#include "rut-asset.h"

typedef struct _RutMaterial RutMaterial;
#define RUT_MATERIAL(p) ((RutMaterial *)(p))
extern RutType rut_material_type;

enum {
  RUT_MATERIAL_PROP_AMBIENT,
  RUT_MATERIAL_PROP_DIFFUSE,
  RUT_MATERIAL_PROP_SPECULAR,
  RUT_MATERIAL_PROP_SHININESS,
  RUT_MATERIAL_PROP_ALPHA_MASK_THRESHOLD,
  RUT_MATERIAL_N_PROPS
};

struct _RutMaterial
{
  RutObjectProps _parent;

  int ref_count;

  RutComponentableProps component;
  RutAsset *texture_asset;
  RutAsset *normal_map_asset;
  RutAsset *alpha_mask_asset;

  CoglColor ambient;
  CoglColor diffuse;
  CoglColor specular;
  float shininess;

  float alpha_mask_threshold;

  int uniforms_age;
  int uniforms_flush_age;

  RutSimpleIntrospectableProps introspectable;
  RutProperty properties[RUT_MATERIAL_N_PROPS];
};

void
_rut_material_init_type (void);

RutMaterial *
rut_material_new (RutContext *ctx,
                  RutAsset *asset);

void
rut_material_set_texture_asset (RutMaterial *material,
                                RutAsset *asset);

RutAsset *
rut_material_get_texture_asset (RutMaterial *material);

void
rut_material_set_normal_map_asset (RutMaterial *material,
                                   RutAsset *normal_map_asset);

RutAsset *
rut_material_get_normal_map_asset (RutMaterial *material);

void
rut_material_set_alpha_mask_asset (RutMaterial *material,
                                   RutAsset *alpha_mask_asset);

RutAsset *
rut_material_get_alpha_mask_asset (RutMaterial *material);

void
rut_material_set_ambient (RutObject *material,
                          const CoglColor *color);

const CoglColor *
rut_material_get_ambient (RutObject *material);

void
rut_material_set_diffuse (RutObject *material,
                          const CoglColor *color);

const CoglColor *
rut_material_get_diffuse (RutObject *material);

void
rut_material_set_specular (RutObject *material,
                           const CoglColor *color);

const CoglColor *
rut_material_get_specular (RutObject *material);

void
rut_material_set_shininess (RutObject *material,
                            float shininess);

float
rut_material_get_shininess (RutObject *material);

float
rut_material_get_alpha_mask_threshold (RutObject *material);

void
rut_material_set_alpha_mask_threshold (RutObject *material,
                                       float alpha_mask_threshold);

void
rut_material_flush_uniforms (RutMaterial *material,
                             CoglPipeline *pipeline);

void
rut_material_flush_uniforms_ignore_age (RutMaterial *material);

#endif /* __RUT_MATERIAL_H__ */
