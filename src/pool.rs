use std::mem;
use std::ptr;

extern {
    pub fn malloc(size: usize) -> *mut u8;
    pub fn free(ptr: *mut u8);
    pub fn git_pool__system_page_size() -> u32;
}

#[repr(C)]
pub struct git_pool_page {
    next: Option<Box<git_pool_page>>,
    size: u32,
    avail: u32,
    // Ideally this would be aligned, but meh for now
    data: [u8; 1],
}

#[repr(C)]
pub struct git_pool {
    // This should be *mut git_pool_page, but for whatever reason that
    // takes up twice the size of *mut u8 and thus makes it not be the
    // size of the C struct.
    pages: *mut u8,
    item_size: u32,
    page_size: u32
}

impl git_pool {
    unsafe fn alloc_page(&mut self, size: u32) -> *mut u8 {
        let new_page_size: u32 =
            if size <= self.page_size { self.page_size } else { size };

        let template = git_pool_page {
            next: Some(Box::from_raw(self.pages as *mut git_pool_page)),
            size: new_page_size,
            avail: new_page_size - size,
            data: [0; 1],
        };

        let page_ptr = new_page_size.checked_add(mem::size_of::<git_pool_page>() as u32)
            .map(|s| malloc(s as usize));

        if page_ptr.is_none() {
            return ptr::null_mut();
        }

        let page = page_ptr.unwrap() as *mut git_pool_page;
        ptr::write(page, template);
        self.pages = mem::transmute(page);


        (*page).data.as_mut_ptr()
    }

    unsafe fn alloc(&mut self, size: u32) -> *mut u8 {
        let page = self.pages as *mut git_pool_page;

        if page.is_null() || (*page).avail < size {
            return self.alloc_page(size);
        }

        // Offset for unallocated data
        let ptr = (*page).data.as_mut_ptr().offset(((*page).size - (*page).avail) as isize);
        (*page).avail -= size;

        ptr
    }

    unsafe fn alloc_size(&mut self, count: u32) -> u32 {
        let align = (mem::size_of::<*mut u8>() - 1) as u32;

        if self.item_size > 1 {
            let item_size = (self.item_size + align) & !align;
            return item_size * count;
        }

        return (count + align) & !(align as u32);

    }

    unsafe fn clear(&mut self) {
        let scan_ptr = self.pages as *mut git_pool_page;
        let mut mscan = if scan_ptr.is_null() { None } else { Some(Box::from_raw(scan_ptr)) };

        while let Some(mut scan) = mscan {
            let mut next = None;
            mem::swap(&mut next, &mut scan.next);
            free(Box::into_raw(scan) as *mut u8);
            mscan = next;
        }

        self.pages = ptr::null_mut();
    }
}

#[no_mangle]
pub unsafe extern "C" fn git_pool_malloc(pool: *mut git_pool, items: u32) -> *mut u8 {
    (*pool).alloc((*pool).alloc_size(items))
}

#[no_mangle]
pub unsafe extern "C" fn git_pool_mallocz(pool: *mut git_pool, items: u32) -> *mut u8 {
    let size = (*pool).alloc_size(items);
    let ptr = (*pool).alloc(size);
    if !ptr.is_null() {
        ptr::write_bytes(ptr, 0, size as usize);
    }

    ptr
}

#[no_mangle]
pub unsafe extern "C" fn git_pool_init(pool: *mut git_pool, item_size: u32)
{
    assert!(!pool.is_null());
    assert!(item_size >= 1);

    let p = git_pool {
        pages: ptr::null_mut(),
        item_size: item_size,
        page_size: git_pool__system_page_size(),
    };

    ptr::write(pool, p);
}

#[no_mangle]
pub unsafe extern "C" fn git_pool_clear(pool: *mut git_pool)
{
    (*pool).clear();
}
