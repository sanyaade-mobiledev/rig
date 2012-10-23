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
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "rig-data.h"
#include "rig-renderer.h"

typedef enum _CacheSlot
{
  CACHE_SLOT_SHADOW,
  CACHE_SLOT_COLOR
} CacheSlot;

static const float jitter_offsets[32] =
{
  0.375f, 0.4375f,
  0.625f, 0.0625f,
  0.875f, 0.1875f,
  0.125f, 0.0625f,

  0.375f, 0.6875f,
  0.875f, 0.4375f,
  0.625f, 0.5625f,
  0.375f, 0.9375f,

  0.625f, 0.3125f,
  0.125f, 0.5625f,
  0.125f, 0.8125f,
  0.375f, 0.1875f,

  0.875f, 0.9375f,
  0.875f, 0.6875f,
  0.125f, 0.3125f,
  0.625f, 0.8125f
};

/* XXX: This assumes that the primitive is being drawn in pixel coordinates,
 * since we jitter the modelview not the projection.
 */
static void
draw_jittered_primitive4f (RigData *data,
                           CoglFramebuffer *fb,
                           CoglPrimitive *prim,
                           float red,
                           float green,
                           float blue)
{
  CoglPipeline *pipeline = cogl_pipeline_new (data->ctx->cogl_context);
  int i;

  cogl_pipeline_set_color4f (pipeline,
                             red / 16.0f,
                             green / 16.0f,
                             blue / 16.0f,
                             1.0f / 16.0f);

  for (i = 0; i < 16; i++)
    {
      const float *offset = jitter_offsets + 2 * i;

      cogl_framebuffer_push_matrix (fb);
      cogl_framebuffer_translate (fb, offset[0], offset[1], 0);
      cogl_framebuffer_draw_primitive (fb, pipeline, prim);
      cogl_framebuffer_pop_matrix (fb);
    }

  cogl_object_unref (pipeline);
}

static void
camera_update_view (RigData *data, RutEntity *camera, RigPass pass)
{
  RutCamera *camera_component =
    rut_entity_get_component (camera, RUT_COMPONENT_TYPE_CAMERA);
  CoglMatrix transform;
  CoglMatrix inverse_transform;
  CoglMatrix view;

  /* translate to z_2d and scale */
  if (pass != RIG_PASS_SHADOW)
    view = data->main_view;
  else
    view = data->identity;

  /* apply the camera viewing transform */
  rut_graphable_get_transform (camera, &transform);
  cogl_matrix_get_inverse (&transform, &inverse_transform);
  cogl_matrix_multiply (&view, &view, &inverse_transform);

  if (pass == RIG_PASS_SHADOW)
    {
      CoglMatrix flipped_view;
      cogl_matrix_init_identity (&flipped_view);
      cogl_matrix_scale (&flipped_view, 1, -1, 1);
      cogl_matrix_multiply (&flipped_view, &flipped_view, &view);
      rut_camera_set_view_transform (camera_component, &flipped_view);
    }
  else
    rut_camera_set_view_transform (camera_component, &view);
}

static void
get_normal_matrix (const CoglMatrix *matrix,
                   float *normal_matrix)
{
  CoglMatrix inverse_matrix;

  /* Invert the matrix */
  cogl_matrix_get_inverse (matrix, &inverse_matrix);

  /* Transpose it while converting it to 3x3 */
  normal_matrix[0] = inverse_matrix.xx;
  normal_matrix[1] = inverse_matrix.xy;
  normal_matrix[2] = inverse_matrix.xz;

  normal_matrix[3] = inverse_matrix.yx;
  normal_matrix[4] = inverse_matrix.yy;
  normal_matrix[5] = inverse_matrix.yz;

  normal_matrix[6] = inverse_matrix.zx;
  normal_matrix[7] = inverse_matrix.zy;
  normal_matrix[8] = inverse_matrix.zz;
}

static void
set_focal_parameters (CoglPipeline *pipeline,
                      float focal_distance,
                      float depth_of_field)
{
  int location;
  float distance;

  /* I want to have the focal distance as positive when it's in front of the
   * camera (it seems more natural, but as, in OpenGL, the camera is facing
   * the negative Ys, the actual value to give to the shader has to be
   * negated */
  distance = -focal_distance;

  location = cogl_pipeline_get_uniform_location (pipeline,
                                                 "dof_focal_distance");
  cogl_pipeline_set_uniform_float (pipeline,
                                   location,
                                   1 /* n_components */, 1 /* count */,
                                   &distance);

  location = cogl_pipeline_get_uniform_location (pipeline,
                                                 "dof_depth_of_field");
  cogl_pipeline_set_uniform_float (pipeline,
                                   location,
                                   1 /* n_components */, 1 /* count */,
                                   &depth_of_field);
}

