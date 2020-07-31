from __future__ import print_function
import hou
import voptoolutils
import re


class VopEffect(object):
    def __init__(self, node_type=None, preset=None, label=None,
                 data_types=None, custom_creator=None, passthrough_input=None,
                 output=0):
        """ Describes a Shader Effect

            node_type
                The type of node to create. Inputs of the created node will be
                created recursively using input presets. A named preset may be
                specified using 'preset'

            preset
                The name of the input preset to use. If this is None and node_type
                is specified, the default preset for the node type will be used.

            custom_creator
                A callback to create a custom setup.
        """

        # Sanity checks
        if node_type is not None and custom_creator is not None:
            raise hou.Error("Conflicting arguments: when providing a "
                            "custom_creator node_type may not be provided as well")
        elif node_type is None and custom_creator is None:
            raise hou.Error("Both node_type and custom_creator are None "
                            "This would not create anything")
        elif (node_type is not None and
                hou.nodeType(hou.vopNodeTypeCategory(), node_type) is None):
            raise hou.Error("node_type \'%s\' doesn\'t exist in VOPs" %
                            node_type)

        self.node_type = node_type
        self.preset = preset
        self.data_types = data_types
        self.custom_creator = custom_creator
        self.output = output
        if label is not None:
            self.label = label
        elif node_type is not None:
            type = hou.nodeType(hou.vopNodeTypeCategory(), node_type)
            self.label = type.description()
        self.passthrough_input = passthrough_input

    def execute(self, target_node, input_idx):
        orig_input_node = voptoolutils.getInputNode(target_node, input_idx)
        keep_input = self.passthrough_input is not None and orig_input_node is not None
        if keep_input:
            conn = target_node.inputConnections()[0]
            out_idx = conn.outputIndex()

        # TODO: Implement radio button strip in the shader effects menu
        # to change the preference for this. Until then we just do this:
        parm_gen_type = hou.vopParmGenType.Parameter

        # parm_mode = hou.expandString('$VOPFX_PARMMODE')
        # if parm_mode == 'subnetinput':
        #     parm_gen_type = hou.vopParmGenType.SubnetInput \
        #         if target_node.parent().type().category() == hou.vopNodeTypeCategory() and \
        #         target_node.parent().childTypeCategory() == hou.vopNodeTypeCategory() \
        #         else hou.vopParmGenType.Parameter
        # elif parm_mode == 'shader':
        #     parm_gen_type = hou.vopParmGenType.Parameter
        # else:
        #     parm_gen_type = None

        created_nodes = self._executeEffect(
            target_node,
            input_idx,
            replace_existing=not keep_input,
            parm_gen_type=parm_gen_type,
            output_index_or_name=self.output)

        voptoolutils.movePromotedChildParameters(
            target_node.parent(),
            included_parm_vops=created_nodes)

        if keep_input and len(created_nodes):
            voptoolutils.setConnection(orig_input_node, out_idx,
                                       created_nodes[0],
                                       self.passthrough_input)

        # voptoolutils.embedInputParms(target_node, input_idx)
        #
        # for node in created_nodes:
        #     node.hide(True)

        return created_nodes

    def _executeEffect(self, target_node, input_index_or_name,
                       mix_with_existing_type=None, mix_node_input=None,
                       replace_existing=True,
                       parm_gen_type=hou.vopParmGenType.SubnetInput,
                       output_index_or_name=0):
        """
            mix_with_existing_type
                If this is a node type, any existing node plugged into the input
                will be kept and newly created nodes will be mixed with the original
                input using the node_type specified, i.e. 'multiply', 'colormix', etc.

            parm_gen_type
                Specifies the parmscope to use when creating Parameter VOPs.
        """

        # target_node = hou.node(kwargs['node'])
        # input_idx = int(kwargs['input'])
        # parm = target_node.parmTuple(kwargs['parm'])
        input_idx = voptoolutils.getInputIndex(target_node, input_index_or_name)
        parm = voptoolutils.getInputParmTuple(target_node, input_idx)

        # If this is a parm node, we need to turn on its input.
        if target_node.parm('usebound') is not None:
            target_node.parm('usebound').set(True)
            input_idx = 0

        # Get the node currently plugged into our input.
        orig_input_node = None
        connector = voptoolutils.getInputConnector(target_node, input_idx)
        if connector is not None:
            orig_input_node = connector.inputNode()

        input_name = voptoolutils.getInputName(target_node, input_idx)

        # Ensure the input is visible
        # This may fail if we're on a parameter node, since inputs
        # on it are handled in a special way.
        try:
            target_node.setIsInputVisible(input_name, True)
        except:
            pass

        # This is the node we'll plug into our input.
        final_out_node = None

        # We'll use this to accumulate nodes we create so we can
        # position them at the end.
        created_nodes = ()

        if self.node_type is not None:
            cfg = voptoolutils.VopInputPresetManager()

        # Check if we should delete the nodes currently plugged into our input.
        if (replace_existing and orig_input_node is not None and
                mix_with_existing_type is None):
            # We have an existing input node and we're not mixing with it
            # so we might be ok to delete the input node.
            if (self.node_type is None or
                    not cfg.currentInputUsedByPreset(self.node_type, self.preset)):
                # Either no node_type was provided, so we're not using an input preset,
                # or we are but the preset isn't going to use the existing input.
                #
                # Delete any nodes that only output to this input.
                voptoolutils.destroyExclusiveInputNodes(
                    target_node,
                    endnode_input=input_idx)

        if self.node_type is not None:
            # Get data associated with out target input
            input_data = voptoolutils.getInputData(target_node, input_idx)
            created_nodes = cfg.createInput(target_node, input_idx, type='node',
                                            node_type=self.node_type, preset=self.preset,
                                            passthrough_input=input_data,
                                            replace_existing=False,
                                            node_output=output_index_or_name,
                                            parm_gen_type=parm_gen_type)
            final_out_node = created_nodes[0] if len(created_nodes) > 0 else None
        elif self.custom_creator is not None:
            # If we're provided with a custom creator, see if it does anything:
            custom_source_nodes = ()
            custom_res = self.custom_creator.generateNodes(
                target_node, input_idx, parm,
                self.node_type,
                custom_source_nodes)
            if len(custom_res) >= 2:
                final_out_node = custom_res[0]
                created_nodes = custom_res[1]

        if final_out_node is not None:
            if mix_with_existing_type is not None:
                mix_node = voptoolutils.insertFilterNode(
                    target_node, input_idx,
                    orig_input_node, final_out_node,
                    mix_with_existing_type, mix_node_input)
                if mix_node is not None:
                    created_nodes = created_nodes + (mix_node, )

            voptoolutils.positionItems(target_node, created_nodes)

        return created_nodes

    def supportsDataType(self, data_type):
        if self.data_types is None or len(self.data_types) == 0:
            return True
        else:
            return data_type in self.data_types

    def __repr__(self):
        return '<VopEffect label:\'%s\'>' % self.label

