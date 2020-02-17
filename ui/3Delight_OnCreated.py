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
    import toolutils
    viewer = toolutils.sceneViewer()
    # to get the current matrix of viewport
    matrix = viewer.curViewport().viewTransform()
    obj = hou.node("/obj")
    strs = camname.rsplit("/", 1)
    newnode = obj.createNode("cam", strs[1])
    newnode.moveToGoodPosition()
    # we want cam to see objects in viewport
    newnode.setWorldTransform(matrix)
    settings = viewer.curViewport().settings()
    settings.setCamera(newnode)
