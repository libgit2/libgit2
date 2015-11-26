use std::ptr;
use std::slice;
use std::cmp::Ordering::Equal;
use super::errors::git_error_t;

extern {
    pub fn strlen(s: *const u8) -> usize;
    pub fn giterr_set(klass: git_error_t, fmt: *const u8, arg0: *const u8);
}

static FROM_HEX: [i32; 256] = [
-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, /* 00 */
-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, /* 10 */
-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, /* 20 */
 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, -1, -1, -1, -1, -1, -1, /* 30 */
-1, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1, /* 40 */
-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, /* 50 */
-1, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1, /* 60 */
-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, /* 70 */
-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, /* 80 */
-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, /* 90 */
-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, /* a0 */
-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, /* b0 */
-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, /* c0 */
-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, /* d0 */
-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, /* e0 */
-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, /* f0 */
];

#[inline]
fn fromhex(h: u8) -> i32 {
    FROM_HEX[h as usize]
}

//static TO_HEX: [u8; 16] = [ '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'];
static TO_HEX: &'static [u8; 16] = b"0123456789abcdef";

#[inline]
unsafe fn fmt_one(mut pstr: *mut u8, val: i32) -> *mut u8 {
    *pstr = TO_HEX[(val >> 4) as usize];
    pstr = pstr.offset(1);
    *pstr = TO_HEX[(val & 0xf) as usize];
    pstr.offset(1)
}

const GIT_OID_RAWSZ: usize = 20;
const GIT_OID_HEXSZ: usize = 40;

#[inline]
fn oid_error_invalid(msg: &[u8]) -> Result<Oid, i32> {
    unsafe {
        giterr_set(git_error_t::GITERR_INVALID, b"Unable to parse OID - %s" as *const _, msg.as_ptr() as *const _);
    }
    Err(-1)
}

// git_oid
#[derive(Default)]
#[repr(C)]
pub struct Oid {
    id: [u8; GIT_OID_RAWSZ],
}

impl Oid {

    pub fn from_slice(input: &[u8]) -> Result<Oid, i32> {
        if input.len() == 0 {
            return oid_error_invalid(b"too short");
        }

        if input.len() > GIT_OID_HEXSZ {
            return oid_error_invalid(b"too long");
        }

        let mut oid: Oid = Default::default();
        for (p, c) in input.iter().enumerate() {
            let v = fromhex(*c);
            if v < 0 {
                return oid_error_invalid(b"contains invalid characters");
            }

            // Apply alternatingly to the higher or lower nibble
            let shift: u8 = ((!p & 0x1) * 4) as u8;
            oid.id[p/2] |= (v as u8) << shift;
        }

        Ok(oid)
    }

    unsafe fn strcmp(&self, mut pstr: *const u8) -> i32 {
        for a in &self.id {
            if *pstr == 0 {
                break;
            }

            let mut hexval = fromhex(*pstr);
            if hexval < 0 {
                return -1;
            }
            pstr = pstr.offset(1);

            let mut strval = (hexval << 4) as u8;

            if *pstr != 0 {
                hexval = fromhex(*pstr);
                pstr = pstr.offset(1);

                if hexval < 0 {
                    return -1;
                }

                strval |= hexval as u8;
            }

            if *a != strval {
                return (*a as i32) - (strval as i32);
            }
        }

        0
    }

    fn iszero(&self) -> bool {
        self.id.iter().all(|a| *a == 0)
    }

    unsafe fn nfmt(&self, mut pstr: *mut u8, mut n: usize) {
        if n > GIT_OID_HEXSZ {
            ptr::write_bytes(pstr.offset(GIT_OID_HEXSZ as isize), 0, n - GIT_OID_HEXSZ);
            n = GIT_OID_HEXSZ;
        }

        for i in 0..n/2 {
            pstr = fmt_one(pstr, self.id[i] as i32);
        }

        if n & 1 != 0 {
            *pstr = TO_HEX[(self.id[n/2] >> 4) as usize];
        }
    }

    fn parse_header(buffer: &[u8], header: &[u8]) -> Result<Oid, i32> {
        if header.len() + GIT_OID_HEXSZ + 1 > buffer.len() {
            return Err(-1);
        }

        if header.cmp(&buffer[..header.len()]) != Equal {
            return Err(-1);
        }

        if buffer[header.len() + GIT_OID_HEXSZ] != b'\n' {
            return Err(-1);
        }

        let oid = try!(Oid::from_slice(&buffer[header.len()..header.len() + GIT_OID_HEXSZ]));

        Ok(oid)
    }
}

#[no_mangle]
pub unsafe extern "C" fn git_oid_fromraw(oid: &mut Oid, raw: *const u8) {
    ptr::copy_nonoverlapping(raw, oid.id.as_mut_ptr(), GIT_OID_RAWSZ);
}

#[no_mangle]
pub unsafe extern "C" fn git_oid_cpy(out: &mut Oid, src: &Oid) {
    ptr::copy_nonoverlapping(src.id.as_ptr(), out.id.as_mut_ptr(), GIT_OID_RAWSZ);
}

#[no_mangle]
pub unsafe extern "C" fn git_oid_strcmp(oid: &Oid, pstr: *const u8) -> i32 {
    oid.strcmp(pstr)
}

#[no_mangle]
pub unsafe extern "C" fn git_oid_iszero(oid: &Oid) -> i32 {
    oid.iszero() as i32
}

#[no_mangle]
pub unsafe extern "C" fn git_oid_nfmt(pstr: *mut u8, n: usize, oid: *const Oid) {
    if oid.is_null() {
        ptr::write_bytes(pstr, 0, n);
    } else {
        (*oid).nfmt(pstr, n);
    }
}

#[no_mangle]
pub unsafe extern "C" fn git_oid_fromstrn(out: *mut Oid, pstr: *const u8, length: usize) -> i32 {
    assert!(!out.is_null() && !pstr.is_null());

    ptr::write_bytes(out, 0, 1);
    let sl = slice::from_raw_parts(pstr, length);

    match Oid::from_slice(sl) {
        Err(error) => error,
        Ok(ref oid) => {
            ptr::copy_nonoverlapping(oid, out, 1);
            0
        },
    }

}

#[allow(non_snake_case)]
#[no_mangle]
pub unsafe extern "C" fn git_oid__parse(out: *mut Oid, buffer_out: *mut *const u8, buffer_end: *const u8, header: *const u8) -> i32 {
    let header_len = strlen(header);
    let header_slice = slice::from_raw_parts(header, header_len);

    let buffer = *buffer_out;
    let buffer_len = (buffer_end as usize) - (buffer as usize);
    let buffer_slice = slice::from_raw_parts(buffer, buffer_len);

    match Oid::parse_header(&buffer_slice, &header_slice) {
        Err(error) => error,
        Ok(ref oid) => {
            ptr::copy_nonoverlapping(oid, out, 1);
            ptr::replace(buffer_out, buffer.offset((header_len + GIT_OID_HEXSZ + 1) as isize));
            0
        }
    }
}
