#ifndef PROBE_MANAGER_H
#define PROBE_MANAGER_H

#include "sc2api/sc2_api.h"

class ProbeManager {
public:
    const sc2::Unit* FindBuilder(std::vector<const sc2::Unit*> our_workers);

private:
};

#endif
