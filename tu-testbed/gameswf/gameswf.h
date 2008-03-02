// gameswf.h	-- Thatcher Ulrich <tu@tulrich.com> 2003

// This source code has been donated to the Public Domain.  Do
// whatever you want with it.

// SWF (Shockwave Flash) player library.  The info for this came from
// http://www.openswf.org, the flashsource project, and swfparse.cpp


#ifndef GAMESWF_H
#define GAMESWF_H


#include <ctype.h>	// for poxy wchar_t
//#include <stdarg.h>	// for va_list arg to movie_interface::call_method_args()
#include <assert.h>
#include "base/image.h"	// for delete m_suspended_image
#include "base/container.h"	// for hash<...>

class tu_file;
class render_handler;
class weak_proxy;	// forward decl; defined in base/smart_ptr.h

// @@ forward decl to avoid including base/image.h; TODO change the
// render_handler interface to not depend on these structs at all.
namespace image { struct rgb; struct rgba; struct image_base; }

// forward decl
namespace jpeg { struct input; }
class tu_string;
class tu_stringi;

// Forward decl for D3D render handlers, in case they are
// instantiated.  Harmless on non-D3D platforms.
struct IDirect3DDevice9;
struct IDirect3DDevice8;

namespace gameswf
{
	// Forward declarations.
	struct action_buffer;
	struct as_value;
	struct bitmap_info;
	struct character;
	struct execute_tag;
	struct font;
	struct movie_interface;
	struct render_handler;
	struct resource;
	struct rgba;
	struct sound_handler;
	struct stream;
	struct video_handler;
	struct event_id;
	struct movie_root;
	struct movie_def_impl;
	struct rect;

	exported_module movie_interface* get_current_root();

	//
	// Log & error reporting control.
	//

	// Supply a function pointer to receive log & error messages.
	exported_module void	register_log_callback(void (*callback)(bool error, const char* message));

	// Control verbosity of specific categories.
	exported_module bool get_verbose_parse();
	exported_module bool get_verbose_debug();
	exported_module bool get_verbose_action();
	exported_module void	set_verbose_action(bool verbose);
	exported_module void	set_verbose_parse(bool verbose);

	// Get and set the render handler.  This is one of the first
	// things you should do to initialise the player (assuming you
	// want to display anything).
	exported_module void	set_render_handler(render_handler* s);

	// Pass in a sound handler, so you can handle audio on behalf of
	// gameswf.  This is optional; if you don't set a handler, or set
	// NULL, then sounds won't be played.
	//
	// If you want sound support, you should set this at startup,
	// before loading or playing any movies!
	exported_module void	set_sound_handler(sound_handler* s);

	// You probably don't need this. (@@ make it private?)
	exported_module sound_handler*	get_sound_handler();

	// Register a callback to the host, for providing a file,
	// given a "URL" (i.e. a path name).  This is the only means
	// by which the gameswf library accesses file data, for
	// loading movies, cache files, and so on.
	//
	// gameswf will call this when it needs to open a file.
	//
	// NOTE: the returned tu_file* will be delete'd by gameswf
	// when it is done using it.  Your file_opener_function may
	// return NULL in case the requested file can't be opened.
	typedef tu_file* (*file_opener_callback)(const char* url_or_path);
	exported_module void	register_file_opener_callback(file_opener_callback opener);

	// ActionScripts embedded in a movie can use the built-in
	// fscommand() function to send data back to the host
	// application.	 If you are interested in this data, register
	// a handler, which will be called when the embedded scripts
	// call fscommand().
	//
	// The handler gets the movie_interface* that the script is
	// embedded in, and the two string arguments passed by the
	// script to fscommand().
	typedef void (*fscommand_callback)(movie_interface* movie, const char* command, const char* arg);
	exported_module void	register_fscommand_callback(fscommand_callback handler);

	// Use this to control how finely curves are subdivided.  1.0
	// is the default; it's a pretty good value.  Larger values
	// result in coarser, more angular curves with fewer vertices.
	exported_module void	set_curve_max_pixel_error(float pixel_error);
	exported_module float	get_curve_max_pixel_error();

	// Some helpers that may or may not be compiled into your
	// version of the library, depending on platform etc.
	exported_module render_handler*	create_render_handler_xbox();
	exported_module render_handler*	create_render_handler_ogl();
	exported_module render_handler* create_render_handler_d3d(IDirect3DDevice9* _pDevice);
	exported_module render_handler* create_render_handler_d3d(IDirect3DDevice8* _pDevice);
	exported_module sound_handler*	create_sound_handler_sdl();


