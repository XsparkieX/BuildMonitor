// Copyright Sander Brattinga. All rights reserved.

use crate::monitor::{Header, MessageType};
use crate::project::{Project, Volunteer};
use crate::utils::get_username;

use socket2::{Domain, Protocol, Socket, Type};
use std::net::{IpAddr, Ipv4Addr, Ipv6Addr, SocketAddr, UdpSocket};
use std::sync::{Arc, RwLock};
use std::thread::JoinHandle;

struct MonitorClientThreadData {
    running: bool,
    version: u32,
    projects: RwLock<Vec<Project>>,
    volunteers: Arc<RwLock<Vec<Volunteer>>>,
    multicast_address: SocketAddr,
}

fn client_connection_thread(data: &Arc<RwLock<MonitorClientThreadData>>) {
    let multicast_address;
    {
        let lock = data.read();
        let unwrapped = lock.unwrap();
        multicast_address = unwrapped.multicast_address.clone();
    }

    let socket = Socket::new(Domain::IPV4, Type::DGRAM, Some(Protocol::UDP))
        .expect("Unable to create socket.");
    socket
        .set_reuse_address(true)
        .expect("Failed to apply reuse address");
    socket
        .set_nonblocking(true)
        .expect("Failed to set non blocking.");

    let bind_address = match multicast_address.ip() {
        IpAddr::V4(ip) => {
            socket.set_multicast_ttl_v4(1).expect("Failed to set ttl.");
            socket
                .join_multicast_v4(&ip, &Ipv4Addr::UNSPECIFIED)
                .expect("Failed to join multicast group.");
            SocketAddr::new(IpAddr::V4(Ipv4Addr::UNSPECIFIED), multicast_address.port())
        }
        IpAddr::V6(ip) => {
            socket
                .join_multicast_v6(&ip, 0)
                .expect("Failed to join multicast group.");
            SocketAddr::new(IpAddr::V6(Ipv6Addr::UNSPECIFIED), multicast_address.port())
        }
    };
    socket
        .bind(&bind_address.into())
        .expect("Failed to bind socket.");

    let socket: UdpSocket = socket.into();
    let mut has_received_projects = false;
    let mut from_address = None;
    loop {
        let running;
        let version;
        {
            let data_read_lock = data.read().unwrap();
            running = data_read_lock.running;
            version = data_read_lock.version;
        }

        if !running {
            break;
        }

        const RECV_BUFFER_SIZE: usize = 1 * 1024 * 1024;
        let mut recv_buffer: [u8; RECV_BUFFER_SIZE] = [0; RECV_BUFFER_SIZE];
        let mut deserialize_buffer = Vec::new();
        loop {
            match socket.recv_from(&mut recv_buffer) {
                Ok((bytes_read, from)) => {
                    println!("Received {} bytes", bytes_read);
                    deserialize_buffer.extend_from_slice(&recv_buffer[..bytes_read]);
                    from_address = Some(from);
                }
                Err(error) => {
                    if error.kind() != std::io::ErrorKind::WouldBlock {
                        eprintln!("Failed to receive data: {}", error);
                    }
                    break;
                }
            }

            let mut header = Header::new();
            let header_raw: Vec<u8> = deserialize_buffer
                .drain(..bincode::serialized_size(&header).unwrap() as usize)
                .collect();
            header = bincode::deserialize::<Header>(&header_raw).unwrap();

            if header.version == version {
                println!("Version matched! {} | {}", header.msg_type, header.msg_size);
                if header.msg_type == MessageType::ProjectUpdate {
                    println!("Message type is project update!");
                    // Ensure the full message fit into the packet.
                    if header.msg_size >= deserialize_buffer.len() as u32 {
                        println!("Received a project update");
                        let projects_raw: Vec<u8> = deserialize_buffer
                            .drain(..header.msg_size as usize)
                            .collect();
                        let projects = bincode::deserialize::<Vec<Project>>(&projects_raw).unwrap();
                        let read_locked = data.read().unwrap();
                        *read_locked.projects.write().unwrap() = projects;
                        has_received_projects = true;
                    }
                } else if !has_received_projects && header.msg_type == MessageType::Beacon {
                    let mut send_header = Header::new();
                    send_header.version = version;
                    send_header.msg_type = MessageType::NewConnection;
                    send_header.msg_size = 0;

                    let mut write_buffer: Vec<u8> = Vec::new();
                    write_buffer.append(&mut bincode::serialize(&send_header).unwrap());
                    println!("Requesting a project update. Address {}", from_address.unwrap());
                    socket.send_to(&write_buffer, &from_address.unwrap()).unwrap();
                }
            }
        }

        {
            let volunteers: Vec<Volunteer>;
            {
                let data_read_lock = data.read().unwrap();
                let volunteer_write_lock = data_read_lock.volunteers.write();
                volunteers = volunteer_write_lock.unwrap().drain(..).collect();
            }
            for volunteer in volunteers {
                let mut send_header = Header::new();
                send_header.version = version;
                send_header.msg_type = MessageType::VolunteerAdded;

                let mut volunteer_data: Vec<u8> = bincode::serialize(&volunteer).unwrap();
                send_header.msg_size = volunteer_data.len() as u32;

                let mut write_buffer: Vec<u8> = Vec::new();
                write_buffer.append(&mut bincode::serialize(&send_header).unwrap());
                write_buffer.append(&mut volunteer_data);

                match from_address {
                    Some(from_address) => {
                        match socket.send_to(&write_buffer, &from_address) {
                            Ok(_bytes_written) => {},
                            Err(e) => eprintln!("Failed to send volunteer list. {}", e)
                        }
                    },
                    None => eprintln!("Don't have a valid address to send to.")
                }
            }
        }

        std::thread::sleep(std::time::Duration::from_millis(500));
    }
}

pub struct MonitorClient {
    connection_thread: Option<JoinHandle<()>>,
    thread_data: Arc<RwLock<MonitorClientThreadData>>,
}

impl MonitorClient {
    pub fn new(multicast_address: SocketAddr, version: u32) -> MonitorClient {
        let thread_data = Arc::new(RwLock::new(MonitorClientThreadData {
            running: true,
            version,
            projects: RwLock::new(Vec::new()),
            volunteers: Arc::new(RwLock::new(Vec::<Volunteer>::new())),
            multicast_address,
        }));
        let thread_data_for_thread = thread_data.clone();

        MonitorClient {
            connection_thread: Some(std::thread::spawn(move || {
                client_connection_thread(&thread_data_for_thread)
            })),
            thread_data,
        }
    }

    pub fn set_volunteering(&self, project_id: u64) {
        let username = get_username();
        let volunteer = Volunteer {
            id: project_id,
            volunteer: username,
        };

        let guard = self.thread_data.read().unwrap();
        let guard2 = guard.volunteers.write();
        let mut pending_volunteers = guard2.unwrap();
        pending_volunteers.push(volunteer);
    }

    pub fn get_projects(&self) -> (bool, Vec<Project>) {
        let thread_data_guard = self.thread_data.read().unwrap();
        let mut projects_guard = thread_data_guard.projects.write().unwrap();
        if !projects_guard.is_empty() {
            let projects_result: Vec<Project> = projects_guard.drain(..).collect();
            return (true, projects_result);
        }
        (false, Vec::new())
    }
}

impl Drop for MonitorClient {
    fn drop(&mut self) {
        self.thread_data.write().unwrap().running = false;

        let join_result = self.connection_thread.take().unwrap().join();
        if join_result.is_err() {
            eprintln!("Failed to join the connection thread thread.")
        }
    }
}
