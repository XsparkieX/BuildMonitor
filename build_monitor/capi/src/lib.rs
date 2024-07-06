// Copyright Sander Brattinga. All rights reserved.

use libc::*;
use build_monitor::monitors::MONITORS;
use build_monitor::project;
use std::ffi::CStr;
use std::ffi::CString;
use std::os::raw::c_char;
use std::slice;

#[repr(C)]
pub enum ProjectStatusFFI {
    Success,
    Unstable,
    Failed,
    NotBuilt,
    Aborted,
    Disabled,
    Unknown,
}

#[repr(C)]
pub struct ProjectsFFI {
    id: u64,
    folder_name: *mut c_char,
    project_name: *mut c_char,
    url: *mut c_char,
    status: ProjectStatusFFI,
    is_building: bool,
    last_successful_build_time: u64,
    duration: u64,
    estimated_duration: u64,
    timestamp: u64,
    culprits: *mut *mut c_char,
    culprits_num: u64,
    volunteer: *mut c_char,
}

#[cfg(windows)]
pub fn output_debug_string(s: &str) {
    let len = s.encode_utf16().count() + 1;
    let mut utf16: Vec<u16> = Vec::with_capacity(len);
    utf16.extend(s.encode_utf16());
    utf16.push(0);
    unsafe {
        OutputDebugStringW(&utf16[0]);
    }
}

#[cfg(windows)]
extern "stdcall" {
    fn OutputDebugStringW(chars: *const u16);
}

#[no_mangle]
pub extern "C" fn bm_create(url_cstr: *const c_char) -> u8 {
    unsafe {
        match CStr::from_ptr(url_cstr).to_str() {
            Ok(val) => MONITORS.create(val),
            Err(e) => {
                eprintln!("{}", e);
                0
            }
        }
    }
}

#[no_mangle]
pub extern "C" fn bm_destroy(handle: u8) {
    unsafe {
        MONITORS.destroy(handle);
    }
}

#[no_mangle]
pub extern "C" fn bm_refresh_projects(handle: u8) -> i32 {
    unsafe {
        match MONITORS.get(handle) {
            Ok(monitor_cell) => {
                let mut monitor = monitor_cell.borrow_mut();
                match futures::executor::block_on(monitor.refresh_projects()) {
                    Ok(new_projects) => {
                        if new_projects {
                            1
                        }
                        else {
                            0
                        }
                    },
                    Err(e) => {
                        eprintln!("{}", e);
                        -1
                    }
                }
            },
            Err(e) => {
                eprintln!("{}", e);
                -1
            }
        }
    }
}

#[no_mangle]
pub extern "C" fn bm_start_server(handle: u8, address_cstr: *const c_char, multicast: bool) {
    unsafe {
        let address = match CStr::from_ptr(address_cstr).to_str() {
            Ok(val) => val,
            Err(e) => panic!("{}", e),
        };

        match MONITORS.get(handle) {
            Ok(monitor_cell) => {
                let mut monitor = monitor_cell.borrow_mut();
                monitor.start_server(address, multicast);
            },
            Err(e) => {
                eprintln!("{}", e);
            }
        }
    }
}

#[no_mangle]
pub extern "C" fn bm_stop_server(handle: u8) {
    unsafe {
        match MONITORS.get(handle) {
            Ok(monitor_cell) => {
                let mut monitor = monitor_cell.borrow_mut();
                monitor.stop_server();
            },
            Err(e) => {
                eprintln!("{}", e);
            }
        }
    }
}

#[no_mangle]
pub extern "C" fn bm_start_client(handle: u8, server_address_cstr: *const c_char, multicast: bool) {
    unsafe {
        let server_address = match CStr::from_ptr(server_address_cstr).to_str() {
            Ok(val) => val,
            Err(e) => panic!("{}", e),
        };

        match MONITORS.get(handle) {
            Ok(monitor_cell) => {
                let mut monitor = monitor_cell.borrow_mut();
                monitor.start_client(server_address, "0.0.0.0:0", multicast);
            },
            Err(e) => {
                eprintln!("{}", e);
            }
        }
    }
}

#[no_mangle]
pub extern "C" fn bm_stop_client(handle: u8) {
    unsafe {
        match MONITORS.get(handle) {
            Ok(monitor_cell) => {
                let mut monitor = monitor_cell.borrow_mut();
                monitor.stop_client();
            },
            Err(e) => {
                eprintln!("{}", e);
            }
        }
    }
}

#[no_mangle]
pub extern "C" fn bm_get_num_projects(handle: u8) -> u32 {
    unsafe {
        match MONITORS.get(handle) {
            Ok(monitor_cell) => {
                let monitor = monitor_cell.borrow_mut();
                let projects_unlocked = monitor.get_projects().read().unwrap();
                projects_unlocked.len() as u32
            },
            Err(e) => {
                eprintln!("{}", e);
                0
            }
        }
    }
}