	// For stuff that's tricky to keep track of w/r/t ownership & cleanup.
	struct ref_counted
	{
		exported_module ref_counted();
		exported_module virtual ~ref_counted();
		exported_module void	add_ref() const;
		exported_module void	drop_ref();
		exported_module int	get_ref_count() const { return m_ref_count; }
		exported_module weak_proxy*	get_weak_proxy() const;

	private:
		mutable int	m_ref_count;
		mutable weak_proxy*	m_weak_proxy;
	};


	struct font;
	struct character_def;
	struct sound_sample;
	struct video_stream_definition;
	struct sprite_definition;
	struct as_as_function;
	struct as_object_interface;
	struct canvas;

	// An interface for casting to different types of
	// resources.
	struct resource : public ref_counted
	{
		virtual ~resource() {}

		// Override in derived classes that implement corresponding interfaces.
		virtual font* cast_to_font() { return 0; }
		virtual character_def* cast_to_character_def() { return 0; }
		virtual sprite_definition* cast_to_sprite_definition() { return 0; }
		virtual sound_sample* cast_to_sound_sample() { return 0; }
		virtual video_stream_definition* cast_to_video_stream_definition() { return 0; }
		virtual as_as_function* cast_to_as_function() { return 0; }
		virtual as_object_interface* cast_to_as_object_interface() { return 0; }
		virtual canvas* cast_to_canvas() { return 0; }
	};

	// Forward decls for cast_to_*
	struct as_array;
	struct as_color;
	struct as_netstream;
	struct as_netconnection;
	struct as_key;
	struct as_sound;
	struct video_stream_instance;
	struct sprite_instance;
	struct as_timer;
	struct sprite_instance;
	struct as_mcloader;
	struct as_object;
	struct as_environment;
	struct as_xmlsock;
	struct tu_string_as_object;
	struct as_textformat;
	struct x3ds_instance;
	struct edit_text_character;
	struct as_as_function;
	struct as_date;
	struct as_listener;
	struct as_selection;

	// This is the base class for all ActionScript-able objects
	// ("as_" stands for ActionScript).
	struct as_object_interface : public resource
	{
		virtual ~as_object_interface() {}

		// So that text_character's can return something reasonable.
		virtual const char*	get_text_value() const { return 0; }

		virtual bool	set_member(const tu_stringi& name, const as_value& val) = 0;
		virtual bool	get_member(const tu_stringi& name, as_value* val) = 0;
		virtual bool	on_event(const event_id& id) { return false; }
		virtual void advance(float delta_time) { assert(0); }
		virtual movie_root*		get_root() { return (movie_root*) get_current_root(); }

		// Replacements for dynamic_cast<>.  Override in subclasses
		// that implement the corresponding interfaces.
		virtual video_stream_instance* cast_to_video_stream_instance() { return 0; }
		virtual as_array* cast_to_as_array() { return 0; }
		virtual as_color* cast_to_as_color() { return 0; }
		virtual as_key* cast_to_as_key() { return 0; }
		virtual tu_string_as_object* cast_to_as_string() { return 0; }
		virtual as_sound* cast_to_as_sound() { return 0; }
		virtual as_netstream* cast_to_as_netstream() { return 0; }
		virtual as_netconnection* cast_to_as_netconnection() { return 0; }
		virtual character* cast_to_character() { return 0; }
		virtual as_timer* cast_to_as_timer() { return 0; }
		virtual sprite_instance* cast_to_sprite() { return 0; }
		virtual as_mcloader* cast_to_as_mcloader() { return 0; }
		virtual as_textformat* cast_to_as_textformat() { return 0; }
		virtual as_xmlsock* cast_to_as_xmlsock() { return 0; }
		virtual x3ds_instance* cast_to_3ds() { return 0; }
		virtual as_object* cast_to_as_object() { return 0; }
		virtual edit_text_character* cast_to_edit_text_character() { return 0; }
		virtual as_date* cast_to_as_date() { return 0; }
		virtual as_listener* cast_to_as_listener() { return 0; }
		virtual as_object_interface* cast_to_as_object_interface() { return this; }
		virtual as_selection* cast_to_as_selection() { return 0; }

		// retrieves members/variables from THIS & pushes them into env
		virtual	void enumerate(as_environment* env) { assert(0); }

		// retrieves the reference(__proto__ ) to the prototype property of the class
		// (ActionScript 2.0) or constructor function
		// __proto__ is a function object !!!
		virtual as_object_interface* get_proto() const { return 0; }

		// Registers an event handler to be invoked when a specified property changes.
		virtual bool watch(const tu_string& name,	as_as_function* callback, const as_value& user_data)
		{
			return false; 
		}

		// Removes a watchpoint that Object.watch() created.
		// This method returns a value of true if the watchpoint is successfully removed,
		// false otherwise.
		virtual bool unwatch(const tu_string& name) { return false; }

