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

	set roughness_def = `run("opparm -ql $arg1 roughness")`
	if("$roughness_def" != "") then

		set ogl_diff_rough_def = `run("opparm -ql $arg1 ogl_diff_rough")`
		if("$ogl_diff_rough_def" == "") then
			opproperty -f -F OpenGL $arg1 material* ogl_diff_rough
		endif

		chadd -t 0 0 $arg1 ogl_diff_rough
		chkey -t 0 -F 'ch("roughness")' $arg1/ogl_diff_rough

	endif

	set metallic_def = `run("opparm -ql $arg1 metallic")`
	if("$metallic_def" != "") then

		set ogl_metallic_def = `run("opparm -ql $arg1 ogl_metallic")`
		if("$ogl_metallic_def" == "") then
			opproperty -f -F OpenGL $arg1 material* ogl_metallic
		endif

		chadd -t 0 0 $arg1 ogl_metallic
		chkey -t 0 -F 'ch("metallic")' $arg1/ogl_metallic

	endif

	set incandescence_def = `run("opparm -ql $arg1 incandescence")`
	if("$incandescence_def" != "") then

		set ogl_emit_def = `run("opparm -ql $arg1 ogl_emit")`
		if("$ogl_emit_def" == "") then
			opproperty -f -F OpenGL $arg1 material* ogl_emit
		endif

		chadd -t 0 0 $arg1 ogl_emitr
		chkey -t 0 -F 'ch("incandescencer")' $arg1/ogl_emitr
		chadd -t 0 0 $arg1 ogl_emitg
		chkey -t 0 -F 'ch("incandescenceg")' $arg1/ogl_emitg
		chadd -t 0 0 $arg1 ogl_emitb
		chkey -t 0 -F 'ch("incandescenceb")' $arg1/ogl_emitb

	endif

	set incandescence_intensity_def = `run("opparm -ql $arg1 incandescence_intensity")`
	if("$incandescence_intensity_def" != "") then

		set ogl_emit_intensity_def = `run("opparm -ql $arg1 ogl_emit_intensity")`
		if("$ogl_emit_intensity_def" == "") then
			opproperty -f -F OpenGL $arg1 material* ogl_emit_intensity
		endif

		chadd -t 0 0 $arg1 ogl_emit_intensity
		chkey -t 0 -F 'ch("incandescence_intensity")' $arg1/ogl_emit_intensity

	endif

	set ogl_spec_model_def = `run("opparm -ql $arg1 ogl_spec_model")`
	if("$ogl_spec_model_def" == "" && "$i_color_def" != "" && "$roughness_def"=="" ) then
		opproperty -f -F OpenGL $arg1 material* ogl_spec_model
		opparm $arg1 ogl_spec_model ggx
	endif

endif