#[no_mangle]
pub extern "C" fn bm_acquire_projects(
    handle: u8,
    array_size: u32,
    array_ptr: *mut ProjectsFFI,
) -> bool {
    let monitor_cell;
    unsafe {
        match MONITORS.get(handle) {
            Ok(cell) => monitor_cell = cell,
            Err(e) => {
                eprintln!("Unable to find monitor. {}", e);
                return false;
            }
        }
    }
    let monitor = monitor_cell.borrow();
    let projects = monitor.get_projects().read().unwrap();

    if projects.len() != array_size as usize {
        return false;
    }

    let array_slice = unsafe { slice::from_raw_parts_mut(array_ptr, array_size as usize) };
    for (index, entry) in array_slice.iter_mut().enumerate() {
        entry.id = projects[index].id();
        let project_name = CString::new(projects[index].name()).unwrap();
        unsafe {
            let ptr = project_name.as_ptr();
            let len = strlen(ptr) + 1;
            (*entry).project_name = std::mem::transmute(malloc(len));
            memcpy(
                (*entry).project_name as *mut c_void,
                ptr as *const c_void,
                len,
            );
        }
        let folder_name = CString::new(projects[index].folder()).unwrap();
        unsafe {
            let ptr = folder_name.as_ptr();
            let len = strlen(ptr) + 1;
            (*entry).folder_name = std::mem::transmute(malloc(len));
            memcpy(
                (*entry).folder_name as *mut c_void,
                ptr as *const c_void,
                len,
            );
        }
        let url = CString::new(projects[index].url()).unwrap();
        unsafe {
            let ptr = url.as_ptr();
            let len = strlen(ptr) + 1;
            (*entry).url = std::mem::transmute(malloc(len));
            memcpy((*entry).url as *mut c_void, ptr as *const c_void, len);
        }
        entry.status = match projects[index].status() {
            project::ProjectStatus::Success => ProjectStatusFFI::Success,
            project::ProjectStatus::Unstable => ProjectStatusFFI::Unstable,
            project::ProjectStatus::Failed => ProjectStatusFFI::Failed,
            project::ProjectStatus::NotBuilt => ProjectStatusFFI::NotBuilt,
            project::ProjectStatus::Aborted => ProjectStatusFFI::Aborted,
            project::ProjectStatus::Disabled => ProjectStatusFFI::Disabled,
            project::ProjectStatus::Unknown => ProjectStatusFFI::Unknown,
        };
        entry.is_building = projects[index].is_building();
        entry.last_successful_build_time = projects[index].last_successful_build_time();
        entry.duration = projects[index].duration();
        entry.estimated_duration = projects[index].estimated_duration();
        entry.timestamp = projects[index].timestamp();

        let mut culprits_vec : Vec<*mut c_char> = Vec::new();
        for culprit in projects[index].culprits().iter() {
            let culprit_cstr = CString::new(culprit.as_str()).unwrap();
            unsafe {
                let ptr = culprit_cstr.as_ptr();
                let len = strlen(ptr) + 1;
                let vec_entry = std::mem::transmute(malloc(len));
                memcpy(vec_entry as *mut c_void, ptr as *const c_void, len);
                culprits_vec.push(vec_entry);
            }
        }
        culprits_vec.shrink_to_fit();
        assert!(culprits_vec.len() == culprits_vec.capacity());
        entry.culprits = culprits_vec.as_mut_ptr();
        entry.culprits_num = culprits_vec.len() as u64;
        std::mem::forget(culprits_vec);

        let volunteer = CString::new(projects[index].volunteer()).unwrap();
        unsafe {
            let ptr = volunteer.as_ptr();
            let len = strlen(ptr) + 1;
            (*entry).volunteer = std::mem::transmute(malloc(len));
            memcpy((*entry).volunteer as *mut c_void, ptr as *const c_void, len);
        }
    }

    true
}

#[no_mangle]
pub extern "C" fn bm_release_projects(
    array_size: u32,
    array_ptr: *mut ProjectsFFI,
) {
    let array_slice = unsafe { slice::from_raw_parts_mut(array_ptr, array_size as usize) };
    for entry in array_slice.iter_mut() {
        unsafe {
            free(std::mem::transmute(entry.project_name));
            free(std::mem::transmute(entry.folder_name));
            free(std::mem::transmute(entry.url));
            let culprits_num = entry.culprits_num as usize;
            let culprits_vec = Vec::from_raw_parts(entry.culprits, culprits_num, culprits_num);
            for &culprit in culprits_vec.iter() {
                free(std::mem::transmute(culprit));
            }
            drop(culprits_vec);
            free(std::mem::transmute(entry.volunteer));
        }
    }
}

#[no_mangle]
pub extern "C" fn bm_has_projects(handle: u8) -> bool {
    unsafe {
        match MONITORS.get(handle) {
            Ok(monitor_cell) => {
                let monitor = monitor_cell.borrow_mut();
                let projects_unlocked = monitor.get_projects().read().unwrap();
                projects_unlocked.len() > 0
            },
            Err(e) => {
                eprintln!("{}", e);
                false
            }
        }
    }
}

#[no_mangle]
pub extern "C" fn bm_set_volunteer(handle: u8, project_id: u64) {
    unsafe {
        match MONITORS.get(handle) {
            Ok(monitor_cell) => {
                let monitor = monitor_cell.borrow_mut();
                monitor.set_volunteering(project_id);
            },
            Err(e) => {
                eprintln!("{}", e);
            }
        }
    }
}