static void
get_light_modelviewprojection (const CoglMatrix *model_transform,
                               RutEntity  *light,
                               const CoglMatrix *light_projection,
                               CoglMatrix *light_mvp)
{
  const CoglMatrix *light_transform;
  CoglMatrix light_view;

  /* TODO: cache the bias * light_projection * light_view matrix! */

  /* Move the unit data from [-1,1] to [0,1], column major order */
  float bias[16] = {
    .5f, .0f, .0f, .0f,
    .0f, .5f, .0f, .0f,
    .0f, .0f, .5f, .0f,
    .5f, .5f, .5f, 1.f
  };

  light_transform = rut_entity_get_transform (light);
  cogl_matrix_get_inverse (light_transform, &light_view);

  cogl_matrix_init_from_array (light_mvp, bias);
  cogl_matrix_multiply (light_mvp, light_mvp, light_projection);
  cogl_matrix_multiply (light_mvp, light_mvp, &light_view);

  cogl_matrix_multiply (light_mvp, light_mvp, model_transform);
}

static void
reshape_cb (RutShape *shape, void *user_data)
{
  RutComponentableProps *componentable =
    rut_object_get_properties (shape, RUT_INTERFACE_ID_COMPONENTABLE);
  RutEntity *entity = componentable->entity;
  rig_dirty_entity_pipelines (entity);
}

static CoglPipeline *
get_entity_mask_pipeline (RigData *data,
                          RutEntity *entity,
                          RutComponent *geometry)
{
  CoglPipeline *pipeline;

  pipeline = rut_entity_get_pipeline_cache (entity, CACHE_SLOT_SHADOW);
  if (pipeline)
    return cogl_object_ref (pipeline);

  /* TODO: move into init() somewhere */
  if (G_UNLIKELY (!data->dof_pipeline_template))
    {
      CoglPipeline *pipeline;
      CoglDepthState depth_state;
      CoglSnippet *snippet;

      pipeline = cogl_pipeline_new (data->ctx->cogl_context);

      cogl_pipeline_set_color_mask (pipeline, COGL_COLOR_MASK_ALPHA);

      cogl_pipeline_set_blend (pipeline, "RGBA=ADD(SRC_COLOR, 0)", NULL);

      cogl_depth_state_init (&depth_state);
      cogl_depth_state_set_test_enabled (&depth_state, TRUE);
      cogl_pipeline_set_depth_state (pipeline, &depth_state, NULL);

      snippet = cogl_snippet_new (COGL_SNIPPET_HOOK_VERTEX,

                                  /* definitions */
                                  "uniform float dof_focal_distance;\n"
                                  "uniform float dof_depth_of_field;\n"

                                  "varying float dof_blur;\n",
                                  //"varying vec4 world_pos;\n",

                                  /* compute the amount of bluriness we want */
                                  "vec4 world_pos = cogl_modelview_matrix * cogl_position_in;\n"
                                  //"world_pos = cogl_modelview_matrix * cogl_position_in;\n"
                                  "dof_blur = 1.0 - clamp (abs (world_pos.z - dof_focal_distance) /\n"
                                  "                  dof_depth_of_field, 0.0, 1.0);\n"
      );

      cogl_pipeline_add_snippet (pipeline, snippet);
      cogl_object_unref (snippet);

      /* This was used to debug the focal distance and bluriness amount in the DoF
       * effect: */
#if 0
      cogl_pipeline_set_color_mask (pipeline, COGL_COLOR_MASK_ALL);
      snippet = cogl_snippet_new (COGL_SNIPPET_HOOK_FRAGMENT,
                                  "varying vec4 world_pos;\n"
                                  "varying float dof_blur;",

                                  "cogl_color_out = vec4(dof_blur,0,0,1);\n"
                                  //"cogl_color_out = vec4(1.0, 0.0, 0.0, 1.0);\n"
                                  //"if (world_pos.z < -30.0) cogl_color_out = vec4(0,1,0,1);\n"
                                  //"if (abs (world_pos.z + 30.f) < 0.1) cogl_color_out = vec4(0,1,0,1);\n"
                                  "cogl_color_out.a = dof_blur;\n"
                                  //"cogl_color_out.a = 1.0;\n"
      );

      cogl_pipeline_add_snippet (pipeline, snippet);
      cogl_object_unref (snippet);
#endif

      data->dof_pipeline_template = pipeline;
    }

  /* TODO: move into init() somewhere */
  if (G_UNLIKELY (!data->dof_diamond_pipeline))
    {
      CoglPipeline *dof_diamond_pipeline =
        cogl_pipeline_copy (data->dof_pipeline_template);
      CoglSnippet *snippet;

      rut_diamond_apply_mask (RUT_DIAMOND (geometry),
                              dof_diamond_pipeline);

      snippet = cogl_snippet_new (COGL_SNIPPET_HOOK_FRAGMENT,
                                  /* declarations */
                                  "varying float dof_blur;",

                                  /* post */
                                  "if (cogl_color_out.a <= 0.0)\n"
                                  "  discard;\n"
                                  "\n"
                                  "cogl_color_out.a = dof_blur;\n");

      cogl_pipeline_add_snippet (dof_diamond_pipeline, snippet);
      cogl_object_unref (snippet);

      set_focal_parameters (dof_diamond_pipeline, 30.f, 3.0f);

      data->dof_diamond_pipeline = dof_diamond_pipeline;
    }

  /* TODO: move into init() somewhere */
  if (G_UNLIKELY (!data->dof_pipeline))
    {
      CoglPipeline *dof_pipeline =
        cogl_pipeline_copy (data->dof_pipeline_template);
      CoglSnippet *snippet;

      /* store the bluriness in the alpha channel */
      snippet = cogl_snippet_new (COGL_SNIPPET_HOOK_FRAGMENT,
                                  "varying float dof_blur;",

                                  "cogl_color_out.a = dof_blur;\n"
      );
      cogl_pipeline_add_snippet (dof_pipeline, snippet);
      cogl_object_unref (snippet);

      set_focal_parameters (dof_pipeline, 30.f, 3.0f);

      data->dof_pipeline = dof_pipeline;
    }

  if (rut_object_get_type (geometry) == &rut_diamond_type)
    {
      pipeline = cogl_object_ref (data->dof_diamond_pipeline);
    }
  else if (rut_object_get_type (geometry) == &rut_shape_type)
    {
      RutMaterial *material =
        rut_entity_get_component (entity, RUT_COMPONENT_TYPE_MATERIAL);
      CoglSnippet *snippet;

      pipeline = cogl_pipeline_copy (data->dof_diamond_pipeline);

      if (rut_shape_get_shaped (RUT_SHAPE (geometry)))
        {
          CoglTexture *shape_texture =
            rut_shape_get_shape_texture (RUT_SHAPE (geometry));

          cogl_pipeline_set_layer_texture (pipeline, 0, shape_texture);
        }

      if (material)
        {
          RutAsset *texture_asset = rut_material_get_texture_asset (material);
          RutAsset *alpha_mask_asset =
            rut_material_get_alpha_mask_asset (material);

          if (texture_asset)
            cogl_pipeline_set_layer_texture (pipeline, 1,
                                             rut_asset_get_texture (texture_asset));

          if (alpha_mask_asset)
            {
              /* We don't want this layer to be automatically modulated with the
               * previous layers so we set its combine mode to "REPLACE" so it
               * will be skipped past and we can sample its texture manually */
              cogl_pipeline_set_layer_combine (pipeline, 2, "RGBA=REPLACE(PREVIOUS)", NULL);
              cogl_pipeline_set_layer_texture (pipeline, 2,
                                               rut_asset_get_texture (alpha_mask_asset));

              snippet = cogl_snippet_new (COGL_SNIPPET_HOOK_FRAGMENT,
                                          /* definitions */
                                          "uniform float material_alpha_threshold;\n",

                                          /* post */
                                          "if (texture2D(cogl_sampler2, cogl_tex_coord2_in.st).a <= \n"
                                          "    material_alpha_threshold)\n"
                                          "  discard;\n");

              cogl_pipeline_add_snippet (pipeline, snippet);
              cogl_object_unref (snippet);
            }
        }
    }
  else
    pipeline = cogl_object_ref (data->dof_pipeline);

  rut_entity_set_pipeline_cache (entity, CACHE_SLOT_SHADOW, pipeline);

  return pipeline;
}

