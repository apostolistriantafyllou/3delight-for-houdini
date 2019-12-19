import sys

# get node to patch
node = hou.node(sys.argv[1])
# get template group
group = node.parmTemplateGroup()
# get the folder to move
dl_folder = group.containingFolder("_3dl_spread")
# remove it
group.remove(dl_folder)
# then insert it after the ever-present "Misc" folder
group.insertAfter(group.findIndicesForFolder("Misc"), dl_folder)
# update the node
node.setParmTemplateGroup(group)
