# -*- coding: utf-8 -*-

# Copyright (c) 2015-2016 Ericsson AB
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import functools
import mpy_port
import calvinsys

def manage(include=None, exclude=None):
    if include and type(include) is not list or exclude and type(exclude) is not list:
        raise Exception("@manage decorator: Must use list as argument")

    include = set(include) if include else set()
    exclude = set(exclude) if exclude else set()

    def wrap(f):
        @functools.wraps(f)
        def manage_wrapper(instance, *args, **kwargs):
            exclude.update(instance.__dict__)
            r = f(instance, *args, **kwargs)
            if not include:
                # include set not given, so construct the implicit include set
                include.update(instance.__dict__)
                include.remove('_managed')
                include.difference_update(exclude)
            instance._managed = list(include)
            return r
        return manage_wrapper
    return wrap



def condition(action_input=[], action_output=[]):
    """
    Decorator condition specifies the required input data and output space.
    Both parameters are lists of tuples: (port, #tokens consumed/produced)
    Optionally, the port spec can be a port only, meaning #tokens is 1.
    Return value is an ActionResult object

    FIXME:
    - Modify ActionResult to specify how many tokens were read/written from/to each port
      E.g. ActionResult.tokens_consumed/produced are dicts: {'port1':4, 'port2':1, ...}
      Since reading is done in the wrapper, tokens_consumed is fixed and given by action_input.
      The action fills in tokens_produced and the wrapper uses that info when writing to ports.
    - We can keep the @condition syntax by making the change in the normalize step below.
    """
    #
    # Normalize argument list (fill in a default repeat of 1 if not stated)
    #
    action_input = [p if isinstance(p, (list, tuple)) else (p, 1) for p in action_input]
    action_output = [p if isinstance(p, (list, tuple)) else (p, 1) for p in action_output]

    def wrap(action_method):

        @functools.wraps(action_method)
        def condition_wrapper(self):
            # Check if input ports have enough tokens. Note that all([]) evaluates to True
            input_ok = mpy_port.ccmp_tokens_available(self.cc_actor, action_input)
            output_ok = mpy_port.ccmp_slots_available(self.cc_actor, action_output)

            if not input_ok or not output_ok:
                return ActionResult(did_fire=False)  # FIXME Is it enough to return actionResult.did_fire

            # Build the arguments for the action from the input port(s)
            # TODO check for exception tokens
            args = mpy_port.ccmp_peek_token(self.cc_actor, action_input)

            # Perform the action (N.B. the method may be wrapped in a guard)
            action_result = action_method(self, *args)

            if action_result.did_fire:  # and valid_production:
                # Commit to the read from the FIFOs
                mpy_port.ccmp_peek_commit(self.cc_actor, action_input)
                # Write the results from the action to the output port(s)
                mpy_port.ccmp_write_token(self.cc_actor, action_output, action_result.production)
            else:
                # cancel the read from the FIFOs
                mpy_port.ccmp_peek_cancel(self.cc_actor, action_input)

            return action_result  # FIXME Is it enough to return actionResult.did_fire
        return condition_wrapper
    return wrap


def guard(action_guard):
    """
    Decorator guard refines the criteria for picking an action to run by stating a function
    with THE SAME signature as the guarded action returning a boolean (True if action allowed).
    If the speciified function is unbound or a lambda expression, you must account for 'self',
    e.g. 'lambda self, a, b: a>0'
    """

    def wrap(action_method):

        @functools.wraps(action_method)
        def guard_wrapper(self, *args):
            retval = ActionResult(did_fire=False)  # FIXME Is it enough to return actionResult.did_fire
            guard_ok = action_guard(self, *args)
            if guard_ok:
                retval = action_method(self, *args)
            return retval

        return guard_wrapper
    return wrap


class ActionResult(object):

    """Return type from action and @guard"""

    def __init__(self, did_fire=True, production=()):
        super(ActionResult, self).__init__()
        self.did_fire = did_fire
        self.production = production

    def merge(self, other_result):
        """
        Update this ActionResult by mergin data from other_result:
             did_fire will be OR:ed together
             production will be DISCARDED
        """
        self.did_fire |= other_result.did_fire


class Actor(object):
    """
    Base class for all actors
    """

    # Class variable controls action priority order
    action_priority = tuple()

    test_args = ()
    test_kwargs = {}

    def __init__(self, cc_actor):
        """Should _not_ be overridden in subclasses."""
        super(Actor, self).__init__()
        self.cc_actor = cc_actor
        self._managed = set(('id', 'name', '_deployment_requirements', '_signature', 'subject_attributes', 'migration_info'))
        #TODO handle calvinsys
        self._calvinsys = calvinsys.Sys()
        self._using = {}

    def init(self):
        raise Exception("Implementing 'init()' is mandatory.")

    #TODO handle calvinsys
    def __getitem__(self, attr):
        if attr in self._using:
            return self._using[attr]
        raise KeyError(attr)

    #TODO handle calvinsys
    def use(self, requirement, shorthand):
        self._using[shorthand] = self._calvinsys.use_requirement(self, requirement)

    def fire(self):
        total_result = ActionResult(did_fire=False)
        while True:
            # Re-try action in list order after EVERY firing
            for action_method in self.__class__.action_priority:
                action_result = action_method(self)
                total_result.merge(action_result)
                # Action firing should fire the first action that can fire,
                # hence when fired start from the beginning
                if action_result.did_fire:
                    break

            if not action_result.did_fire:
                # We reached the end of the list without ANY firing => return
                return total_result.did_fire
        # Redundant as of now, kept as reminder for when rewriting exception handling.
        raise Exception('Exit from fire should ALWAYS be from previous line.')

    def state(self):
        state = {}
        # Manual state handling
        # Not available until after __init__ completes
        state['_managed'] = list(self._managed)
        # Managed state handling
        for key in self._managed:
            obj = self.__dict__[key]
            if _implements_state(obj):
                state[key] = obj.state()
            else:
                state[key] = obj
        return state

    def exception_handler(self, action, args, context):
        """Defult handler when encountering ExceptionTokens"""
        raise Exception("ExceptionToken NOT HANDLED")

