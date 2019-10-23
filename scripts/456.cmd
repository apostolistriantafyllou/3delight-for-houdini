# Default script run when a scene is open or create

set nodes = `run("opfind -S -p /obj -t geo")`
#message Nodes are $nodes
if( "$nodes" == "" ) then
#	message No nodes found!
	exit
endif

foreach node ( $nodes )
	source $HIH/scripts/obj/geo.cmd $node
end
