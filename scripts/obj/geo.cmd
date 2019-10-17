# Default script run when a geometry object is created
# $arg1 is the name of the object to create

source $HH/scripts/obj/geo.cmd

\set noalias = 1
if ( "$arg1" != "" ) then
#   message Some new informative message for $arg1
   opproperty -f $arg1 3Delight obj_geo_label1_group
   opproperty -f $arg1 3Delight vis_diffuse
   opproperty -f $arg1 3Delight compositing
   opproperty -f $arg1 3Delight obj_geo_label2_group
   opproperty -f $arg1 3Delight deformation
   opproperty -f $arg1 3Delight add_samples
   opproperty -f $arg1 3Delight obj_geo_label3_group
   opproperty -f $arg1 3Delight override_vol
   opproperty -f $arg1 3Delight over_compositing_group
   opproperty -f $arg1 3Delight over_vis_camera_group
   opproperty -f $arg1 3Delight over_vis_diffuse_group
   opproperty -f $arg1 3Delight over_vis_reflection_group
   opproperty -f $arg1 3Delight over_vis_refraction_group
   opproperty -f $arg1 3Delight override_ss
endif
