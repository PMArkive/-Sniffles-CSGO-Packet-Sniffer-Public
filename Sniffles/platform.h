#ifndef PLATFORM_H
#define PLATFORM_H

#ifdef OSX
#include <malloc/malloc.h>
#else
#include <malloc.h>
#endif
#include <limits.h>
#include <float.h>
#include <stdlib.h>
#include <string.h>

#ifdef COMPILER_GCC
#include <new>
#else
#include <new.h>
#endif

typedef unsigned char uint8;
typedef signed char int8;

#if defined( _WIN32 )

typedef unsigned int			uint;
typedef __int16					int16;
typedef unsigned __int16		uint16;
typedef __int32					int32;
typedef unsigned __int32		uint32;
typedef __int64					int64;
typedef unsigned __int64		uint64;
typedef unsigned long			CRC32_t;

#ifdef PLATFORM_64BITS
typedef __int64 intp;				// intp is an integer that can accomodate a pointer
typedef unsigned __int64 uintp;		// (ie, sizeof(intp) >= sizeof(int) && sizeof(intp) >= sizeof(void *)
#else
typedef __int32 intp;
typedef unsigned __int32 uintp;
#endif


#if !defined( MAX_OSPATH )
#define	MAX_OSPATH		260			// max length of a filesystem pathname
#endif

// Use this to specify that a function is an override of a virtual function.
// This lets the compiler catch cases where you meant to override a virtual
// function but you accidentally changed the function signature and created
// an overloaded function. Usage in function declarations is like this:
// int GetData() const OVERRIDE;
#define OVERRIDE override

#else // _WIN32

typedef short					int16;
typedef unsigned short			uint16;
typedef int						int32;
typedef unsigned int			uint32;
typedef long long				int64;
typedef unsigned long long		uint64;
#ifdef PLATFORM_64BITS
typedef long long			intp;
typedef unsigned long long	uintp;
#else
typedef int					intp;
typedef unsigned int		uintp;
#endif
typedef void *HWND;

// Avoid redefinition warnings if a previous header defines this.
#undef OVERRIDE
#if __cplusplus >= 201103L
#define OVERRIDE override
#if defined(__clang__)
// warning: 'override' keyword is a C++11 extension [-Wc++11-extensions]
// Disabling this warning is less intrusive than enabling C++11 extensions
#pragma GCC diagnostic ignored "-Wc++11-extensions"
#endif
#else
#define OVERRIDE
#endif

#endif // else _WIN32


#if defined(_MSC_VER)
#define SELECTANY __declspec(selectany)
#define RESTRICT __restrict
#define RESTRICT_FUNC __declspec(restrict)
#define FMTFUNCTION( a, b )
#elif defined(GNUC)
#define SELECTANY __attribute__((weak))
#if defined(LINUX) && !defined(DEDICATED)
#define RESTRICT
#else
#define RESTRICT __restrict
#endif
#define RESTRICT_FUNC
// squirrel.h does a #define printf DevMsg which leads to warnings when we try
// to use printf as the prototype format function. Using __printf__ instead.
#define FMTFUNCTION( fmtargnumber, firstvarargnumber ) __attribute__ (( format( __printf__, fmtargnumber, firstvarargnumber )))
#else
#define SELECTANY static
#define RESTRICT
#define RESTRICT_FUNC
#define FMTFUNCTION( a, b )
#endif

#if defined( _WIN32 )

// Used for dll exporting and importing
#define DLL_EXPORT				extern "C" __declspec( dllexport )
#define DLL_IMPORT				extern "C" __declspec( dllimport )

// Can't use extern "C" when DLL exporting a class
#define DLL_CLASS_EXPORT		__declspec( dllexport )
#define DLL_CLASS_IMPORT		__declspec( dllimport )

// Can't use extern "C" when DLL exporting a global
#define DLL_GLOBAL_EXPORT		extern __declspec( dllexport )
#define DLL_GLOBAL_IMPORT		extern __declspec( dllimport )

#define DLL_LOCAL

#elif defined GNUC
// Used for dll exporting and importing
#define  DLL_EXPORT   extern "C" __attribute__ ((visibility("default")))
#define  DLL_IMPORT   extern "C"

