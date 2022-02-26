# Default script run when a camera is created
# $arg1 is the name of the camera
# $DELIGHT_AUTOSPAREPARAMS environment variable is used to load/unload 3Delight spare parameters
# $arg2 !="" tells us that the script will be called from the shelf button.

\set noalias = 1
if ( "$arg1" != "" && ("$DELIGHT_AUTOSPAREPARAMS" !=0 || "$arg2" !="")) then

	# Remember whether some parameters were already defined before parameter
	# collections are added.
	set fov_defined = `run("opparm -ql $arg1 _3dl_fov")`
	set fov_updated = `run("opparm -ql $arg1 _3dl_updated_fov")`
	set shutter_defined = `run("opparm -ql $arg1 _3dl_shutter_duration")`
	set blades_defined = `run("opparm -ql $arg1 _3dl_aperture_blades")`
	set focal_distance_defined = `run("opparm -ql $arg1 _3dl_focal_distance")`
	set fstop_defined = `run("opparm -ql $arg1 _3dl_fstop")`
	set projection_defined = `run("opparm -ql $arg1 _3dl_projection_type")`
	set offset_defined = `run("opparm -ql $arg1 _3dl_shutter_offset")`

	# Add properties into tab 3Delight
	opproperty -f $arg1 3Delight _3dl_shutter_title
	opproperty -f $arg1 3Delight _3dl_shutter_group
	opproperty -f $arg1 3Delight _3dl_shutter_group2
	if("$fstop_defined" == "" && "$blades_defined" != "") then
		# Here, we want to add fstop and focal_distance in their proper
		# position, but the other parameters of their collection were already
		# there.

		# Retrieve existing values in order to set them back at the end
		set enable_dof = `ch("$arg1/_3dl_enable_dof")`
		set enable_blades = `ch("$arg1/_3dl_enable_aperture_blades")`
		set blades = `ch("$arg1/_3dl_aperture_blades")`
		set blades_rotation = `ch("$arg1/_3dl_aperture_blades_rotation")`

		# Remove all parameters from the aperture collection and its title
		# collection.
		opspare -d _3dl_aperture_space $arg1
		opspare -d _3dl_aperture_separator1 $arg1
		opspare -d _3dl_aperture_label1 $arg1
		opspare -d _3dl_aperture_separator2 $arg1
		opspare -d _3dl_enable_dof $arg1
		opspare -d _3dl_enable_aperture_blades $arg1
		opspare -d _3dl_aperture_blades $arg1
		opspare -d _3dl_aperture_blades_rotation $arg1

		# Add back whole aperture collection and its title collection
		opproperty -f $arg1 3Delight _3dl_aperture_title
		opproperty -f $arg1 3Delight _3dl_aperture_group

		# Set back values of previously existing parameters
		opparm -q $arg1 _3dl_enable_dof $enable_dof
		opparm -q $arg1 _3dl_enable_aperture_blades $enable_blades
		opparm -q $arg1 _3dl_aperture_blades $blades
		opparm -q $arg1 _3dl_aperture_blades_rotation $blades_rotation
	else
		opproperty -f $arg1 3Delight _3dl_aperture_title
		opproperty -f $arg1 3Delight _3dl_aperture_group
	endif

	opproperty -f $arg1 3Delight _3dl_lens_distortion_title
	opproperty -f $arg1 3Delight _3dl_lens_distortion_group
	opproperty -f $arg1 3Delight _3dl_projection_title
	opproperty -f $arg1 3Delight _3dl_projection_group

	# Link some parameters to their existing Houdini equivalent using an
	# expression, taking care of not overriding a new value specified by the
	# user.

	if("$fov_defined" == "" || "$fov_updated" == "") then
		# Houdini's camera has an horizontal aperture parameter.  However,
		# since we don't specify a "screenwindow" when exporting the screen,
		# it will range from -1 to 1 along the vertical axis (while the
		# horizontal values will depend upon the image resolution), so the
		# "fov" attribute will be used as a vertical field of view.
		#
		# Here, we suppose that the camera's aperture and focal length are
		# using the same unit, which seems to be what Mantra does, even though
		# the "focalunits" parameter seems to apply only to the focal length
		# in the UI.
		# Also update the fov for previously saved scene with the new changes.
		chadd -t 0 0 $arg1 _3dl_fov
		chkey -t 0 -T a -F '2.0 * atan((ch("aperture") / 2.0) * (ch("resy") / (ch("resx") * ch("aspect"))) / ch("focal"))' $arg1/_3dl_fov
		chkey -t 0 -T a -F '2.0 * atan((ch("aperture") / 2.0) * (ch("resy") / (ch("resx") * ch("aspect"))) / ch("focal"))' $arg1/_3dl_updated_fov
	endif
	if("$shutter_defined" == "") then
		chadd -t 0 0 $arg1 _3dl_shutter_duration
		chkey -t 0 -T a -F 'ch("shutter")' $arg1/_3dl_shutter_duration
	endif
	if("$focal_distance_defined" == "") then
		chadd -t 0 0 $arg1 _3dl_focal_distance
		chkey -t 0 -T a -F 'ch("focus")' $arg1/_3dl_focal_distance
	endif
	if("$fstop_defined" == "") then
		chadd -t 0 0 $arg1 _3dl_fstop
		chkey -t 0 -T a -F 'ch("fstop")' $arg1/_3dl_fstop
	endif
	if("$projection_defined" == "") then
		# String values are a special case since they can be their own
		# expression when surrounded by backticks.
		opparm -q $arg1 _3dl_projection_type '`ifs(strcmp(chs("projection"),"ortho")==0,"orthographiccamera",ifs(strcmp(chs("projection"),"cylinder")==0,"cylindricalcamera",ifs(strcmp(chs("projection"),"sphere")==0,"sphericalcamera","perspectivecamera")))`'
	endif
	if("$offset_defined" == "") then
		chadd -t 0 0 $arg1 _3dl_shutter_offset
		chkey -t 0 -T a -F 'ifs(strcmp(chs("_3dl_shutter_offset_type"),"close")==0,-1,ifs(strcmp(chs("_3dl_shutter_offset_type"),"center")==0,0,ifs(strcmp(chs("_3dl_shutter_offset_type"),"open")==0,1,0)))' $arg1/_3dl_shutter_offset
	endif

   opproperty -f $arg1 3Delight _3dl_obj_geo_label2_group
   opproperty -f $arg1 3Delight _3dl_transformation_blur
   opproperty -f $arg1 3Delight _3dl_transformation_extra_samples

endif
