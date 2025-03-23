#include "probeManager.h"

const sc2::Unit* ProbeManager::FindBuilder(std::vector<const sc2::Unit*> our_workers) {
	if (our_workers.empty()) {
		return nullptr;
	}
	
	// Just return the first idle worker for now
	for (const auto& worker : our_workers) {
		if (worker->orders.empty()) {
			return worker;
		}
	}
	
	// If no idle workers, just return the first one
	return our_workers.front();
}
