# Default script run when a scene is opened or created

set geo_nodes = `run("opfind -S -p /obj -t geo")`
#message Geometry nodes are $geo_nodes
if( "$geo_nodes" == "" ) then
#	message No geometry nodes found!
	exit
endif

foreach node ( $geo_nodes )
	obj/geo.cmd $node
end

set cam_nodes = `run("opfind -S -p /obj -t cam")`
#message Camera nodes are $cam_nodes
if( "$cam_nodes" == "" ) then
#	message No camera nodes found!
	exit
endif

foreach node ( $cam_nodes )
	obj/cam.cmd $node
end
