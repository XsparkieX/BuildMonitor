// Copyright Sander Brattinga. All rights reserved.

use crate::error::BuildMonitorError;
use crate::monitor_client::MonitorClient;
use crate::monitor_server::MonitorServer;
use crate::project::{Project, ProjectStatus};
use crate::utils::get_username;

use json;
use serde::{Deserialize, Serialize};
use std::collections::hash_map::DefaultHasher;
use std::collections::VecDeque;
use std::fmt;
use std::hash::{Hash, Hasher};
use std::sync::Arc;
use std::sync::RwLock;

#[derive(Deserialize, PartialEq, Serialize)]
pub enum MessageType {
    Invalid,
    Beacon,
    ProjectUpdate,
    NewConnection,
    VolunteerAdded,
}

impl std::fmt::Display for MessageType {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match &*self {
            MessageType::Invalid => write!(f, "Invalid"),
            MessageType::Beacon => write!(f, "Beacon"),
            MessageType::ProjectUpdate => write!(f, "ProjectUpdate"),
            MessageType::NewConnection => write!(f, "NewConnection"),
            MessageType::VolunteerAdded => write!(f, "VolunteerAdded"),
        }
    }
}

#[derive(Deserialize, PartialEq, Serialize)]
pub struct Header {
    pub version: u32,
    pub msg_size: u32,
    pub msg_type: MessageType,
}

impl Header {
    pub fn new() -> Header {
        Header {
            version: 1,
            msg_size: 0,
            msg_type: MessageType::Invalid,
        }
    }
}

pub struct Monitor {
    handle: u8,
    jenkins_server: String,
    version: u32,
    projects: Arc<RwLock<Vec<Project>>>,
    projects_hash: u64,
    server: Option<MonitorServer>,
    client: Option<MonitorClient>,
}

struct RefreshedFolder {
    folders: Vec<String>,
    projects: Vec<Project>,
}

impl Monitor {
    pub fn new(handle: u8, server: &str) -> Monitor {
        Monitor {
            handle,
            jenkins_server: server.to_string(),
            version: 1,
            projects: Arc::new(RwLock::new(Vec::new())),
            projects_hash: u64::MAX,
            server: None,
            client: None,
        }
    }

    pub fn get_handle(&self) -> u8 {
        self.handle
    }

    pub async fn refresh_projects(&mut self) -> Result<bool, BuildMonitorError> {
        match &self.client {
            Some(client) => {
                let (has_projects, new_projects) = client.get_projects();
                if has_projects {
                    println!("Found new projects");
                    *self.projects.write().unwrap() = new_projects;
                }
            }
            None => {
                let reqwest_client = Arc::new(
                    reqwest::blocking::Client::builder()
                        .danger_accept_invalid_certs(true)
                        .build()
                        .unwrap(),
                );

                let mut volunteers = Vec::new();
                {
                    let projects = self.projects.read().unwrap();
                    for project in projects.iter() {
                        if project.volunteer().len() > 0 {
                            let id = project.id();
                            let volunteer = project.volunteer().to_owned();
                            volunteers.push((id, volunteer));
                        }
                    }
                }

                {
                    let mut projects = self.projects.write().unwrap();
                    projects.clear();
                    let mut folders_to_crawl: VecDeque<String> = VecDeque::new();
                    folders_to_crawl.push_back(self.jenkins_server.clone());
                    while !folders_to_crawl.is_empty() {
                        let url = folders_to_crawl.pop_front().unwrap();
                        let mut result = self.refresh_folder(&reqwest_client, &url).await?;
                        for element in result.folders {
                            folders_to_crawl.push_back(element);
                        }
                        projects.append(&mut result.projects);
                    }

                    projects.sort_by(|lhs, rhs| {
                        let sort = lhs.folder().cmp(rhs.folder());
                        if sort == std::cmp::Ordering::Equal {
                            lhs.name().cmp(rhs.name())
                        }
                        else {
                            sort
                        }
                    });

                    for (id, name) in volunteers.iter() {
                        match projects.iter_mut().find(|p| p.id() == *id) {
                            Some(project) => {
                                if project.status() != ProjectStatus::Success {
                                    project.set_volunteer(name);
                                }
                            },
                            None => {}
                        }
                    }
                }
            }
        };

        let projects_hash = self.generate_projects_hash(&self.projects.read().unwrap());
        let has_new_projects = projects_hash != self.projects_hash;
        if has_new_projects {
            self.projects_hash = projects_hash;
            match &mut self.server {
                Some(server) => server.update_clients(),
                None => {}
            }
        }

        Ok(has_new_projects)
    }

