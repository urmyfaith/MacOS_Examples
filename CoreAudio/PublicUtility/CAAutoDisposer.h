/*	Copyright: 	� Copyright 2005 Apple Computer, Inc. All rights reserved.

	Disclaimer:	IMPORTANT:  This Apple software is supplied to you by Apple Computer, Inc.
			("Apple") in consideration of your agreement to the following terms, and your
			use, installation, modification or redistribution of this Apple software
			constitutes acceptance of these terms.  If you do not agree with these terms,
			please do not use, install, modify or redistribute this Apple software.

			In consideration of your agreement to abide by the following terms, and subject
			to these terms, Apple grants you a personal, non-exclusive license, under Apple�s
			copyrights in this original Apple software (the "Apple Software"), to use,
			reproduce, modify and redistribute the Apple Software, with or without
			modifications, in source and/or binary forms; provided that if you redistribute
			the Apple Software in its entirety and without modifications, you must retain
			this notice and the following text and disclaimers in all such redistributions of
			the Apple Software.  Neither the name, trademarks, service marks or logos of
			Apple Computer, Inc. may be used to endorse or promote products derived from the
			Apple Software without specific prior written permission from Apple.  Except as
			expressly stated in this notice, no other rights or licenses, express or implied,
			are granted by Apple herein, including but not limited to any patent rights that
			may be infringed by your derivative works or by other works in which the Apple
			Software may be incorporated.

			The Apple Software is provided by Apple on an "AS IS" basis.  APPLE MAKES NO
			WARRANTIES, EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION THE IMPLIED
			WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY AND FITNESS FOR A PARTICULAR
			PURPOSE, REGARDING THE APPLE SOFTWARE OR ITS USE AND OPERATION ALONE OR IN
			COMBINATION WITH YOUR PRODUCTS.

			IN NO EVENT SHALL APPLE BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL OR
			CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
			GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
			ARISING IN ANY WAY OUT OF THE USE, REPRODUCTION, MODIFICATION AND/OR DISTRIBUTION
			OF THE APPLE SOFTWARE, HOWEVER CAUSED AND WHETHER UNDER THEORY OF CONTRACT, TORT
			(INCLUDING NEGLIGENCE), STRICT LIABILITY OR OTHERWISE, EVEN IF APPLE HAS BEEN
			ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
/*=============================================================================
 *  CAAutoDisposer.h
 *  CAServices
 *
 *  Created by James McCartney on 10/12/05.
 *  Copyright 2005 Apple Computer. All rights reserved.
 *
 * CAAutoFree is a simple auto_ptr-like smart pointer template for automatically freeing malloced pointers when they go out of scope.
 * std::auto_ptr handles a single C++ object. In CoreAudio we are often working with malloced arrays of "plain old data" types.
 * Using this class eliminates memory leaks even when exceptions get thrown.
 * This class has the same ownership passing semantics that auto_ptr does.
 * If you pass a CAAutoFree by value to a function you are passing ownership of the pointer to that function. (The contained pointer
 * in the caller will be NULL after passing the CAAutoFree.)
 * If you return a CAAutoFree by value from a function you are passing ownership to the caller.
 * If you pass a CAAutoFree const reference  (CAAutoFree<T> const&) to a function then the function can use the pointer, 
 * but does not take ownership and the pointer remains valid. 
 *
 *
 * CAAutoArrayDelete is for holding arrays of new'ed C++ objects. Bjarne Stroustrup says that "auto_array_ptr" isn't 
 * necessary and recommends using a vector. But vector is rather heavy when all I need is a simple smart pointer. 
 * And vectors have slightly different semantics when passing them as const references than a smart pointer does.
 *
 * CAAutoDelete is essentially the same as auto_ptr
 * 
 =============================================================================*/

#if !defined(__CAPtr_h__)
#define __CAPtr_h__

#include <stdlib.h>		// for malloc, calloc
#include <new>			// for bad_alloc

// helper class for automatic conversions
template <typename T>
struct CAPtrRef
{
	T* ptr_;

	explicit CAPtrRef(T* ptr) : ptr_(ptr) {}
};

template <typename T>
class CAAutoFree
{
private:
	T* ptr_;

public:
	
	CAAutoFree() : ptr_(0) {}
	
	explicit CAAutoFree(T* ptr) : ptr_(ptr) {}
	
	template<typename U>
	CAAutoFree(CAAutoFree<U>& that) : ptr_(that.release()) {} // take ownership

	// C++ std says: a template constructor is never a copy constructor
	CAAutoFree(CAAutoFree<T>& that) : ptr_(that.release()) {} // take ownership