""" 
	TODO: Restrict 3Delight Materials(delight_shader_nodes)
	to connect to closure data type only
"""
effects = {
    'delight_pattern_nodes': [
        VopEffect('3Delight::bulge'),
        VopEffect('3Delight::checker'),
        VopEffect('3Delight::cloth'),
        VopEffect('3Delight::fractal'),
        VopEffect('3Delight::granite'),
        VopEffect('3Delight::grid'),
        VopEffect('3Delight::leather'),
        VopEffect('3Delight::noise'),
        VopEffect('3Delight::dlRamp'),
        VopEffect('3Delight::dlTexture'),
    ],

	'delight_pattern3d_nodes': [
        VopEffect('3Delight::brownian'),
        VopEffect('3Delight::cloud'),
        VopEffect('3Delight::dlFlakes'),
        VopEffect('3Delight::marble'),
        VopEffect('3Delight::rock'),
        VopEffect('3Delight::dlSolidRamp'),
        VopEffect('3Delight::solidFractal'),
        VopEffect('3Delight::dlTriplanar'),
        VopEffect('3Delight::volumeNoise'),
        VopEffect('3Delight::wood'),
        VopEffect('3Delight::dlWorleyNoise'),
    ],

	'delight_utilities_nodes': [
        VopEffect('3Delight::dlAttributeRead'),
        VopEffect('3Delight::clamp'),
        VopEffect('3Delight::dlColorBlend'),
        VopEffect('3Delight::dlColorBlendMulti'),
        VopEffect('3Delight::dlColorCorrection'),
        VopEffect('3Delight::dlColorToFloat'),
        VopEffect('3Delight::dlColorVariation'),
        VopEffect('3Delight::condition'),
        VopEffect('3Delight::dlFacingRatio'),
        VopEffect('3Delight::dlFloatBlend'),
        VopEffect('3Delight::dlFloatMath'),
        VopEffect('3Delight::dlFloatToColor'),
        VopEffect('3Delight::dlFloatToUV'),
        VopEffect('3Delight::gammaCorrect'),
        VopEffect('3Delight::dlRandomInputColor'),
        VopEffect('3Delight::dlRemap'),
        VopEffect('3Delight::setRange'),
        VopEffect('3Delight::dlUV'),
    ],
	
	'delight_shader_nodes': [
        VopEffect('3Delight::dlAtmosphere',data_types=['color']),
        VopEffect('3Delight::dlCarPaint',data_types=['color']),
        VopEffect('3Delight::dlGlass',data_types=['color']),
        VopEffect('3Delight::dlHairAndFur',data_types=['color']),
        VopEffect('3Delight::dlLayeredMaterial',data_types=['color']),
        VopEffect('3Delight::dlMetal',data_types=['color']),
        VopEffect('3Delight::dlPrincipled',data_types=['color']),
        VopEffect('3Delight::dlRandomMaterial',data_types=['color']),
        VopEffect('3Delight::dlSkin',data_types=['color']),
        VopEffect('3Delight::dlSubstance',data_types=['color']),
        VopEffect('3Delight::dlThin',data_types=['color']),
        VopEffect('3Delight::dlToon',data_types=['color']),
    ],
}


