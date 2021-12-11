// Copyright Sander Brattinga. All rights reserved.

use crate::error::BuildMonitorError;

use json;
use serde::{Deserialize, Serialize};
use std::collections::hash_map::DefaultHasher;
use std::fmt;
use std::hash::{Hash, Hasher};
use std::sync::Arc;

#[derive(Clone, Deserialize, Hash, PartialEq, Serialize)]
pub enum ProjectStatus {
    Success,
    Unstable,
    Failed,
    NotBuilt,
    Aborted,
    Disabled,
    Unknown,
}

impl fmt::Display for ProjectStatus {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match *self {
            ProjectStatus::Success => write!(f, "Success"),
            ProjectStatus::Unstable => write!(f, "Unstable"),
            ProjectStatus::Failed => write!(f, "Failed"),
            ProjectStatus::NotBuilt => write!(f, "NotBuilt"),
            ProjectStatus::Aborted => write!(f, "Aborted"),
            ProjectStatus::Disabled => write!(f, "Disabled"),
            ProjectStatus::Unknown => write!(f, "Unknown"),
        }
    }
}

#[derive(Clone, Deserialize, Serialize)]
pub struct Project {
    id: u64,
    name: String,
    folder: String,
    url: String,
    status: ProjectStatus,
    is_building: bool,
    last_successful_build_time: u64,
    duration: u64,
    estimated_duration: u64,
    timestamp: u64,
    culprits: Vec<String>,
    volunteer: String,
}

#[derive(Deserialize, Serialize)]
pub struct Volunteer {
    pub id: u64,
    pub volunteer: String,
}

impl Project {
    pub fn new(folder: &str, url: &str) -> Project {
        let mut hasher = DefaultHasher::new();
        url.hash(&mut hasher);

        Project {
            id: hasher.finish(),
            name: String::new(),
            folder: folder.to_string(),
            url: url.to_string(),
            status: ProjectStatus::Unknown,
            is_building: false,
            last_successful_build_time: 0,
            duration: 0,
            estimated_duration: 0,
            timestamp: 0,
            culprits: Vec::new(),
            volunteer: String::new(),
        }
    }

    pub async fn refresh_status(self: &mut Project, client: Arc<reqwest::blocking::Client>) -> Result<(), BuildMonitorError> {
        self.refresh_project(client.clone()).await?;

        if self.status() != ProjectStatus::Disabled {
            self.refresh_last_build(client.clone()).await?;
            self.refresh_last_successful_build(client).await?;
        }

        Ok(())
    }

    pub fn id(self: &Project) -> u64 {
        self.id
    }

    pub fn name(self: &Project) -> &str {
        &self.name
    }

    pub fn folder(self: &Project) -> &str {
        &self.folder
    }

    pub fn url(self: &Project) -> &str {
        &self.url
    }

    pub fn status(self: &Project) -> ProjectStatus {
        self.status.clone()
    }

    pub fn is_building(self: &Project) -> bool {
        self.is_building
    }

    pub fn last_successful_build_time(self: &Project) -> u64 {
        self.last_successful_build_time
    }

    pub fn duration(self: &Project) -> u64 {
        self.duration
    }

    pub fn estimated_duration(self: &Project) -> u64 {
        self.estimated_duration
    }

    pub fn timestamp(self: &Project) -> u64 {
        self.timestamp
    }

    pub fn culprits(self: &Project) -> &Vec<String> {
        &self.culprits
    }

    pub fn volunteer(self: &Project) -> &str {
        &self.volunteer
    }

    pub fn set_volunteer(self: &mut Project, volunteer: &str) {
        self.volunteer = volunteer.to_string();
    }

    async fn get_json(self: &Project, client: Arc<reqwest::blocking::Client>, url: &str) -> Result<json::JsonValue, BuildMonitorError> {
        let api_url = url.to_string() + "/api/json";
        let response = client.get(&api_url).send()?;
        if response.status() != reqwest::StatusCode::OK {
            Err(BuildMonitorError::PageNotFoundError())
        } else {
            let text = response.text()?;
            Ok(json::parse(&text)?)
        }
    }

    async fn refresh_project(self: &mut Project, client: Arc<reqwest::blocking::Client>) -> Result<(), BuildMonitorError> {
        let json_result = self.get_json(client, &self.url).await;
        match json_result {
            Ok(json) => {
                self.name = json["name"]
                    .as_str()
                    .ok_or_else(|| BuildMonitorError::FieldError {})
                    .unwrap()
                    .to_string();
                let buildable = json["buildable"]
                    .as_bool()
                    .ok_or_else(|| BuildMonitorError::FieldError {})
                    .unwrap();
                if !buildable {
                    self.status = ProjectStatus::Disabled;
                    self.is_building = false;
                }
                Ok(())
            }
            Err(error) => match error {
                BuildMonitorError::PageNotFoundError() => {
                    self.name = self.url.clone();
                    self.status = ProjectStatus::NotBuilt;
                    self.is_building = false;
                    Ok(())
                }
                _ => Err(error),
            },
        }
    }

