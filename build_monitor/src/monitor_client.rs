// Copyright Sander Brattinga. All rights reserved.

use crate::monitor::{Header, MessageType, Monitor};
use crate::project::{Project, Volunteer};
use crate::utils::{get_username, get_local_addresses};

use socket2::{Domain, Protocol, Socket, Type};
use std::net::{IpAddr, Ipv4Addr, Ipv6Addr, SocketAddr, UdpSocket};
use std::sync::{Arc, RwLock};
use std::thread::JoinHandle;

struct MonitorClientThreadData {
    running: bool,
    version: u32,
    projects: RwLock<Vec<Project>>,
    volunteers: Arc<RwLock<Vec<Volunteer>>>,
    server_address: SocketAddr,
    client_address: SocketAddr,
}

fn client_request_server_update(socket: &UdpSocket, version: u32, address: &SocketAddr, projects_hash: u64) {
    println!("Sending hash {}", projects_hash);
    let mut header = Header::new();
    header.version = version;
    header.msg_type = MessageType::ProjectUpdateRequest;
    let mut serialized_msg = bincode::serialize(&projects_hash).unwrap();
    header.msg_size = serialized_msg.len() as u32;

    let mut write_buffer: Vec<u8> = Vec::new();
    write_buffer.append(&mut bincode::serialize(&header).unwrap());
    write_buffer.append(&mut serialized_msg);
    println!("Requesting a project update. Address {}", address);
    socket.send_to(&write_buffer, address).unwrap();
}

fn client_receive_packet(socket: &UdpSocket, recv_buffer: &mut [u8]) -> Result<(Header, Vec<u8>, SocketAddr), ()> {
    let mut deserialize_buffer = Vec::new();
    let from_address;
    match socket.recv_from(recv_buffer) {
        Ok((bytes_read, from)) => {
            println!("Received {} bytes", bytes_read);
            deserialize_buffer.extend_from_slice(&recv_buffer[..bytes_read]);
            from_address = from;
        }
        Err(error) => {
            if error.kind() != std::io::ErrorKind::WouldBlock {
                eprintln!("Failed to receive data: {}", error);
            }
            return Err(());
        }
    }

    let mut header = Header::new();
    let header_raw: Vec<u8> = deserialize_buffer
        .drain(..bincode::serialized_size(&header).unwrap() as usize)
        .collect();
    header = bincode::deserialize::<Header>(&header_raw).unwrap();

    return Ok((header, deserialize_buffer, from_address));
}

fn client_send_volunteers(data: &Arc<RwLock<MonitorClientThreadData>>, socket: &UdpSocket, address: &Option<SocketAddr>) {
    let volunteers: Vec<Volunteer>;
    let version;
    {
        let data_read_lock = data.read().unwrap();
        version = data_read_lock.version;
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

        match address {
            Some(address) => {
                match socket.send_to(&write_buffer, &address) {
                    Ok(_bytes_written) => {},
                    Err(e) => eprintln!("Failed to send volunteer list. {}", e)
                }
            },
            None => eprintln!("Don't have a valid address to send to.")
        }
    }
}