def generateMenuItems(kwargs, submenu_name):
    if 'node' not in kwargs:
        return []
    target_node = hou.node(kwargs['node'])
    input_idx = kwargs['input']
    data_types = target_node.inputDataTypes()
    parm_types = target_node.inputParmTypes()
    if input_idx < 0 or input_idx > len(data_types):
        print('menu script kwargs[\'input\'] is out of range(%d)' % input_idx)
        return []
    items = []
    data_type = data_types[input_idx]
    parm_type = parm_types[input_idx]
    parm_tuple = voptoolutils.getInputParmTuple(target_node, input_idx)
    query_type = parm_type if parm_tuple is not None and \
        not data_type.startswith('struct_') and \
        len(parm_type) != 0 \
        else data_type
    for i, effect in enumerate(effects[submenu_name]):
        if effect.supportsDataType(query_type):
            # Append effect index to submenu_name to get a unique token.
            effect_token = "%s_%d" % (submenu_name, i)
            items.append(effect_token)
            items.append(effect.label)
    return items


def executeMenuAction(kwargs, submenu_name):
    token = kwargs['selectedtoken']

    # Extract effect id from menu token.
    match = re.match(".*_([0-9]+)$", token)
    if not match:
        raise hou.Error('Internal error finding action for kwargs[\'selectedtoken\']')

    effect_id = int(match.groups()[0])

    effect = effects[submenu_name][effect_id]
    target_node = hou.node(kwargs['node'])
    input_idx = kwargs['input']
    effect.execute(target_node, input_idx)


def canCreateSubnetInput(kwargs):
    return False


def deleteInputNodes(kwargs):
    target_node = hou.node(kwargs['node'])
    input_idx = int(kwargs['input'])
    voptoolutils.destroyExclusiveInputNodes(target_node,endnode_input=input_idx)

