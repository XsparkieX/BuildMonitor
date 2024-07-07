// Copyright Sander Brattinga. All rights reserved.

pub mod monitor;
pub mod project;

mod error;
mod monitor_client;
mod monitor_server;
mod utils;

#[cfg(test)]
mod tests {
    use super::monitor::Monitor;

    #[test]
    fn run_jenkins_test() {
        let mut monitor = Monitor::new("https://jenkins");
        let result = futures::executor::block_on(monitor.refresh_projects());
        match result {
            Ok(_) => {}
            Err(e) => {
                panic!("Failed reason: {}", e);
            }
        }
    }

    fn run_server_test(server_address: &str, client_address: &str, multicast: bool) {
        let jenkins = "https://jenkins";
        let mut server_monitor = Monitor::new(jenkins);

        match futures::executor::block_on(server_monitor.refresh_projects()) {
            Ok(_) => {},
            Err(e) => panic!("Failed reason: {}", e)
        }
        match server_monitor.start_server(server_address, multicast) {
            Ok(_) => {},
            Err(e) => panic!("Start server failed, reason: {}", e)
        }

        let mut client_monitor = Monitor::new(jenkins);
        {
            match client_monitor.start_client(server_address, client_address, multicast) {
                Ok(_) => {},
                Err(e) => panic!("Start client failed, reason: {}", e)
            }

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

        client_monitor.stop_client();
        server_monitor.stop_server();

        println!("ServerMonitor:\n {}", server_monitor);
        println!("ClientMonitor:\n {}", client_monitor);
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