	CAAutoFree(size_t n, bool clear = false) 
		// this becomes an ambiguous call if n == 0
		: ptr_(static_cast<T*>(clear ? calloc(n, sizeof(T)) : malloc(n * sizeof(T)))) 
		{
			if (!ptr_) 
				throw std::bad_alloc();
		}
	
	~CAAutoFree() { free(); }
	
	void alloc(size_t numItems, bool clear = false) 
	{
		free();
		ptr_ = static_cast<T*>(clear ? calloc(numItems, sizeof(T)) : malloc(numItems * sizeof(T)));
		if (!ptr_) 
			throw std::bad_alloc();
	}
	
	void allocBytes(size_t numBytes, bool clear = false) 
	{
		free();
		ptr_ = static_cast<T*>(clear ? calloc(1, numBytes) : malloc(numBytes));
		if (!ptr_) 
			throw std::bad_alloc();
	}
	
	template <typename U>
	CAAutoFree& operator=(CAAutoFree<U>& that) 
	{ 
		set(that.release());	// take ownership
		return *this;
	}
	
	CAAutoFree& operator=(CAAutoFree& that) 
	{ 
		set(that.release());	// take ownership
		return *this;
	}
	
	CAAutoFree& operator=(T* ptr) 
	{
		set(ptr); 
		return *this;
	}
	
	template <typename U>
	CAAutoFree& operator=(U* ptr) 
	{
		set(ptr); 
		return *this;
	}
		
	T& operator*() const { return *ptr_; }
	T* operator->() const { return ptr_; }
	
	T* operator()() const { return ptr_; }
	T* get() const { return ptr_; }
	operator T*() const { return ptr_; }

	bool operator==(CAAutoFree const& that) const { return ptr_ == that.ptr_; }
	bool operator!=(CAAutoFree const& that) const { return ptr_ != that.ptr_; }
	bool operator==(T* ptr) const { return ptr_ == ptr; }
	bool operator!=(T* ptr) const { return ptr_ != ptr; }
		
	T* release() 
	{
		// release ownership
		T* result = ptr_;
		ptr_ = 0;
		return result;
	}
	
	void set(T* ptr)
	{
		if (ptr != ptr_)
		{
			::free(ptr_);
			ptr_ = ptr;
		}
	}
	
	void free() 
	{
		set(0);
	}


	// automatic conversions to allow assignment from results of functions.
	// hard to explain. see auto_ptr implementation and/or Josuttis' STL book.
	CAAutoFree(CAPtrRef<T> ref) : ptr_(ref.ptr_) { }

	CAAutoFree& operator=(CAPtrRef<T> ref)
	{
		set(ref.ptr_);
		return *this;
	}

	template<typename U>
	operator CAPtrRef<U>()
		{ return CAPtrRef<U>(release()); }

	template<typename U>
	operator CAAutoFree<U>()
		{ return CAAutoFree<U>(release()); }
	
};


template <typename T>
class CAAutoDelete
{
private:
	T* ptr_;

public:
	CAAutoDelete() : ptr_(0) {}

	explicit CAAutoDelete(T* ptr) : ptr_(ptr) {}
	
	template<typename U>
	CAAutoDelete(CAAutoDelete<U>& that) : ptr_(that.release()) {} // take ownership

	// C++ std says: a template constructor is never a copy constructor
	CAAutoDelete(CAAutoDelete<T>& that) : ptr_(that.release()) {} // take ownership
	
	~CAAutoDelete() { free(); }
	
	template <typename U>
	CAAutoDelete& operator=(CAAutoDelete<U>& that) 
	{ 
		set(that.release());	// take ownership
		return *this;
	}
	
	CAAutoDelete& operator=(CAAutoDelete& that) 
	{ 
		set(that.release());	// take ownership
		return *this;
	}
	
	CAAutoDelete& operator=(T* ptr) 
	{
		set(ptr); 
		return *this;
	}
	
	template <typename U>
	CAAutoDelete& operator=(U* ptr) 
	{
		set(ptr); 
		return *this;
	}
		
	T& operator*() const { return *ptr_; }
	T* operator->() const { return ptr_; }
	
	T* operator()() const { return ptr_; }
	T* get() const { return ptr_; }
	operator T*() const { return ptr_; }
	
	bool operator==(CAAutoDelete const& that) const { return ptr_ == that.ptr_; }
	bool operator!=(CAAutoDelete const& that) const { return ptr_ != that.ptr_; }
	bool operator==(T* ptr) const { return ptr_ == ptr; }
	bool operator!=(T* ptr) const { return ptr_ != ptr; }
		
	T* release() 
	{
		// release ownership
		T* result = ptr_;
		ptr_ = 0;
		return result;
	}
	
	void set(T* ptr)
	{
		if (ptr != ptr_)
		{
			delete ptr_;
			ptr_ = ptr;
		}
	}
	
	void free() 
	{
		set(0);
	}


