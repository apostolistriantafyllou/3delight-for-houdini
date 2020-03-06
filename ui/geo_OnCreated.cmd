# Default script run when a geometry object is created
# $arg1 is the name of the object to create

\set noalias = 1
if ( "$arg1" != "" ) then
# Add properties into tab 3Delight
   opproperty -f $arg1 3Delight _3dl_render_poly_as_subd
   opproperty -f $arg1 3Delight _3dl_smooth_curves
   opproperty -f $arg1 3Delight _3dl_obj_geo_label1_group
   opproperty -f $arg1 3Delight _3dl_visibility_camera
   opproperty -f $arg1 3Delight _3dl_visibility_diffuse
   opproperty -f $arg1 3Delight _3dl_visibility_reflection
   opproperty -f $arg1 3Delight _3dl_visibility_refraction
   opproperty -f $arg1 3Delight _3dl_compositing
   opproperty -f $arg1 3Delight _3dl_obj_geo_label2_group
   opproperty -f $arg1 3Delight _3dl_transformation_blur
   opproperty -f $arg1 3Delight _3dl_transformation_extra_samples
   opproperty -f $arg1 3Delight _3dl_deformation
   opproperty -f $arg1 3Delight _3dl_add_samples
   opproperty -f $arg1 3Delight _3dl_obj_geo_label3_group

   opproperty -f $arg1 3Delight _3dl_spatial_override
   opproperty -f $arg1 3Delight _3dl_override_compositing_group
   opproperty -f $arg1 3Delight _3dl_override_visibility_camera_group
   opproperty -f $arg1 3Delight _3dl_override_visibility_diffuse_group
   opproperty -f $arg1 3Delight _3dl_override_visibility_reflection_group
   opproperty -f $arg1 3Delight _3dl_override_visibility_refraction_group
   opproperty -f $arg1 3Delight _3dl_override_surface_shader
endif
