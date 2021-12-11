// Copyright Sander Brattinga. All rights reserved.

use std::env;

#[cfg(windows)]
pub fn get_username() -> String {
    match env::var("USERNAME") {
        Ok(val) => val.to_string(),
        Err(_) => "Unknown".to_string(),
    }
}

#[cfg(unix)]
pub fn get_username() -> String {
    match env::var("USER") {
        Ok(val) => val.to_string(),
        Err(_) => match env::var("USERNAME") {
            Ok(val) => val.to_string(),
            Err(_) => "Unknown".to_string(),
        },
    }
}

