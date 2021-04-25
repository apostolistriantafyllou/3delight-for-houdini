#pragma once

#include <string>

class settings;
class OP_BundlePattern;
class OBJ_Node;

/**
	\brief Encapsulates logic needed to decide if an object/light is
	visible or not.
*/
struct object_visibility_resolver
{
	object_visibility_resolver(
		const std::string &i_rop, const settings &, double i_time );
	~object_visibility_resolver( );

	bool object_displayed( const OBJ_Node& ) const;
	bool object_is_matte( const OBJ_Node& ) const;
	bool object_is_phantom(const OBJ_Node&) const;

	OP_BundlePattern* m_objects_to_render_pattern{nullptr};
	OP_BundlePattern* m_lights_to_render_pattern{nullptr};
	OP_BundlePattern* m_mattes_pattern{nullptr};
	OP_BundlePattern* m_phantom_pattern{ nullptr };

	std::string m_rop_path;

	double m_current_time;
};