static CoglPipeline *
get_entity_color_pipeline (RigData *data,
                           RutEntity *entity,
                           RutComponent *geometry)
{
  CoglSnippet *snippet;
  CoglDepthState depth_state;
  RutMaterial *material;
  CoglTexture *texture = NULL;
  CoglTexture *normal_map = NULL;
  CoglTexture *alpha_mask = NULL;
  CoglPipeline *pipeline;
  CoglFramebuffer *shadow_fb;

  pipeline = rut_entity_get_pipeline_cache (entity, CACHE_SLOT_COLOR);
  if (pipeline)
    {
      cogl_object_ref (pipeline);
      goto FOUND;
    }

  pipeline = cogl_pipeline_new (data->ctx->cogl_context);

  material = rut_entity_get_component (entity, RUT_COMPONENT_TYPE_MATERIAL);
  if (material)
    {
      RutAsset *texture_asset = rut_material_get_texture_asset (material);
      RutAsset *normal_map_asset =
        rut_material_get_normal_map_asset (material);
      RutAsset *alpha_mask_asset =
        rut_material_get_alpha_mask_asset (material);

      if (texture_asset)
        texture = rut_asset_get_texture (texture_asset);
      if (texture)
        cogl_pipeline_set_layer_texture (pipeline, 1, texture);

      if (normal_map_asset)
        normal_map = rut_asset_get_texture (normal_map_asset);

      if (alpha_mask_asset)
        alpha_mask = rut_asset_get_texture (alpha_mask_asset);
    }

#if 0
  /* NB: Our texture colours aren't premultiplied */
  cogl_pipeline_set_blend (pipeline,
                           "RGB = ADD(SRC_COLOR*(SRC_COLOR[A]), DST_COLOR*(1-SRC_COLOR[A]))"
                           "A   = ADD(SRC_COLOR, DST_COLOR*(1-SRC_COLOR[A]))",
                           NULL);
#endif

#if 0
  if (rut_object_get_type (geometry) == &rut_shape_type)
    rut_geometry_component_update_pipeline (geometry, pipeline);

  pipeline = cogl_pipeline_new (rut_cogl_context);
#endif

  cogl_pipeline_set_color4f (pipeline, 0.8f, 0.8f, 0.8f, 1.f);

  /* enable depth testing */
  cogl_depth_state_init (&depth_state);
  cogl_depth_state_set_test_enabled (&depth_state, TRUE);
  cogl_pipeline_set_depth_state (pipeline, &depth_state, NULL);

  /* Vertex shader setup for lighting */
  snippet = cogl_snippet_new (COGL_SNIPPET_HOOK_VERTEX,

      /* definitions */
      "uniform mat3 normal_matrix;\n"
      "varying vec3 normal, eye_direction;\n",

      /* post */
      "normal = normalize(normal_matrix * cogl_normal_in);\n"
      "eye_direction = -vec3(cogl_modelview_matrix * cogl_position_in);\n"
  );

  cogl_pipeline_add_snippet (pipeline, snippet);
  cogl_object_unref (snippet);

  if (normal_map)
    {
      snippet = cogl_snippet_new (COGL_SNIPPET_HOOK_VERTEX,
          /* definitions */
          "uniform vec3 light0_direction_norm;\n"
          "attribute vec3 tangent_in;\n"
          "varying vec3 light_direction;\n",

          /* post */
          "vec3 tangent = normalize(normal_matrix * tangent_in);\n"
          "vec3 binormal = cross(normal, tangent);\n"

          /* Transform the light direction into tangent space */
          "vec3 v;\n"
          "v.x = dot (light0_direction_norm, tangent);\n"
          "v.y = dot (light0_direction_norm, binormal);\n"
          "v.z = dot (light0_direction_norm, normal);\n"
          "light_direction = normalize (v);\n"

          /* Transform the eye direction into tangent space */
          "v.x = dot (eye_direction, tangent);\n"
          "v.y = dot (eye_direction, binormal);\n"
          "v.z = dot (eye_direction, normal);\n"
          "eye_direction = normalize (v);\n"
      );

      cogl_pipeline_add_snippet (pipeline, snippet);
      cogl_object_unref (snippet);
    }

  if (rut_entity_get_receive_shadow (entity))
    {
      /* Vertex shader setup for shadow mapping */
      snippet = cogl_snippet_new (COGL_SNIPPET_HOOK_VERTEX,

          /* definitions */
          "uniform mat4 light_shadow_matrix;\n"
          "varying vec4 shadow_coords;\n",

          /* post */
          "shadow_coords = light_shadow_matrix * cogl_position_in;\n"
      );

      cogl_pipeline_add_snippet (pipeline, snippet);
      cogl_object_unref (snippet);
    }

  /* and fragment shader */

  if (material)
    {
      if (alpha_mask)
        {
          /* We don't want this layer to be automatically modulated with the
           * previous layers so we set its combine mode to "REPLACE" so it
           * will be skipped past and we can sample its texture manually */
          cogl_pipeline_set_layer_combine (pipeline, 2, "RGBA=REPLACE(PREVIOUS)", NULL);
          cogl_pipeline_set_layer_texture (pipeline, 2, alpha_mask);

          snippet = cogl_snippet_new (COGL_SNIPPET_HOOK_FRAGMENT,
                                      /* definitions */
                                      "uniform float material_alpha_threshold;\n",

                                      /* post */
                                      "if (texture2D(cogl_sampler2, cogl_tex_coord2_in.st).a <= \n"
                                      "    material_alpha_threshold)\n"
                                      "  discard;\n");

          cogl_pipeline_add_snippet (pipeline, snippet);
          cogl_object_unref (snippet);
        }

      if (normal_map)
        {
          /* We don't want this layer to be automatically modulated with the
           * previous layers so we set its combine mode to "REPLACE" so it
           * will be skipped past and we can sample its texture manually */
          cogl_pipeline_set_layer_combine (pipeline, 5, "RGBA=REPLACE(PREVIOUS)", NULL);
          cogl_pipeline_set_layer_texture (pipeline, 5, normal_map);

          snippet = cogl_snippet_new (COGL_SNIPPET_HOOK_FRAGMENT,
              /* definitions */
              "uniform vec4 light0_ambient, light0_diffuse, light0_specular;\n"
              "uniform vec4 material_ambient, material_diffuse, material_specular;\n"
              "uniform float material_shininess;\n"
              "varying vec3 light_direction, eye_direction;\n",

              /* post */
              "vec4 final_color;\n"

              "if (cogl_color_out.a <= 0.0)\n"
              "  discard;\n"

              "vec3 L = normalize(light_direction);\n"

	      "vec3 N = texture2D(cogl_sampler5, cogl_tex_coord5_in.st).rgb;\n"
	      "N = 2.0 * N - 1.0;\n"
              "N = normalize(N);\n"

              "vec4 ambient = light0_ambient * material_ambient;\n"

              "final_color = ambient * cogl_color_out;\n"
              "float lambert = dot(N, L);\n"

              "if (lambert > 0.0)\n"
              "{\n"
              "  vec4 diffuse = light0_diffuse * material_diffuse;\n"
              "  vec4 specular = light0_specular * material_specular;\n"

              "  final_color += cogl_color_out * diffuse * lambert;\n"

              "  vec3 E = normalize(eye_direction);\n"
              "  vec3 R = reflect (-L, N);\n"
              "  float specular_factor = pow (max(dot(R, E), 0.0), material_shininess);\n"
              "  final_color += specular * specular_factor;\n"
              "}\n"

              "cogl_color_out = final_color;\n"
          );
        }
      else
        {
          snippet = cogl_snippet_new (COGL_SNIPPET_HOOK_FRAGMENT,
              /* definitions */
              "varying vec3 normal, eye_direction;\n"
              "uniform vec4 light0_ambient, light0_diffuse, light0_specular;\n"
              "uniform vec3 light0_direction_norm;\n"
              "uniform vec4 material_ambient, material_diffuse, material_specular;\n"
              "uniform float material_shininess;\n",

              /* post */
              "vec4 final_color;\n"

              "if (cogl_color_out.a <= 0.0)\n"
              "  discard;\n"

              "vec3 L = light0_direction_norm;\n"
              "vec3 N = normalize(normal);\n"

              "vec4 ambient = light0_ambient * material_ambient;\n"

              "final_color = ambient * cogl_color_out;\n"
              "float lambert = dot(N, L);\n"

              "if (lambert > 0.0)\n"
              "{\n"
              "  vec4 diffuse = light0_diffuse * material_diffuse;\n"
              "  vec4 specular = light0_specular * material_specular;\n"

              "  final_color += cogl_color_out * diffuse * lambert;\n"

              "  vec3 E = normalize(eye_direction);\n"
              "  vec3 R = reflect (-L, N);\n"
              "  float specular_factor = pow (max(dot(R, E), 0.0), material_shininess);\n"
              "  final_color += specular * specular_factor;\n"
              "}\n"

              "cogl_color_out = final_color;\n"
          );
        }
    }
  else
    {
      snippet = cogl_snippet_new (COGL_SNIPPET_HOOK_FRAGMENT,
          /* definitions */
          "varying vec3 normal, eye_direction;\n"
          "uniform vec4 light0_ambient, light0_diffuse, light0_specular;\n"
          "uniform vec3 light0_direction_norm;\n",

          /* post */
          "vec4 final_color;\n"

          "vec3 L = light0_direction_norm;\n"
          "vec3 N = normalize(normal);\n"

          "if (cogl_color_out.a <= 0.0)\n"
          "  discard;\n"

          "final_color = light0_ambient * cogl_color_out;\n"
          "float lambert = dot(N, L);\n"

          "if (lambert > 0.0)\n"
          "{\n"
          "  final_color += cogl_color_out * light0_diffuse * lambert;\n"

          "  vec3 E = normalize(eye_direction);\n"
          "  vec3 R = reflect (-L, N);\n"
          "  float specular = pow (max(dot(R, E), 0.0),\n"
          "                        2.);\n"
          "  final_color += light0_specular * vec4(.6, .6, .6, 1.0) * specular;\n"
          "}\n"

          "cogl_color_out = final_color;\n"
      );
    }

  cogl_pipeline_add_snippet (pipeline, snippet);
  cogl_object_unref (snippet);


  if (rut_entity_get_receive_shadow (entity))
    {
      /* Hook the shadow map sampling */

      cogl_pipeline_set_layer_texture (pipeline, 7, data->shadow_map);
      /* For debugging the shadow mapping... */
      //cogl_pipeline_set_layer_texture (pipeline, 7, data->shadow_color);
      //cogl_pipeline_set_layer_texture (pipeline, 7, data->gradient);

      /* We don't want this layer to be automatically modulated with the
       * previous layers so we set its combine mode to "REPLACE" so it
       * will be skipped past and we can sample its texture manually */
      cogl_pipeline_set_layer_combine (pipeline, 7, "RGBA=REPLACE(PREVIOUS)", NULL);

      /* Handle shadow mapping */

      snippet = cogl_snippet_new (COGL_SNIPPET_HOOK_FRAGMENT,
          /* declarations */
          "varying vec4 shadow_coords;\n",

          /* post */
          "vec4 texel7 =  texture2D (cogl_sampler7, shadow_coords.xy);\n"
          "float distance_from_light = texel7.z + 0.0005;\n"
          "float shadow = 1.0;\n"
          "if (distance_from_light < shadow_coords.z)\n"
          "  shadow = 0.5;\n"

          "cogl_color_out = shadow * cogl_color_out;\n"
      );

      cogl_pipeline_add_snippet (pipeline, snippet);
      cogl_object_unref (snippet);
    }

  if (rut_object_get_type (geometry) == &rut_shape_type)
    {
      CoglTexture *shape_texture;

      if (rut_shape_get_shaped (RUT_SHAPE (geometry)))
        {
          shape_texture =
            rut_shape_get_shape_texture (RUT_SHAPE (geometry));
          cogl_pipeline_set_layer_texture (pipeline, 0, shape_texture);
        }

      rut_shape_add_reshaped_callback (RUT_SHAPE (geometry),
                                       reshape_cb,
                                       NULL,
                                       NULL);
    }
  else if (rut_object_get_type (geometry) == &rut_diamond_type)
    rut_diamond_apply_mask (RUT_DIAMOND (geometry), pipeline);

  rut_entity_set_pipeline_cache (entity, CACHE_SLOT_COLOR, pipeline);

FOUND:

  /* FIXME: there's lots to optimize about this! */
  shadow_fb = COGL_FRAMEBUFFER (data->shadow_fb);

  /* update uniforms in pipelines */
  {
    CoglMatrix light_shadow_matrix, light_projection;
    CoglMatrix model_transform;
    const float *light_matrix;
    int location;

    cogl_framebuffer_get_projection_matrix (shadow_fb, &light_projection);

    /* XXX: This is pretty bad that we are having to do this. It would
     * be nicer if cogl exposed matrix-stacks publicly so we could
     * maintain the entity model_matrix incrementally as we traverse
     * the scenegraph. */
    rut_graphable_get_transform (entity, &model_transform);

    get_light_modelviewprojection (&model_transform,
                                   data->light,
                                   &light_projection,
                                   &light_shadow_matrix);

    light_matrix = cogl_matrix_get_array (&light_shadow_matrix);

    location = cogl_pipeline_get_uniform_location (pipeline,
                                                   "light_shadow_matrix");
    cogl_pipeline_set_uniform_matrix (pipeline,
                                      location,
                                      4, 1,
                                      FALSE,
                                      light_matrix);
  }

  return pipeline;
}

