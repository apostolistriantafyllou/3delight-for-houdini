#pragma once

#include <ndspy.h>

PtDspyError MPlayDspyImageOpen(
	PtDspyImageHandle *i_phImage,
	const char * /*i_drivername*/,
	const char *i_filename,
	int i_width, int i_height,
	int i_paramCount,
	const UserParameter *i_parameters,
	int i_numFormats,
	PtDspyDevFormat i_formats[],
	PtFlagStuff * /*flagstuff*/ );

PtDspyError MPlayDspyImageQuery(
	PtDspyImageHandle i_hImage,
	PtDspyQueryType i_type,
	int i_datalen,
	void *i_data );

PtDspyError MPlayDspyImageData(
	PtDspyImageHandle i_hImage,
	int i_xmin, int i_xmax_plusone,
	int i_ymin, int i_ymax_plusone,
	int i_entrySize,
	const unsigned char* i_cdata );

PtDspyError MPlayDspyImageClose( PtDspyImageHandle i_hImage );