		// for debugging
		// dumps the object
		virtual void dump() {};

		// Nulls out object's members that links to 'this_ptr'
		// to avoid cross-link memory leak
		virtual void clear_refs(hash<as_object_interface*, bool>* visited_objects,
			as_object_interface* this_ptr) {}

		// Marks object as 'not garbage'
		virtual void not_garbage() {}

		virtual as_environment*	get_environment() { return 0; }
		virtual void copy_to(as_object_interface* target) {}
	};


	// For caching precomputed stuff.  Generally of
	// interest to gameswf_processor and programs like it.
	struct cache_options
	{
		bool	m_include_font_bitmaps;

		cache_options()
			:
			m_include_font_bitmaps(true)
		{
		}
	};


	// A character_def is the immutable data representing the template of a
	// movie element.
	//
	// @@ This is not really a public interface.  It's here so it
	// can be mixed into movie_definition, movie_definition_sub,
	// and sprite_definition, without using multiple inheritance.
	struct character_def : public resource
	{
	private:
		int	m_id;

	public:
		character_def()
			:
			m_id(-1)
		{
		}

		virtual ~character_def() {}

		virtual void	display(character* instance_info) {}
		virtual bool	point_test_local(float x, float y) { return false; }
		virtual void get_bound(rect* bound) { assert(0); };

		// Should stick the result in a smart_ptr immediately.
		virtual character*	create_character_instance(character* parent, int id);	// default is to make a generic_character

		// From resource interface.
		virtual character_def*	cast_to_character_def() { return this; }
		virtual movie_def_impl* cast_to_movie_def_impl() { return 0; }
		//
		// Caching.
		//

		virtual void	output_cached_data(tu_file* out, const cache_options& options) {}
		virtual void	input_cached_data(tu_file* in) {}

		// for definetext, definetext2 & defineedittext tags
		virtual void	csm_textsetting(stream* in, int tag_type) { assert(0); };

	};


	//
	// This is the client program's interface to the definition of
	// a movie (i.e. the shared constant source info).
	//
	struct movie_definition : public character_def
	{
		virtual int	get_version() const = 0;
		virtual float	get_width_pixels() const = 0;
		virtual float	get_height_pixels() const = 0;
		virtual int	get_frame_count() const = 0;
		virtual float	get_frame_rate() const = 0;

		// This calls add_ref() on the movie_interface internally.
		// Call drop_ref() on the movie_interface when you're done with it.
		// Or use smart_ptr<T> from base/smart_ptr.h if you want.
		virtual movie_interface*	create_instance() = 0;
		virtual void clear_instance() {};

		virtual void	output_cached_data(tu_file* out, const cache_options& options) = 0;
		virtual void	input_cached_data(tu_file* in) = 0;

		// Causes this movie def to generate texture-mapped
		// versions of all the fonts it owns.  This improves
		// speed and quality of text rendering.	 The
		// texture-map data is serialized in the
		// output/input_cached_data() calls, so you can
		// preprocess this if you load cached data.
//		virtual void	generate_font_bitmaps() = 0;

		//
		// (optional) API to support gameswf::create_movie_no_recurse().
		//

		// Call visit_imported_movies() to retrieve a list of
		// names of movies imported into this movie.
		// visitor->visit() will be called back with the name
		// of each imported movie.
		struct import_visitor
		{
			virtual ~import_visitor() {}
			virtual void	visit(const char* imported_movie_filename) = 0;
		};
		virtual void	visit_imported_movies(import_visitor* visitor) = 0;

		// Call this to resolve an import of the given movie.
		// Replaces the dummy placeholder with the real
		// movie_definition* given.
		virtual void	resolve_import(const char* name, movie_definition* def) = 0;

		//
		// (optional) API to support host-driven creation of textures.
		//
		// Create the movie using gameswf::create_movie_no_recurse(..., DO_NOT_LOAD_BITMAPS),
		// and then initialize each bitmap info via get_bitmap_info_count(), get_bitmap_info(),
		// and bitmap_info::init_*_image() or your own subclassed API.
		//
		// E.g.:
		//
		// // During preprocessing:
		// // This will create bitmap_info's using the rgba, rgb, alpha contructors.
		// my_def = gameswf::create_movie_no_recurse("myfile.swf", DO_LOAD_BITMAPS);
		// int ct = my_def->get_bitmap_info_count();
		// for (int i = 0; i < ct; i++)
		// {
		//	my_bitmap_info_subclass*	bi = NULL;
		//	my_def->get_bitmap_info(i, (bitmap_info**) &bi);
		//	my_precomputed_textures.push_back(bi->m_my_internal_texture_reference);
		// }
		// // Save out my internal data.
		// my_precomputed_textures->write_into_some_cache_stream(...);
		//
		// // Later, during run-time loading:
		// my_precomputed_textures->read_from_some_cache_stream(...);
		// // This will create blank bitmap_info's.
		// my_def = gameswf::create_movie_no_recurse("myfile.swf", DO_NOT_LOAD_BITMAPS);
		//
		// // Push cached texture info into the movie's bitmap_info structs.
		// int	ct = my_def->get_bitmap_info_count();
		// for (int i = 0; i < ct; i++)
		// {
		//	my_bitmap_info_subclass*	bi = (my_bitmap_info_subclass*) my_def->get_bitmap_info(i);
		//	bi->set_internal_texture_reference(my_precomputed_textures[i]);
		// }
		virtual int	get_bitmap_info_count() const = 0;
		virtual bitmap_info*	get_bitmap_info(int i) const = 0;
	};

