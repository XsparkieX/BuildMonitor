// Copyright Sander Brattinga. All rights reserved.

pub mod monitor;
pub mod monitors;
pub mod project;

mod error;
mod monitor_client;
mod monitor_server;
mod utils;

#[cfg(test)]
mod tests {
    use super::monitors::MONITORS;

    #[test]
    fn run_jenkins_test() {
        unsafe {
            let monitor_handle = MONITORS.create("https://jenkins");
            let monitor = MONITORS.get(monitor_handle).expect("Failed to get monitor");
            let result = futures::executor::block_on(monitor.borrow_mut().refresh_projects());
            match result {
                Ok(_) => {}
                Err(e) => {
                    panic!("Failed reason: {}", e);
                }
            }
            MONITORS.destroy(monitor_handle);
        }
    }

    #[test]
    fn run_server_test() {
        unsafe {
            let server_monitor_handle = MONITORS.create("https://jenkins");
            let server_monitor_cell = MONITORS.get(server_monitor_handle).expect("Failed to get server monitor");
            let mut server_monitor = server_monitor_cell.borrow_mut();
            let refresh_result = futures::executor::block_on(server_monitor.refresh_projects());
            match refresh_result {
                Ok(_) => {}
                Err(e) => {
                    MONITORS.destroy(server_monitor_handle);
                    panic!("Failed reason: {}", e);
                }
            }

            server_monitor.start_server("239.255.13.37:8090");

            let client_monitor_handle = MONITORS.create("https://jenkins");
            let client_monitor_cell = MONITORS.get(client_monitor_handle).expect("Failed to get client monitor");
            let mut client_monitor = client_monitor_cell.borrow_mut();
            client_monitor.start_client("239.255.13.37:8090");

            while !client_monitor.get_projects().read().unwrap().len() == 0 {
                std::thread::sleep(std::time::Duration::from_millis(500));
            }

            {
                let projects_unlocked = client_monitor.get_projects().read().unwrap();
                client_monitor.set_volunteering(projects_unlocked[0].id());
            }

            std::thread::sleep(std::time::Duration::from_secs(1));

            client_monitor.stop_client();
            server_monitor.stop_server();

            println!("{}", MONITORS);

            MONITORS.destroy(client_monitor_handle);
            MONITORS.destroy(server_monitor_handle);
        }
    }
}
