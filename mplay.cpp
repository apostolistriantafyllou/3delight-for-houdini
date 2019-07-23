#include <TIL/TIL_TileMPlay.h>
#include <IMG/IMG_TileOptions.h>

#include "mplay.h"

#include <string.h>
#include <errno.h>

PtDspyError MPlayDspyImageOpen(
	PtDspyImageHandle *i_phImage,
	const char * /*i_drivername*/,
	const char *i_filename,
	int i_width, int i_height,
	int i_paramCount,
	const UserParameter *i_parameters,
	int i_numFormats,
	PtDspyDevFormat i_formats[],
	PtFlagStuff * /*flagstuff*/ )
{
	if( !i_phImage )
	{
		return PkDspyErrorBadParams;
	}

#if 0
	int *original_size = (int *)
		GetParameter( "OriginalSize", i_paramCount, i_parameters );
	int *origin = (int *)
		GetParameter( "origin", i_paramCount, i_parameters );
	int *passes = (int *)
		GetParameter( "passes", i_paramCount, i_parameters );
#endif

	IMG_ColorModel color_model;

	if( i_numFormats == 1 )
	{
		color_model = IMG_1CHAN;
	}
	else if( i_numFormats == 2 )
	{
		color_model = IMG_2CHAN;
	}
	else if( i_numFormats == 3 )
	{
		color_model = IMG_RGB;
	}
	else if( i_numFormats == 4 )
	{
		color_model = IMG_RGBA;
	}
	else
	{
		fprintf(
			stderr,
			"3Delight for Houdini: mplay driver only support 1, 2, 3 and " \
				"4 channel images, not %d\n", i_numFormats );
		return PkDspyErrorBadParams;
	}

	/* Just in case, make all formats of the same type */
	for( int i =1; i<i_numFormats; i++ )
		i_formats[i].type = i_formats[0].type;

	IMG_DataType data_type;
	switch( i_formats[0].type & PkDspyMaskType )
	{
		case PkDspyFloat16: data_type = IMG_HALF; break;
		case PkDspyFloat32: data_type = IMG_FLOAT32; break;
		case PkDspyUnsigned8: data_type = IMG_UCHAR; break;
		case PkDspyUnsigned16: data_type = IMG_USHORT; break;
		case PkDspyUnsigned32: data_type = IMG_UINT; break;
		case PkDspySigned8: data_type = IMG_INT8; break;
		case PkDspySigned16: data_type = IMG_INT16; break;
		case PkDspySigned32: data_type = IMG_INT32; break;
		default:
			fprintf( stderr, "3Delight for Houdini: unknown type %d\n",
				i_formats[0].type & PkDspyMaskType );

			return PkDspyErrorBadParams;
	};

	TIL_TileMPlay *mplay = new TIL_TileMPlay(1, true);
	*i_phImage = (PtDspyImageHandle)mplay;

	IMG_TileOptions options;
	for( int i=0; i<i_numFormats; i++ )
	{
		options.setPlaneInfo(
			i_filename, i_formats[i].name, 0, data_type, color_model );
	}

	mplay->open( options, i_width, i_height, 16, 16, 1.0 );

	return PkDspyErrorNone;
}

/* Query */
PtDspyError MPlayDspyImageQuery(
	PtDspyImageHandle i_hImage,
	PtDspyQueryType i_type,
	int i_datalen,
	void *i_data )
{
	if( !i_data && i_type != PkStopQuery )
		return PkDspyErrorBadParams;

	auto mplay = (TIL_TileMPlay*)i_hImage;

	switch(i_type)
	{
	case PkSizeQuery:
	{
		/* old-school, useless here */
		PtDspySizeInfo sizeQ;
		sizeQ.width = 512;
		sizeQ.height = 512;
		sizeQ.aspectRatio = 1;
		memcpy( i_data, &sizeQ,
		    i_datalen>int(sizeof(sizeQ)) ? sizeof(sizeQ) : i_datalen );
		break;
	}

	case PkProgressiveQuery:
	{
		if( i_datalen < int(sizeof(PtDspyProgressiveInfo)) )
			return PkDspyErrorBadParams;
		((PtDspyProgressiveInfo*)i_data)->acceptProgressive = 1;
		break;
	}

	case PkCookedQuery:
	{
		PtDspyCookedInfo cookedQ;
		cookedQ.cooked = 1;
		memcpy( i_data, &cookedQ,
		    i_datalen>int(sizeof(cookedQ)) ? sizeof(cookedQ) : i_datalen );
		break;
	}

	case PkStopQuery:
	{
		if( mplay && mplay->checkInterrupt() )
		{
			/* FIXME: returns true all the time! */
			//return PkDspyErrorStop;
		}
		break;
	}
	default :
		return PkDspyErrorUnsupported;
	}

	return PkDspyErrorNone;
}

/*
	NOTES
	- We assume that the caller correctly set the quantize to
	  "0 255 0 255"
	- We have to reverse the image in Y since MPlay does it the
	  other way around
*/
PtDspyError MPlayDspyImageData(
	PtDspyImageHandle i_hImage,
	int i_xmin, int i_xmax_plusone,
	int i_ymin, int i_ymax_plusone,
	int i_entrySize,
	const unsigned char* i_cdata )
{
	auto mplay = (TIL_TileMPlay*)i_hImage;
	int bucket_w = i_xmax_plusone - i_xmin;
	int bucket_h = i_ymax_plusone - i_ymin;

	mplay->writeTile(
		i_cdata,
		i_xmin, i_xmax_plusone-1,
		i_ymin, i_ymax_plusone-1 );

	return PkDspyErrorNone;
}

PtDspyError MPlayDspyImageClose( PtDspyImageHandle i_hImage )
{
	auto mplay = (TIL_TileMPlay*)i_hImage;
	mplay->close( true /* keep alive */ );
	delete mplay;
	return PkDspyErrorNone;
}