	// Keyboard handling
	namespace key
	{
		enum code
		{
			INVALID = 0,
			A = 65,
			B,
			C,
			D,
			E,
			F,
			G,
			H,
			I,
			J,
			K,
			L,
			M,
			N,
			O,
			P,
			Q,
			R,
			S,
			T,
			U,
			V,
			W,
			X,
			Y,
			Z,
			_0 = 48,
			_1,
			_2,
			_3,
			_4,
			_5,
			_6,
			_7,
			_8,
			_9,
			KP_0 = 96,
			KP_1,
			KP_2,
			KP_3,
			KP_4,
			KP_5,
			KP_6,
			KP_7,
			KP_8,
			KP_9,
			KP_MULTIPLY,
			KP_ADD,
			KP_ENTER,
			KP_SUBTRACT,
			KP_DECIMAL,
			KP_DIVIDE,
			F1 = 112,
			F2,
			F3,
			F4,
			F5,
			F6,
			F7,
			F8,
			F9,
			F10,
			F11,
			F12,
			F13,
			F14,
			F15,
			BACKSPACE = 8,
			TAB,
			CLEAR = 12,
			ENTER,
			SHIFT = 16,
			CONTROL,
			ALT,
			CAPSLOCK = 20,
			ESCAPE = 27,
			SPACE = 32,
			PGDN,
			PGUP,
			END = 35,
			HOME,
			LEFT,
			UP,
			RIGHT,
			DOWN,
			INSERT = 45,
			DELETEKEY,
			HELP,
			NUM_LOCK = 144,
			SEMICOLON = 186,
			EQUALS = 187,
			MINUS = 189,
			SLASH = 191,
			BACKTICK = 192,
			LEFT_BRACKET = 219,
			BACKSLASH = 220,
			RIGHT_BRACKET = 221,
			QUOTE = 222,

			KEYCOUNT
		};
	}	// end namespace key

	//
	// This is the client program's interface to an instance of a
	// movie (i.e. an independent stateful live movie).
	//
	struct movie_interface : public as_object_interface
	{
		virtual movie_definition*	get_movie_definition() = 0;

		// Frame counts in this API are 0-based (unlike ActionScript)
		virtual int	get_current_frame() const = 0;
		virtual bool	has_looped() const = 0;

		virtual void	advance(float delta_time) = 0;
		virtual void	goto_frame(int frame_number) = 0;

		// Returns true if labeled frame is found.
		virtual bool	goto_labeled_frame(const char* label) = 0;
		virtual void	display() = 0;

		enum play_state
		{
			PLAY,
			STOP
		};
		virtual void	set_play_state(play_state s) = 0;
		virtual play_state	get_play_state() const = 0;

		virtual void	set_background_color(const rgba& bg_color) = 0;

		// Set to 0 if you don't want the movie to render its
		// background at all.  1 == full opacity.
		virtual void	set_background_alpha(float alpha) = 0;
		virtual float	get_background_alpha() const = 0;

		// move/scale the movie...
		virtual void	set_display_viewport(int x0, int y0, int w, int h) = 0;

		// Input.
		virtual void	notify_mouse_state(int x, int y, int buttons) = 0;

		// Set an ActionScript variable within this movie.
		// You can use this to set the value of text fields,
		// ordinary variables, or properties of characters
		// within the script.
		//
		// This version accepts UTF-8
		virtual void	set_variable(const char* path_to_var, const char* new_value) = 0;
		// This version accepts UCS-2 or UCS-4, depending on sizeof(wchar_t)
		virtual void	set_variable(const char* path_to_var, const wchar_t* new_value) = 0;
		// @@ do we want versions that take a number?

		// Get the value of an ActionScript variable.
		//
		// Value is ephemeral & not thread safe!!!  Use it or
		// copy it immediately.
		//
		// Returns UTF-8
		virtual const char*	get_variable(const char* path_to_var) const = 0;
		// @@ do we want a version that returns a number?

