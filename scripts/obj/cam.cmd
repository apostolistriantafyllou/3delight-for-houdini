# Default script run when a camera is created
# $arg1 is the name of the camera

\set noalias = 1
if ( "$arg1" != "" ) then

	# Remember whether some parameters were already defined before parameter
	# collections are added.
	set fov_defined = `run("opparm -ql $arg1 _3dl_fov")`
	set shutter_defined = `run("opparm -ql $arg1 _3dl_shutter_duration")`
	set focal_distance_defined = `run("opparm -ql $arg1 _3dl_focal_distance")`
	set fstop_defined = `run("opparm -ql $arg1 _3dl_fstop")`
	set projection_defined = `run("opparm -ql $arg1 _3dl_projection_type")`

	# Add properties into tab 3Delight
	opproperty -f $arg1 3Delight _3dl_shutter_title
	opproperty -f $arg1 3Delight _3dl_shutter_group
	opproperty -f $arg1 3Delight _3dl_aperture_title
	opproperty -f $arg1 3Delight _3dl_aperture_group
	opproperty -f $arg1 3Delight _3dl_lens_distortion_title
	opproperty -f $arg1 3Delight _3dl_lens_distortion_group
	opproperty -f $arg1 3Delight _3dl_projection_title
	opproperty -f $arg1 3Delight _3dl_projection_group

	# Link some parameters to their existing Houdini equivalent using an
	# expression, taking care of not overriding a new value specified by the
	# user.

	if("$fov_defined" == "") then
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
		chadd -t 0 0 $arg1 _3dl_fov
		chkey -t 0 -T a -F '2.0 * atan((ch("aperture") / 2.0) * (ch("resy") / ch("resx")) / ch("focal"))' $arg1/_3dl_fov
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

endif
