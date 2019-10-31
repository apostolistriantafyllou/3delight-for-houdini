# Default script run when a geometry object is created
# $arg1 is the name of the object to create

source $HH/scripts/obj/geo.cmd

\set noalias = 1
if ( "$arg1" != "" ) then
#   message Some new informative message for $arg1
   opproperty -f $arg1 3Delight _3dl_obj_geo_label1_group
   opproperty -f $arg1 3Delight _3dl_vis_diffuse
   opproperty -f $arg1 3Delight _3dl_compositing
   opproperty -f $arg1 3Delight _3dl_obj_geo_label2_group
   opproperty -f $arg1 3Delight _3dl_deformation
   opproperty -f $arg1 3Delight _3dl_add_samples
   opproperty -f $arg1 3Delight _3dl_obj_geo_label3_group
   opproperty -f $arg1 3Delight _3dl_override_vol
   opproperty -f $arg1 3Delight _3dl_over_compositing_group
   opproperty -f $arg1 3Delight _3dl_over_vis_camera_group
   opproperty -f $arg1 3Delight _3dl_over_vis_diffuse_group
   opproperty -f $arg1 3Delight _3dl_over_vis_reflection_group
   opproperty -f $arg1 3Delight _3dl_over_vis_refraction_group
   opproperty -f $arg1 3Delight _3dl_override_ss
endif