		// ActionScript method call.  Return value points to a
		// static string buffer with the result; caller should
		// use the value immediately before making more calls
		// to gameswf.

		// NOT THREAD SAFE!!!
		//
		// DO NOT USE TO CALL CLASS MEMBER!!!
		//
		// method_name is the name of the method (possibly namespaced).
		//
		// method_arg_fmt is a printf-style declaration of
		// the method call, where the arguments are
		// represented by { %d, %s, %f, %ls }, followed by the
		// vararg list of actual arguments.
		//
		// E.g.
		//
		// m->call_method("path.to.method_name", "%d, %s, %f", i, "hello", 2.7f);
		//
		// The format args are a small subset of printf, namely:
		//
		// %d -- integer arg
		// %s -- 0-terminated char* string arg
		// %ls -- 0-terminated wchar_t* string arg
		// %f -- float/double arg
		//
		// Whitespace and commas in the format string are ignored.
		//
		// This is not an ActionScript language parser, it
		// doesn't recognize expressions or anything tricky.

#ifdef __GNUC__
		// use the following to catch errors: (only with gcc)
		virtual const char*	call_method(const char* method_name, const char* method_arg_fmt, ...)
			__attribute__((format (printf, 3, 4))) = 0;	// "this" is an implied param, so fmt is 3 and ... is 4!
#else	// not __GNUC__
		virtual const char*	call_method(const char* method_name, const char* method_arg_fmt, ...) = 0;
#endif	// not __GNUC__
		virtual const char*	call_method_args(const char* method_name, const char* method_arg_fmt, va_list args) = 0;


		// Make the movie visible/invisible.  An invisible
		// movie does not advance and does not render.
		virtual void	set_visible(bool visible) = 0;

		// Return visibility status.
		virtual bool	get_visible() const = 0;

		// Set and get userdata, that's useful for the fs_command handler.
		virtual void   *get_userdata() = 0;
		virtual void   set_userdata(void *) = 0;

		// Display callbacks, for client rendering.  Callback
		// is called after rendering the object it's attached
		// to.
		//
		// Attach NULL to disable the callback.
		virtual void	attach_display_callback(const char* path_to_object, void (*callback)(void* user_ptr), void* user_ptr) = 0;

		// for external movies
		virtual character*	get_root_movie() = 0;

		// External interface for the host to report key events.
		virtual void	notify_key_event(key::code k, bool down) { /*assert(0);*/ }

		// Movie info
		virtual int	get_movie_version() { return 0; }
		virtual int	get_movie_width() { return 0; }
		virtual int	get_movie_height() { return 0; }
		virtual float	get_movie_fps() { return 0.0f; }
	};

	// Try to grab movie info from the header of the given .swf
	// file.
	//
	// Sets *version to 0 if info can't be extracted.
	//
	// You can pass NULL for any entries you're not interested in.
	void	get_movie_info(
		const char*	filename,
		int*		version,
		int*		width,
		int*		height,
		float*		frames_per_second,
		int*		frame_count,
		int*		tag_count);

	// Enable/disable attempts to read cache files (.gsc) when
	// loading movies.
	exported_module void	set_use_cache_files(bool use_cache);

	// @@ Hm, need to think about these creation API's.  Perhaps
	// divide it into "low level" and "high level" calls.  Also,
	// perhaps we need a "context" object that contains all
	// global-ish flags, libraries, callback pointers, font
	// library, etc.
	//
	// Create a gameswf::movie_definition from the given file name.
	// Normally, will also try to load any cached data file
	// (".gsc") that corresponds to the given movie file.  This
	// will still work even if there is no cache file.  You can
	// disable the attempts to load cache files by calling
	// gameswf::use_cache_files(false).
	//
	// Uses the registered file-opener callback to read the files
	// themselves.
	//
	// This calls add_ref() on the newly created definition; call
	// drop_ref() when you're done with it.
	// Or use smart_ptr<T> from base/smart_ptr.h if you want.
	exported_module movie_definition*	create_movie(const char* filename);

	// Creates the movie from the given input stream.  Only reads
	// from the given stream; does not open files.	If the movie
	// imports resources from other movies, the created movie
	// inserts proxy stubs in place of those resources.  The list
	// of imported movie filenames can be retrieved with
	// movie_definition::visit_imported_movies().  The proxies can
	// be replaced with actual movie_definition's via
	// movie_definition::resolve_proxy(name,def).
	//
	// Use DO_NOT_LOAD_BITMAPS if you have pre-processed bitmaps
	// stored externally somewhere, and you plan to install them
	// via get_bitmap_info()->...
	enum create_bitmaps_flag
	{
		DO_LOAD_BITMAPS,
		DO_NOT_LOAD_BITMAPS
	};
	// Use DO_NOT_LOAD_FONT_SHAPES if you know you have
	// precomputed texture glyphs (in cached data) and you know
	// you always want to render text using texture glyphs.
	enum create_font_shapes_flag
	{
		DO_LOAD_FONT_SHAPES,
		DO_NOT_LOAD_FONT_SHAPES
	};
	movie_definition*	create_movie_no_recurse(
		tu_file*		input_stream,
		create_bitmaps_flag	cbf,
		create_font_shapes_flag cfs);

