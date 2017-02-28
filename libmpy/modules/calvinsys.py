packages = {("gpiohandler", "calvinsys.io.gpiohandler"), ("environmental", "calvinsys.sensors.environmental")}

class Sys(object):
    """ Calvin system object """
    def __init__(self):
        self._node = 0
        self.modules = {}

        for package in packages:
			(package_name, _, _) = package[1].partition(".")
			# this is a loadable module
			self.modules[package[1]] = {'name': package[1], 'nameshort': package[0],'module': None, 'error': None}

    def use_requirement(self, actor, modulename):
        if self.require(modulename):
            raise self.modules[modulename]['error']
        return self.modules[modulename]['module'].register(actor)

    def require(self, modulename):
        if not self.modules.get(modulename, None):
            self.modules[modulename] = {'error': Exception("No such capability '%s'" % (modulename,)), 'module': None}

        self._loadmodule(modulename)

        return self.modules[modulename]['error']

    def _loadmodule(self, modulename):
        if self.modules[modulename]['module'] or self.modules[modulename]['error']:
            return
        try:
            self.modules[modulename]['module'] = __import__(self.modules[modulename]['nameshort'])
        except Exception as e:
            self.modules[modulename]['error'] = e

    def has_capability(self, requirement):
        """
        Returns True if "requirement" is satisfied in this system,
        otherwise False.
        """
        return not self.require(requirement)
