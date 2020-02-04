newnode = kwargs.get("node", None)
camname = newnode.evalParm("camera")

camnode = hou.node(camname)
found = camnode != None
if found:
    nodetype = camnode.type()
    found = nodetype.name() == "cam"
if not found:
    obj = hou.node("/obj")
    # try to find another camera
    for node in obj.children():
        nodetype = node.type()
        if nodetype.name() == "cam":
            newnode.setParms({"camera": node.path()})
            found = True
            break
# no camera found: creates one
if not found:
    obj = hou.node("/obj")
    strs = camname.rsplit("/", 1)
    newnode = obj.createNode("cam", strs[1])