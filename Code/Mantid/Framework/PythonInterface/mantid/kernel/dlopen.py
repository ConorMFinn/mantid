"""
Provides a function to load a module that is a dynamic library such that the
flags used to open the shared library ensure that all symbols are imported
by default.

For Mantid this ensures the singleton symbols are wired up correctly
"""
import sys
import os
import platform

#######################################################################
# Ensure the correct Mantid shared libaries are found when loading the
# python shared libraries
#######################################################################
if sys.platform.startswith('win32'):
    _var = 'PATH'
    _sep = ';'
else:
    _sep = ':'
    if sys.platform.startswith('linux'):
        _var = 'LD_LIBRARY_PATH'
    elif sys.platform.startswith('darwin'):
        _var = 'DYLD_LIBRARY_PATH'
    else: # Give it a go how it is
        _var = ''
        _sep = ''
try:
    _oldpath = os.environ[_var]
except:
    _oldpath = ''
    _sep = ''
os.environ[_var] = os.environ['MANTIDPATH'] + _sep + _oldpath       

#######################################################################
# Public api
#######################################################################
        
def setup_dlopen(library, depends=[]):
    """Set the flags for a call to import a shared library
    such that all symbols are imported.
    
    On Linux this sets the flags for dlopen so that 
    all symbols from the library are imported in to
    the global symbol table.
    
    Without this each shared library gets its own
    copy of any singleton, which is not the correct
    behaviour
    
    Args:
      library - The path to the library we are opening
      depends - A list of dependents to open (default=[])
    
    Returns the original flags
    """
    if os.name == 'nt': return None
    old_flags = sys.getdlopenflags()

    import _dlopen
    dlloader = _dlopen.loadlibrary
    import subprocess
    
    def get_libpath(mainlib, dependency):
        if platform.system() == 'Linux':
            cmd = 'ldd %s | grep %s' % (mainlib, dependency)
            part = 2
        else:
            cmd = 'otool -L %s | grep %s' % (mainlib, dependency)
            part = 0
        subp = subprocess.Popen(cmd,stdout=subprocess.PIPE,
                                stderr=subprocess.STDOUT,shell=True)
        out = subp.communicate()[0]
        # ldd produces a string that always has 4 columns. The full path
        # is in the 3rd column
        return out.split()[part]
    # stdc++ has to be loaded first or exceptions don't get translated 
    # properly across bounadries
    # NeXus has to be loaded as well as there seems to be an issue with
    # the thread-local storage not being initialized properly unles
    # it is loaded before other libraries.
    _bin = os.path.join(os.path.abspath(os.path.dirname(__file__)), '../../')
    library_var = "LD_LIBRARY_PATH"
    if platform.system() == 'Darwin':
        library_var = 'DY' + library_var
    ldpath = os.environ.get(library_var, "")
    ldpath += ":" + _bin
    os.environ[library_var] = ldpath

    pythonlib = library
    dlloader(get_libpath(pythonlib, 'stdc++'))
    dlloader(get_libpath(pythonlib, 'libNeXus'))
    for dep in depends:
        dlloader(get_libpath(pythonlib, dep))

    oldflags = sys.getdlopenflags()
    if platform.system() == "Darwin":
        try:
            import dl
            RTLD_LOCAL = dl.RTLD_LOCAL
            RTLD_NOW = dl.RTLD_NOW
        except ImportError:
            RTLD_LOCAL = 0x4
            RTLD_NOW = 0x2
        sys.setdlopenflags(RTLD_LOCAL|RTLD_NOW)
    
    return old_flags
    
def restore_flags(flags):
    """Restores the dlopen flags to those provided,
    usually with the results from a call to setup_dlopen
    """
    if flags is None: return
    sys.setdlopenflags(flags)
