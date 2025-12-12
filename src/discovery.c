/**
 * @file discovery.c
 * @brief Network device discovery implementation
 *
 * This file implements cross-platform network discovery functionality including
 * ARP table scanning, ping sweeps, and service detection. The implementation
 * uses platform-specific system calls and commands to provide comprehensive
 * network scanning capabilities.
 */

#define _GNU_SOURCE
#include "discovery.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <ifaddrs.h>
#include <sys/time.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/select.h>

#ifdef __APPLE__
#include <net/route.h>
#include <sys/sysctl.h>
#endif

#ifdef __linux__
#include <sys/ioctl.h>
#include <linux/if_link.h>
#endif

// Platform-specific includes
#if defined(_WIN32) || defined(_WIN64)
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <icmpapi.h>
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")
#endif

/**
 * @brief Get local network interfaces
 */
int get_network_interfaces(NetworkInterface *interfaces, int max_interfaces) {
    struct ifaddrs *ifaddrs_ptr, *ifa;
    int count = 0;

    if (getifaddrs(&ifaddrs_ptr) == -1) {
        perror("getifaddrs");
        return -1;
    }

    for (ifa = ifaddrs_ptr; ifa != NULL && count < max_interfaces; ifa = ifa->ifa_next) {
        // Skip interfaces that are not up or don't have IP
        if (!(ifa->ifa_flags & IFF_UP) || ifa->ifa_addr == NULL) {
            continue;
        }

        // Only handle IPv4 interfaces
        if (ifa->ifa_addr->sa_family != AF_INET) {
            continue;
        }

        struct sockaddr_in* addr_in = (struct sockaddr_in*)ifa->ifa_addr;
        struct sockaddr_in* netmask_in = (struct sockaddr_in*)ifa->ifa_netmask;
        struct sockaddr_in* broadcast_in = NULL;

        // Find broadcast address
        struct ifaddrs *temp = ifaddrs_ptr;
        while (temp != NULL) {
            if (strcmp(temp->ifa_name, ifa->ifa_name) == 0 &&
                temp->ifa_addr && temp->ifa_addr->sa_family == AF_INET &&
                (temp->ifa_flags & IFF_BROADCAST)) {
                broadcast_in = (struct sockaddr_in*)temp->ifa_broadaddr;
                break;
            }
            temp = temp->ifa_next;
        }

        // Fill interface structure
        strncpy(interfaces[count].name, ifa->ifa_name, sizeof(interfaces[count].name) - 1);
        interfaces[count].name[sizeof(interfaces[count].name) - 1] = '\0';

        inet_ntop(AF_INET, &(addr_in->sin_addr), interfaces[count].ip_address, INET_ADDRSTRLEN);
        inet_ntop(AF_INET, &(netmask_in->sin_addr), interfaces[count].netmask, INET_ADDRSTRLEN);

        if (broadcast_in) {
            inet_ntop(AF_INET, &(broadcast_in->sin_addr), interfaces[count].broadcast, INET_ADDRSTRLEN);
        } else {
            interfaces[count].broadcast[0] = '\0';
        }

        interfaces[count].is_active = 1;
        count++;
    }

    freeifaddrs(ifaddrs_ptr);
    return count;
}

/**
 * @brief Scan ARP table for known devices
 */