// Can't use extern "C" when DLL exporting a class
#define  DLL_CLASS_EXPORT __attribute__ ((visibility("default")))
#define  DLL_CLASS_IMPORT

// Can't use extern "C" when DLL exporting a global
#define  DLL_GLOBAL_EXPORT   extern __attribute ((visibility("default")))
#define  DLL_GLOBAL_IMPORT   extern

#define  DLL_LOCAL __attribute__ ((visibility("hidden")))

#else
#error "Unsupported Platform."
#endif

// Used for standard calling conventions
#if defined( _WIN32 ) && !defined( _X360 )
#define  STDCALL				__stdcall
#define  FASTCALL				__fastcall
#define  FORCEINLINE			__forceinline
// GCC 3.4.1 has a bug in supporting forced inline of templated functions
// this macro lets us not force inlining in that case
#define  FORCEINLINE_TEMPLATE		__forceinline
#elif defined( _X360 )
#define  STDCALL				__stdcall
#ifdef FORCEINLINE
#undef FORCEINLINE
#endif 
#define  FORCEINLINE			__forceinline
#define  FORCEINLINE_TEMPLATE		__forceinline
#else
#define  STDCALL
#define  FASTCALL
#ifdef _LINUX_DEBUGGABLE
#define  FORCEINLINE
#else
#define  FORCEINLINE inline __attribute__ ((always_inline))
#endif
// GCC 3.4.1 has a bug in supporting forced inline of templated functions
// this macro lets us not force inlining in that case
#define FORCEINLINE_TEMPLATE	inline
//	#define  __stdcall			__attribute__ ((__stdcall__))
#endif

// Force a function call site -not- to inlined. (useful for profiling)
#define DONT_INLINE(a) (((int)(a)+1)?(a):(a))


// decls for aligning data
#ifdef _WIN32
#define DECL_ALIGN(x) __declspec(align(x))

#elif GNUC
#define DECL_ALIGN(x) __attribute__((aligned(x)))
#else
#define DECL_ALIGN(x) /* */
#endif

#ifdef _MSC_VER
// MSVC has the align at the start of the struct
#define ALIGN4 DECL_ALIGN(4)
#define ALIGN8 DECL_ALIGN(8)
#define ALIGN16 DECL_ALIGN(16)
#define ALIGN32 DECL_ALIGN(32)
#define ALIGN128 DECL_ALIGN(128)

#define ALIGN4_POST
#define ALIGN8_POST
#define ALIGN16_POST
#define ALIGN32_POST
#define ALIGN128_POST
#elif defined( GNUC )
// gnuc has the align decoration at the end
#define ALIGN4
#define ALIGN8 
#define ALIGN16
#define ALIGN32
#define ALIGN128

#define ALIGN4_POST DECL_ALIGN(4)
#define ALIGN8_POST DECL_ALIGN(8)
#define ALIGN16_POST DECL_ALIGN(16)
#define ALIGN32_POST DECL_ALIGN(32)
#define ALIGN128_POST DECL_ALIGN(128)
#else
#error
#endif

//-----------------------------------------------------------------------------
// Convert int<-->pointer, avoiding 32/64-bit compiler warnings:
//-----------------------------------------------------------------------------
#define INT_TO_POINTER( i ) (void *)( ( i ) + (char *)NULL )
#define POINTER_TO_INT( p ) ( (int)(uintp)( p ) )


#if defined( _SGI_SOURCE ) || defined( PLATFORM_X360 )
#define	PLAT_BIG_ENDIAN 1
#else
#define PLAT_LITTLE_ENDIAN 1
#endif

// If a swapped float passes through the fpu, the bytes may get changed.
// Prevent this by swapping floats as DWORDs.
#define SafeSwapFloat( pOut, pIn )	(*((uint*)pOut) = DWordSwap( *((uint*)pIn) ))

#if defined(PLAT_LITTLE_ENDIAN)
#define BigShort( val )				WordSwap( val )
#define BigWord( val )				WordSwap( val )
#define BigLong( val )				DWordSwap( val )
#define BigDWord( val )				DWordSwap( val )
#define LittleShort( val )			( val )
#define LittleWord( val )			( val )
#define LittleLong( val )			( val )
#define LittleDWord( val )			( val )
#define SwapShort( val )			BigShort( val )
#define SwapWord( val )				BigWord( val )
#define SwapLong( val )				BigLong( val )
#define SwapDWord( val )			BigDWord( val )

