/**
 * @file discovery.h
 * @brief Network device discovery functionality
 *
 * This file defines the network discovery system for finding available devices
 * on the local network that can receive file transfers. The system combines
 * passive ARP table scanning with active network scanning to provide a
 * comprehensive list of reachable devices.
 */

#ifndef DISCOVERY_H
#define DISCOVERY_H

#include "platform.h"  // Cross-platform socket types and functions

// Default port configuration
#define DEFAULT_NETTF_PORT 9876

// Device information structure
typedef struct {
    char ip_address[16];        // IPv4 address string (xxx.xxx.xxx.xxx)
    char mac_address[18];       // MAC address string (xx:xx:xx:xx:xx:xx)
    char hostname[256];         // Device hostname if available
    int is_active;             // 1 if device responded to ping, 0 otherwise
    int has_nettf_service;     // 1 if device has NETTF service running, 0 otherwise
    double response_time;      // Ping response time in milliseconds
} NetworkDevice;

// Network interface information
typedef struct {
    char name[32];             // Interface name (e.g., "eth0", "en0")
    char ip_address[16];       // Interface IP address
    char netmask[16];          // Network mask
    char broadcast[16];        // Broadcast address
    int is_active;             // 1 if interface is up and has IP
} NetworkInterface;

// Function declarations for network discovery

/**
 * @brief Get local network interfaces
 *
 * @param interfaces Array to store interface information
 * @param max_interfaces Maximum number of interfaces to store
 * @return Number of interfaces found, -1 on error
 */
int get_network_interfaces(NetworkInterface *interfaces, int max_interfaces);

/**
 * @brief Scan ARP table for known devices
 *
 * @param devices Array to store device information
 * @param max_devices Maximum number of devices to store
 * @return Number of devices found in ARP table, -1 on error
 */
int scan_arp_table(NetworkDevice *devices, int max_devices);

/**
 * @brief Perform ping sweep on network range
 *
 * @param network Base network address (e.g., "192.168.1.0")
 * @param netmask Network mask (e.g., "255.255.255.0")
 * @param devices Array to store discovered devices
 * @param max_devices Maximum number of devices to store
 * @param timeout_ms Ping timeout in milliseconds
 * @return Number of active devices found, -1 on error
 */
int ping_sweep(const char *network, const char *netmask, NetworkDevice *devices, int max_devices, int timeout_ms);

/**
 * @brief Check if a device has NETTF service running on specified port
 *
 * @param ip_address IP address of the device to check
 * @param port Port number to check (default 8080)
 * @param timeout_ms Connection timeout in milliseconds
 * @return 1 if service is available, 0 if not, -1 on error
 */
int check_nettf_service(const char *ip_address, int port, int timeout_ms);

/**
 * @brief Discover all available devices on local network
 *
 * This is the main discovery function that combines ARP scanning,
 * ping sweep, and service detection to provide a comprehensive list
 * of available devices.
 *
 * @param devices Array to store discovered devices
 * @param max_devices Maximum number of devices to store
 * @param check_services Set to 1 to check for NETTF service, 0 to skip
 * @param timeout_ms Timeout for network operations in milliseconds
 * @return Number of devices discovered, -1 on error
 */
int discover_network_devices(NetworkDevice *devices, int max_devices, int check_services, int timeout_ms);

/**
 * @brief Print discovered devices in a formatted table
 *
 * @param devices Array of discovered devices
 * @param num_devices Number of devices in the array
 * @param show_services Set to 1 to show service status
 */
void print_discovered_devices(const NetworkDevice *devices, int num_devices, int show_services);

/**
 * @brief Calculate network range from IP and netmask
 *
 * @param ip IP address
 * @param netmask Network mask
 * @param network Output buffer for network address
 * @param broadcast Output buffer for broadcast address
 * @return 0 on success, -1 on error
 */
int calculate_network_range(const char *ip, const char *netmask, char *network, char *broadcast);

/**
 * @brief Perform a simple ping check
 *
 * @param ip_address Target IP address
 * @param timeout_ms Timeout in milliseconds
 * @param response_time Output for response time in milliseconds
 * @return 1 if reachable, 0 if not, -1 on error
 */
int ping_device(const char *ip_address, int timeout_ms, double *response_time);

#endif // DISCOVERY_H