	// Helper to pregenerate cached data (basically, shape
	// tesselations).  Does this by running through each frame of
	// the movie and displaying the shapes with a null renderer.
	// The pregenerated data is stored in the movie_definition
	// object itself, and is included with the cached data written
	// by movie_definition::output_cached_data().
	//
	// Note that this tesselates shapes to the resolution they
	// explicitly appear in the linear frames of the movie.	 Does
	// not try very hard to run your ActionScript to account for
	// dynamic scaling (that's more or less futile anyway due to
	// the halting problem).
	void	precompute_cached_data(movie_definition* movie_def);

	// Maximum release of resources.  Calls clear_library() and
	// fontlib::clear(), and also clears some extra internal stuff
	// that may have been allocated (e.g. global ActionScript
	// objects).  This should get all gameswf structures off the
	// heap, with the exception of any objects that are still
	// referenced by the host program and haven't had drop_ref()
	// called on them.
	exported_module void	clear_gameswf();


	//
	// Library management
	//

	// Release any library movies we've cached.  Do this when you want
	// maximum cleanup.
	void	clear_library();

	//
	// Sound callback handler.
	//

	// You may define a subclass of this, and pass an instance to
	// set_sound_handler().
	struct sound_handler
	{
		// audio for video
		typedef void (*aux_streamer_ptr)(as_object_interface* netstream, unsigned char* stream, int len);

		enum format_type
		{
			FORMAT_RAW = 0,		// unspecified format.	Useful for 8-bit sounds???
			FORMAT_ADPCM = 1,	// gameswf doesn't pass this through; it uncompresses and sends FORMAT_NATIVE16
			FORMAT_MP3 = 2,
			FORMAT_UNCOMPRESSED = 3,	// 16 bits/sample, little-endian
			FORMAT_NELLYMOSER = 6,	// Mystery proprietary format; see nellymoser.com

			// gameswf tries to convert data to this format when possible:
			FORMAT_NATIVE16 = 7	// gameswf extension: 16 bits/sample, native-endian
		};

		// If stereo is true, samples are interleaved w/ left sample first.

		// gameswf calls at load-time with sound data, to be
		// played later.  You should create a sample with the
		// data, and return a handle that can be used to play
		// it later.  If the data is in a format you can't
		// deal with, then you can return 0 (for example), and
		// then ignore 0's in play_sound() and delete_sound().
		//
		// Assign handles however you like.
		virtual int	create_sound(
			void*		data,
			int		data_bytes,
			int		sample_count,
			format_type	format,
			int		sample_rate,	/* one of 5512, 11025, 22050, 44100 */
			bool		stereo) = 0;

		// loads external sound file
		virtual int	load_sound(const char* url) = 0;

		virtual void append_sound(int sound_handle, void* data, int data_bytes) = 0;

		// gameswf calls this when it wants you to play the defined sound.
		//
		// loop_count == 0 means play the sound once (1 means play it twice, etc)
		virtual void	play_sound(int sound_handle, int loop_count /* , volume, pan, etc? */) = 0;

		virtual void	set_volume(int sound_handle, int volume) = 0;

		// Stop the specified sound if it's playing.
		// (Normally a full-featured sound API would take a
		// handle specifying the *instance* of a playing
		// sample, but SWF is not expressive that way.)
		virtual void	stop_sound(int sound_handle) = 0;
		virtual void	stop_all_sounds() = 0;

		// gameswf calls this when it's done with a particular sound.
		virtual void	delete_sound(int sound_handle) = 0;

		// They are called from netstream when it wishes to sound video
		virtual void attach_aux_streamer(aux_streamer_ptr ptr, as_object_interface* netstream) {};
		virtual void detach_aux_streamer(as_object_interface* netstream) {};
		virtual void cvt(short int** adjusted_data, int* adjusted_size, unsigned char* data,
				 int size, int channels, int freq) {};

		virtual ~sound_handler() {};
		virtual bool is_open() { return false; };
		virtual void pause(int sound_handle, bool paused) {};

		// The number of milliseconds a sound has been playing. 
		// If the sound is looped, the position is reset to 0 at the beginning of each loop.
		virtual int get_position(int sound_handle) { return 0; };
	};

