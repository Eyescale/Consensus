#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "io.h"
#include "parser_macros.h"

//===========================================================================
//	newIO / freeIO
//===========================================================================
void
io_init( CNIO *io, void *stream, IOStreamType type )
{
	memset( io, 0, sizeof(CNIO) );
	io->stream = stream;
	io->type = type;
	io->line = 1;
}

void
io_exit( CNIO *io )
{
}

//===========================================================================
//	io_getc
//===========================================================================
static int preprocess( int event, int *mode, int *buffer, int *skipped );

int
io_getc( CNIO *io, int last_event )
/*
	filters out comments and \cr line continuation from input
*/
{
	int buffer = io->buffer;
	if ( last_event=='\n' ) {
		io->column = 0;
		io->line++; }
	int event, skipped[ 2 ] = { 0, 0 };
	do {
		if ( buffer ) {
			event=buffer; buffer=0; }
		else switch ( io->type ) {
		case IOStreamFile:
			event = fgetc( io->stream );
			break; }
		event = preprocess( event, io->mode, &buffer, skipped ); }
	while ( !event );
	if ( skipped[ 1 ] ) {
		io->line += skipped[ 1 ];
		io->column = skipped[ 0 ]; }
	else {
		io->column += skipped[ 0 ]; }
	if (!( event==EOF || event=='\n' )) {
		io->column++; }
	io->buffer = buffer;
	return event;
}
#define COMMENT		0
#define BACKSLASH	1
#define STRING		2
#define QUOTE		3
static int
preprocess( int event, int *mode, int *buffer, int *skipped )
{
	int output = 0;
	if ( event == EOF )
		output = event;
	else if ( mode[ COMMENT ] ) {
		switch ( event ) {
		case '/':
			if ( mode[ COMMENT ] == 1 ) {
				mode[ COMMENT ] = 2;
				skipped[ 0 ]++; }
			else if ( mode[ COMMENT ] == 4 )
				mode[ COMMENT ] = 0;
			skipped[ 0 ]++;
			break;
		case '\n':
			if ( mode[ COMMENT ] == 1 ) {
				mode[ COMMENT ] = 0;
				output = '/';
				*buffer = '\n'; }
			else if ( mode[ COMMENT ] == 2 ) {
				mode[ COMMENT ] = 0;
				output = '\n'; }
			else {
				if ( mode[ COMMENT ] == 4 )
					mode[ COMMENT ] = 3;
				skipped[ 0 ] = 0;
				skipped[ 1 ]++; }
			break;
		case '*':
			if ( mode[ COMMENT ] == 1 ) {
				mode[ COMMENT ] = 3;
				skipped[ 0 ]++; }
			else if ( mode[ COMMENT ] == 3 )
				mode[ COMMENT ] = 4;
			else if ( mode[ COMMENT ] == 4 )
				mode[ COMMENT ] = 3;
			skipped[ 0 ]++;
			break;
		default:
			if ( mode[ COMMENT ] == 1 ) {
				mode[ COMMENT ] = 0;
				output = '/';
				*buffer = event; }
			else {
				if ( mode[ COMMENT ] == 4 )
					mode[ COMMENT ] = 3;
				skipped[ 0 ]++; } } }
	else if ( mode[ BACKSLASH ] ) {
		if ( mode[ BACKSLASH ] == 4 ) {
			mode[ BACKSLASH ] = 0;
			output = event; }
		else switch ( event ) {
		case ' ':
		case '\t':
			if ( mode[ BACKSLASH ] == 1 ) {
				mode[ BACKSLASH ] = 2;
				skipped[ 0 ]++; }
			skipped[ 0 ]++;
			break;
		case '\\':
			if ( mode[ BACKSLASH ] == 1 ) {
				mode[ BACKSLASH ] = 4;
				output = '\\';
				*buffer = '\\'; }
			else mode[ BACKSLASH ] = 1;
			break;
		case '\n':
			if ( mode[ BACKSLASH ] == 3 ) {
				mode[ BACKSLASH ] = 0;
				output = '\n'; }
			else {
				mode[ BACKSLASH ] = 3;
				skipped[ 0 ] = 0;
				skipped[ 1 ]++; }
			break;
		default:
			if ( mode[ BACKSLASH ] == 1 ) {
				mode[ BACKSLASH ] = 4;
				output = '\\';	
				*buffer = event; }
			else {
				mode[ BACKSLASH ] = 0;
				*buffer = event; } } }
	else {
		switch ( event ) {
		case '\\':
			switch ( mode[ QUOTE ] ) {
			case 0:
				mode[ BACKSLASH ] = 1;
				break;
			case 1: // quote just turned on
				mode[ QUOTE ] = -4;
				output = event;
				break;
			case -4: // case '\\
				mode[ QUOTE ] = -1;
				output = event;
				break;
			default:
				mode[ QUOTE ] = 0;
				mode[ BACKSLASH ] = 1;
				output = event; }
			break;
		case '"':
			output = event;
			switch ( mode[ QUOTE ] ) {
			case 0:
				mode[ STRING ] = !mode[ STRING ];
				break;
			case 1: // quote just turned on
				mode[ QUOTE ] = -1;
				break;
			case -4: // allowing '\"'
				mode[ QUOTE ] = -1;
				break;
			default:
				mode[ QUOTE ] = 0;
				mode[ STRING ] = !mode[ STRING ];
				break; }
			break;
		case '\'':
			output = event;
			switch ( mode[ QUOTE ] ) {
			case 0:
				if ( mode[ STRING ] )
					break;
				mode[ QUOTE ] = 1;
				break;
			case 1: // allowing ''
				mode[ QUOTE ] = 0;
				break;
			case -4: // allowing '\''
				mode[ QUOTE ] = -1;
				break;
			default:
				mode[ QUOTE] = 0; }
			break;
		case '/':
			switch ( mode[ QUOTE ] ) {
			case 0:
				if ( !mode[ STRING ] ) {
					mode[ COMMENT ] = 1;
					break; }
				output = event;
				break;
			case 1: // quote just turned on
				mode[ QUOTE ] = -1;
				output = event;
				break;
			default: // mode[ STRING ] excluded
				mode[ QUOTE ] = 0;
				output = event; }
			break;
		default:
			output = event;
			if ( event == '\n' ) {
				mode[ QUOTE ] = 0;
				break; }
			switch ( mode[ QUOTE ] ) {
			case 0: break;
			case 1:
				mode[ QUOTE ] = -1;
				break;
			case -4:
				if (!( event == 'x' )) {
					mode[ QUOTE ] = -1;
					break; }
				// no break
			default:
				mode[ QUOTE ]++;
				break; } } }
	return output;
}

