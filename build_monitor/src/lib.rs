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
        let monitor_handle;
        let monitor;
        unsafe {
            monitor_handle = MONITORS.create("https://jenkins");
            monitor = MONITORS.get(monitor_handle).expect("Failed to get monitor");
        }

        let result = futures::executor::block_on(monitor.borrow_mut().refresh_projects());
        match result {
            Ok(_) => {}
            Err(e) => {
                panic!("Failed reason: {}", e);
            }
        }

        unsafe {
            MONITORS.destroy(monitor_handle);
        }
    }

    fn run_server_test(server_address: &str, client_address: &str, multicast: bool) {
        let jenkins = "https://jenkins";
        let server_monitor_handle;
        let server_monitor_cell;
        unsafe {
            server_monitor_handle = MONITORS.create(jenkins);
            server_monitor_cell = MONITORS.get(server_monitor_handle).expect("Failed to get server monitor");
        }

        match futures::executor::block_on(server_monitor_cell.borrow_mut().refresh_projects()) {
            Ok(_) => {},
            Err(e) => {
                unsafe {
                    MONITORS.destroy(server_monitor_handle);
                }
                panic!("Failed reason: {}", e);
            }
        }
        server_monitor_cell.borrow_mut().start_server(server_address, multicast);

        let client_monitor_handle;
        let client_monitor_cell;
        unsafe {
            client_monitor_handle = MONITORS.create(jenkins);
            client_monitor_cell = MONITORS.get(client_monitor_handle).expect("Failed to get client monitor");
        }

        {
            let mut client_monitor = client_monitor_cell.borrow_mut();
            client_monitor.start_client(server_address, client_address, multicast);

            while client_monitor.get_projects().read().unwrap().len() == 0 {
                match futures::executor::block_on(client_monitor.refresh_projects()) {
                    Ok(_) => {},
                    Err(e) => {
                        panic!("Failed reason: {}", e);
                    }
                }
                std::thread::sleep(std::time::Duration::from_millis(500));
            }

            {
                let project_id = client_monitor.get_projects().read().unwrap()[0].id();
                client_monitor.set_volunteering(project_id);
            }
        }

        std::thread::sleep(std::time::Duration::from_secs(1));

        client_monitor_cell.borrow_mut().stop_client();
        server_monitor_cell.borrow_mut().stop_server();

        unsafe {
            println!("{}", MONITORS);

            MONITORS.destroy(client_monitor_handle);
            MONITORS.destroy(server_monitor_handle);
        }
    }

    #[test]
    fn run_multicast_server_test() {
        run_server_test("239.255.13.37:8090", "239.255.13.37:8090", true);
    }

    #[test]
    fn run_query_server_test() {
        run_server_test("127.0.0.1:8090", "127.0.0.1:8091", false);
    }
}
