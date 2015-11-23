use std::mem;
use std::ptr;

extern {
    pub fn malloc(size: usize) -> *mut u8;
    pub fn free(ptr: *mut u8);
    pub fn git_pool__system_page_size() -> u32;
}

#[derive(Debug)]
#[repr(C)]
pub struct Page {
    next: Option<Box<Page>>,
    size: u32,
    avail: u32,
    // Ideally this would be aligned, but meh for now
    data: [u8; 1],
}

impl Page {
    unsafe fn alloc(&mut self, size: u32) -> Option<*mut u8> {
        if self.avail < size {
            return None;
        }

        // Offset for unallocated data
        let ptr = self.data.as_mut_ptr().offset((self.size - self.avail) as isize);
        self.avail -= size;

        Some(ptr)
    }
}

#[repr(C)]
pub struct git_pool {
    pages: Option<Box<Page>>,
    item_size: u32,
    page_size: u32
}

impl git_pool {
    unsafe fn alloc_page(&mut self, size: u32) {
        let new_page_size: u32 =
            if size <= self.page_size { self.page_size } else { size };

        let page_ptr = match
            new_page_size.checked_add(mem::size_of::<Page>() as u32)
            .map(|s| malloc(s as usize)) {
                None => { return },
                Some(ptr) => ptr as *mut Page,
            };

        // Zero out the data we're going to overwrite which avoids
        // trying to free whatever the 'next' pointer ends up having.
        ptr::write_bytes(page_ptr, 0, mem::size_of::<Page>());

        let mut page = Box::from_raw(page_ptr);
        page.size = new_page_size;
        page.avail = new_page_size;
        mem::swap(&mut page.next, &mut self.pages);
        self.pages = Some(page);
    }

    unsafe fn alloc(&mut self, size: u32) -> *mut u8 {
        if self.pages.is_none() || self.pages.as_mut().unwrap().avail < size {
            self.alloc_page(size);
        }

        match self.pages.as_mut().map(|page| page.alloc(size)) {
            None => ptr::null_mut(),
            Some(None) => panic!(),
            Some(Some(ptr)) => ptr,
        }
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
        let mut mscan = mem::replace(&mut self.pages, None);

        while let Some(mut scan) = mscan {
            mscan = mem::replace(&mut scan.next, None);
            free(Box::into_raw(scan) as *mut u8);
        }
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
        pages: None,
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