static CoglPipeline *
get_entity_pipeline (RigData *data,
                     RutEntity *entity,
                     RutComponent *geometry,
                     RigPass pass)
{
  if (pass == RIG_PASS_COLOR)
    return get_entity_color_pipeline (data, entity, geometry);
  else if (pass == RIG_PASS_DOF_DEPTH || pass == RIG_PASS_SHADOW)
    return get_entity_mask_pipeline (data, entity, geometry);

  g_warn_if_reached ();
  return NULL;
}

static void
draw_entity_camera_frustum (RigData *data,
                            RutEntity *entity,
                            CoglFramebuffer *fb)
{
  RutCamera *camera =
    rut_entity_get_component (entity, RUT_COMPONENT_TYPE_CAMERA);
  CoglPrimitive *primitive = rut_camera_create_frustum_primitive (camera);
  CoglPipeline *pipeline = cogl_pipeline_new (rut_cogl_context);
  CoglDepthState depth_state;

  /* enable depth testing */
  cogl_depth_state_init (&depth_state);
  cogl_depth_state_set_test_enabled (&depth_state, TRUE);
  cogl_pipeline_set_depth_state (pipeline, &depth_state, NULL);

  cogl_framebuffer_draw_primitive (fb, pipeline, primitive);

  cogl_object_unref (primitive);
  cogl_object_unref (pipeline);
}