	// tu_float is used in matrix & cxform because
	// Flash converts inf to zero when works with matrix & cxform
	struct tu_float
	{
		operator float() const { return m_float; }
		inline void	operator=(const float x)
		{
			m_float = x >= -3.402823466e+38F && x <= 3.402823466e+38F ? x : 0.0f;
		}
		void	operator+=(const float x) { operator=(m_float + x); }
		void	operator-=(const float x) { operator=(m_float - x); }
		void	operator*=(const float x) { operator=(m_float * x); }
		void	operator/=(const float x) { operator=(m_float / x); }

	private:
		float m_float;
	};



	//
	// matrix type, used by render handler
	//

	struct point;
	struct matrix
	{
		tu_float	m_[2][3];

		static matrix	identity;

		exported_module matrix();
//		bool	is_valid() const;
		exported_module void	set_identity();
		void	concatenate(const matrix& m);
		void	concatenate_translation(float tx, float ty);
		void	concatenate_scale(float s);
		void	set_lerp(const matrix& m1, const matrix& m2, float t);
		void	set_scale_rotation(float x_scale, float y_scale, float rotation);
		void	read(stream* in);
		void	print() const;
		void	transform(point* result, const point& p) const;
		void	transform(rect* bound) const;
		void	transform_vector(point* result, const point& p) const;
		void	transform_by_inverse(point* result, const point& p) const;
		void	transform_by_inverse(rect* bound) const;
		void	set_inverse(const matrix& m);
		bool	does_flip() const;	// return true if we flip handedness
		float	get_determinant() const;	// determinant of the 2x2 rotation/scale part only
		float	get_max_scale() const;	// return the maximum scale factor that this transform applies
		float	get_x_scale() const;	// return the magnitude scale of our x coord output
		float	get_y_scale() const;	// return the magnitude scale of our y coord output
		float	get_rotation() const;	// return our rotation component (in radians)
	};


	//
	// point: used by rect which is used by render_handler (otherwise would be in internal gameswf_types.h)
	//


	struct point
	{
		float	m_x, m_y;

		point() : m_x(0), m_y(0) {}
		point(float x, float y) : m_x(x), m_y(y) {}

		void	set_lerp(const point& a, const point& b, float t)
		// Set to a + (b - a) * t
		{
			m_x = a.m_x + (b.m_x - a.m_x) * t;
			m_y = a.m_y + (b.m_y - a.m_y) * t;
		}

		bool operator==(const point& p) const { return m_x == p.m_x && m_y == p.m_y; }

		bool	bitwise_equal(const point& p) const;

		void twips_to_pixels();
		void pixels_to_twips();
	};


	//
	// rect: rectangle type, used by render handler
	//


	struct rect
	{
		float	m_x_min, m_x_max, m_y_min, m_y_max;

		void	read(stream* in);
		void	print() const;
		bool	point_test(float x, float y) const;
		bool	bound_test(const rect& bound) const;
		void	set_to_point(float x, float y);
		void	set_to_point(const point& p);
		void	expand_to_point(float x, float y);
		void	expand_to_point(const point& p);
		void	expand_to_rect(const rect& r);
		float	width() const { return m_x_max - m_x_min; }
		float	height() const { return m_y_max - m_y_min; }

		point	get_corner(int i) const;

		void	enclose_transformed_rect(const matrix& m, const rect& r);

		void	set_lerp(const rect& a, const rect& b, float t);

		void twips_to_pixels();
		void pixels_to_twips();
	};


	//
	// cxform: color transform type, used by render handler
	//
	struct cxform
	{
		tu_float	m_[4][2];	// [RGBA][mult, add]

		cxform();
		void	concatenate(const cxform& c);
		rgba	transform(const rgba in) const;
		void	read_rgb(stream* in);
		void	read_rgba(stream* in);
		void	clamp();  // Force component values to be in range.
		void	print() const;

		static cxform	identity;
	};


	//
	// texture and render callback handler.
	//

	// Your render_handler creates bitmap_info's for gameswf.  You
	// need to subclass bitmap_info in order to add the
	// information and functionality your app needs to render
	// using textures.
	struct bitmap_info : public ref_counted
	{
		unsigned int	m_texture_id;		// nuke?
		int		m_original_width;	// nuke?
		int		m_original_height;	// nuke?

		virtual void layout_image(image::image_base* im) { };
		image::image_base* m_suspended_image;
		
		bitmap_info()
			:
			m_texture_id(0),
			m_original_width(0),
			m_original_height(0),
			m_suspended_image(0)
		{
		}
		
		~bitmap_info()
		{
			delete m_suspended_image;
		}

		int get_width() const
		{
			return m_suspended_image ? m_suspended_image->m_width : m_original_width;
		}

