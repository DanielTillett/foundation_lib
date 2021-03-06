/* main.c  -  Foundation crash test  -  Public Domain  -  2013 Mattias Jansson / Rampant Pixels
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
#include <test/test.h>


static bool _crash_callback_called = false;


application_t test_crash_application( void )
{
	application_t app = {0};
	app.name = "Foundation crash tests";
	app.short_name = "test_crash";
	app.config_dir = "test_crash";
	app.flags = APPLICATION_UTILITY;
	return app;
}


memory_system_t test_crash_memory_system( void )
{
	return memory_system_malloc();
}


int test_crash_initialize( void )
{
	return 0;
}


void test_crash_shutdown( void )
{
}


void test_crash_callback( const char* dump_path )
{
	log_infof( HASH_TEST, "Crash callback called: %s", dump_path );
	_crash_callback_called = true;
}


int instant_crash( void* arg )
{
	log_info( HASH_TEST, "Causing illegal memory write" );
	*(volatile int*)3 = 0;
	return 1;
}


void* thread_crash( object_t thread, void* arg )
{
	return (void*)(uintptr_t)instant_crash( arg );
}


DECLARE_TEST( crash, crash_guard )
{
	int crash_result;
	
	_crash_callback_called = false;
	crash_result = crash_guard( instant_crash, 0, test_crash_callback, "instant_crash" );
	EXPECT_EQ( crash_result, CRASH_DUMP_GENERATED );
	EXPECT_TRUE( _crash_callback_called );

	return 0;
}


DECLARE_TEST( crash, crash_thread )
{
	object_t thread = 0;
	
	_crash_callback_called = false;
	crash_guard_set( test_crash_callback, "thread_crash" );
	
	thread = thread_create( thread_crash, "crash", THREAD_PRIORITY_NORMAL, 0 );
	thread_start( thread, 0 );
	thread_sleep( 100 );
	thread_terminate( thread );
	thread_destroy( thread );
	while( thread_is_thread( thread ) )
		thread_yield();
	thread_sleep( 100 );
	
	EXPECT_TRUE( _crash_callback_called );

	return 0;
}


void test_crash_declare( void )
{
	ADD_TEST( crash, crash_guard );
	ADD_TEST( crash, crash_thread );
}


test_suite_t test_crash_suite = {
	test_crash_application,
	test_crash_memory_system,
	test_crash_declare,
	test_crash_initialize,
	test_crash_shutdown
};


#if FOUNDATION_PLATFORM_ANDROID

int test_crash_run( void )
{
	test_suite = test_crash_suite;
	return test_run_all();
}

#else

test_suite_t test_suite_define( void )
{
	return test_crash_suite;
}

#endif