static RutTraverseVisitFlags
entitygraph_pre_paint_cb (RutObject *object,
                          int depth,
                          void *user_data)
{
  RigPaintContext *paint_ctx = user_data;
  RutPaintContext *rut_paint_ctx = user_data;
  RutCamera *camera = rut_paint_ctx->camera;
  CoglFramebuffer *fb = rut_camera_get_framebuffer (camera);

  if (rut_object_is (object, RUT_INTERFACE_ID_TRANSFORMABLE))
    {
      const CoglMatrix *matrix = rut_transformable_get_matrix (object);
      cogl_framebuffer_push_matrix (fb);
      cogl_framebuffer_transform (fb, matrix);
    }

  if (rut_object_get_type (object) == &rut_entity_type)
    {
      RutEntity *entity = RUT_ENTITY (object);
      RutComponent *geometry;
      CoglPipeline *pipeline;
      CoglPrimitive *primitive;
      CoglMatrix modelview_matrix;
      float normal_matrix[9];
      RutMaterial *material;

      if (!rut_entity_get_visible (entity) ||
          (paint_ctx->pass == RIG_PASS_SHADOW && !rut_entity_get_cast_shadow (entity)))
        return RUT_TRAVERSE_VISIT_CONTINUE;

      geometry =
        rut_entity_get_component (object, RUT_COMPONENT_TYPE_GEOMETRY);
      if (!geometry)
        {
          if (!paint_ctx->data->play_mode &&
              object == paint_ctx->data->light)
            draw_entity_camera_frustum (paint_ctx->data, object, fb);
          return RUT_TRAVERSE_VISIT_CONTINUE;
        }

      pipeline = get_entity_pipeline (paint_ctx->data,
                                      object,
                                      geometry,
                                      paint_ctx->pass);

      if (paint_ctx->pass == RIG_PASS_COLOR)
        {
          int location;
          RutLight *light = rut_entity_get_component (paint_ctx->data->light,
                                                      RUT_COMPONENT_TYPE_LIGHT);
          /* FIXME: only update the lighting uniforms when the light has
           * actually moved! */
          rut_light_set_uniforms (light, pipeline);

          /* FIXME: only update the material uniforms when the material has
           * actually changed! */
          material = rut_entity_get_component (object, RUT_COMPONENT_TYPE_MATERIAL);
          if (material)
            rut_material_flush_uniforms (material, pipeline);

          cogl_framebuffer_get_modelview_matrix (fb, &modelview_matrix);
          get_normal_matrix (&modelview_matrix, normal_matrix);

          location = cogl_pipeline_get_uniform_location (pipeline, "normal_matrix");
          cogl_pipeline_set_uniform_matrix (pipeline,
                                            location,
                                            3, /* dimensions */
                                            1, /* count */
                                            FALSE, /* don't transpose again */
                                            normal_matrix);
        }

      primitive = rut_primable_get_primitive (geometry);
      cogl_framebuffer_draw_primitive (fb,
                                       pipeline,
                                       primitive);

      cogl_object_unref (pipeline);

      return RUT_TRAVERSE_VISIT_CONTINUE;
    }

  return RUT_TRAVERSE_VISIT_CONTINUE;
}