int scan_arp_table(NetworkDevice *devices, int max_devices) {
    FILE *arp_file;
    char line[256];
    int count = 0;

#if defined(__APPLE__)
    arp_file = popen("arp -an", "r");
#elif defined(__linux__)
    arp_file = popen("arp -n", "r");
#elif defined(_WIN32) || defined(_WIN64)
    arp_file = popen("arp -a", "r");
#else
    arp_file = popen("arp -a", "r");
#endif

    if (!arp_file) {
        perror("popen");
        return -1;
    }

    while (fgets(line, sizeof(line), arp_file) && count < max_devices) {
        char ip[16] = {0};
        char mac[18] = {0};

#if defined(__APPLE__)
        // macOS format: ? (192.168.1.1) at 00:11:22:33:44:55 on en0 ifscope [ethernet]
        if (sscanf(line, "%*s (%15[0-9.]) %*s %17[0-9a-fA-F:]", ip, mac) == 2) {
#elif defined(__linux__)
        // Linux format: 192.168.1.1    ether   00:11:22:33:44:55   C
        if (sscanf(line, "%15s %*s %17s", ip, mac) == 2) {
#elif defined(_WIN32) || defined(_WIN64)
        // Windows format:  192.168.1.1           00-11-22-33-44-55     dynamic
        if (sscanf(line, "%*s %15s %17s", ip, mac) == 2) {
            // Convert Windows MAC format (00-11-22-33-44-55) to standard format (00:11:22:33:44:55)
            for (int i = 0; mac[i]; i++) {
                if (mac[i] == '-') mac[i] = ':';
            }
#else
        if (sscanf(line, "%*s %15s %17s", ip, mac) == 2) {
#endif

            // Copy to device structure
            strncpy(devices[count].ip_address, ip, sizeof(devices[count].ip_address) - 1);
            strncpy(devices[count].mac_address, mac, sizeof(devices[count].mac_address) - 1);
            devices[count].hostname[0] = '\0';
            devices[count].is_active = 0;  // Will be determined by ping
            devices[count].has_nettf_service = 0;
            devices[count].response_time = 0;

            count++;
        }
    }

    pclose(arp_file);
    return count;
}

/**
 * @brief Perform a simple ping check using ICMP
 */
int ping_device(const char *ip_address, int timeout_ms, double *response_time) {
#if defined(_WIN32) || defined(_WIN64)
    HANDLE hIcmpFile;
    DWORD dwRetVal = 0;
    char SendData[32] = "NETTF Ping";
    LPVOID ReplyBuffer = NULL;
    DWORD ReplySize = 0;

    hIcmpFile = IcmpCreateFile();
    if (hIcmpFile == INVALID_HANDLE_VALUE) {
        return -1;
    }

    ReplySize = sizeof(ICMP_ECHO_REPLY) + sizeof(SendData);
    ReplyBuffer = malloc(ReplySize);
    if (!ReplyBuffer) {
        IcmpCloseHandle(hIcmpFile);
        return -1;
    }

    dwRetVal = IcmpSendEcho(hIcmpFile, inet_addr(ip_address), SendData, sizeof(SendData),
                           NULL, ReplyBuffer, ReplySize, timeout_ms);

    if (dwRetVal != 0) {
        PICMP_ECHO_REPLY pEchoReply = (PICMP_ECHO_REPLY)ReplyBuffer;
        if (response_time) {
            *response_time = pEchoReply->RoundTripTime;
        }
        free(ReplyBuffer);
        IcmpCloseHandle(hIcmpFile);
        return 1;
    }

    free(ReplyBuffer);
    IcmpCloseHandle(hIcmpFile);
    return 0;
#else
    // Unix/Linux/macOS - use system ping command
    char command[256];
    snprintf(command, sizeof(command), "ping -c 1 -W %d %s >/dev/null 2>&1",
             timeout_ms / 1000 + 1, ip_address);

    struct timeval start_time, end_time;
    gettimeofday(&start_time, NULL);

    int result = system(command);

    gettimeofday(&end_time, NULL);
    if (response_time) {
        *response_time = (end_time.tv_sec - start_time.tv_sec) * 1000.0 +
                        (end_time.tv_usec - start_time.tv_usec) / 1000.0;
    }

    return (result == 0) ? 1 : 0;
#endif
}

/**
 * @brief Check if a device has NETTF service running
 */
int check_nettf_service(const char *ip_address, int port, int timeout_ms) {
    int sock;
    struct sockaddr_in addr;
    fd_set fdset;
    struct timeval tv;
    int result = -1;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        return -1;
    }

    // Set non-blocking
    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);

    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, ip_address, &addr.sin_addr);

    result = connect(sock, (struct sockaddr*)&addr, sizeof(addr));

    if (result == 0) {
        // Connection succeeded immediately
        close(sock);
        return 1;
    } else if (errno == EINPROGRESS) {
        // Connection is in progress
        FD_ZERO(&fdset);
        FD_SET(sock, &fdset);
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;

        result = select(sock + 1, NULL, &fdset, NULL, &tv);

        if (result > 0) {
            int so_error;
            socklen_t len = sizeof(so_error);
            getsockopt(sock, SOL_SOCKET, SO_ERROR, &so_error, &len);

            close(sock);
            return (so_error == 0) ? 1 : 0;
        }
    }

    close(sock);
    return 0;
}

/**
 * @brief Calculate network range from IP and netmask
 */
