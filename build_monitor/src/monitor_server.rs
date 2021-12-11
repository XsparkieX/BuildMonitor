// Copyright Sander Brattinga. All rights reserved.

use crate::monitor::{Header, MessageType};
use crate::project::{Project, Volunteer};

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
    multicast_address: SocketAddr,
}

fn server_multicast_thread(data: &Arc<RwLock<MonitorServerThreadData>>) {
    let address;
    {
        let data_read_lock = data.read().unwrap();
        address = data_read_lock.multicast_address.to_owned();
    }

    let socket = Socket::new(Domain::IPV4, Type::DGRAM, Some(Protocol::UDP))
        .expect("Unable to create socket.");
    socket
        .set_reuse_address(true)
        .expect("Failed to apply reuse address");
    socket
        .set_nonblocking(true)
        .expect("Failed to set non blocking.");

    let bind_address = match address.ip() {
        IpAddr::V4(_ip) => {
            socket.set_multicast_ttl_v4(1).expect("Failed to set ttl.");
            socket
                .set_multicast_if_v4(&Ipv4Addr::UNSPECIFIED)
                .expect("Failed to set multicast interface.");
            SocketAddr::new(IpAddr::V4(Ipv4Addr::UNSPECIFIED), address.port())
        }
        IpAddr::V6(_ip) => {
            socket
                .set_multicast_if_v6(0)
                .expect("Failed to set multicast interface.");
            SocketAddr::new(IpAddr::V6(Ipv6Addr::UNSPECIFIED), address.port())
        }
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
                let mut deserialize_buffer = Vec::new();
                let from_address;
                match listener.recv_from(&mut recv_buffer) {
                    Ok((bytes_read, from)) => {
                        println!("Received {} bytes.", bytes_read);
                        deserialize_buffer.extend_from_slice(&recv_buffer[..bytes_read]);
                        from_address = from;
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
                    if header.msg_type == MessageType::NewConnection {
                        let mut projects_buffer;
                        {
                            let data_read_lock = data.read().unwrap();
                            projects_buffer = bincode::serialize(&*data_read_lock.projects).unwrap();
                        }
                        let mut header = Header::new();
                        header.version = version;
                        header.msg_type = MessageType::ProjectUpdate;
                        header.msg_size = projects_buffer.len() as u32;

                        let mut write_buffer = Vec::<u8>::new();
                        write_buffer.append(&mut bincode::serialize(&header).unwrap());
                        write_buffer.append(&mut projects_buffer);
                        println!("Sending around the projects.");
                        match listener.send_to(&write_buffer, from_address) {
                            Ok(_) => {},
                            Err(e) => { eprintln!("Failed to send to {}. Error: {}", address, e); }
                        }
                    } else if header.msg_type == MessageType::VolunteerAdded {
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
                                needs_refresh = true;
                            }
                            None => {
                                eprintln!("Unable to find project with id {}. Requested by {}", volunteer.id, volunteer.volunteer);
                            }
                        }
                    }
                }
            }
        }

        if needs_refresh {
            let mut projects_buffer;
            {
                let data_read_lock = data.read().unwrap();
                projects_buffer = bincode::serialize(&*data_read_lock.projects).unwrap();
            }
            let mut header = Header::new();
            header.version = version;
            header.msg_type = MessageType::ProjectUpdate;
            header.msg_size = projects_buffer.len() as u32;

            let mut write_buffer = Vec::<u8>::new();
            write_buffer.append(&mut bincode::serialize(&header).unwrap());
            write_buffer.append(&mut projects_buffer);
            println!("Sending around the projects.");
            listener
                .send_to(&write_buffer, address)
                .expect(&format!("Failed to send to {}.", address));
            {
                let mut data_write_lock = data.write().unwrap();
                data_write_lock.needs_refresh = false;
            }
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

pub struct MonitorServer {
    multicast_thread: Option<JoinHandle<()>>,
    thread_data: Arc<RwLock<MonitorServerThreadData>>,
}

impl MonitorServer {
    pub fn new(
        multicast_address: SocketAddr,
        version: u32,
        projects: Arc<RwLock<Vec<Project>>>,
    ) -> MonitorServer {
        let thread_data = Arc::new(RwLock::new(MonitorServerThreadData {
            running: true,
            needs_refresh: true,
            version,
            projects,
            multicast_address,
        }));
        let thread_data_for_thread = thread_data.clone();

        MonitorServer {
            multicast_thread: Some(std::thread::spawn(move || {
                server_multicast_thread(&thread_data_for_thread)
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

        let join_result = self.multicast_thread.take().unwrap().join();
        if join_result.is_err() {
            eprintln!("Failed to join the multicasting thread.")
        }
    }
}