static RutTraverseVisitFlags
entitygraph_post_paint_cb (RutObject *object,
                           int depth,
                           void *user_data)
{
  if (rut_object_is (object, RUT_INTERFACE_ID_TRANSFORMABLE))
    {
      RutPaintContext *rut_paint_ctx = user_data;
      CoglFramebuffer *fb = rut_camera_get_framebuffer (rut_paint_ctx->camera);
      cogl_framebuffer_pop_matrix (fb);
    }

  return RUT_TRAVERSE_VISIT_CONTINUE;
}

static void
paint_scene (RigPaintContext *paint_ctx)
{
  RutPaintContext *rut_paint_ctx = &paint_ctx->_parent;
  RigData *data = paint_ctx->data;
  CoglContext *ctx = data->ctx->cogl_context;
  CoglFramebuffer *fb = rut_camera_get_framebuffer (rut_paint_ctx->camera);

  if (paint_ctx->pass == RIG_PASS_COLOR)
    {
      CoglPipeline *pipeline = cogl_pipeline_new (ctx);
      cogl_pipeline_set_color4f (pipeline, 0, 0, 0, 1.0);
      cogl_framebuffer_draw_rectangle (fb,
                                       pipeline,
                                       0, 0,
                                       data->device_width, data->device_height);
                                       //0, 0, data->pane_width, data->pane_height);
      cogl_object_unref (pipeline);
    }

  rut_graphable_traverse (data->scene,
                          RUT_TRAVERSE_DEPTH_FIRST,
                          entitygraph_pre_paint_cb,
                          entitygraph_post_paint_cb,
                          paint_ctx);

}

