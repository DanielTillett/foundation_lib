/* log.c  -  Foundation library  -  Public Domain  -  2013 Mattias Jansson / Rampant Pixels
 * 
 * This library provides a cross-platform foundation library in C11 providing basic support data types and
 * functions to write applications and games in a platform-independent fashion. The latest source code is
 * always available at
 * 
 * https://github.com/rampantpixels/foundation_lib
 * 
 * This library is put in the public domain; you can redistribute it and/or modify it without any restrictions.
 *
 */

#include <foundation/foundation.h>
#include <foundation/internal.h>

#include <stdio.h>
#include <stdarg.h>

#if FOUNDATION_PLATFORM_WINDOWS
#include <foundation/windows.h>
#  define snprintf( p, s, ... ) _snprintf_s( p, s, _TRUNCATE, __VA_ARGS__ )
#  define vsnprintf( s, n, format, arg ) _vsnprintf_s( s, n, _TRUNCATE, format, arg )
__declspec(dllimport) void STDCALL OutputDebugStringA(LPCSTR);
#endif

#if FOUNDATION_PLATFORM_ANDROID
#  include <android/log.h>
#endif

#if FOUNDATION_PLATFORM_POSIX
#  include <foundation/posix.h>
#endif

static bool             _log_stdout           = true;
static bool             _log_prefix           = true;
static log_callback_fn  _log_callback         = 0;
static hashtable64_t*   _log_suppress         = 0;
static error_level_t    _log_suppress_default = ERRORLEVEL_NONE;

static char _log_warning_name[WARNING_LAST_BUILTIN][18] = {
	"performance",
	"deprecated",
	"bad data",
	"memory",
	"unsupported",
	"suspicious",
	"system call fail",
	"deadlock",
	"script"
};

static char _log_error_name[ERROR_LAST_BUILTIN][18] = {
	"none",
	"invalid value",
	"unsupported",
	"not implemented",
	"out of memory",
	"memory leak",
	"memory alignment",
	"internal failure",
	"access denied",
	"exception",
	"system call fail",
	"unknown type",
	"unknown resource",
	"deprecated",
	"assert",
	"script"
};

#define make_timestamp()  ((float32_t)( (real)( time_current() - time_startup() ) / (real)time_ticks_per_second() ))


#if BUILD_ENABLE_LOG || BUILD_ENABLE_DEBUG_LOG

#if FOUNDATION_PLATFORM_WINDOWS
#  define LOG_USE_VACOPY 0
#else
#  define LOG_USE_VACOPY 1
#endif

static void _log_outputf( uint64_t context, int severity, const char* prefix, const char* format, va_list list, void* std )
{
	float32_t timestamp = make_timestamp();
	uint64_t tid = thread_id();
	unsigned int pid = thread_hardware();
	int need, more, remain, size = 383;
	char local_buffer[385];
	char* buffer = local_buffer;
	while(1)
	{
		//This is guaranteed to always fit in minimum size of 383 bytes defined above, so need is always > 0
		if( _log_prefix )
			need = snprintf( buffer, size, "[%.3f] <%" PRIx64 ":%d> %s", timestamp, tid, pid, prefix );
		else
			need = snprintf( buffer, size, "%s", prefix );

		remain = size - need;
#if LOG_USE_VACOPY
		{
			va_list clist;
			va_copy( clist, list );
			more = vsnprintf( buffer + need, remain, format, clist );
			va_end( clist );
		}
#else
		more = vsnprintf( buffer + need, remain, format, list );
#endif
			
		if( ( more > -1 ) && ( more < remain ) )
		{
			buffer[need+more] = '\n';
			buffer[need+more+1] = 0;

#if FOUNDATION_PLATFORM_WINDOWS
			OutputDebugStringA( buffer );
#endif

#if FOUNDATION_PLATFORM_ANDROID
			if( _log_stdout )
				__android_log_write( ANDROID_LOG_DEBUG + severity, environment_application()->short_name, buffer );
#else
			if( _log_stdout && std )
				fprintf( std, "%s", buffer );
#endif

			if( _log_callback )
				_log_callback( context, severity, buffer );

			break;
		}

		if( ( more > -1 ) && ( need > -1 ) )
			size = more + need + 1;
		else
			size *= 2;

		if( buffer != local_buffer )
			memory_deallocate( buffer );
		buffer = memory_allocate( size + 2, 0, MEMORY_TEMPORARY );
	}
	if( buffer != local_buffer )
		memory_deallocate( buffer );
}

#endif


#if BUILD_ENABLE_DEBUG_LOG