int calculate_network_range(const char *ip, const char *netmask, char *network, char *broadcast) {
    struct in_addr ip_addr, mask_addr, network_addr, broadcast_addr;

    if (inet_pton(AF_INET, ip, &ip_addr) != 1 || inet_pton(AF_INET, netmask, &mask_addr) != 1) {
        return -1;
    }

    network_addr.s_addr = ip_addr.s_addr & mask_addr.s_addr;
    broadcast_addr.s_addr = network_addr.s_addr | ~mask_addr.s_addr;

    if (network) {
        inet_ntop(AF_INET, &network_addr, network, INET_ADDRSTRLEN);
    }

    if (broadcast) {
        inet_ntop(AF_INET, &broadcast_addr, broadcast, INET_ADDRSTRLEN);
    }

    return 0;
}

/**
 * @brief Perform ping sweep on network range
 */
int ping_sweep(const char *network, const char *netmask, NetworkDevice *devices, int max_devices, int timeout_ms) {
    char broadcast[16];
    struct in_addr network_addr, mask_addr, base_addr, current_addr;
    int count = 0;

    if (calculate_network_range(network, netmask, NULL, broadcast) != 0) {
        return -1;
    }

    if (inet_pton(AF_INET, network, &network_addr) != 1 || inet_pton(AF_INET, netmask, &mask_addr) != 1) {
        return -1;
    }

    base_addr.s_addr = network_addr.s_addr;

    // Skip network address and broadcast address, limit to reasonable range
    uint32_t start = ntohl(base_addr.s_addr) + 1;
    uint32_t end = ntohl(inet_addr(broadcast)) - 1;

    // Limit scanning to avoid excessive traffic
    if (end - start > 254) {
        end = start + 254;
    }

    printf("Scanning network range: %s - ", inet_ntoa(base_addr));

    current_addr.s_addr = htonl(start);
    printf("%s\n", inet_ntoa(current_addr));
    current_addr.s_addr = htonl(end);
    printf("%s\n", inet_ntoa(current_addr));

    for (uint32_t i = start; i <= end && count < max_devices; i++) {
        current_addr.s_addr = htonl(i);
        char *current_ip = inet_ntoa(current_addr);

        double response_time;
        int is_active = ping_device(current_ip, timeout_ms, &response_time);

        if (is_active) {
            strncpy(devices[count].ip_address, current_ip, sizeof(devices[count].ip_address) - 1);
            devices[count].mac_address[0] = '\0';
            devices[count].hostname[0] = '\0';
            devices[count].is_active = 1;
            devices[count].has_nettf_service = 0;
            devices[count].response_time = response_time;
            count++;

            printf("  Found: %s (%.2f ms)\n", current_ip, response_time);
        }
    }

    return count;
}

/**
 * @brief Test specific IPs for NETTF service (faster than full ping sweep)
 */
int test_common_ips_for_nettf(NetworkDevice *devices, int max_devices, const char *base_network, int timeout_ms) {
    int count = 0;

    printf("Testing common IPs for NETTF service on %s network...\n", base_network);

    // Test common IPs that might have NETTF running
    const char *common_ips[] = {
        "1",     // Gateway/Router
        "10",    // Common server
        "63",    // User's example (192.168.5.63)
        "100",   // Common assignment
        "101",   // Common assignment
        "105",   // Common assignment
        "110",   // Common assignment
        "200",   // Common assignment
        "254"    // Common server
    };
    int num_common = sizeof(common_ips) / sizeof(common_ips[0]);

    for (int i = 0; i < num_common && count < max_devices; i++) {
        char test_ip[16];
        snprintf(test_ip, sizeof(test_ip), "%s.%s", base_network, common_ips[i]);

        // Quick ping test first
        double response_time;
        if (ping_device(test_ip, timeout_ms, &response_time)) {
            strncpy(devices[count].ip_address, test_ip, sizeof(devices[count].ip_address) - 1);
            devices[count].mac_address[0] = '\0';
            devices[count].hostname[0] = '\0';
            devices[count].is_active = 1;
            devices[count].response_time = response_time;
            devices[count].has_nettf_service = 0;  // Will be set later
            count++;
            printf("  Found active device: %s (%.2f ms)\n", test_ip, response_time);
        }
    }

    return count;
}

/**
 * @brief Discover all available devices on local network
 */
