# Default script run when a geometry object is created
# $arg1 is the name of the object to create

\set noalias = 1
if ( "$arg1" != ""  && ("$DELIGHT_AUTOSPAREPARAMS" !=0 || "$arg2"!="")) then
   # Add properties into tab 3Delight
   opproperty -f $arg1 3Delight _3dl_obj_geo_label2_group
   opproperty -f $arg1 3Delight _3dl_transformation_blur
   opproperty -f $arg1 3Delight _3dl_transformation_extra_samples
endif
