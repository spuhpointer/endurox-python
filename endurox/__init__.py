"""Root module of your package"""

import sys
import os
import time
import ctypes
import pkgutil
import imp

flags = sys.getdlopenflags()

# Need as Enduro/X XA drivers are dynamically loaded, and
# they need to see Enduro/X runtime.
sys.setdlopenflags(flags | ctypes.RTLD_GLOBAL)

for loader, module_name, is_pkg in pkgutil.walk_packages(__path__):
    if module_name == '_endurox':
        _module = loader.find_module(module_name).load_module('_endurox')
        print(dir(_module));
        print(_module.__name__);
        _module.__name__='endurox';
        sys.modules['endurox'] = _module
 
sys.setdlopenflags(flags)
#__all__ = ['endurox']
