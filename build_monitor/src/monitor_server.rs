// Copyright Sander Brattinga. All rights reserved.

use crate::monitor::{Header, MessageType, Monitor};
use crate::project::{Project, Volunteer};
use crate::utils::get_local_addresses;

use socket2::{Domain, Protocol, Socket, Type};
use std::iter::Iterator;
use std::net::{IpAddr, Ipv4Addr, Ipv6Addr, SocketAddr, UdpSocket};
use std::sync::{Arc, RwLock};
use std::thread::JoinHandle;
use std::time::{Duration, SystemTime, UNIX_EPOCH};

struct MonitorServerThreadData {
    running: bool,
    needs_refresh: bool,
    version: u32,
    projects: Arc<RwLock<Vec<Project>>>,
    address: SocketAddr,
}

fn server_get_header(listener: &UdpSocket, recv_buffer: &mut [u8]) ->
    Result<(Header, SocketAddr, Vec<u8>), ()> {
    let mut header = Header::new();
    let from_address: SocketAddr;
    let mut deserialize_buffer = Vec::new();
    match listener.recv_from(recv_buffer) {
        Ok((bytes_read, from)) => {
            println!("Received {} bytes.", bytes_read);
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

    let header_raw: Vec<u8> = deserialize_buffer
        .drain(..bincode::serialized_size(&header).unwrap() as usize)
        .collect();
    header = bincode::deserialize::<Header>(&header_raw).unwrap();
    return Ok((header, from_address, deserialize_buffer));
}

fn server_handle_project_update(data: &Arc<RwLock<MonitorServerThreadData>>, listener: &UdpSocket, address: &SocketAddr) {
    let mut projects_buffer;
    let version;
    {
        let data_read_lock = data.read().unwrap();
        projects_buffer = bincode::serialize(&*data_read_lock.projects).unwrap();
        version = data_read_lock.version;
    }
    let mut header = Header::new();
    header.version = version;
    header.msg_type = MessageType::ProjectUpdate;
    header.msg_size = projects_buffer.len() as u32;

    let mut write_buffer = Vec::<u8>::new();
    write_buffer.append(&mut bincode::serialize(&header).unwrap());
    write_buffer.append(&mut projects_buffer);
    println!("Sending projects to {}.", address);
    match listener.send_to(&write_buffer, address) {
        Ok(_) => {},
        Err(e) => { eprintln!("Failed to send to {}. Error: {}", address, e); }
    }
}

fn server_handle_no_project_update(data: &Arc<RwLock<MonitorServerThreadData>>, listener: &UdpSocket, address: &SocketAddr) {
    let version;
    {
        let data_read_lock = data.read().unwrap();
        version = data_read_lock.version;
    }
    let mut header = Header::new();
    header.version = version;
    header.msg_type = MessageType::NoProjectUpdate;
    header.msg_size = 0;

    let mut write_buffer = Vec::<u8>::new();
    write_buffer.append(&mut bincode::serialize(&header).unwrap());
    println!("Sending no need for update to {}.", address);
    match listener.send_to(&write_buffer, address) {
        Ok(_) => {},
        Err(e) => { eprintln!("Failed to send to {}. Error: {}", address, e); }
    }
}

fn server_handle_volunteer_added(data: &Arc<RwLock<MonitorServerThreadData>>, deserialize_buffer: &mut Vec<u8>) -> bool {
    let volunteer_raw: Vec<u8> = deserialize_buffer.drain(..).collect();
    let volunteer = bincode::deserialize::<Volunteer>(&volunteer_raw).unwrap();
    let data_read_lock = data.read().unwrap();
    let mut project_read_lock = data_read_lock.projects.write().unwrap();
    match project_read_lock
        .iter_mut()
        .find(|x| x.id() == volunteer.id)
    {
        Some(project) => {
            println!("Setting volunteer for project with id {}. Requested by {}", volunteer.id, volunteer.volunteer);
            project.set_volunteer(&volunteer.volunteer);
            return true;
        }
        None => {
            eprintln!("Unable to find project with id {}. Requested by {}", volunteer.id, volunteer.volunteer);
            return false;
        }
    }
}

fn server_multicast_thread(data: &Arc<RwLock<MonitorServerThreadData>>) {
    let address;
    {
        let data_read_lock = data.read().unwrap();
        address = data_read_lock.address.to_owned();
    }

    let socket = Socket::new(Domain::IPV4, Type::DGRAM, Some(Protocol::UDP))
        .expect("Unable to create socket.");
    socket
        .set_reuse_address(true)
        .expect("Failed to apply reuse address");
    socket
        .set_nonblocking(true)
        .expect("Failed to set non blocking.");

    match address.ip() {
        IpAddr::V4(_ip) => {
            let local_addresses = get_local_addresses().unwrap();
            for local_address in local_addresses.iter() {
                match local_address {
                    IpAddr::V4(interface) =>
                        socket
                            .set_multicast_if_v4(&interface)
                            .expect("Failed to join multicast group."),
                    IpAddr::V6(_interface) => {}
                }
            }
            socket.set_multicast_ttl_v4(255).expect("Failed to set ttl.");
        },
        IpAddr::V6(_ip) => {
            let local_addresses = get_local_addresses().unwrap();
            for local_address in local_addresses.iter() {
                match local_address {
                    IpAddr::V4(_interface) => {},
                    // TODO: Specify the required id for this interface.
                    IpAddr::V6(_interface) =>
                        socket
                            .set_multicast_if_v6(0)
                            .expect("Failed to join multicast group."),
                }
            }
        }
    };

    let bind_address = match address.ip() {
        IpAddr::V4(_ip) => SocketAddr::new(IpAddr::V4(Ipv4Addr::UNSPECIFIED), address.port()),
        IpAddr::V6(_ip) => SocketAddr::new(IpAddr::V6(Ipv6Addr::UNSPECIFIED), address.port())
    };
    socket
        .bind(&bind_address.into())
        .expect("Failed to bind socket.");

    let listener: UdpSocket = socket.into();
    let mut last_beacon_update: SystemTime = UNIX_EPOCH;
    loop {
        let mut needs_refresh;
        let version;
        let running;
        {
            let data_read_lock = data.read().unwrap();
            running = data_read_lock.running;
            needs_refresh = data_read_lock.needs_refresh;
            version = data_read_lock.version;
        }

        if !running {
            break;
        }

        {
            let mut recv_buffer = [0; 1 * 1024 * 1024];
            loop {
                match server_get_header(&listener, &mut recv_buffer) {
                    Ok((header, from_address, mut deserialize_buffer)) => {
                        if header.version == version {
                            if header.msg_type == MessageType::ProjectUpdateRequest {
                                let _dummy: u64 = 0;
                                let _dummy2: Vec<_> = deserialize_buffer
                                    .drain(..bincode::serialized_size(&_dummy).unwrap() as usize)
                                    .collect();
                                server_handle_project_update(data, &listener, &from_address);
                            } else if header.msg_type == MessageType::VolunteerAdded {
                                needs_refresh = server_handle_volunteer_added(data, &mut deserialize_buffer);
                            }
                        }
                    },
                    Err(_) => {
                        break;
                    }
                }
            }
        }

        if needs_refresh {
            server_handle_project_update(data, &listener, &address);
            let mut data_write_lock = data.write().unwrap();
            data_write_lock.needs_refresh = false;
        }

        const BEACON_UPDATE_INTERVAL: Duration = Duration::from_secs(1);
        if SystemTime::now()
            .duration_since(last_beacon_update)
            .unwrap()
            >= BEACON_UPDATE_INTERVAL
        {
            let mut header = Header::new();
            header.version = version;
            header.msg_type = MessageType::Beacon;
            header.msg_size = 0;

            let mut write_buffer = Vec::<u8>::new();
            write_buffer.append(&mut bincode::serialize(&header).unwrap());
            listener
                .send_to(&write_buffer, address)
                .expect(&format!("Failed to send to {}.", address));
            last_beacon_update = SystemTime::now();
        }

        std::thread::sleep(Duration::from_millis(500));
    }
}

fn server_query_thread(data: &Arc<RwLock<MonitorServerThreadData>>) {
    let address;
    {
        let data_read_lock = data.read().unwrap();
        address = data_read_lock.address.to_owned();
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
        .bind(&address.into())
        .expect("Failed to bind socket.");

    let listener: UdpSocket = socket.into();
    loop {
        let version;
        let running;
        let projects_hash;
        {
            let data_read_lock = data.read().unwrap();
            running = data_read_lock.running;
            version = data_read_lock.version;
            let projects = data_read_lock.projects.read().unwrap();
            projects_hash = Monitor::generate_projects_hash(&projects);
        }

        if !running {
            break;
        }

        let mut recv_buffer = [0; 1 * 1024 * 1024];
        loop {
            match server_get_header(&listener, &mut recv_buffer) {
                Ok((header, from_address, mut deserialize_buffer)) => {
                    if header.version == version {
                        // Ensure the full message fit into the packet.
                        if header.msg_size >= deserialize_buffer.len() as u32 {
                            if header.msg_type == MessageType::ProjectUpdateRequest {
                                let mut client_project_hash: u64 = 0;
                                let client_project_hash_raw: Vec<u8> = deserialize_buffer
                                    .drain(..bincode::serialized_size(&client_project_hash).unwrap() as usize)
                                    .collect();
                                client_project_hash = bincode::deserialize::<u64>(&client_project_hash_raw).unwrap();
                                if projects_hash == client_project_hash {
                                    server_handle_no_project_update(data, &listener, &from_address);
                                }
                                else {
                                    server_handle_project_update(data, &listener, &from_address);
                                }
                            } else if header.msg_type == MessageType::VolunteerAdded {
                                server_handle_volunteer_added(data, &mut deserialize_buffer);
                            }
                        }
                    }
                },
                Err(_) => {
                    break;
                }
            }

        }

        std::thread::sleep(Duration::from_millis(5));
    }
}

pub struct MonitorServer {
    server_thread: Option<JoinHandle<()>>,
    thread_data: Arc<RwLock<MonitorServerThreadData>>,
}

impl MonitorServer {
    pub fn new(
        address: SocketAddr,
        version: u32,
        projects: Arc<RwLock<Vec<Project>>>,
        multicast: bool,
    ) -> MonitorServer {
        let thread_data = Arc::new(RwLock::new(MonitorServerThreadData {
            running: true,
            needs_refresh: true,
            version,
            projects,
            address,
        }));
        let thread_data_for_thread = thread_data.clone();

        MonitorServer {
            server_thread: Some(std::thread::spawn(move || {
                if multicast {
                    server_multicast_thread(&thread_data_for_thread)
                }
                else {
                    server_query_thread(&thread_data_for_thread)
                }
            })),
            thread_data,
        }
    }

    pub fn update_clients(&mut self) {
        self.thread_data.write().unwrap().needs_refresh = true;
    }
}

impl Drop for MonitorServer {
    fn drop(&mut self) {
        self.thread_data.write().unwrap().running = false;

        let join_result = self.server_thread.take().unwrap().join();
        if join_result.is_err() {
            eprintln!("Failed to join the multicasting thread.")
        }
    }
}
