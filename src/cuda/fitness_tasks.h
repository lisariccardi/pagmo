/*****************************************************************************
 *   Copyright (C) 2004-2009 The PaGMO development team,                     *
 *   Advanced Concepts Team (ACT), European Space Agency (ESA)               *
 *   http://apps.sourceforge.net/mediawiki/pagmo                             *
 *   http://apps.sourceforge.net/mediawiki/pagmo/index.php?title=Developers  *
 *   http://apps.sourceforge.net/mediawiki/pagmo/index.php?title=Credits     *
 *   act@esa.int                                                             *
 *                                                                           *
 *   This program is free software; you can redistribute it and/or modify    *
 *   it under the terms of the GNU General Public License as published by    *
 *   the Free Software Foundation; either version 2 of the License, or       *
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


#ifndef __CUDA_PAGMO_FITNESS_TASKS__
#define __CUDA_PAGMO_FITNESS_TASKS__

#include "cudatask.h"
#include "kernel.h"
#include "kernel_dims.h"


using namespace cuda;


namespace pagmo
{
    namespace fitness
    {
	template <typename ty, typename kernel_dims1 = block_complete_dimensions, typename pre_exec = nop_functor<ty> , typename post_exec = nop_functor<ty> >
	    class evaluate_fitness_task : public task <ty>
	{ 
	public:
	    enum fitness_type
	    {
		//Fitness types
		minimal_distance = 0,
		minimal_distance_speed_theta,
		minimal_distance_simple,
		no_attitude_fitness,
		cristos_twodee_fitness1,
		cristos_twodee_fitness2,
		cristos_twodee_fitness3
	    };

	evaluate_fitness_task(info & inf, const std::string & name, fitness_type type , size_t islands, size_t individuals, 
			      size_t taskCount, size_t inputs_, size_t outputs_, ty vicinity_distance, 
			      ty vicinity_speed, ty vic_orientation, ty max_docking_time ) : 
	task<ty>::task(inf, name, islands, individuals, taskCount, 1), m_fitness_type(type), //<TODO> not sure that the task size is 1
	m_inputs (inputs_/*6*/), m_outputs (outputs_/*3*/), m_fitness(4), 
	m_vicinity_distance(vicinity_distance), 
	m_vicinity_speed(vicinity_speed), 
	m_max_docking_time(max_docking_time),
	m_vic_orientation(vic_orientation), 
	m_tdt(0)
		{
		    
		    this->set_shared_chunk(0, 0 , (m_inputs + m_outputs) * sizeof(ty) );
		    this->set_global_chunk(0, 0 , (m_inputs + m_outputs + m_fitness) * sizeof(ty) );
		    this->m_dims = kernel_dimensions::ptr( new kernel_dims1 (&this->m_info, this->get_profile(), this->m_name));	    
		}

	    enum
	    {
		param_inputs = 0,
		param_outputs = 1, 
		param_init_distance = 2,
		param_fitness = 3
	    };

	    void set_time(ty t)
	    {
		m_tdt = t;
	    }


	    //<TODO> use point inputs instead
	    virtual bool set_initial_distance(const data_item & item, const ty & distance)
	    {
		std::vector<ty> d(1,distance);
		return this->set_initial_distance (item, d);
	    }

	    virtual bool set_initial_distance(const data_item & item, const std::vector<ty> & distance)
	    {
		if (distance.size() == 1)
		{
		    return task<ty>::set_inputs (item, param_init_distance, distance, 1);
		}
		return false;
	    }

	    virtual bool set_inputs(const data_item & item, const std::vector<ty> & inputs)
	    {
		if (inputs.size() == m_inputs)
		{
		    return task<ty>::set_inputs (item, param_inputs, inputs, m_inputs);
		}
		return false;
	    }

	    virtual bool set_outputs(const data_item & item, const std::vector<ty> & outputs)
	    {
		if (outputs.size() == m_outputs)
		{
		    return task<ty>::set_inputs (item, param_outputs, outputs, m_outputs);
		}
		return false;
	    }

	    virtual bool get_fitness( const data_item & item, std::vector<ty> & fitness)
	    {
		return task<ty>::get_outputs (item,  param_fitness, fitness);
	    }

	    virtual bool prepare_outputs()
	    {
		return task<ty>::prepare_dataset(data_item::point_mask, param_fitness, m_fitness);
	    }

	    virtual bool launch()
	    {

		typename dataset<ty>::ptr pState = this->get_dataset(param_inputs);
		typename dataset<ty>::ptr pOutData = this->get_dataset(param_outputs);
		typename dataset<ty>::ptr pFitness = this->get_dataset(param_fitness);
		typename dataset<ty>::ptr pInitDistance = this->get_dataset(param_init_distance);

		if (!(pState && pOutData && pFitness && pInitDistance))
		{

		    CUDA_LOG_ERR(this->m_name, " Could not find a dataset ", 0);
		    CUDA_LOG_ERR(this->m_name, " state " , pState);
		    CUDA_LOG_ERR(this->m_name, " outdata ",  pOutData);
		    CUDA_LOG_ERR(this->m_name, " fitness ",  pFitness);
		    CUDA_LOG_ERR(this->m_name, " initial distance ",  pInitDistance);
		    return false;
		}

		block_complete_dimensions dims(&this->m_info, this->get_profile(), this->m_name);

		cudaError_t err = cudaSuccess;
		switch (m_fitness_type)
		{
		case  minimal_distance:
		    err = cu_compute_fitness_mindis<ty, pre_exec, post_exec>(pState->get_data(),pOutData->get_data(), pFitness->get_data(), 
									     pInitDistance->get_data(), pState->get_task_size(), this->m_dims.get()); 
		    break;				
		case  minimal_distance_speed_theta:
		    cu_compute_fitness_mindis_theta<ty, pre_exec, post_exec>(pState->get_data(),pOutData->get_data(), pFitness->get_data(), 
									     pInitDistance->get_data(), pState->get_task_size(), this->m_dims.get());
		    break;
		case  minimal_distance_simple:
		    cu_compute_fitness_mindis_simple<ty, pre_exec, post_exec>(pState->get_data(),pOutData->get_data(),pFitness->get_data(),
									      pInitDistance->get_data(), pState->get_task_size(), this->m_dims.get());
		    break;
		case no_attitude_fitness:
		    cu_compute_fitness_mindis_noatt<ty, pre_exec, post_exec>(pState->get_data() ,pOutData->get_data(), pFitness->get_data(), 
									     pInitDistance->get_data(), m_vicinity_distance, m_vicinity_speed,  
									     m_max_docking_time, m_tdt, pState->get_task_size(), this->m_dims.get());
		    break;
		case cristos_twodee_fitness1:
		    cu_compute_fitness_twodee1<ty, pre_exec, post_exec>(pState->get_data(),pOutData->get_data(),pFitness->get_data(), pInitDistance->get_data(), 
									m_max_docking_time, m_tdt, pState->get_task_size(), this->m_dims.get());
		    break;
		case  cristos_twodee_fitness2:
		    cu_compute_fitness_twodee2<ty, pre_exec, post_exec>(pState->get_data(),pOutData->get_data(),pFitness->get_data(), pInitDistance->get_data(), 
									m_vicinity_distance,m_vicinity_speed, m_vic_orientation, m_max_docking_time, m_tdt, 
									pState->get_task_size(), this->m_dims.get());
		    break;
		case cristos_twodee_fitness3:
		    //TODO orientation?
		    cu_compute_fitness_twodee3<ty, pre_exec, post_exec>(pState->get_data(),pOutData->get_data(),pFitness->get_data(),
									pInitDistance->get_data(), m_vicinity_distance,
									m_vicinity_speed, m_vic_orientation, m_max_docking_time, m_tdt, 
									pState->get_task_size(), this->m_dims.get()); 
		    break;
		default:
		    return false;
		};

		if (err != cudaSuccess)
		{
		    CUDA_LOG_ERR(this->m_name, " launch fail ", err);
		    return false;
		}
		return true;
	    }
	size_t get_inputs() { return m_inputs;}
	size_t get_outputs() { return m_outputs;}
	size_t get_fitness_vector() { return m_fitness;}
	protected:
	
	const size_t m_fitness_type;
	const size_t  m_inputs;
	const size_t  m_outputs;
	const size_t  m_fitness;
	ty m_vicinity_distance;
	ty m_vicinity_speed;
	ty m_max_docking_time;
	ty m_vic_orientation;
	ty m_tdt;
	kernel_dimensions::ptr m_dims;
	
	};
    }
}


#endif