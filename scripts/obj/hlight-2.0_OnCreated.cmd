# Default script run when a node has been created

# message obj/hlight-2.0_OnCreated is called with $arg1
\set noalias = 1
if ( "$arg1" != "" ) then
    opproperty -f $arg1 3Delight _3dl_spread
# patch to move folder after the tab "Misc"
	python $arg0.py $arg1
    opproperty -f $arg1 3Delight _3dl_obj_light_contrib_title
    opproperty -f $arg1 3Delight _3dl_diffuse_contribution
    opproperty -f $arg1 3Delight _3dl_specular_contribution
    opproperty -f $arg1 3Delight _3dl_hair_contribution
    opproperty -f $arg1 3Delight _3dl_volume_contribution
endif
