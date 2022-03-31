# This is the module which handles the translation 3Delight for Houdini's
# shader VOP nodes to USD shaders.
import hou
if hou.applicationVersion()[0] >= 19:
    from husd.shadertranslator \
        import ShaderTranslator as TranslatorBase
    from husd.shadertranslator \
        import ShaderTranslatorHelper as HelperBase
else:
    from husdshadertranslators.default \
        import DefaultShaderTranslator as TranslatorBase
    from husdshadertranslators.default \
        import DefaultShaderTranslatorHelper as HelperBase

class NSIShaderTranslatorHelper(HelperBase):
    def _createMBShader(self, mb_node, requested_shader_type):
        # The default houdini translator attempts to translate the material
        # builder itself which is no good. This forwards translation to the
        # shader inside the 3Delight Material Builder.
        shader_type_to_terminal = {
            hou.shaderType.Surface: 'Surface',
            hou.shaderType.BSDF: 'Surface',
            hou.shaderType.Displacement: 'Displacement',
            hou.shaderType.Atmosphere: 'Volume'}
        # Map houdini shader type to input name of dlTerminal shader.
        terminal = shader_type_to_terminal.get(requested_shader_type, None)
        # Look for a dlTerminal node.
        for child in mb_node.children():
            if child.type().name() == '3Delight::dlTerminal':
                # Pick the right input and translate it.
                for conn in child.inputConnections():
                    if conn.outputName() == terminal:
                        return self.createAndConnectUsdTerminalShader(
                            conn.inputNode(), requested_shader_type,
                            conn.inputName())

    def createAndConnectUsdTerminalShader(self, shader_node, 
            requested_shader_type, shader_node_output_name):
        """ See husdshadertranslators.default """
        if shader_node.type().name() == '3Delight::dlMaterialBuilder':
            return self._createMBShader(shader_node, requested_shader_type)
        return super(NSIShaderTranslatorHelper, self) \
            .createAndConnectUsdTerminalShader(shader_node,
                requested_shader_type, shader_node_output_name)

    
class NSIShaderTranslator(TranslatorBase):
    def matchesRenderMask(self, render_mask):
        """ See husdshadertranslators.default """
        return render_mask == 'nsi'

    def shaderTranslatorHelper(self, translator_id, usd_stage,
            usd_material_path, usd_time_code):
        """ Return our own translator helper. """
        return NSIShaderTranslatorHelper(translator_id,
            usd_stage, usd_material_path, usd_time_code)

################################################################################
# In order to be considered for shader translation, this python module
# implements the factory function that returns a translator object.
# See husd.shadertranslator.usdShaderTranslator() for details.
nsi_translator = NSIShaderTranslator()
def usdShaderTranslator():
    return nsi_translator

# vim: set softtabstop=4 expandtab shiftwidth=4:
