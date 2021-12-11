// Copyright Sander Brattinga. All rights reserved.

use std::fmt;

#[derive(Debug)]
pub enum BuildMonitorError {
    NonExistantHandle(),
    RequestError(reqwest::Error),
    JsonError(json::JsonError),
    PageNotFoundError(),
    FieldError(),
    MutexError(),
}

impl From<reqwest::Error> for BuildMonitorError {
    fn from(error: reqwest::Error) -> Self {
        BuildMonitorError::RequestError(error)
    }
}

impl From<json::JsonError> for BuildMonitorError {
    fn from(error: json::JsonError) -> Self {
        BuildMonitorError::JsonError(error)
    }
}

impl fmt::Display for BuildMonitorError {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match &*self {
            BuildMonitorError::NonExistantHandle() => write!(f, "NonExistantHandle"),
            BuildMonitorError::RequestError(request_error) => {
                write!(f, "RequestError: {}", request_error)
            }
            BuildMonitorError::JsonError(json_error) => write!(f, "JsonError: {}", json_error),
            BuildMonitorError::PageNotFoundError() => write!(f, "PageNotFoundError"),
            BuildMonitorError::FieldError() => write!(f, "FieldError"),
            BuildMonitorError::MutexError() => write!(f, "MutexError"),
        }
    }
}