	// automatic conversions to allow assignment from results of functions.
	// hard to explain. see auto_ptr implementation and/or Josuttis' STL book.
	CAAutoDelete(CAPtrRef<T> ref) : ptr_(ref.ptr_) { }

	CAAutoDelete& operator=(CAPtrRef<T> ref)
	{
		set(ref.ptr_);
		return *this;
	}

	template<typename U>
	operator CAPtrRef<U>()
		{ return CAPtrRef<U>(release()); }

	template<typename U>
	operator CAAutoFree<U>()
		{ return CAAutoFree<U>(release()); }
	
};


template <typename T>
class CAAutoArrayDelete
{
private:
	T* ptr_;

public:
	CAAutoArrayDelete() : ptr_(0) {}

	explicit CAAutoArrayDelete(T* ptr) : ptr_(ptr) {}
	
	template<typename U>
	CAAutoArrayDelete(CAAutoArrayDelete<U>& that) : ptr_(that.release()) {} // take ownership

	// C++ std says: a template constructor is never a copy constructor
	CAAutoArrayDelete(CAAutoArrayDelete<T>& that) : ptr_(that.release()) {} // take ownership
	
	// this becomes an ambiguous call if n == 0
	CAAutoArrayDelete(size_t n) : ptr_(new T[n]) {}
	
	~CAAutoArrayDelete() { free(); }
	
	void alloc(size_t numItems) 
	{
		free();
		ptr_ = new T [numItems];
	}
	
	template <typename U>
	CAAutoArrayDelete& operator=(CAAutoArrayDelete<U>& that) 
	{ 
		set(that.release());	// take ownership
		return *this;
	}
	
	CAAutoArrayDelete& operator=(CAAutoArrayDelete& that) 
	{ 
		set(that.release());	// take ownership
		return *this;
	}
	
	CAAutoArrayDelete& operator=(T* ptr) 
	{
		set(ptr); 
		return *this;
	}
	
	template <typename U>
	CAAutoArrayDelete& operator=(U* ptr) 
	{
		set(ptr); 
		return *this;
	}
		
	T& operator*() const { return *ptr_; }
	T* operator->() const { return ptr_; }
	
	T* operator()() const { return ptr_; }
	T* get() const { return ptr_; }
	operator T*() const { return ptr_; }

	bool operator==(CAAutoArrayDelete const& that) const { return ptr_ == that.ptr_; }
	bool operator!=(CAAutoArrayDelete const& that) const { return ptr_ != that.ptr_; }
	bool operator==(T* ptr) const { return ptr_ == ptr; }
	bool operator!=(T* ptr) const { return ptr_ != ptr; }
		
	T* release() 
	{
		// release ownership
		T* result = ptr_;
		ptr_ = 0;
		return result;
	}
	
	void set(T* ptr)
	{
		if (ptr != ptr_)
		{
			delete [] ptr_;
			ptr_ = ptr;
		}
	}
	
	void free() 
	{
		set(0);
	}


	// automatic conversions to allow assignment from results of functions.
	// hard to explain. see auto_ptr implementation and/or Josuttis' STL book.
	CAAutoArrayDelete(CAPtrRef<T> ref) : ptr_(ref.ptr_) { }

	CAAutoArrayDelete& operator=(CAPtrRef<T> ref)
	{
		set(ref.ptr_);
		return *this;
	}

	template<typename U>
	operator CAPtrRef<U>()
		{ return CAPtrRef<U>(release()); }

	template<typename U>
	operator CAAutoArrayDelete<U>()
		{ return CAAutoFree<U>(release()); }
	
};





// convenience function
template <typename T>
void free(CAAutoFree<T>& p)
{
	p.free();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////

#if 0
// example program showing ownership transfer

CAAutoFree<char> source()
{
	// source allocates and returns ownership to the caller.
	const char* str = "this is a test";
	size_t size = strlen(str) + 1;
	CAAutoFree<char> captr(size, false);
	strlcpy(captr(), str, size);
	printf("source %08X %08X '%s'\n", &captr, captr(), captr());
	return captr;
}

void user(CAAutoFree<char> const& captr)
{
	// passed by const reference. user can access the pointer but does not take ownership.
	printf("user: %08X %08X '%s'\n", &captr, captr(), captr());
}

void sink(CAAutoFree<char> captr)
{
	// passed by value. sink takes ownership and frees the pointer on return.
	printf("sink: %08X %08X '%s'\n", &captr, captr(), captr());
}


int main (int argc, char * const argv[]) 
{

	CAAutoFree<char> captr(source());
	printf("main captr A %08X %08X\n", &captr, captr());
	user(captr);
	sink(captr);
	printf("main captr B %08X %08X\n", &captr, captr());
    return 0;
}
#endif

#endif
