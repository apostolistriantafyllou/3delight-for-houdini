# Default script run when a geometry object is created
# $arg1 is the name of the object to create

source $HH/scripts/obj/geo.cmd

\set noalias = 1
if ( "$arg1" != "" ) then
# Removed old names (without prefix) and badly named
# space override names

   opspare -d obj_geo_separator1 $arg1
   opspare -d obj_geo_label1 $arg1
   opspare -d obj_geo_separator2 $arg1
   opspare -d space2 $arg1
   opspare -d vis_diffuse $arg1
   opspare -d compositing $arg1
   opspare -d obj_geo_separator3 $arg1
   opspare -d obj_geo_label2 $arg1
   opspare -d obj_geo_separator4 $arg1
   opspare -d space3 $arg1
   opspare -d deformation $arg1
   opspare -d add_samples $arg1
   opspare -d obj_geo_separator5 $arg1
   opspare -d obj_geo_label3 $arg1
   opspare -d obj_geo_separator6 $arg1
   opspare -d override_vol $arg1
   opspare -d over_compositing_enable $arg1
   opspare -d over_compositing $arg1
   opspare -d over_vis_camera_enable $arg1
   opspare -d over_vis_camera $arg1
   opspare -d over_vis_diffuse_enable $arg1
   opspare -d over_vis_diffuse $arg1
   opspare -d over_vis_reflection_enable $arg1
   opspare -d over_vis_reflection $arg1
   opspare -d over_vis_refraction_enable $arg1
   opspare -d over_vis_refraction $arg1
   opspare -d override_ss $arg1
   opspare -d _3dl_override_vol $arg1
   opspare -d _3dl_over_compositing_group $arg1
   opspare -d _3dl_over_vis_camera_group $arg1
   opspare -d _3dl_over_vis_diffuse_group $arg1
   opspare -d _3dl_over_vis_reflection_group $arg1
   opspare -d _3dl_over_vis_refraction_group $arg1
   opspare -d _3dl_override_ss $arg1

# Add properties into tab 3Delight
   opproperty -f $arg1 3Delight _3dl_render_poly_as_subd
   opproperty -f $arg1 3Delight _3dl_smooth_curves
   opproperty -f $arg1 3Delight _3dl_obj_geo_label1_group
   opproperty -f $arg1 3Delight _3dl_vis_diffuse
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