void log_debugf( uint64_t context, const char* format, ... )
{
	va_list list;
	va_start( list, format );
	if( log_suppress( context ) < ERRORLEVEL_DEBUG )
		_log_outputf( context, ERRORLEVEL_DEBUG, "", format, list, stdout );
	va_end( list );
}


#endif


#if BUILD_ENABLE_LOG


void log_infof( uint64_t context, const char* format, ... )
{
	va_list list;
	va_start( list, format );
	if( log_suppress( context ) < ERRORLEVEL_INFO )
		_log_outputf( context, ERRORLEVEL_INFO, "", format, list, stdout );
	va_end( list );
}


void log_warnf( uint64_t context, warning_t warn, const char* format, ... )
{
	char prefix[32];
	va_list list;

	if( log_suppress( context ) >= ERRORLEVEL_WARNING )
		return;

	log_error_context( context, ERRORLEVEL_WARNING );

	if( warn < WARNING_LAST_BUILTIN )
		string_format_buffer( prefix, 32, "WARNING [%s]: ", _log_warning_name[warn] );
	else
		string_format_buffer( prefix, 32, "WARNING [%d]: ", warn );
	
	va_start( list, format );
	_log_outputf( context, ERRORLEVEL_WARNING, prefix, format, list, stdout );
	va_end( list );
}


void log_errorf( uint64_t context, error_t err, const char* format, ... )
{
	char prefix[32];
	va_list list;

	if( log_suppress( context ) >= ERRORLEVEL_ERROR )
		return;

	log_error_context( context, ERRORLEVEL_ERROR );

	if( err < ERROR_LAST_BUILTIN )
		string_format_buffer( prefix, 32, "ERROR [%s]: ", _log_error_name[err] );
	else
		string_format_buffer( prefix, 32, "ERROR [%d]: ", err );
	
	va_start( list, format );
	_log_outputf( context, ERRORLEVEL_ERROR, prefix, format, list, stderr );
	va_end( list );

	error_report( ERRORLEVEL_ERROR, err );
}


void log_panicf( uint64_t context, error_t err, const char* format, ... )
{
	char prefix[32];
	va_list list;

	log_error_context( context, ERRORLEVEL_PANIC );

	if( err < ERROR_LAST_BUILTIN )
		string_format_buffer( prefix, 32, "PANIC [%s]: ", _log_error_name[err] );
	else
		string_format_buffer( prefix, 32, "PANIC [%d]: ", err );
	
	va_start( list, format );
	_log_outputf( context, ERRORLEVEL_PANIC, prefix, format, list, stderr );
	va_end( list );

	error_report( ERRORLEVEL_PANIC, err );
}


static void _log_error_contextf( uint64_t context, error_level_t error_level, void* std, const char* format, ... )
{
	va_list list;
	va_start( list, format );
	_log_outputf( context, error_level, "", format, list, std );
	va_end( list );
}


void log_error_context( uint64_t context, error_level_t error_level )
{
	int i;
	error_context_t* err_context = error_context();
	if( err_context && ( log_suppress( context ) < error_level ) )
	{
		error_frame_t* frame = err_context->frame;
		for( i = 0; i < err_context->depth; ++i, ++frame )
			_log_error_contextf( context, error_level, error_level > ERRORLEVEL_WARNING ? stderr : stdout, "When %s: %s", frame->name ? frame->name : "<something>", frame->data ? frame->data : "" );
	}
}

#endif


#if BUILD_ENABLE_LOG

void log_enable_stdout( bool enable )
{
	_log_stdout = enable;
}


void log_set_callback( log_callback_fn callback )
{
	_log_callback = callback;
}


void log_enable_prefix( bool enable )
{
	_log_prefix = enable;
}


void log_set_suppress( uint64_t context, error_level_t level )
{
	if( !context )
		_log_suppress_default = level;
	else if( _log_suppress )
	{
		if( level > ERRORLEVEL_NONE )
			hashtable64_set( _log_suppress, context, (uint64_t)level );
		else
			hashtable64_erase( _log_suppress, context );
	}
}


error_level_t log_suppress( uint64_t context )
{
	if( !context )
		return _log_suppress_default;
	else if( _log_suppress )
		return (error_level_t)hashtable64_get( _log_suppress, context ); //Defaults to 0 - ERRORLEVEL_NONE
	return ERRORLEVEL_NONE;

}


#endif


int _log_initialize( void )
{
#if BUILD_ENABLE_LOG || BUILD_ENABLE_DEBUG_LOG
	_log_suppress = hashtable64_allocate( 149 );
#endif
	return 0;
}


void _log_shutdown( void )
{
#if BUILD_ENABLE_LOG || BUILD_ENABLE_DEBUG_LOG
	hashtable64_deallocate( _log_suppress );
	_log_suppress = 0;
#endif
}

