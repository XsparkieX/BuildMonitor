// Copyright Sander Brattinga. All rights reserved.

use build_monitor::monitor::Monitor;

use futures::executor::block_on;
use std::env;
use std::thread::sleep;
use std::time::{Duration, SystemTime};

fn retrieve_info(address: &str) {
    let mut monitor = Monitor::new(0, address); 
    match block_on(monitor.refresh_projects()) {
        Ok(has_projects) => {
            if has_projects {
                println!("{}", monitor);
            }
        },
        Err(_) => {}
    }
}

fn client(address: &str) {
    let mut monitor = Monitor::new(0, "");
    monitor.start_client(address);
    loop {
        {
            match block_on(monitor.refresh_projects()) {
                Ok(has_projects) => {
                    if has_projects {
                        println!("{}", monitor);
                    }
                },
                Err(_) => {}
            }
        }

        sleep(Duration::from_millis(500));
    }
}

fn server(address: &str, broadcast: &str) {
    let mut monitor = Monitor::new(0, address);
    println!("Refreshing initial projects...");
    match block_on(monitor.refresh_projects()) {
        Ok(_) => {}
        Err(e) => eprintln!("Failed to refresh projects. Error: {}", e),
    }
    println!("Starting server...");
    monitor.start_server(broadcast);

    let mut elapsed_time: Duration = Duration::new(10, 0);
    loop {
        if elapsed_time.as_secs_f32() < 10.0f32 {
            sleep(Duration::new(10, 0) - elapsed_time);
        }
        let start_time = SystemTime::now();
        match block_on(monitor.refresh_projects()) {
            Ok(_) => {}
            Err(e) => eprintln!("Failed to refresh projects. Error: {}", e),
        }
        elapsed_time = start_time.elapsed().unwrap_or(Duration::new(999, 0));
        println!(
            "Refreshing projects took {} seconds.",
            elapsed_time.as_secs_f32()
        );
    }
}

fn main() {
    let help_message = "Please specify '--retrieveinfo', '--client' or '--server' on the commandline args.";

    let args: Vec<String> = env::args().collect();
    if args.len() < 2 {
        println!("{}", help_message);
    }
    else {
        if args[1] == "--retrieveinfo" {
            if args.len() != 3 {
                println!("Usage build_monitor_cli.exe --retrieveinfo {{url_to_buildserver}}");
            }
            else {
                retrieve_info(&args[2]);
            }
        }
        else if args[1] == "--client" {
            if args.len() != 3 {
                println!("Usage build_monitor_cli.exe --client {{broadcast_address}}");
            }
            else {
                client(&args[2]);
            }
        }
        else if args[1] == "--server" {
            if args.len() != 4 {
                println!("Usage build_monitor_cli.exe --server {{url_to_buildserver}} {{broadcast_address}}");
            }
            else {
                server(&args[2], &args[3]);
            }
        }
        else
        {
            println!("{}", help_message);
        }
    }
}
