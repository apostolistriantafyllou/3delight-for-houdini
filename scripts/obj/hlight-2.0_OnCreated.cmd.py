import sys

# get node to patch
node = hou.node(sys.argv[1])
# get template group
group = node.parmTemplateGroup()
# get the folder to move
f = group.findFolder(["Misc","3Delight"])
# remove it
group.remove(f.name())
# move the folder at the correct place
ind = group.findIndicesForFolder("Misc")
group.insertAfter(ind,f)
# update the node
node.setParmTemplateGroup(group)
