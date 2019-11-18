# Default script run when a camera is created
# $arg1 is the name of the camera

source $HH/scripts/obj/cam.cmd

\set noalias = 1
if ( "$arg1" != "" ) then
# Add properties into tab 3Delight
   opproperty -f $arg1 3Delight _3dl_enable_dof
endif
