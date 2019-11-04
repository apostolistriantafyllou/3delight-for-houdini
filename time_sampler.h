#pragma once

#include "context.h"


/**
	\brief Iterator of time samples.

	Conveniently lists time samples to be used for a specific object, in
	increasing order, from the "shutter open" to the "shutter close" time.

	The number of time samples may vary depending on the rendering context and
	the object that is to be sampled over time.
*/
class time_sampler
{
public:

	enum blur_source { e_transformation, e_deformation };

	/**
		\brief Constructor.

		\param i_context
			Current rendering context. Used to determine whether motion blur is
			required, as well as the current time and shutter information.
		\param i_node
			The object which is to be sampled over time. The number of samples
			may vary from one object to the other. In particular, some objects
			are static and don't need motion blur at all.
		\param i_type
			Specifies whether we're iterating over deformation blur or
			transformation blur time samples.
	*/
	time_sampler(const context& i_context, OBJ_Node& i_node, blur_source i_type);

	/// Returns a null pointer if iteration is over
	operator void*()const
	{
		return m_current_sample <= m_nb_intervals ? (void*)1 : nullptr;
	}

	/// Returns the current time sample
	double operator*()const
	{
		if(m_nb_intervals == 0)
		{
			return m_first;
		}
		return m_first + (double(m_current_sample) / double(m_nb_intervals)) * m_length;
	}

	/// Move on to the next time sample.
	void operator++(int)
	{
		m_current_sample++;
	}

	/// Returns the number of time samples iterated on.
	unsigned nb_samples()const
	{
		return m_nb_intervals + 1;
	}

	/// Returns the index of the current time sample.
	unsigned sample_index()const
	{
		return m_current_sample;
	}

private:

	double m_first;
	double m_length;
	unsigned m_nb_intervals;
	unsigned m_current_sample;
};
