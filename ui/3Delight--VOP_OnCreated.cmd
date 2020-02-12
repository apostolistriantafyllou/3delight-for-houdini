# Default script run when one of our VOP 3Delight is created
# $arg1 is the name of the node

\set noalias = 1
if ( "$arg1" != "" ) then

	set i_color_def = `run("opparm -ql $arg1 i_color")`
	if("$i_color_def" != "") then

		set ogl_diff_def = `run("opparm -ql $arg1 ogl_diff")`
		if("$ogl_diff_def" == "") then
			opproperty -f -F OpenGL $arg1 material* ogl_diff
		endif

		chadd -t 0 0 $arg1 ogl_diffr
		chkey -t 0 -F 'ch("i_colorr")' $arg1/ogl_diffr
		chadd -t 0 0 $arg1 ogl_diffg
		chkey -t 0 -F 'ch("i_colorg")' $arg1/ogl_diffg
		chadd -t 0 0 $arg1 ogl_diffb
		chkey -t 0 -F 'ch("i_colorb")' $arg1/ogl_diffb

	endif

endif
