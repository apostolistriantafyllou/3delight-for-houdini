# Script run when a light node has been created

# message $arg0 is called with $arg1
\set noalias = 1
if ( "$arg1" != "" ) then

	opproperty -f $arg1 3Delight _3dl_spread
	opproperty -f $arg1 3Delight _3dl_obj_light_contrib_title
	opproperty -f $arg1 3Delight _3dl_contributions_group
	opproperty -f $arg1 3Delight _3dl_compositing_group_title
	opproperty -f $arg1 3Delight _3dl_light_prelit

	# The 3Delight tab is erroneously added inside an existing tab instead of at
	# the root.  Let's fix this by removing the 3Delight folder from the node
	# and re-inserting it after the "Misc" folder, which is always present.
	# This can't seem to be done in HScript, so we do it in Python.
	# However, since calling an external Python script doesn't always work, we
	# pass the Python statements directly in a string argument.
	# The strange quoting around $arg1, below, is the result of quoting a first
	# string with single-quotes in order for the double-quote to appear in it,
	# then juxtaposing (without spaces, to enable concatenation) a double-quoted
	# string that allows the substitution of arg1's value, then juxtaposing
	# (again without spaces), another single-quoted string that starts with the
	# closing double-quote that matches the one in the first string.

	python -c 'node = hou.node("'"$arg1"'"); \
		group = node.parmTemplateGroup(); \
		dl_folder = group.containingFolder("_3dl_spread"); \
		group.remove(dl_folder); \
		group.insertAfter(group.findIndicesForFolder("Misc"), dl_folder); \
		node.setParmTemplateGroup(group)'

endif
