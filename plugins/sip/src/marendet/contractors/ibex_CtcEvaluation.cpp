//============================================================================
//                                  I B E X                                   
// File        : ibex_CtcEvaluation.cpp
// Author      : Antoine Marendet, Gilles Chabert
// Copyright   : Ecole des Mines de Nantes (France)
// License     : See the LICENSE file
// Created     : May 4, 2018
// Last Update : May 4, 2018
//============================================================================

#include "ibex_CtcEvaluation.h"

#include "system/ibex_SIConstraintCache.h"

#include "ibex_Function.h"
#include "ibex_Interval.h"
#include "ibex_IntervalVector.h"

#include <vector>

namespace ibex {
CtcEvaluation::CtcEvaluation(const SIPSystem& system)
: Ctc(system.ext_nb_var), system_(system) {

}

CtcEvaluation::~CtcEvaluation() {
}

void CtcEvaluation::add_property(const IntervalVector& init_box, BoxProperties& map) {
    if(map[BxpNodeData::id] == nullptr) {
        map.add(new BxpNodeData());
    }
}

void CtcEvaluation::contract(IntervalVector& box) {
    ibex_warning("CtcEvaluation: called with no context");

}

void CtcEvaluation::contract(IntervalVector& box, ContractContext& context) {
	if(system_.goal_function_ != NULL)
		box[nb_var-1] = box[nb_var-1] & system_.goal_function_->eval(box.subvector(0, nb_var-2));

	BxpNodeData* node_data = (BxpNodeData*) context.prop[BxpNodeData::id];
	if(node_data == nullptr) {
		ibex_error("CtcEvaluation: BxpNodeData must be set");
	}

	for(int i = 0; i < system_.sic_constraints_.size(); ++i) {
		auto& cache = node_data->sic_constraints_caches[i];
		//system_.sic_constraints_[i].cache_->update_cache(*system_.sic_constraints_[i].function_, box);
		cache.update_cache(*system_.sic_constraints_[i].function_, box);
		//const auto& cacheList = system_.node_data_->sic_constraints_caches[i];
		for(const auto& mem_box : cache.parameter_caches_) {
			if(mem_box.evaluation.lb() > 0) {
				box.set_empty();
				return;
			}
		}
	}
	int normalConstraintsWithoutGoal;
	if(system_.goal_function_ != NULL) {
		normalConstraintsWithoutGoal = system_.normal_constraints_.size()-1;
	} else {
		normalConstraintsWithoutGoal = system_.normal_constraints_.size();
	}
	for(int i = 0; i < normalConstraintsWithoutGoal; ++i) {
		if(system_.normal_constraints_[i].evaluate(box).lb() > 0) {
			box.set_empty();
			return;
		}
	}
}
} // end namespace ibex