    pub fn get_projects(&self) -> &Arc<RwLock<Vec<Project>>> {
        &self.projects
    }

    pub fn generate_projects_hash(&self, projects: &Vec<Project>) -> u64 {
        let mut hasher = DefaultHasher::new();
        for project in projects.iter() {
            project.hash(&mut hasher);
        }
        hasher.finish()
    }

    pub fn start_server(&mut self, address: &str, multicast: bool) {
        self.server = Some(MonitorServer::new(
            address.parse().unwrap(),
            self.version,
            self.projects.clone(),
            multicast
        ));
    }

    pub fn stop_server(self: &mut Monitor) {
        self.server = None;
    }

    pub fn start_client(&mut self, server_address: &str, client_address: &str, multicast: bool) {
        self.client = Some(MonitorClient::new(
            server_address.parse().unwrap(),
            client_address.parse().unwrap(),
            self.version,
            multicast
        ));
    }

    pub fn stop_client(&mut self) {
        self.client = None;
    }

    pub fn set_volunteering(&self, project_id: u64) {
        match &self.client {
            Some(client) => {
                {
                    let mut projects_unlocked = self.projects.write().unwrap();
                    let project = projects_unlocked.iter_mut().find(|element| element.id() == project_id);
                    let username = get_username();
                    match project {
                        Some(project) => project.set_volunteer(&username),
                        None => eprintln!("Unable to find project in existing list.")
                    }
                }
                client.set_volunteering(project_id);
            }
            None => eprintln!("Failed to set volunteering on project. Client hasn't been created."),
        }
    }

    async fn refresh_folder(
        &self,
        reqwest_client: &Arc<reqwest::blocking::Client>,
        refresh_url: &str,
    ) -> Result<RefreshedFolder, BuildMonitorError> {
        let refresh_url = refresh_url.to_owned();
        let json = self.get_json(&reqwest_client, &refresh_url).await?;
        let mut folder_start = self.jenkins_server.len();
        if self.jenkins_server.chars().nth(folder_start - 1).unwrap() != '/' {
            folder_start += 1;
        }
        let mut folder_name = String::new();
        if folder_start < refresh_url.len() {
            folder_name = refresh_url[folder_start..].replace("job/", "");
        }

        let mut folders: Vec<String> = Vec::new();
        let mut projects: Vec<Project> = Vec::new();
        for job in json["jobs"].members() {
            let class = &job["_class"];
            if class == "com.cloudbees.hudson.plugins.folder.Folder" {
                let url = job["url"]
                    .as_str()
                    .ok_or_else(|| BuildMonitorError::FieldError {})
                    .unwrap();
                folders.push(url.to_string());
            } else if class == "hudson.model.FreeStyleProject" {
                let url = job["url"]
                    .as_str()
                    .ok_or_else(|| BuildMonitorError::FieldError {})
                    .unwrap();
                let project = Project::new(&folder_name, url);
                projects.push(project);
            }
        }

        for project in projects.iter_mut() {
            project.refresh_status(reqwest_client.clone()).await?;
        }

        Ok(RefreshedFolder { folders, projects })
    }

    async fn get_json(
        &self,
        reqwest_client: &Arc<reqwest::blocking::Client>,
        url: &str,
    ) -> Result<json::JsonValue, BuildMonitorError> {
        let api_url = url.to_string() + "/api/json";
        let text = reqwest_client.get(&api_url).send()?.text()?;
        Ok(json::parse(&text)?)
    }
}

impl fmt::Display for Monitor {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let mut projects_message: String = String::new();
        let projects = self.projects.read().unwrap();
        for project in projects.iter() {
            projects_message += &format!("{}\n", project);
        }
        write!(f, "{}", projects_message)
    }
}
