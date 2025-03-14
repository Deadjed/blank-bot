#pragma once
#ifndef BUILD_MANAGER_H
#define BUILD_MANAGER_H

#include "sc2api/sc2_api.h"

class BuildManager {
public:
    void BuildAssimilator(const sc2::ObservationInterface* observation, sc2::ActionInterface* actions);

private:
};

#endif
