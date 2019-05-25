#include "ibex_LPSolver.h"

namespace ibex {

using namespace soplex;

LPSolver::LPSolver(int nb_vars1, int max_iter, double max_time_out, double eps) :
			nb_vars(nb_vars1), nb_rows(0), boundvar(nb_vars1) , sense(LPSolver::MINIMIZE),
			obj_value(0.0), primal_solution(nb_vars1), dual_solution(1 /*tmp*/),
			status_prim(false), status_dual(false){

	mysoplex= new soplex::SoPlex();
	mysoplex->setIntParam(SoPlex::VERBOSITY, SoPlex::VERBOSITY_ERROR);

	mysoplex->setIntParam(SoPlex::OBJSENSE, SoPlex::OBJSENSE_MINIMIZE);
	mysoplex->setIntParam(SoPlex::ITERLIMIT, max_iter);
	mysoplex->setRealParam(SoPlex::TIMELIMIT, max_time_out);
	mysoplex->setRealParam(SoPlex::FEASTOL, eps);
	mysoplex->setRealParam(SoPlex::OPTTOL, eps);
	mysoplex->setIntParam(SoPlex::SOLVEMODE, SoPlex::SOLVEMODE_REAL);
	
	// initialize the number of variables of the LP
	soplex::DSVectorReal col1(0);
	for (int j=0; j<nb_vars; j++){
		mysoplex->addColReal(soplex::LPColReal(0.0, col1, soplex::infinity, -( soplex::infinity )));
	}

	// initialize the constraint of the bound of the variable
	soplex::DSVectorReal row1(nb_vars);
	for (int j=0; j<nb_vars; j++){
		row1.add (j,1.0);
		mysoplex->addRowReal(soplex::LPRowReal(-soplex::infinity, row1, soplex::infinity));
		row1.clear();
	}

	nb_rows += nb_vars;

}

LPSolver::~LPSolver() {
	delete mysoplex;
}

LPSolver::Status_Sol LPSolver::solve() {
	obj_value = Interval::all_reals();


	SPxSolver::Status stat = SPxSolver::UNKNOWN;

	try{
		status_prim = false;
		status_dual = false;
		stat = mysoplex->solve();
		switch (stat) {
		case SPxSolver::OPTIMAL : {
			obj_value = mysoplex->objValueReal();

			// the primal solution : used by choose_next_variable
			DVectorReal primal(nb_vars);
			mysoplex->getPrimalReal(primal);
			status_prim = true;
			for (int i=0; i< nb_vars ; i++) {
				primal_solution[i]=primal[i];
			}
			// the dual solution ; used by Neumaier Shcherbina test
			DVectorReal dual(nb_rows);

			dual_solution.resize(nb_rows);

			mysoplex->getDualReal(dual);
			status_dual = true;
			for (int i=0; i<nb_rows; i++) {
				if 	( ((mysoplex->rhsReal(i) >=  default_max_bound) && (dual[i]<=0)) ||
						((mysoplex->lhsReal(i) <= -default_max_bound) && (dual[i]>=0))   ) {
					dual_solution[i]=0;
				}
				else {
					dual_solution[i]=dual[i];
				}
			}
			return OPTIMAL;
		}
	    case SPxSolver::ABORT_TIME: {
	    	return TIME_OUT;
	    }
	    case SPxSolver::ABORT_ITER:{
	    	return MAX_ITER;
	    }
	    case SPxSolver::INFEASIBLE:{
	    	return INFEASIBLE;
	    }
	    default : {
	    	return UNKNOWN;
	    }
	    }
	}catch(...){
		return UNKNOWN;
	}

}

void LPSolver::write_file(const char* name) {
	try {
		mysoplex->writeFileReal(name, NULL, NULL, NULL);
	}
	catch(...) {
		throw LPException();
	}
	return ;
}

ibex::Vector LPSolver::get_coef_obj() const {
	ibex::Vector obj(nb_vars);
	try {
		DVectorReal newobj(nb_vars);
		mysoplex->getObjReal(newobj);
		for (int j=0;j<nb_vars; j++){
			obj[j] = newobj[j];
		}
	}
	catch(...) {
		throw LPException();
	}
	return obj;
}

ibex::Matrix LPSolver::get_rows() const {
	ibex::Matrix A(nb_rows, nb_vars);
	try {
		DSVectorReal row;
		for (int i=0;i<nb_rows; i++){
			mysoplex->getRowVectorReal(i, row);
			for (int j=0;j<nb_vars; j++){
				A.row(i)[j] = row[j];
			}
		}
	}
	catch(...) {
		throw LPException();
	}
	return A;
}


ibex::Matrix LPSolver::get_rows_trans() const {
	ibex::Matrix A_trans(nb_vars,nb_rows);
	try {
		DSVectorReal row;
		for (int i=0;i<nb_rows; i++){
			mysoplex->getRowVectorReal(i, row);
			for (int j=0;j<nb_vars; j++){
				A_trans.row(j)[i] = row[j];
			}
		}
	}
	catch(...) {
		throw LPException();
	}
	return A_trans;
}

IntervalVector  LPSolver::get_lhs_rhs() const{
	IntervalVector B(nb_rows);
	try {
		for (int i=0;i<nb_rows; i++){
			B[i]=Interval( mysoplex->lhsReal(i) , mysoplex->rhsReal(i) );
		}
//		// Get the bounds of the variables
//		for (int i=0;i<nb_vars; i++){
//			B[i]=Interval( mysoplex->lhs(i) , mysoplex->rhs(i) );
//		}
//
//		// Get the bounds of the constraints
//		for (int i=nb_vars;i<nb_rows; i++){
//			B[i]=Interval( 	(mysoplex->lhs(i)>-default_max_bound)? mysoplex->lhs(i):-default_max_bound,
//					        (mysoplex->rhs(i)< default_max_bound)? mysoplex->rhs(i): default_max_bound   );
//			//	B[i]=Interval( 	mysoplex->lhs(i),mysoplex->rhs(i));
//			//Idea: replace 1e20 (resp. -1e20) by Sup([g_i]) (resp. Inf([g_i])), where [g_i] is an evaluation of the nonlinear function <-- IA
//			//           cout << B(i+1) << endl;
//		}
	}
	catch(...) {
		throw LPException();
	}
	return B;
}

ibex::Vector LPSolver::get_infeasible_dir() const {

	try {
		ibex::Vector sol(nb_rows);
		SPxSolver::Status stat1;
		DVector sol_found(nb_rows);
		mysoplex->getDualFarkasReal(sol_found);
		//stat1 = mysoplex->getDualfarkas(sol_found);
		// if (stat1==SoPlex::SPxSolver::OPTIMAL) // TODO I'm not sure of the value that return getDualfarkas : this condition does not work BNE
//		for (int i=0; i<nb_rows; i++) {
//			if (((mysoplex->lhs(i) <= -default_max_bound) && (sol_found[i]>=0))||
//				((mysoplex->rhs(i) >=  default_max_bound) && (sol_found[i]<=0))	) {
//				sol[i]=0.0;
//				}
//			else {
//				sol[i]=sol_found[i];
//			}
//		}
		//	else	res = FAIL; this condition does not work BNE

		for(int i=0; i<nb_rows; i++) {
			sol[i]=sol_found[i];
		}
		return sol;
	}
	catch(...) {
		throw LPException();
	}
}

double LPSolver::get_epsilon() const {
	return mysoplex->realParam(SoPlex::FEASTOL);
}

void LPSolver::clean_ctrs() {

	try {
		status_prim = false;
		status_dual = false;
		if ((nb_vars)<=  (nb_rows - 1))  {
			mysoplex->removeRowRangeReal(nb_vars, nb_rows-1);
		}
		nb_rows = nb_vars;
		obj_value = POS_INFINITY;
	}
	catch(...) {
		throw LPException();
	}
	return ;

}


void LPSolver::set_max_iter(int max) {

	try {
		mysoplex->setIntParam(SoPlex::ITERLIMIT, max);
	}
	catch(...) {
		throw LPException();
	}
	return ;
}

void LPSolver::set_max_time_out(double time) {

	try {
		double t = time;
		mysoplex->setRealParam(SoPlex::TIMELIMIT, t);
	}
	catch(...) {
		throw LPException();
	}
	return ;
}

void LPSolver::set_sense(Sense s) {

	try {
		if (s==LPSolver::MINIMIZE) {
			mysoplex->setIntParam(SoPlex::OBJSENSE, SoPlex::OBJSENSE_MINIMIZE);
			sense = LPSolver::MINIMIZE;
		}
		else if (s==LPSolver::MAXIMIZE) {
			mysoplex->setIntParam(SoPlex::OBJSENSE, SoPlex::OBJSENSE_MAXIMIZE);
			sense = LPSolver::MAXIMIZE;
		}
		else
			throw LPException();

	}
	catch(...) {
		throw LPException();
	}
	return ;
}

void LPSolver::set_obj(const ibex::Vector& coef) {

	try {
		for (int i=0;i< nb_vars; i++) {
			mysoplex->changeObjReal(i,coef[i]);
		}
	}
	catch(...) {
		throw LPException();
	}

	return;
}

void LPSolver::set_obj_var(int var, double coef) {

	try {
		mysoplex->changeObjReal(var, coef);
	}
	catch(...) {
		throw LPException();
	}
	return ;
}

void LPSolver::set_bounds(const IntervalVector& bounds) {

	try {
		for (int j=0; j<nb_vars; j++){
			// Change the LHS and RHS of each constraint associated to the bounds of the variable
			mysoplex->changeRangeReal(j ,bounds[j].lb(),bounds[j].ub());
		}
		boundvar = bounds;
	}
	catch(...) {
		throw LPException();
	}
	return ;
}

void LPSolver::set_bounds_var(int var, const Interval& bound) {

	try {
		mysoplex->changeRangeReal(var ,bound.lb(),bound.ub());
		//std::cout << "improve bound var "<<var<< std::endl;
		boundvar[var] = bound;
	}
	catch(...) {
		throw LPException();
	}
	return ;
}

void LPSolver::set_epsilon(double eps) {

	try {
		mysoplex->setRealParam(SoPlex::FEASTOL, eps);
		mysoplex->setRealParam(SoPlex::OPTTOL, eps);
	}
	catch(...) {
		throw LPException();
	}
	return ;
}

void LPSolver::add_constraint(const ibex::Vector& row, CmpOp sign, double rhs) {

	try {
		DSVectorReal row1(nb_vars);
		for (int i=0; i< nb_vars ; i++) {
			row1.add(i, row[i]);
		}

		if (sign==LEQ || sign==LT) {
			mysoplex->addRowReal(LPRowReal(-soplex::infinity, row1, rhs));
			nb_rows++;
		}
		else if (sign==GEQ || sign==GT) {
			mysoplex->addRowReal(LPRowReal(rhs, row1, soplex::infinity));
			nb_rows++;
		}
		else
			throw LPException();

	}
	catch(...) {
		throw LPException();
	}

	return ;
}

} /* end namespace ibex */