fn client_multicast_connection_thread(data: &Arc<RwLock<MonitorClientThreadData>>) {
    let multicast_address;
    {
        let lock = data.read();
        let unwrapped = lock.unwrap();
        multicast_address = unwrapped.server_address.clone();
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
        IpAddr::V4(_ip) => SocketAddr::new(IpAddr::V4(Ipv4Addr::UNSPECIFIED), multicast_address.port()),
        IpAddr::V6(_ip) => SocketAddr::new(IpAddr::V6(Ipv6Addr::UNSPECIFIED), multicast_address.port())
    };
    socket
        .bind(&bind_address.into())
        .expect("Failed to bind socket.");

    match multicast_address.ip() {
        IpAddr::V4(ip) => {
            let local_addresses = get_local_addresses().unwrap();
            for local_address in local_addresses.iter() {
                match local_address {
                    IpAddr::V4(interface) =>
                        socket
                            .join_multicast_v4(&ip, interface)
                            .expect("Failed to join multicast group."),
                    IpAddr::V6(_interface) => {}
                }
            }
            socket.set_multicast_ttl_v4(255).expect("Failed to set ttl.");
        }
        IpAddr::V6(ip) => {
            let local_addresses = get_local_addresses().unwrap();
            for local_address in local_addresses.iter() {
                match local_address {
                    IpAddr::V4(_interface) => {},
                    // TODO: Specify the required id for this interface.
                    IpAddr::V6(_interface) =>
                        socket
                            .join_multicast_v6(&ip, 0)
                            .expect("Failed to join multicast group."),
                }
            }
        }
    };

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
        loop {
            match client_receive_packet(&socket, &mut recv_buffer) {
                Ok((header, mut deserialize_buffer, address)) => {
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
                            from_address = Some(address);
                            client_request_server_update(&socket, version, &address, 0);
                        }
                    }
                },
                Err(()) => break
            }
        }

        client_send_volunteers(data, &socket, &from_address);

        std::thread::sleep(std::time::Duration::from_millis(500));
    }
}

fn client_query_connection_thread(data: &Arc<RwLock<MonitorClientThreadData>>) {
    let server_address;
    let client_address;
    {
        let lock = data.read();
        let unwrapped = lock.unwrap();
        server_address = unwrapped.server_address.clone();
        client_address = unwrapped.client_address.clone();
    }

    let socket = Socket::new(Domain::IPV4, Type::DGRAM, Some(Protocol::UDP))
        .expect("Unable to create socket.");
    socket
        .set_reuse_address(true)
        .expect("Failed to apply reuse address");
    socket
        .set_nonblocking(true)
        .expect("Failed to set non blocking.");

    socket
        .bind(&client_address.into())
        .expect("Failed to bind socket.");

    let socket: UdpSocket = socket.into();
    let mut projects_hash = 0;
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
        client_request_server_update(&socket, version, &server_address, projects_hash);
        // Give the server some time to respond, before going into the slower delay
        std::thread::sleep(std::time::Duration::from_millis(100));

        match client_receive_packet(&socket, &mut recv_buffer) {
            Ok((header, mut deserialize_buffer, _from_address)) => {
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
                        projects_hash = Monitor::generate_projects_hash(&projects);
                        let read_locked = data.read().unwrap();
                        *read_locked.projects.write().unwrap() = projects;
                    }
                }
                else if header.msg_type == MessageType::NoProjectUpdate {
                    println!("No project update needed.");
                }
            },
            Err(()) => {}
        }

        client_send_volunteers(data, &socket, &Some(server_address));

        std::thread::park_timeout(std::time::Duration::from_secs(15));
    }
}

pub struct MonitorClient {
    connection_thread: Option<JoinHandle<()>>,
    thread_data: Arc<RwLock<MonitorClientThreadData>>,
}

impl MonitorClient {
    pub fn new(server_address: SocketAddr, client_address: SocketAddr, version: u32, multicast: bool) -> MonitorClient {
        let thread_data = Arc::new(RwLock::new(MonitorClientThreadData {
            running: true,
            version,
            projects: RwLock::new(Vec::new()),
            volunteers: Arc::new(RwLock::new(Vec::<Volunteer>::new())),
            server_address,
            client_address,
        }));
        let thread_data_for_thread = thread_data.clone();

        MonitorClient {
            connection_thread: Some(std::thread::spawn(move || {
                if multicast {
                    client_multicast_connection_thread(&thread_data_for_thread);
                }
                else {
                    client_query_connection_thread(&thread_data_for_thread);
                }
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

        let join_handle = self.connection_thread.take().unwrap();
        join_handle.thread().unpark();
        let join_result = join_handle.join();
        if join_result.is_err() {
            eprintln!("Failed to join the connection thread thread.")
        }
    }
}
