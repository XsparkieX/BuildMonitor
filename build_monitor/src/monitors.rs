// Copyright Sander Brattinga. All rights reserved.

use crate::error::BuildMonitorError;
use crate::monitor::Monitor;

use std::cell::RefCell;
use std::fmt;
use std::rc::Rc;
use std::sync::Mutex;

pub static mut MONITORS: Monitors = Monitors::new();

pub struct Monitors {
    monitors: Option<Mutex<Vec<Rc<RefCell<Monitor>>>>>,
}

impl Monitors {
    // NOTE (sparkie): The constructor of Vec::new is not const and we want to construct
    //                 this object as a static in lib.rs, which contains our FFI.
    pub const fn new() -> Monitors {
        Monitors { monitors: None }
    }

    pub fn create(self: &mut Monitors, server: &str) -> u8 {
        if self.monitors.is_none() {
            self.monitors = Some(Mutex::new(Vec::new()));
        }

        let mut new_handle: u8 = 1;
        let mutex = self.monitors.as_ref().unwrap();
        let mut monitors_unwrapped = mutex.lock().unwrap();
        while monitors_unwrapped
            .iter()
            .any(|x| x.borrow().get_handle() == new_handle)
        {
            new_handle += 1;
        }
        monitors_unwrapped.push(Rc::new(RefCell::new(Monitor::new(new_handle, server))));
        new_handle
    }

    pub fn destroy(self: &mut Monitors, handle_to_remove: u8) {
        if self.monitors.is_none() {
            return;
        }

        let mutex = self.monitors.as_ref().unwrap();
        let mut monitors_unwrapped = mutex.lock().unwrap();
        monitors_unwrapped.retain(|x| x.borrow().get_handle() != handle_to_remove);
    }

    pub fn get(self: &mut Monitors, handle: u8) -> Result<Rc<RefCell<Monitor>>, BuildMonitorError> {
        let mutex = self
            .monitors
            .as_ref()
            .ok_or_else(|| BuildMonitorError::MutexError {});
        let mut monitors_unwrapped = mutex.unwrap().lock().unwrap();
        for monitor in monitors_unwrapped.iter_mut() {
            if monitor.borrow().get_handle() == handle {
                return Ok(monitor.clone());
            }
        }
        Err(BuildMonitorError::NonExistantHandle())
    }
}

impl fmt::Display for Monitors {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let mut monitors: String = String::new();

        let mutex = self.monitors.as_ref();
        let monitors_unwrapped = mutex.unwrap().lock().unwrap();
        for (index, monitor) in monitors_unwrapped.iter().enumerate() {
            monitors += &format!("Monitor {}:\n{}\n", index, monitor.borrow());
        }
        write!(f, "{}", monitors)
    }
}