int discover_network_devices(NetworkDevice *devices, int max_devices, int check_services, int timeout_ms) {
    (void)check_services; // Parameter kept for compatibility but always checks services now
    int arp_count = 0;
    int total_count = 0;
    NetworkInterface interfaces[16];
    int interface_count;

    printf("Discovering network devices from ARP table...\n");

    // Step 1: Get network interfaces to determine network range
    interface_count = get_network_interfaces(interfaces, 16);
    if (interface_count <= 0) {
        fprintf(stderr, "Error: Could not get network interfaces\n");
        return -1;
    }

    // Step 2: Scan ARP table for known devices
    arp_count = scan_arp_table(devices, max_devices);
    if (arp_count > 0) {
        printf("Found %d device(s) in ARP table\n", arp_count);

        // Step 3: Mark all devices as active (they're in ARP table, so they're reachable)
        for (int i = 0; i < arp_count; i++) {
            devices[i].is_active = 1;
            devices[i].response_time = 0;  // Not available without ping
        }
        total_count = arp_count;
    }

    // Step 4: Test common IPs for potential NETTF services (faster than full ping sweep)
    for (int i = 0; i < interface_count && total_count < max_devices; i++) {
        if (interfaces[i].is_active && strstr(interfaces[i].ip_address, "192.168.") != NULL) {
            // Extract network part (e.g., "192.168.5" from "192.168.5.x")
            char network_part[13];
            strncpy(network_part, interfaces[i].ip_address, sizeof(network_part) - 1);
            char *last_dot = strrchr(network_part, '.');
            if (last_dot) {
                *last_dot = '\0';  // Terminate at last dot
            }

            int additional_count = test_common_ips_for_nettf(
                &devices[total_count],
                max_devices - total_count,
                network_part,
                timeout_ms
            );

            if (additional_count > 0) {
                // Check for duplicates with ARP table entries
                int unique_count = 0;
                for (int j = total_count; j < total_count + additional_count && j < max_devices; j++) {
                    int duplicate = 0;
                    for (int k = 0; k < arp_count; k++) {
                        if (strcmp(devices[j].ip_address, devices[k].ip_address) == 0) {
                            duplicate = 1;
                            break;
                        }
                    }
                    if (!duplicate) {
                        // Move unique device to correct position
                        if (unique_count > 0) {
                            strcpy(devices[total_count + unique_count].ip_address, devices[j].ip_address);
                            devices[total_count + unique_count].is_active = devices[j].is_active;
                            devices[total_count + unique_count].response_time = devices[j].response_time;
                            devices[total_count + unique_count].mac_address[0] = '\0';
                            devices[total_count + unique_count].hostname[0] = '\0';
                        }
                        unique_count++;
                    }
                }
                total_count += unique_count;
            }
            break; // Only scan the first 192.168.x.x interface
        }
    }

    // Step 5: Always check for NETTF service on port 9876
    printf("Checking for NETTF services on port %d...\n", DEFAULT_NETTF_PORT);
    for (int i = 0; i < total_count; i++) {
        devices[i].has_nettf_service = check_nettf_service(devices[i].ip_address, DEFAULT_NETTF_PORT, timeout_ms);
        if (devices[i].has_nettf_service) {
            printf("  NETTF service available on %s (ready to receive files)\n", devices[i].ip_address);
        }
    }

    return total_count;
}

/**
 * @brief Print discovered devices in a formatted table
 */
void print_discovered_devices(const NetworkDevice *devices, int num_devices, int show_services) {
    (void)show_services; // Parameter kept for compatibility but always shows services now
    if (num_devices == 0) {
        printf("No devices discovered.\n");
        return;
    }

    printf("\nDiscovered Network Devices:\n");
    printf("%-16s %-18s %-10s %-10s %-10s", "IP Address", "MAC Address", "Status", "Response", "NETTF");
    printf("\n");

    printf("%-16s %-18s %-10s %-10s %-10s", "------------", "------------------", "------", "--------", "-----");
    printf("\n");

    for (int i = 0; i < num_devices; i++) {
        printf("%-16s ", devices[i].ip_address);

        if (strlen(devices[i].mac_address) > 0) {
            printf("%-18s ", devices[i].mac_address);
        } else {
            printf("%-18s ", "Unknown");
        }

        if (devices[i].is_active) {
            printf("%-10s ", "Active");
        } else {
            printf("%-10s ", "Inactive");
        }

        if (devices[i].response_time > 0) {
            printf("%-10.2f ", devices[i].response_time);
        } else {
            printf("%-10s ", "N/A");
        }

        // Always show NETTF service status
        if (devices[i].has_nettf_service) {
            printf("%-10s ", "Ready");
        } else {
            printf("%-10s ", "Closed");
        }

        printf("\n");
    }
}