void
rig_paint_camera_entity (RutEntity *camera, RigPaintContext *paint_ctx)
{
  RutPaintContext *rut_paint_ctx = &paint_ctx->_parent;
  RutCamera *save_camera = rut_paint_ctx->camera;
  RutCamera *camera_component =
    rut_entity_get_component (camera, RUT_COMPONENT_TYPE_CAMERA);
  RigData *data = paint_ctx->data;
  CoglFramebuffer *fb = rut_camera_get_framebuffer (camera_component);
  //CoglFramebuffer *shadow_fb;

  rut_paint_ctx->camera = camera_component;

  if (rut_entity_get_component (camera, RUT_COMPONENT_TYPE_LIGHT))
    paint_ctx->pass = RIG_PASS_SHADOW;
  else
    paint_ctx->pass = RIG_PASS_COLOR;

  camera_update_view (data, camera, paint_ctx->pass);

  if (paint_ctx->pass != RIG_PASS_SHADOW &&
      data->enable_dof)
    {
      const float *viewport = rut_camera_get_viewport (camera_component);
      int width = viewport[2];
      int height = viewport[3];
      int save_viewport_x = viewport[0];
      int save_viewport_y = viewport[1];
      RigPass save_pass = paint_ctx->pass;
      CoglFramebuffer *pass_fb;

      rut_camera_set_viewport (camera_component, 0, 0, width, height);

      rut_dof_effect_set_framebuffer_size (data->dof, width, height);

      pass_fb = rut_dof_effect_get_depth_pass_fb (data->dof);
      rut_camera_set_framebuffer (camera_component, pass_fb);

      rut_camera_flush (camera_component);
      cogl_framebuffer_clear4f (pass_fb,
                                COGL_BUFFER_BIT_COLOR|COGL_BUFFER_BIT_DEPTH,
                                1, 1, 1, 1);

      paint_ctx->pass = RIG_PASS_DOF_DEPTH;
      paint_scene (paint_ctx);
      paint_ctx->pass = save_pass;

      rut_camera_end_frame (camera_component);

      pass_fb = rut_dof_effect_get_color_pass_fb (data->dof);
      rut_camera_set_framebuffer (camera_component, pass_fb);

      rut_camera_flush (camera_component);
      cogl_framebuffer_clear4f (pass_fb,
                                COGL_BUFFER_BIT_COLOR|COGL_BUFFER_BIT_DEPTH,
                                0.22, 0.22, 0.22, 1);

      paint_ctx->pass = RIG_PASS_COLOR;
      paint_scene (paint_ctx);
      paint_ctx->pass = save_pass;

      rut_camera_end_frame (camera_component);

      rut_camera_set_framebuffer (camera_component, fb);
      rut_camera_set_clear (camera_component, FALSE);

      rut_camera_flush (camera_component);

      rut_camera_end_frame (camera_component);

      rut_camera_set_viewport (camera_component,
                               save_viewport_x,
                               save_viewport_y,
                               width, height);
      rut_paint_ctx->camera = save_camera;
      rut_camera_flush (save_camera);
      rut_dof_effect_draw_rectangle (data->dof,
                                     rut_camera_get_framebuffer (save_camera),
                                     data->main_x,
                                     data->main_y,
                                     data->main_x + data->main_width,
                                     data->main_y + data->main_height);
      rut_camera_end_frame (save_camera);
    }
  else
    {
      rut_camera_set_framebuffer (camera_component, fb);

      rut_camera_flush (camera_component);

      paint_scene (paint_ctx);

      rut_camera_end_frame (camera_component);
    }

  if (paint_ctx->pass == RIG_PASS_COLOR)
    {
      rut_camera_flush (camera_component);

      /* Use this to visualize the depth-of-field alpha buffer... */
#if 0
      CoglPipeline *pipeline = cogl_pipeline_new (data->ctx->cogl_context);
      cogl_pipeline_set_layer_texture (pipeline, 0, data->dof.depth_pass);
      cogl_pipeline_set_blend (pipeline, "RGBA=ADD(SRC_COLOR, 0)", NULL);
      cogl_framebuffer_draw_rectangle (fb,
                                       pipeline,
                                       0, 0,
                                       200, 200);
#endif

      /* Use this to visualize the shadow_map */
#if 0
      CoglPipeline *pipeline = cogl_pipeline_new (data->ctx->cogl_context);
      cogl_pipeline_set_layer_texture (pipeline, 0, data->shadow_map);
      //cogl_pipeline_set_layer_texture (pipeline, 0, data->shadow_color);
      cogl_pipeline_set_blend (pipeline, "RGBA=ADD(SRC_COLOR, 0)", NULL);
      cogl_framebuffer_draw_rectangle (fb,
                                       pipeline,
                                       0, 0,
                                       200, 200);
#endif

      if (data->debug_pick_ray && data->picking_ray)
      //if (data->picking_ray)
        {
          cogl_framebuffer_draw_primitive (fb,
                                           data->picking_ray_color,
                                           data->picking_ray);
        }

#ifdef RIG_EDITOR_ENABLED
      if (!_rig_in_device_mode && !data->play_mode)
        {
          draw_jittered_primitive4f (data, fb, data->grid_prim, 0.5, 0.5, 0.5);

          if (data->selected_entity)
            {
              rut_tool_update (data->tool, data->selected_entity);
              rut_tool_draw (data->tool, fb);
            }
        }
#endif /* RIG_EDITOR_ENABLED */

      rut_camera_end_frame (camera_component);
    }

  rut_paint_ctx->camera = save_camera;
}

void
rig_dirty_entity_pipelines (RutEntity *entity)
{
  rut_entity_set_pipeline_cache (entity, CACHE_SLOT_COLOR, NULL);
  rut_entity_set_pipeline_cache (entity, CACHE_SLOT_SHADOW, NULL);
}