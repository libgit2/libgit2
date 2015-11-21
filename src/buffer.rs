use std::ptr;
use std::slice;
use super::errors::{git_error_code, git_error_t};

extern {
    pub fn git__realloc(ptr: *mut u8, size: usize) -> *mut u8;
    pub fn git__free(ptr: *mut u8);
    pub fn giterr_set_oom();
    pub fn giterr_set(code: git_error_t, message: *const u8);
    pub fn realloc(ptr: *mut u8, size: usize) -> *mut u8;
    pub fn free(ptr: *mut u8);
    pub fn memmove(dst: *mut u8, src: *const u8, len: usize);
}

// This serves as a way to make sure we always have a NUL-terminated
// buffer, even when the git_buf is empty.
#[no_mangle]
pub static mut git_buf__initbuf: [u8; 1] = ['\0' as u8];

// And this one serves as a marker for a failed allocation
#[no_mangle]
pub static mut git_buf__oom: [u8; 1] = ['\0' as u8];

#[repr(C)]
pub struct git_buf {
    ptr: *mut u8,
    asize: usize,
    size: usize,
}

macro_rules! checked_add {
    ($n:expr, $toadd:expr) => (match $n.checked_add($toadd) {
        Option::Some(res) => res,
        Option::None => {
            giterr_set_oom();
            return Err(-1);
        },
    })
}

impl git_buf {
    pub unsafe fn new() -> git_buf {
        git_buf {
            ptr: git_buf__initbuf.as_mut_ptr(),
            asize: 0,
            size: 0,
        }
    }

    pub unsafe fn init(&mut self, initial_size: usize) -> Result<(), i32> {
        self.asize = 0;
        self.size  = 0;
        self.ptr   = git_buf__initbuf.as_mut_ptr();

        if initial_size != 0 {
            try!(self.grow(initial_size));
        }

        Ok(())
    }

    unsafe fn set_oom(&mut self) {
        self.ptr = git_buf__oom.as_mut_ptr();
    }

    pub fn is_oom(&self) -> bool {
        self.ptr == unsafe { git_buf__oom.as_mut_ptr() }
    }

    pub unsafe fn set(&mut self, data: *const u8, len: usize) -> Result<(), i32> {
        if len == 0 || data.is_null() {
            self.clear();
        } else {
            if !data.is_null() {
                let alloclen = checked_add!(len, 1);
                try!(self.grow(alloclen));
                memmove(self.ptr, data, len);
            }

            self.size = len;

            if self.asize > self.size {
                let mut s = slice::from_raw_parts_mut(self.ptr, self.asize);
                s[self.size] = '\0' as u8;
            }
        }

        Ok(())
    }

    unsafe fn try_grow(&mut self, target_size: usize, mark_oom: bool) -> i32 {
        if self.ptr == git_buf__oom.as_mut_ptr() {
            return -1;
        }

        if self.asize == 0 && self.size != 0 {
	    giterr_set(git_error_t::GITERR_INVALID, b"cannot grow a borrowed buffer\0".as_ptr());
            return git_error_code::GIT_EINVALID as i32;
        }

        let mut target_size = target_size;
        if target_size == 0 {
            target_size = self.size;
        }

	if target_size <= self.asize {
	    return 0;
        }

        let mut new_ptr: *mut u8;
        let mut new_size: usize;
        if self.asize == 0 {
            new_size = target_size;
            new_ptr = ptr::null_mut();
	} else {
            new_size = self.asize;
            new_ptr = self.ptr;
	}

	// Grow the buffer size by 1.5, until it's big enough to fit
	// our target size
	while new_size < target_size {
	    new_size = (new_size << 1) - (new_size >> 1);
        }

	// Round allocation up to multiple of 8
	new_size = (new_size + 7) & !7;

	if new_size < self.size {
	    if mark_oom {
		self.ptr = git_buf__oom.as_mut_ptr();
            }

	    giterr_set_oom();
	    return -1;
	}

	new_ptr = realloc(new_ptr, new_size);

	if new_ptr.is_null() {
	    if mark_oom {
		if self.ptr != git_buf__initbuf.as_mut_ptr() {
		    free(self.ptr);
                }
                giterr_set_oom();
		self.ptr = git_buf__oom.as_mut_ptr();
	    }
	    return -1;
	}

	self.asize = new_size;
	self.ptr   = new_ptr;

	// truncate the existing buffer size if necessary
	if self.size >= self.asize {
	    self.size = self.asize - 1;
        }

        let mut s = slice::from_raw_parts_mut(self.ptr, self.asize);
        s[self.size] = '\0' as u8;

        return 0;
    }

    pub unsafe fn grow(&mut self, size: usize) -> Result<(), i32> {
        let error = self.try_grow(size, true);
        if error < 0 {
            Err(error)
        } else {
            Ok(())
        }
    }

    pub unsafe fn grow_by(&mut self, additional_size: usize) -> Result<(), i32> {
        let new_size = checked_add!(self.size, additional_size);
        self.grow(new_size)
    }

    pub unsafe fn clear(&mut self) {
        self.size = 0;

        if self.ptr.is_null() {
            self.ptr = git_buf__initbuf.as_mut_ptr();
            self.asize = 0;
        }

        if self.asize > 0 {
            let mut s = slice::from_raw_parts_mut(self.ptr, self.asize);
            s[self.size] = '\0' as u8;
        }
    }

    pub unsafe fn free(&mut self) {
        if self.asize > 0 && !self.ptr.is_null() && self.ptr != git_buf__oom.as_mut_ptr() {
            free(self.ptr);
        }

        self.init(0).ok();
    }
}

#[allow(unused_must_use)]
#[no_mangle]
pub unsafe extern "C" fn git_buf_init(buf: *mut git_buf, initial_size: usize) {
    (*buf).init(initial_size);
}

#[no_mangle]
pub unsafe extern "C" fn git_buf_try_grow(buf: *mut git_buf, size: usize, mark_oom: bool) -> i32 {
    (*buf).try_grow(size, mark_oom)
}

#[no_mangle]
pub unsafe extern "C" fn git_buf_grow(buf: *mut git_buf, target_size: usize) -> i32 {
    (*buf).try_grow(target_size, true)
}

#[no_mangle]
pub unsafe extern "C" fn git_buf_grow_by(buf: *mut git_buf, additional_size: usize) -> i32 {
    match (*buf).grow_by(additional_size) {
        Ok(()) => return 0,
        Err(error) => {
            (*buf).set_oom();
            return error;
        },
    }
}

#[no_mangle]
pub unsafe extern "C" fn git_buf_sanitize(buf: *mut git_buf) {
    if (*buf).ptr.is_null() {
        assert!((*buf).size == 0 && (*buf).asize == 0);
        (*buf).ptr = git_buf__initbuf.as_mut_ptr();
    } else if (*buf).asize > (*buf).size {
            let mut s = slice::from_raw_parts_mut((*buf).ptr, (*buf).asize);
            s[(*buf).size] = '\0' as u8;
    }
}

#[no_mangle]
pub unsafe extern "C" fn git_buf_set(buf: *mut git_buf, data: *const u8, len: usize) -> i32 {
    match (*buf).set(data, len) {
        Ok(()) => return 0,
        Err(error) => return error,
    }
}

#[no_mangle]
pub unsafe extern "C" fn git_buf_clear(buf: *mut git_buf) {
    (*buf).clear()
}

#[no_mangle]
pub unsafe extern "C" fn git_buf_free(buf: *mut git_buf) {
    if buf.is_null() {
        return;
    }

    (*buf).free();
}
