Include files description:
--------------------------

C_buffer.h
   Variable-sized buffer template class, managing allocation. Similar to std::vector, but not growing/shrinking.

C_cache.h
   File I/O class, reading from/writing to file or memory buffer, providing similar interface for
   all methods. File I/O is performed by using dta-reader (Dta_read.h), memory I/O is perfomed
   by memory buffers (allocated/freed by the class if writing).

C_chunk.h
   Chunk-based I/O class, using the C_cache class (C_cache.h). Suitable for files I/O composed of
   nested chunks.

C_except.h
   Exception class for usage with C++ exception handling mechanism, inheriting from std::exception.

C_linklist.h
   Simple one-directional linked-list system - definition of linked list base class, and
   linked list class. No memory allocations used. Very simple, efficient system.

CmdLine.h
   Command-line base class, used to scan and fill-in command parameters. Inherited classes provide
   only specialized behavior (through callbacks) and command-definition table.

Config.h
   Game configurator - parameter structure declaration, and medhod invoking configuration dialog.

Dta_read.h
   Data reader - file I/O, using either standard file system, or custom data packages.

Editctrl.h
   Interface to HighWare text editor - simple customizable text editor, invokable from programs.

FPS_counter.h
   Class used for counting frames-per-second count.

Iexcpt.h
   Hardware exception handling for Win32 API, catching standard and FPU hardware exceptions.
   Dialog displaying, retrieving call stack, setting handler.

Igraph2.h
   Interface to IGraph library.

INet2.h
   Interface to INet library.

Integer.h
   Several integer bit-based functions.

IPhysics.h
   Interface to IPhysics library.

Iscript.h
   Interface to IScript library.

Isound2.h
   Interface to ISound library.

Profile.h
   Simple profile functions, and block-profiler class.

Rules.h
   General types required for compilation of Insanity group of libraries.

SmartPtr.h
   Smart pointer class for use by classes using reference counting. Const and non-const versions.

Tabler2.h
   Interface to ITabler library.

Win_reg.h
   Windows registry access functions.

Win_res.h
   Windows resource access functions.

Zlib.h
   Zlib main header file.

C_str.hpp
   Reference-counting string class.

C_unknwn.hpp
   Base class used for reference-counted classes.

Conv_rgb.hpp
   Color-model conversion class.

Sortlist.hpp
   Implementation of radix sort algorighm.

//----------------------------

I3D\Bin_format.h
   Chunk IDs and structures for .bin file format.

I3D\CameraPath.h
   Camera path bezier interpolator - class building bezier line from 3D points, and allowing animation
   along the path.

I3D\Editor.h
   Interface to IEditor library.

I3D\I3D_cache.h
   Insanity resource cache - for models, sounds and animations.

I3D\I3D_format.h
   Chunk IDs and structures for .i3d file format.

I3D\I3d_math.h
   Insanity math classes include file.

I3D\I3D2.h
   Interface to Insanity3D library.

I3D\PoseAnim.h
   Pose animation loader class.

//----------------------------

Insanity\3DTexts.h
   Sprite-based text output system.

I3D\AppInit.h
   Interface to Startup.dll library, which contains common game-specific tasks, dialogs, etc,
   and manages the initialization, shutdown, exceptions and common plugins for game application.

Insanity\Assert.h
   Customized assert macro.

Insanity\Controller.h
   Game controller class.

Insanity\os.h
   Operation-system specific functions (date, file, dialogs, etc).

Insanity\PhysTemplate.h
   Template for physics set-up on a model, edited in the PhysicsStudio plugin, applied onto physics actor.

Insanity\SourceControl.h
   Source-control access.

Insanity\Sprite.h
   Polygon-based sprite system.

Insanity\Texts.h
   Localized text reader used to read and keep game texts in various languages (unicode supported).

//----------------------------

Math\Bezier.hpp
    Template bezier curve class.

Math\Ease.hpp
   Easiness control - morphing of linear value from 0 to 1 into non-linear value in the same range.
   Typically used for time morphing.

Math\Spline.hpp
   Template spline curve class.