		int get_height() const
		{
			return m_suspended_image ? m_suspended_image->m_height : m_original_height;
		}

	};

	// You must define a subclass of render_handler, and pass an
	// instance to set_render_handler().
	struct render_handler
	{
		virtual ~render_handler() {}

		// Your handler should return these with a ref-count of 0.  (@@ is that the right policy?)
		virtual bitmap_info*	create_bitmap_info_empty() = 0;	// used when DO_NOT_LOAD_BITMAPS is set
		virtual bitmap_info*	create_bitmap_info_alpha(int w, int h, unsigned char* data) = 0;
		virtual bitmap_info*	create_bitmap_info_rgb(image::rgb* im) = 0;
		virtual bitmap_info*	create_bitmap_info_rgba(image::rgba* im) = 0;
		virtual video_handler*	create_video_handler() = 0;

		// Bracket the displaying of a frame from a movie.
		// Fill the background color, and set up default
		// transforms, etc.
		virtual void	begin_display(
			rgba background_color,
			int viewport_x0, int viewport_y0,
			int viewport_width, int viewport_height,
			float x0, float x1, float y0, float y1) = 0;
		virtual void	end_display() = 0;

		// Geometric and color transforms for mesh and line_strip rendering.
		virtual void	set_matrix(const matrix& m) = 0;
		virtual void	set_cxform(const cxform& cx) = 0;

		// Draw triangles using the current fill-style 0.
		// Clears the style list after rendering.
		//
		// coords is a list of (x,y) coordinate pairs, in
		// triangle-strip order.  The type of the array should
		// be Sint16[vertex_count*2]
		virtual void	draw_mesh_strip(const void* coords, int vertex_count) = 0;
		// As above, but coords is in triangle list order.
		virtual void	draw_triangle_list(const void* coords, int vertex_count) = 0;

		// Draw a line-strip using the current line style.
		// Clear the style list after rendering.
		//
		// Coords is a list of (x,y) coordinate pairs, in
		// sequence.  Each coord is a 16-bit signed integer.
		virtual void	draw_line_strip(const void* coords, int vertex_count) = 0;

		// Set line and fill styles for mesh & line_strip
		// rendering.
		enum bitmap_wrap_mode
		{
			WRAP_REPEAT,
			WRAP_CLAMP
		};
		virtual void	fill_style_disable(int fill_side) = 0;
		virtual void	fill_style_color(int fill_side, const rgba& color) = 0;
		virtual void	fill_style_bitmap(int fill_side, bitmap_info* bi, const matrix& m, bitmap_wrap_mode wm) = 0;

		virtual void	line_style_disable() = 0;
		virtual void	line_style_color(rgba color) = 0;
		virtual void	line_style_width(float width) = 0;

		// Special function to draw a rectangular bitmap;
		// intended for textured glyph rendering.  Ignores
		// current transforms.
		virtual void	draw_bitmap(
			const matrix&		m,
			bitmap_info*	bi,
			const rect&		coords,
			const rect&		uv_coords,
			rgba			color) = 0;
		virtual void	set_antialiased(bool enable) = 0;

		virtual void begin_submit_mask() = 0;
		virtual void end_submit_mask() = 0;
		virtual void disable_mask() = 0;

		// Mouse cursor handling.
		enum cursor_type
		{
			SYSTEM_CURSOR,
			ACTIVE_CURSOR
		};
		virtual void set_cursor(cursor_type cursor) {}
		virtual bool is_visible(const rect& bound) = 0;
	};

	// Key events are global throughout gameswf.
	// @@ Maybe someday make these local to the movie_interface?
	// Vitaly: I do not see the sense to send events inactive movie
	// in multifile games
//	void	notify_key_event(key::code k, bool down);


	// Some optional helpers.
	namespace tools
	{
		struct process_options
		{
			bool	m_zip_whole_file;	// @@ not implemented yet (low priority?)
			bool	m_remove_image_data;	// removes existing image data; leaves minimal placeholder tags
			bool	m_remove_font_glyph_shapes;

			process_options()
				:
				m_zip_whole_file(false),
				m_remove_image_data(false),
				m_remove_font_glyph_shapes(false)
			{}
		};

		// Copy tags from *in to *out, applying the given
		// options.  *in should be a SWF-format stream.	 The
		// output will be a SWF-format stream.
		//
		// Returns 0 on success, or a non-zero error-code on
		// failure.
		int	process_swf(tu_file* swf_out, tu_file* swf_in, const process_options& options);
	}

}	// namespace gameswf


#endif // GAMESWF_H


// Local Variables:
// mode: C++
// c-basic-offset: 8
// tab-width: 8
// indent-tabs-mode: t
// End:
