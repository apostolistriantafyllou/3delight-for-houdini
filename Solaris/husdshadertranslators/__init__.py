# The following line allows husdshadertranslators to be a multi-directory 
# (namespace) package.
# I.e., modules in other husdshadertranslators directories will be importable.
# All husdshadertranslators namespace package directories are required to have 
# an __init__.py with this line in it.
__path__ = __import__('pkgutil').extend_path(__path__, __name__)

