#pragma once

#include "exporter.h"

/**
	\brief Exporter for a GT_PrimPointMesh.
*/
class pointmesh : public exporter
{
public:
	pointmesh( const context&, OBJ_Node *, const GT_PrimitiveHandle & );

	void create( void ) const override;
	void set_attributes( void ) const override;
	void set_attributes_at_time( double i_time ) const override;
};
