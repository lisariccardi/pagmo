/*****************************************************************************
 *   Copyright (C) 2004-2013 The PaGMO development team,                     *
 *   Advanced Concepts Team (ACT), European Space Agency (ESA)               *
 *   http://apps.sourceforge.net/mediawiki/pagmo                             *
 *   http://apps.sourceforge.net/mediawiki/pagmo/index.php?title=Developers  *
 *   http://apps.sourceforge.net/mediawiki/pagmo/index.php?title=Credits     *
 *   act@esa.int                                                             *
 *                                                                           *
 *   This program is free software; you can redistribute it and/or modify    *
 *   it under the terms of the GNU General Public License as published by    *
 *   the Free Software Foundation; either version 3 of the License, or       *
 *   (at your option) any later version.                                     *
 *                                                                           *
 *   This program is distributed in the hope that it will be useful,         *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of          *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the           *
 *   GNU General Public License for more details.                            *
 *                                                                           *
 *   You should have received a copy of the GNU General Public License       *
 *   along with this program; if not, write to the                           *
 *   Free Software Foundation, Inc.,                                         *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.               *
 *****************************************************************************/

#include <Python.h>
#include <boost/python/class.hpp>
#include <boost/python/module.hpp>
#include <boost/python/register_ptr_to_python.hpp>
#include <boost/utility.hpp>
#include <boost/python/enum.hpp>
#include <sstream>
#include <string>

#include "../../src/algorithms.h"
#include "../../src/population.h"
#include "../utils.h"
#include "python_base.h"

using namespace boost::python;
using namespace pagmo;

// Wrapper method for algorithm evolve that uses copy instead of pass-by-non-const-reference.
static inline population evolve_copy(const algorithm::base &a, const population &pop)
{
	population pop_copy(pop);
	a.evolve(pop_copy);
	return pop_copy;
}

// Wrapper to expose algorithms.
template <class Algorithm>
static inline class_<Algorithm,bases<algorithm::base> > algorithm_wrapper(const char *name, const char *descr)
{
	class_<Algorithm,bases<algorithm::base> > retval(name,descr,init<const Algorithm &>());
	retval.def(init<>());
	retval.def("__copy__", &Py_copy_from_ctor<Algorithm>);
	retval.def("__deepcopy__", &Py_deepcopy_from_ctor<Algorithm>);
	retval.def("evolve", &evolve_copy);
	retval.def_pickle(generic_pickle_suite<Algorithm>());
	retval.def("cpp_loads", &py_cpp_loads<Algorithm>);
	retval.def("cpp_dumps", &py_cpp_dumps<Algorithm>);
	return retval;
}

// Meta-algorithms need specialised pickle suites, as they contains pointers to classes that can be implemented in Python.
template <class Algorithm>
struct meta_algorithm_pickle_suite : boost::python::pickle_suite
{
	static boost::python::tuple getinitargs(const Algorithm &)
	{
		return boost::python::make_tuple();
	}
	static boost::python::tuple getstate(const Algorithm &algo)
	{
		std::stringstream ss;
		boost::archive::text_oarchive oa(ss);
		oa << algo;
		return boost::python::make_tuple(ss.str(),algo.get_algorithm());
	}
	static void setstate(Algorithm &algo, boost::python::tuple state)
	{
		if (len(state) != 2)
		{
			PyErr_SetObject(PyExc_ValueError,("expected 2-item tuple in call to __setstate__; got %s" % state).ptr());
			throw_error_already_set();
		}
		const std::string str = extract<std::string>(state[0]);
		std::stringstream ss(str);
		boost::archive::text_iarchive ia(ss);
		ia >> algo;
		const algorithm::base_ptr internal_algo = boost::python::extract<algorithm::base_ptr>(state[1]);
		algo.set_algorithm(*internal_algo);
	}
};

template <>
inline class_<algorithm::ms,bases<algorithm::base> > algorithm_wrapper(const char *name, const char *descr)
{
	class_<algorithm::ms,bases<algorithm::base> > retval(name,descr,init<const algorithm::ms &>());
	retval.def(init<>());
	retval.def("__copy__", &Py_copy_from_ctor<algorithm::ms>);
	retval.def("__deepcopy__", &Py_deepcopy_from_ctor<algorithm::ms>);
	retval.def("evolve", &evolve_copy);
	retval.def_pickle(meta_algorithm_pickle_suite<algorithm::ms>());
	retval.def("cpp_loads", &py_cpp_loads<algorithm::ms>);
	retval.def("cpp_dumps", &py_cpp_dumps<algorithm::ms>);
	return retval;
}

template <>
inline class_<algorithm::mbh,bases<algorithm::base> > algorithm_wrapper(const char *name, const char *descr)
{
	class_<algorithm::mbh,bases<algorithm::base> > retval(name,descr,init<const algorithm::mbh &>());
	retval.def(init<>());
	retval.def("__copy__", &Py_copy_from_ctor<algorithm::mbh>);
	retval.def("__deepcopy__", &Py_deepcopy_from_ctor<algorithm::mbh>);
	retval.def("evolve", &evolve_copy);
	retval.def_pickle(meta_algorithm_pickle_suite<algorithm::mbh>());
	retval.def("cpp_loads", &py_cpp_loads<algorithm::mbh>);
	retval.def("cpp_dumps", &py_cpp_dumps<algorithm::mbh>);
	return retval;
}

BOOST_PYTHON_MODULE(_algorithm) {
	common_module_init();

	// Expose base algorithm class, including the virtual methods.
	class_<algorithm::python_base, boost::noncopyable>("_base", "All algorithms derive from this class. It cannot be instantiated", init<>())
		.def("__repr__",&algorithm::base::human_readable)
		.def("reset_rngs", &algorithm::base::reset_rngs)
		.add_property("screen_output",&algorithm::base::get_screen_output,&algorithm::base::set_screen_output)
		// Virtual methods that can be (re)implemented.
		.def("get_name", &algorithm::base::get_name, &algorithm::python_base::default_get_name)
		// NOTE: This needs special treatment because its prototype changes in the wrapper.
		.def("evolve",&algorithm::python_base::py_evolve, "Returns the evolved population")
		.def("human_readable_extra", &algorithm::base::human_readable_extra, &algorithm::python_base::default_human_readable_extra)
		.def_pickle(python_class_pickle_suite<algorithm::python_base>());

	// Exposing enums
	enum_<algorithm::sga::mutation::type>("_mutation_type")
		.value("RANDOM", algorithm::sga::mutation::RANDOM)
		.value("GAUSSIAN", algorithm::sga::mutation::GAUSSIAN);
	
	enum_<algorithm::sga::crossover::type>("_crossover_type")
		.value("BINOMIAL", algorithm::sga::crossover::BINOMIAL)
		.value("EXPONENTIAL", algorithm::sga::crossover::EXPONENTIAL);
		
	enum_<algorithm::sga::selection::type>("_selection_type")
		.value("BEST20", algorithm::sga::selection::BEST20)
		.value("ROULETTE", algorithm::sga::selection::ROULETTE);
		
	// Expose algorithms.

	// Null.
	algorithm_wrapper<algorithm::null>("null","Null algorithm.");

	// IHS.
	algorithm_wrapper<algorithm::ihs>("ihs","Improved harmony search.")
		.def(init<optional<int, const double &, const double &, const double &, const double &, const double &> >());
	
	// CS.
	algorithm_wrapper<algorithm::cs>("cs","Compass search solver.")
		.def(init<const int &, const double &, optional<const double &, const double &> >());

	// CMAES
	algorithm_wrapper<algorithm::cmaes>("cmaes","Covariance Matrix Adaptation Evolutionary Startegy")
		.def(init<optional<int, double, double, double, double, double, double, double, bool> >())
		.add_property("gen",&algorithm::cmaes::get_gen,&algorithm::cmaes::set_gen)
		.add_property("cc",&algorithm::cmaes::get_cc,&algorithm::cmaes::set_cc)
		.add_property("cs",&algorithm::cmaes::get_cs,&algorithm::cmaes::set_cs)
		.add_property("c1",&algorithm::cmaes::get_c1,&algorithm::cmaes::set_c1)
		.add_property("cmu",&algorithm::cmaes::get_cmu,&algorithm::cmaes::set_cmu)
		.add_property("sigma",&algorithm::cmaes::get_sigma,&algorithm::cmaes::set_sigma)
		.add_property("ftol",&algorithm::cmaes::get_ftol,&algorithm::cmaes::set_ftol)
		.add_property("xtol",&algorithm::cmaes::get_xtol,&algorithm::cmaes::set_xtol);


	// Monte-carlo.
	algorithm_wrapper<algorithm::monte_carlo>("monte_carlo","Monte-Carlo search.")
		.def(init<int>());

	// Artificial Bee Colony Optimization (ABC).
	algorithm_wrapper<algorithm::bee_colony>("bee_colony","Artificial Bee Colony optimization (ABC) algorithm.")
		.def(init<optional<int,int> >());

	// Ant Colony Optimization (ACO). [planned in the future ....]
	//algorithm_wrapper<algorithm::aco>("aco","Ant Colony Optimization (ACO) algorithm.")
	//	.def(init<int,optional<double> >());
	
	// Firefly (FA). [Does not work!!!!!! The agorithm sucks!!!]
	// algorithm_wrapper<algorithm::firefly>("firefly","Firefly optimization algorithm.")
	//	.def(init<int,optional<double, double, double> >());
	
	// Monotonic Basin Hopping.
	algorithm_wrapper<algorithm::mbh>("mbh","Monotonic Basin Hopping.")
		.def(init<optional<const algorithm::base &,int, double> >())
		.def(init<optional<const algorithm::base &,int, const std::vector<double> &> >())
		.add_property("algorithm",&algorithm::mbh::get_algorithm,&algorithm::mbh::set_algorithm);
	
	// Multistart.
	algorithm_wrapper<algorithm::ms>("ms","Multistart.")
		.def(init<const algorithm::base &, int>())
		.add_property("algorithm",&algorithm::ms::get_algorithm,&algorithm::ms::set_algorithm);
	
	// Particle Swarm Optimization (Steady state)
	algorithm_wrapper<algorithm::pso>("pso", "Particle Swarm Optimization (steady-state)")
		.def(init<optional<int,double, double, double, double, int, int, int> >());

	// Particle Swarm Optimization (generational)
	algorithm_wrapper<algorithm::pso_generational>("pso_gen", "Particle Swarm Optimization (generational)")
		.def(init<optional<int,double, double, double, double, int, int, int> >());
	
	// Simple Genetic Algorithm.
	algorithm_wrapper<algorithm::sga>("sga", "A simple genetic algorithm (generational)")
		.def(init<int, optional<const double &, const double &, int, algorithm::sga::mutation::type, double, algorithm::sga::selection::type, algorithm::sga::crossover::type> >());
	
	// (N+1)-EA - Simple Evolutionary Algorithm
	algorithm_wrapper<algorithm::sea>("sea", "(N+1)-EA - A Simple Evolutionary Algorithm")
		.def(init<optional<int> >());

	// NSGA II
	algorithm_wrapper<algorithm::nsga2>("nsga_II", "The NSGA-II algorithm")
		.def(init<optional<int, double, double, double, double> >());
	
	// PaDe
	algorithm_wrapper<algorithm::pade>("pade", "Parallel Decomposition")
		.def(init<optional<int, const algorithm::base &> >());

	// Differential evolution.
	algorithm_wrapper<algorithm::de>("de", "Differential evolution algorithm.\n")
		.def(init<optional<int,const double &, const double &, int, double, double> >())
		.add_property("cr",&algorithm::de::get_cr,&algorithm::de::set_cr)
		.add_property("f",&algorithm::de::get_f,&algorithm::de::set_f);

	// Differential evolution (jDE)
	algorithm_wrapper<algorithm::jde>("jde", "Self-Adaptive Differential Evolution Algorithm: jDE.\n")
		.def( init<optional<int, int, int, double, double, bool> >());

	// Differential evolution (mde_pbx)
	algorithm_wrapper<algorithm::mde_pbx>("mde_pbx", "Self-Adaptive Differential Evolution Algorithm: mde_pbx.\n")
		.def(init<optional<int, double, double, double, double> >());

	// Differential evolution (our own brew)
	algorithm_wrapper<algorithm::de_1220>("de_1220", "Differential Evolution Algorithm (our brew ...).\n")
		.def(init<optional<int, int, std::vector<int>, bool, double, double> >());
		
	// Simulated annealing, Corana's version.
	algorithm_wrapper<algorithm::sa_corana>("sa_corana","Simulated annealing, Corana's version with adaptive neighbourhood.")
		.def(init<optional<int, const double &, const double &, int,int,const double &> >());

	// GSL algorithms.
	#ifdef PAGMO_ENABLE_GSL

	// GSL's BFGS.
	algorithm_wrapper<algorithm::gsl_bfgs>("gsl_bfgs","GSL BFGS algorithm.")
		.def(init<optional<int, const double &, const double &, const double &, const double &> >());

	algorithm_wrapper<algorithm::gsl_bfgs2>("gsl_bfgs2","GSL BFGS2 algorithm.")
		.def(init<optional<int, const double &, const double &, const double &, const double &> >());
	
	// GSL's Fletcher-Reeves.
	algorithm_wrapper<algorithm::gsl_fr>("gsl_fr","GSL Fletcher-Reeves algorithm.")
		.def(init<optional<int, const double &, const double &, const double &, const double &> >());

	// GSL's Nelder-Mead.
	algorithm_wrapper<algorithm::gsl_nm>("gsl_nm","GSL Nelder-Mead simplex method.")
		.def(init<optional<int, const double &, const double &> >());

	// GSL's Nelder-Mead, version 2.
	algorithm_wrapper<algorithm::gsl_nm2>("gsl_nm2","GSL Nelder-Mead simplex method, version 2.")
		.def(init<optional<int, const double &, const double &> >());

	// GSL's Nelder-Mead, version 2 + random initial simplex.
	algorithm_wrapper<algorithm::gsl_nm2rand>("gsl_nm2rand","GSL Nelder-Mead simplex method, version 2 with randomised initial simplex.")
		.def(init<optional<int, const double &, const double &> >());

	// GSL's Polak-Ribiere.
	algorithm_wrapper<algorithm::gsl_pr>("gsl_pr","GSL Polak-Ribiere algorithm.")
		.def(init<optional<int, const double &, const double &, const double &, const double &> >());

	#endif

	// NLopt algorithms.
	#ifdef PAGMO_ENABLE_NLOPT

	// NLopt's COBYLA.
	algorithm_wrapper<algorithm::nlopt_cobyla>("nlopt_cobyla","NLopt's COBYLA algorithm.")
		.def(init<optional<int, const double &, const double &> >());

	// NLopt's BOBYQA.
	algorithm_wrapper<algorithm::nlopt_bobyqa>("nlopt_bobyqa","NLopt's BOBYQA algorithm.")
		.def(init<optional<int, const double &, const double &> >());

	// NLopt's Sbplx.
	algorithm_wrapper<algorithm::nlopt_sbplx>("nlopt_sbplx","NLopt's Sbplx algorithm.")
		.def(init<optional<int, const double &, const double &> >());

	// NLopt's SLSQP.
	algorithm_wrapper<algorithm::nlopt_slsqp>("nlopt_slsqp","NLopt's SLSQP algorithm.")
		.def(init<optional<int, const double &, const double &> >());

	// NLopt's MMA.
	algorithm_wrapper<algorithm::nlopt_mma>("nlopt_mma","NLopt's MMA algorithm.")
		.def(init<optional<int, const double &, const double &> >());

	// NLopt's Aumented Lagrangian.
	algorithm_wrapper<algorithm::nlopt_aug_lag>("nlopt_auglag","NLopt's Augmented agrangian algorithm.")
		.def(init<optional<int, int, const double &, const double &,int, const double &, const double &> >());

	// NLopt's Aumented Lagrangian (EQ)
	algorithm_wrapper<algorithm::nlopt_aug_lag_eq>("nlopt_auglag_eq","NLopt's Augmented agrangian algorithm (using penalties only for the equalities).")
		.def(init<optional<int, int, const double &, const double &,int, const double &, const double &> >());


	#endif

	// Snopt solver.
	#ifdef PAGMO_ENABLE_SNOPT
	algorithm_wrapper<algorithm::snopt>("snopt","Snopt solver.")
		.def(init<int, optional<double, double> >());
	
	#endif
		
	// Ipopt solver.
	#ifdef PAGMO_ENABLE_IPOPT
	algorithm_wrapper<algorithm::ipopt>("ipopt","Ipopt solver.")
		.def(init<int, optional<double, double,double,bool,double,double> >());
	
	#endif	

	// Register to_python conversion from smart pointer.
	register_ptr_to_python<algorithm::base_ptr>();
}