    async fn refresh_last_build(self: &mut Project, client: Arc<reqwest::blocking::Client>) -> Result<(), BuildMonitorError> {
        let last_built_url = self.url.clone() + "/lastBuild";
        let json_result = self.get_json(client.clone(), &last_built_url).await;
        match json_result {
            Ok(json) => {
                self.is_building = json["building"]
                    .as_bool()
                    .ok_or_else(|| BuildMonitorError::FieldError {})
                    .unwrap();
                if self.is_building {
                    self.refresh_last_completed_build(client).await?;
                }
                else {
                    let result = json["result"]
                        .as_str()
                        .ok_or_else(|| BuildMonitorError::FieldError {})
                        .unwrap_or_else(|_| "");
                    if result == "SUCCESS" {
                        self.status = ProjectStatus::Success;
                    } else if result == "UNSTABLE" {
                        self.status = ProjectStatus::Unstable;
                    } else if result == "FAILURE" {
                        self.status = ProjectStatus::Failed;
                    } else if result == "ABORTED" {
                        self.status = ProjectStatus::Aborted;
                    } else {
                        self.status = ProjectStatus::Unknown;
                    }
                }

                if json.has_key("duration") {
                    self.duration = json["duration"]
                        .as_u64()
                        .ok_or_else(|| BuildMonitorError::FieldError {})
                        .unwrap();
                    self.estimated_duration = 0;
                }

                if json.has_key("estimatedDuration") {
                    let estimated_duration = json["estimatedDuration"]
                        .as_i64()
                        .ok_or_else(|| BuildMonitorError::FieldError {})
                        .unwrap();
                    if estimated_duration >= 0 {
                        self.estimated_duration = estimated_duration as u64;
                    }
                }

                if json.has_key("timestamp") {
                    self.timestamp = json["timestamp"]
                        .as_u64()
                        .ok_or_else(|| BuildMonitorError::FieldError {})
                        .unwrap();
                }

                for culprit in json["culprits"].members() {
                    self.culprits.push(
                        culprit["fullName"]
                            .as_str()
                            .ok_or_else(|| BuildMonitorError::FieldError {})
                            .unwrap()
                            .into(),
                    );
                }

                Ok(())
            }
            Err(error) => match error {
                BuildMonitorError::PageNotFoundError() => {
                    self.status = ProjectStatus::NotBuilt;
                    self.is_building = false;
                    Ok(())
                }
                _ => Err(error),
            },
        }
    }

    async fn refresh_last_successful_build(self: &mut Project, client: Arc<reqwest::blocking::Client>) -> Result<(), BuildMonitorError> {
        let last_successful_build_url = self.url.clone() + "/lastSuccessfulBuild";
        let json_result = self.get_json(client, &last_successful_build_url).await;
        match json_result {
            Ok(json) => {
                self.last_successful_build_time = json["timestamp"]
                    .as_u64()
                    .ok_or_else(|| BuildMonitorError::FieldError {})
                    .unwrap();
                Ok(())
            }
            Err(error) => match error {
                BuildMonitorError::PageNotFoundError() => Ok(()),
                _ => Err(error),
            },
        }
    }

    async fn refresh_last_completed_build(self: &mut Project, client: Arc<reqwest::blocking::Client>) -> Result<(), BuildMonitorError> {
        let last_completed_build_url = self.url.clone() + "/lastCompletedBuild";
        let json_result = self.get_json(client, &last_completed_build_url).await;
        match json_result {
            Ok(json) => {
                let result = json["result"]
                    .as_str()
                    .ok_or_else(|| BuildMonitorError::FieldError {})
                    .unwrap_or_else(|_| "");
                if result == "SUCCESS" {
                    self.status = ProjectStatus::Success;
                } else if result == "UNSTABLE" {
                    self.status = ProjectStatus::Unstable;
                } else if result == "FAILURE" {
                    self.status = ProjectStatus::Failed;
                } else if result == "ABORTED" {
                    self.status = ProjectStatus::Aborted;
                } else {
                    self.status = ProjectStatus::Unknown;
                }
                Ok(())
            }
            Err(error) => match error {
                BuildMonitorError::PageNotFoundError() => Ok(()),
                _ => Err(error),
            },
        }
    }
}

impl fmt::Display for Project {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(
            f,
            concat!("[{}] {}: {}{}\n",
                "\tStatus: {}, Building: {}, Volunteer: {} Culprits: {}\n",
                "\tLast Successful Build: {}, Duration: {}, Estimated Duration: {}, Timestamp: {}"),
            self.id(),
            &self.url(),
            &self.folder(),
            &self.name(),
            self.status(),
            self.is_building(),
            self.volunteer(),
            self.culprits().join(", "),
            self.last_successful_build_time(),
            self.duration(),
            self.estimated_duration(),
            self.timestamp()
        )
    }
}

impl Hash for Project {
    fn hash<H: Hasher>(&self, state: &mut H) {
        self.id.hash(state);
        self.status.hash(state);
        self.is_building.hash(state);
        self.volunteer.hash(state);
        self.last_successful_build_time.hash(state);
        self.duration.hash(state);
        self.culprits.hash(state);
    }
}

impl PartialEq for Project {
    fn eq(&self, other: &Project) -> bool {
        self.id == other.id
            && self.status == other.status
            && self.is_building == other.is_building
            && self.volunteer == other.volunteer
    }
}
