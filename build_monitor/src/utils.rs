// Copyright Sander Brattinga. All rights reserved.

use libc::*;
use std::env;
use std::io::Error;
use std::io::ErrorKind;
use std::net::{IpAddr, Ipv4Addr, Ipv6Addr};
use std::ptr::null_mut;

#[cfg(unix)]
pub fn get_local_addresses() -> Result<Vec<IpAddr>, Error> {
    use std::mem;

    unsafe
    {
        let mut address_ptr = libc::malloc(mem::size_of::<* mut libc::ifaddrs>()) as *mut libc::ifaddrs;
        let result = getifaddrs(&mut address_ptr);

        if result != 0 {
            libc::freeifaddrs(address_ptr);
            return Err(Error::new(ErrorKind::Other, "Failed to get the address."))
        }

        let mut result: Vec<IpAddr> = Vec::new();
        while address_ptr != null_mut() {
            let address = *address_ptr;
            if address.ifa_addr == null_mut() {
                address_ptr = address.ifa_next;
                continue;
            }

            if (address.ifa_flags & (IFF_UP as u32)) != 0 && (address.ifa_flags & (IFF_LOOPBACK as u32)) == 0 && 
                (*address.ifa_addr).sa_family == AF_INET as u16 {
                    let ip4 = *(address.ifa_addr as *mut sockaddr_in);
                    let sin_addr: *const in_addr = &ip4.sin_addr;
                    let raw_ip: *const [u8; 4] = sin_addr as *const [u8; 4];
                    result.push(IpAddr::V4(Ipv4Addr::from(*raw_ip)));
            }
        }

        Ok(result)
    }
}

#[cfg(windows)]
pub fn get_local_addresses() -> Result<Vec<IpAddr>, Error> {
    use winapi::um::iphlpapi::GetAdaptersAddresses;
    use winapi::um::iptypes::*;
    use winapi::shared::inaddr::in_addr;
    use winapi::shared::in6addr::in6_addr;
    use winapi::shared::ifdef::IfOperStatusUp;
    use winapi::shared::ipifcons::{IF_TYPE_ETHERNET_CSMACD, IF_TYPE_IEEE80211};
    use winapi::shared::ws2def::{AF_UNSPEC, AF_INET, SOCKADDR_STORAGE, SOCKADDR_IN};
    use winapi::shared::ws2ipdef::SOCKADDR_IN6;
    use winapi::shared::winerror::{ERROR_BUFFER_OVERFLOW, ERROR_SUCCESS};

    let family = AF_UNSPEC as u32;
    let flags = GAA_FLAG_INCLUDE_PREFIX | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_DNS_SERVER | GAA_FLAG_SKIP_FRIENDLY_NAME;

    let mut result: Vec<IpAddr> = Vec::new();
    let mut size = 0u32;
    unsafe
    {
        let get_adapter_address_size_result = GetAdaptersAddresses(family, flags, null_mut(), null_mut(), &mut size);
        if get_adapter_address_size_result != ERROR_BUFFER_OVERFLOW {
            return Err(Error::new(ErrorKind::Other, format!("Failed to get size for adapter addresses. Error: {}", get_adapter_address_size_result)));
        }

        let addresses_memory: PIP_ADAPTER_ADDRESSES = std::mem::transmute(malloc(size as usize));

        let get_adapter_address_result = GetAdaptersAddresses(family, flags, null_mut(), addresses_memory, &mut size);
        if get_adapter_address_result != ERROR_SUCCESS {
            free(addresses_memory as *mut c_void);
            return Err(Error::new(ErrorKind::Other, format!("Failed to get adapter addresses. Error {}", get_adapter_address_result)));
        }

        let mut address = addresses_memory;
        while address != null_mut() {
            let deref_address = *address;
            let matches_type = deref_address.IfType == IF_TYPE_ETHERNET_CSMACD ||
                deref_address.IfType == IF_TYPE_IEEE80211;
            if matches_type && deref_address.OperStatus == IfOperStatusUp {
                let mut unicast_address = deref_address.FirstUnicastAddress;
                while unicast_address != null_mut() {
                    let deref_unicast_address = *unicast_address;
                    if (deref_unicast_address.u.s().Flags & IP_ADAPTER_ADDRESS_DNS_ELIGIBLE as u32) != 0 {
                        let raw_addr = deref_unicast_address.Address.lpSockaddr as *mut SOCKADDR_STORAGE;
                        if (*raw_addr).ss_family == AF_INET as u16 {
                            let ip4 = *(raw_addr as *mut SOCKADDR_IN);
                            let sin_addr: *const in_addr = &ip4.sin_addr;
                            let raw_ip: *const [u8; 4] = sin_addr as *const [u8; 4];
                            result.push(IpAddr::V4(Ipv4Addr::from(*raw_ip)));
                        }
                        else {
                            let ip6 = *(raw_addr as *mut SOCKADDR_IN6);
                            let sin_addr: *const in6_addr = &ip6.sin6_addr;
                            let raw_ip: *const [u8; 16] = sin_addr as *const [u8; 16];
                            IpAddr::V6(Ipv6Addr::from(*raw_ip));
                        }
                    }
                    unicast_address = deref_unicast_address.Next;
                }
            }
            address = deref_address.Next;
        }

        free(addresses_memory as *mut c_void);
    }

    return Ok(result);
}

#[cfg(unix)]
pub fn get_username() -> String {
    match env::var("USER") {
        Ok(val) => val.to_string(),
        Err(_) => match env::var("USERNAME") {
            Ok(val) => val.to_string(),
            Err(_) => "Unknown".to_string(),
        },
    }
}

#[cfg(windows)]
pub fn get_username() -> String {
    match env::var("USERNAME") {
        Ok(val) => val.to_string(),
        Err(_) => "Unknown".to_string(),
    }
}