// Pass floats by pointer for swapping to avoid truncation in the fpu
#define BigFloat( pOut, pIn )		SafeSwapFloat( pOut, pIn )
#define LittleFloat( pOut, pIn )	( *pOut = *pIn )
#define SwapFloat( pOut, pIn )		BigFloat( pOut, pIn )

#elif defined(PLAT_BIG_ENDIAN)

#define BigShort( val )				( val )
#define BigWord( val )				( val )
#define BigLong( val )				( val )
#define BigDWord( val )				( val )
#define LittleShort( val )			WordSwap( val )
#define LittleWord( val )			WordSwap( val )
#define LittleLong( val )			DWordSwap( val )
#define LittleDWord( val )			DWordSwap( val )
#define SwapShort( val )			LittleShort( val )
#define SwapWord( val )				LittleWord( val )
#define SwapLong( val )				LittleLong( val )
#define SwapDWord( val )			LittleDWord( val )

// Pass floats by pointer for swapping to avoid truncation in the fpu
#define BigFloat( pOut, pIn )		( *pOut = *pIn )
#define LittleFloat( pOut, pIn )	SafeSwapFloat( pOut, pIn )
#define SwapFloat( pOut, pIn )		LittleFloat( pOut, pIn )

#else

// @Note (toml 05-02-02): this technique expects the compiler to
// optimize the expression and eliminate the other path. On any new
// platform/compiler this should be tested.
inline short BigShort(short val)		{ int test = 1; return (*(char *)&test == 1) ? WordSwap(val) : val; }
inline uint16 BigWord(uint16 val)		{ int test = 1; return (*(char *)&test == 1) ? WordSwap(val) : val; }
inline long BigLong(long val)			{ int test = 1; return (*(char *)&test == 1) ? DWordSwap(val) : val; }
inline uint32 BigDWord(uint32 val)	{ int test = 1; return (*(char *)&test == 1) ? DWordSwap(val) : val; }
inline short LittleShort(short val)	{ int test = 1; return (*(char *)&test == 1) ? val : WordSwap(val); }
inline uint16 LittleWord(uint16 val)	{ int test = 1; return (*(char *)&test == 1) ? val : WordSwap(val); }
inline long LittleLong(long val)		{ int test = 1; return (*(char *)&test == 1) ? val : DWordSwap(val); }
inline uint32 LittleDWord(uint32 val)	{ int test = 1; return (*(char *)&test == 1) ? val : DWordSwap(val); }
inline short SwapShort(short val)					{ return WordSwap(val); }
inline uint16 SwapWord(uint16 val)				{ return WordSwap(val); }
inline long SwapLong(long val)					{ return DWordSwap(val); }
inline uint32 SwapDWord(uint32 val)				{ return DWordSwap(val); }

// Pass floats by pointer for swapping to avoid truncation in the fpu
inline void BigFloat(float *pOut, const float *pIn)		{ int test = 1; (*(char *)&test == 1) ? SafeSwapFloat(pOut, pIn) : (*pOut = *pIn); }
inline void LittleFloat(float *pOut, const float *pIn)	{ int test = 1; (*(char *)&test == 1) ? (*pOut = *pIn) : SafeSwapFloat(pOut, pIn); }
inline void SwapFloat(float *pOut, const float *pIn)		{ SafeSwapFloat(pOut, pIn); }
#endif


#if _X360
FORCEINLINE unsigned long LoadLittleDWord(const unsigned long *base, unsigned int dwordIndex)
{
	return __loadwordbytereverse(dwordIndex << 2, base);
}

FORCEINLINE void StoreLittleDWord(unsigned long *base, unsigned int dwordIndex, unsigned long dword)
{
	__storewordbytereverse(dword, dwordIndex << 2, base);
}
#else
FORCEINLINE unsigned long LoadLittleDWord(const unsigned long *base, unsigned int dwordIndex)
{
	return LittleDWord(base[dwordIndex]);
}

FORCEINLINE void StoreLittleDWord(unsigned long *base, unsigned int dwordIndex, unsigned long dword)
{
	base[dwordIndex] = LittleDWord(dword);
}
#endif

#endif