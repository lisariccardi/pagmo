#ifndef __PAGMO_CUDA_INTEGRATOR__
#define __PAGMO_CUDA_INTEGRATOR__

#include "../cuda/cudainfo.h"
#include "../cuda/cudatask.h"
#include "../cuda/kernel.h"


namespace pagmo
{
    namespace odeint
    {

	using namespace cuda;
	template <typename ty, typename system, size_t in_, size_t out_, size_t system_params>
	    class integrator : public task<ty>
	{
	public:
	    integrator (cuda::info & inf, const std::string & name, size_t islands, size_t individuals, size_t task_count_) : 
	    cuda::task<ty>(inf, name, islands, individuals, task_count_, 1), 
		m_param_t(0),  m_param_dt(0), m_param_scale_limits(0)
	    {
	    }
	    virtual ~integrator ()
	    {
	  
	    }

	    enum
	    {
		param_x = 0,
		param_o
	    };

	    virtual bool set_inputs(const data_item & item, const std::vector<ty> & inputs)
	    {
		if (inputs.size() == get_size())
		{
		    return task<ty>::set_inputs (item, this->param_x, inputs, get_size(), "inputs");
		}
		return false;
	    }

	    virtual bool set_dynamical_inputs(const data_item & item, const std::vector<ty> & inputs)
	    {
		if (inputs.size() == system_params)
		{
		    return task<ty>::set_inputs (item, this->param_o, inputs, system_params, "dynamical inputs");
		}
		return false;
	    }

	    virtual bool get_outputs( const data_item & item, std::vector<ty> & outputs)
	    {
		return task<ty>::get_outputs (item, this->param_x, outputs);
	    }
     
	    virtual bool prepare_outputs()
	    {
		return true;
	    }
	    unsigned int get_size()
	    {
		return in_;
	    }

	    unsigned int get_system_size()
	    {
		return system_params;
	    }

	    void set_params(ty t, ty dt, ty scale_limits)
	    {
		m_param_t = t;
		m_param_dt = dt;
		m_param_scale_limits = scale_limits;
	    }

	    virtual bool launch() = 0;
	protected:

	    ty m_param_t;
	    ty m_param_dt;
	    ty m_param_scale_limits;	
	};
    }
}


#endif