#basic stuff needed in each test

#in each test just do: from esptest import *

#look in this file for basic functions to use in your tests

# from espeasy import *
from node import *
from espeasy import *
from controlleremu import *

import config
import shelve
import os
from espcore import *



### create node objects and espeasy objects

node=[]
espeasy=[]

for n in config.nodes:
    node.append(Node(n, "node"+str(len(node))))
    espeasy.append(EspEasy(node[-1]))


steps=[]

log=logging.getLogger("esptest")


## controller emulators
controller=ControllerEmu()

### keep test state, so we can skip tests.
state={
    'module': None,
    'name': None,
    'title':""
}
with shelve.open("test.state") as shelve_db:
    if 'state' in shelve_db:
        global state
        state=shelve_db['state']


def step(title=""):
    def step_dec(step):
        """add test step. test can resume from every test-step"""
        if state['module'] and ( state['module'] != step.__module__ or state['name'] != step.__name__ or state['title'] != title):
            log.debug("Skipping step "+title+": "+step.__module__ + "." + step.__name__ )
        else:
            step.title=title
            steps.append(step)
            # add the rest of the steps as well
            state['module']=None
    return(step_dec)


### run all the tests
def run():
    """run all tests"""

    for step in steps:
        print()
        log.info("*** Starting "+step.title+": "+step.__module__ + "." + step.__name__ )

        # store this step so we may resume later
        state['module']=step.__module__
        state['name']=step.__name__
        state['title']=step.title
        with shelve.open("test.state") as shelve_db:
            shelve_db['state']=state

        #run the step. if there is an exception we resume this step the next time
        step()


        # log.info("Completed step")

    # all Completed
    os.unlink("test.state")
    log.info("*** All tests complete ***")


### auxillary test functions
def test_in_range(value, min, max):

    if value < min or value > max:
        raise(Exception("Value {value} should be between {min} and {max}".format(value=value, min=min, max=max)))

    log.info("OK: value {value} is between {min} and {max}".format(value=value, min=min, max=max))

def test_is(value, shouldbe):
    if value!=shouldbe:
        raise(Exception("Value {value} should be {shouldbe}".format(value=value, shouldbe=shouldbe)))

    log.info("OK: Value is {value}".format(value=value, shouldbe=shouldbe))
