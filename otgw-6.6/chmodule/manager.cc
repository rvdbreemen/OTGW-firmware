/* IN_MODULE should be defined for modules */
#define IN_MODULE

#include <gpsim/modules.h>

#include "opentherm.h"
#include "probe.h"

Module_Types available_modules[] = {
    {{"thermostat", "master"}, Thermostat::construct},
    {{"boiler", "slave"}, Boiler::construct},
    {{"probe", "probe"}, Probe::construct},
    // No more modules
    {{0,0}, 0}
};

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */
    
/********************************************************************************
 * mod_list - Display all of the modules in this library.
 *
 * This is a required function for gpsim compliant libraries.
 */

    Module_Types *get_mod_list() {
	return available_modules;
    }

#ifdef __cplusplus
}
#endif /* __cplusplus */
