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

// Created by Juxi Leitner on 2010-02-11

#include <exception>
#include <string>
#include <vector>
#include <iostream>
#include <sstream>

#include "base.h"
#include "docking.h"

#include "../../exceptions.h"
#include "../basic/population.h"
#include "../../ann_toolbox/neural_network.h"
#include "../../ann_toolbox/ctrnn.h"
#include "../../odeint/odeint.hpp"


extern std::string max_log_string;
extern double max_log_fitness;

namespace pagmo {
namespace problem {	
	
// Constructors
docking::docking(ann_toolbox::neural_network* ann_, size_t random_positions, double max_time, double max_thr) :
	base(ann_->get_number_of_weights()),
	max_thrust(max_thr),
	max_docking_time(max_time),	
	time_neuron_threshold(.99),
	ann(ann_)
{						
	// the docking problem needs:
	// 	3 inputs (and starting conditions): x, z, theta
	//  2 outputs (control): ul, ur		(the two thrusters, [0,1] needs to be mapped to [-1,1])
	// and final conditions: x, z,	// later maybe theta, v
		
	// Set the boundaries for the values in the genome, this is important for the multilayer_perceptron!!
//	set_ub(	std::vector<double> (ann->get_number_of_weights(),  10.0) );
//	set_lb(	std::vector<double> (ann->get_number_of_weights(), -10.0) );
	
	// take the best result during the whole integration steps not the one at the end!!
	take_best = true;
	
	// disable genome logging
	log_genome = false;
	
	random_starting_postions = random_positions;
}

void docking::set_start_condition(size_t number) {
	if(number < random_start.size())
		starting_condition = random_start[number];
	else
		pagmo_throw(value_error, "wrong index for random start position");
}

void docking::set_start_condition(double *start_cnd, size_t size) {
	starting_condition = std::vector<double> (start_cnd, start_cnd + size);
}

void docking::set_start_condition(std::vector<double> &start_cond) {
	starting_condition = start_cond;
}

void docking::set_log_genome(bool b) {
	log_genome = b;
}
void docking::set_take_best(bool b) {
	take_best = b;
}

void docking::pre_evolution(population &pop) const {
	// Change the starting positions to random numbers (given by random_starting_positions number)
	random_start.clear();
	rng_double drng = rng_double(static_rng_uint32()());

	double r, a, theta, x, y;	
	for(int i = 0; i < random_starting_postions; i++) {
		r = 1.5 + 0.5 * drng();	// radius between 1.5 and 2
		a = drng() * 2 * M_PI;	// alpha between 0-2Pi
	
		x = r * cos(a);
		y = r * sin(a);
		theta = drng() * 2 * M_PI - M_PI;	// theta between -Pi and Pi

		// Start Condt:  x,  vx, y,  vy, theta, omega
		double cnd[] = { x, 0.0, y, 0.0, theta, 0.0 };
		random_start.push_back(std::vector<double> (cnd, cnd + 6));
	}
	
/*
	printf("\t%.3f\t%.3f\t%.3f\t%.3f\t%.3f\t%.3f\t%.3f\n", 
		random_start[i][0], random_start[i][1], random_start[i][2], random_start[i][3], random_start[i][4], random_start[i][5]);		
*/
		
/* IS THAT NEEDED?
	//Re-evaluate the population with respect to the new seed (Internal Sampling Method)
	for (size_t i=0; i < pop.size(); ++i) {
		pop[i] = individual(*this, pop[i].get_decision_vector(), pop[i].get_velocity());
	}*/
}

// Objective function to be minimized
double docking::objfun_(const std::vector<double> &v) const {
	if(v.size() != ann->get_number_of_weights()) {
		pagmo_throw(value_error, "wrong number of weights in the chromosome");
	}
	
	double average = 0.0, fitness;
	std::string log = "", runlog = "";
	char h[999];
		
	///////////////////////////////////
	// LOGGING
	if(log_genome) {
		std::stringstream oss (std::stringstream::out);
		oss << *(ann_toolbox::ctrnn*)ann << std::endl;
		sprintf(h, "%s\tGenome:%s", h, oss.str().c_str());
	}		
	std::stringstream ss (std::stringstream::out);
	ss << *(ann_toolbox::ctrnn*)ann << std::endl;
	log = ss.str();	
	log += "\tx\tvx\ty\tvy\ttheta\tomega\tul\tur\tt-neuron\n";
	////////////////////////////////
	
	int i;
	for(i = 0;i < random_start.size();i++) {
		// Initialize ANN and interpret the chromosome
		ann->set_weights(v);
		
		// change starting position
		starting_condition = random_start[i];

// LOG START Conditions to file		
//		sprintf(h, "#%2di:\t%f,%f,%f\t%f,%f,%f\n", i, starting_condition[0], starting_condition[2], starting_condition[4], starting_condition[1], starting_condition[3], starting_condition[5]);
//		log += h;
		
		fitness = one_run(runlog);
//		printf("Fitness:%.3f\t%.3f\t%.3f\t%.3f\t%.3f\t%.3f\t%.3f\t%.3f\n", fitness,
//			random_start[i][0], random_start[i][1], random_start[i][2], random_start[i][3], random_start[i][4], random_start[i][5]);		
		
		average += fitness;
//		if(log.size() > 0) log = log + "\n\n";
		log += runlog;
	}
	average = average / i;
	
	//////////////////////////////////////
	// Add the best fitness to the logger
 	if(max_log_fitness > average) {
		sprintf(h, "docking::objfun_: return value:  %f\tdist\n", average); //:%f theta: %f speed: %f\n, distance, theta, speed);
		log = log + h;

		max_log_fitness = average;
		max_log_string = log;
	}
	/////////////////////////////////////
	
	return average;
}

double docking::one_run(std::string &log) const {
	// Helper variables
	double retval = 0.0, distance, theta, speed, best_retval = 0.0;
	int counter_at_goal = 0;
	
	// Integrator System
	DynamicSystem sys(this);
	// Create Integration Stepper
	odeint::ode_step_runge_kutta_4< std::vector<double>, double > stepper;

	// initialize the inputs (= starting conditions) to the ANN and allocate the outputs 
	std::vector<double> inputs = starting_condition, out;
	
	///////////////////////////////////////////////////////
	// LOGGER
	// Logging for the best individual
//	log = "\tx\tvx\ty\tvy\ttheta\tomega\tul\tur\tt-neuron\n";
	log = "";
	char h[999];
	////////////////////////////////////////////////////////

	// run evaluation of the ANN
	double  dt = .1, t, initial_distance = sqrt(inputs[0] * inputs[0] + inputs[2] * inputs[2]);
	for(t = 0;t < max_docking_time;t += dt) {
		// Perform the integration step
		stepper.next_step( sys, inputs, t, dt );
		out = sys.get_last_outputs();
		
		if( out[2] > time_neuron_threshold 	// if the time neuron tells us the network is finished
		   || t == max_docking_time ) { 	// or the maximum time is reached
			// evaluate the output of the network
			
			// distance to the final position (0,0) = sqrt(x^2 + z^2)
			distance = sqrt(inputs[0] * inputs[0] + inputs[2] * inputs[2]) ;
			speed = sqrt(inputs[1]*inputs[1] + inputs[3]*inputs[3]);		// sqrt(vx^2 + vy^2)
			theta = inputs[4];
			
			// keep theta between -180° and +180°
			if(theta > M_PI) theta -= 2 * M_PI;
			if(theta < -1*M_PI) theta += 2 * M_PI;
			inputs[4] = theta;

			// Calculate return value
			retval = 1.0/((1 + distance) * (1 + fabs(theta)) * (1 + speed));
			
			if(distance < initial_distance) {
				if(distance < 0.1 && fabs(theta) < M_PI/8 && speed < 0.1)
					retval += retval * (max_docking_time - t + dt)/max_docking_time;
			}
			else retval = 0.0;
			
			
			////////////////////////////////
			// LOGGING
			// Log the result (for later output & plotting)
			//printf("%.2f:\t%.3f\t%.3f\t%.4f\t%.2f\t%.2f\t%.2f\t%.3f\t%.3f\n", t, state[0], state[1], state[2], state[3], state[4], state[5], state[6], state[7]);
			sprintf(h, "%.2f:\t%.3f\t%.3f\t%.4f\t%.2f\t%.2f\t%.2f\t%.3f\t%.3f\t%.3f\tCalc: %f\t%f\t%f\t%f", 
			 			t+dt, inputs[0], inputs[1], inputs[2], inputs[3], inputs[4], inputs[5], out[0], out[1], out[2],
						retval, distance, theta, speed
			);	
			log = log + h + "\n";
			////////////////////////////////
			
			break;
			
		}
				
		/*
		// m_pi/6 = 30°
		if(distance < .1 && fabs(theta) < M_PI/6)
			counter_at_goal++;
		else counter_at_goal = 0;
		
		if(counter_at_goal >= needed_count_at_goal) retval = 1.0 + 2/t;		// we reached the goal!!! 2/t to give it a bit more range*/

		//if(take_best)
		//	if(retval > best_retval) best_retval = retval;
		
		// if(retval > 1.0) break;
	}
	
	// if take_best is FALSE we do not take the best overall but the result
	// at the end of the iteration (max_docking_time)
	// if(!take_best) best_retval = retval;
	
	
	// PaGMO minimizes the objective function!! therefore the minus here
	return -retval;
}

std::vector<double> docking::scale_outputs(const std::vector<double> outputs) const {
	std::vector<double> out(outputs.size());
	for(size_t i = 0; i < outputs.size(); i++)  {
	 	out[i] = (outputs[i] - 0.5) * 2;		// to have the thrust from 0 and 1 to -1 to 1
	 	out[i] = outputs[i] * max_thrust;		// scale it
	}
	return out;
}


// The dynamic system including the hill's equations
// state_type = x, vx, y, vy, theta, omega
void DynamicSystem::operator()( state_type &state , state_type &dxdt , double t ) {
	if(state.size() != 6) {
		pagmo_throw(value_error, "wrong number of parameters for the integrator");
	}
	
	// constants:
	const double nu = .08, mR = (1.5*.5);	// constant mass * radius of the s/c	
	
	double x = state[0];
	double vx = state[1];
	// not used: double y = state[2];
	double vy = state[3];
	double theta = state[4];
	double omega = state[5];
	
	// Send to the ANN to compute the outputs
	outputs = prob->ann->compute_outputs(state);
	
	// scale only the first two (meaning the thruster!)
	std::vector<double> temp = prob->scale_outputs(outputs);
	outputs[0] = temp[0];
	outputs[1] = temp[1];
	
	double ul = outputs[0];
	double ur = outputs[1];	// maybe save them somewhere?
	
/*	if(t >= prob->breakdown_time && t <= prob->breadkdown_time + prob->breakdown_duration)
		ul = 0.0;*/
	
	dxdt[0] = vx;
	dxdt[1] = 2 * nu * vy + 3 * nu * nu * x + (ul + ur) * cos(theta);
	dxdt[2] = vy;
	dxdt[3] = -2 * nu * vx + (ul + ur) * sin(theta);
	dxdt[4] = omega;
	dxdt[5] = (ul - ur) * 1/mR;
}

std::vector<double> DynamicSystem::get_last_outputs() { return outputs; }

}
}
