#include "router.hh"

#include <iostream>

using namespace std;

// Dummy implementation of an IP router

// Given an incoming Internet datagram, the router decides
// (1) which interface to send it out on, and
// (2) what next hop address to send it to.

// For Lab 6, please replace with a real implementation that passes the
// automated checks run by `make check_lab6`.

// You will need to add private members to the class declaration in `router.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

//! \param[in] route_prefix The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
//! \param[in] prefix_length For this route to be applicable, how many high-order (most-significant) bits of the route_prefix will need to match the corresponding bits of the datagram's destination address?
//! \param[in] next_hop The IP address of the next hop. Will be empty if the network is directly attached to the router (in which case, the next hop address should be the datagram's final destination).
//! \param[in] interface_num The index of the interface to send the datagram out on.
void Router::add_route(const uint32_t route_prefix,
                       const uint8_t prefix_length,
                       const optional<Address> next_hop,
                       const size_t interface_num) {
    cerr << "DEBUG: adding route " << Address::from_ipv4_numeric(route_prefix).ip() << "/" << int(prefix_length)
         << " => " << (next_hop.has_value() ? next_hop->ip() : "(direct)") << " on interface " << interface_num << "\n";

    if (prefix_length > 32)
        return;

    uint32_t mast_code = ~((1ul << (32 - prefix_length)) - 1);
    uint32_t rout_net = route_prefix & mast_code;
    // direct link
    if (!next_hop.has_value()) {
        _next_hop2interface[rout_net] = interface_num;
    } else {
        _next_hop[make_pair(route_prefix, prefix_length)] = next_hop->ipv4_numeric();
        _next_hop2interface[next_hop->ipv4_numeric()] = interface_num;
    }
}

//! \param[in] dgram The datagram to be routed
void Router::route_one_datagram(InternetDatagram &dgram) {
    auto route_prefix = dgram.header().dst;
    int prefix_length = 32;
    // 没找到就逐渐缩减mask长度
    while (prefix_length >= 0) {
        uint32_t mast_code = ~((1ul << (32 - prefix_length)) - 1);
        uint32_t rout_net = route_prefix & mast_code;
        // route link
        if (_next_hop.find(make_pair(rout_net, prefix_length)) != _next_hop.end()) {
            auto _next_hop_id = _next_hop[make_pair(rout_net, prefix_length)];
            if (_next_hop2interface.find(_next_hop_id) == _next_hop2interface.end()) {
                cout << "ERROR, not found interface" << Address::from_ipv4_numeric(_next_hop_id).ip() << "\n";
                return;
            }
            if (dgram.header().ttl > 1) {
                dgram.header().ttl--;
                auto interface_id = _next_hop2interface[_next_hop_id];
                _interfaces[interface_id].send_datagram(dgram, Address::from_ipv4_numeric(_next_hop_id));
            }
            return;
        }
        // direct link
        if (_next_hop2interface.find(rout_net) != _next_hop2interface.end()) {
            auto interface_id = _next_hop2interface[rout_net];
            if (interface_id > _interfaces.size()) {
                cout << "ERROR: interface id:\t" << interface_id << "not found" << endl;
                return;
            }
            if (dgram.header().ttl > 1) {
                dgram.header().ttl--;
                _interfaces[interface_id].send_datagram(dgram, Address::from_ipv4_numeric(route_prefix));
            }
            return;
        }
        --prefix_length;
    }
}

void Router::route() {
    // Go through all the interfaces, and route every incoming datagram to its proper outgoing interface.
    for (auto &interface : _interfaces) {
        auto &queue = interface.datagrams_out();
        while (not queue.empty()) {
            route_one_datagram(queue.front());
            queue.pop();
        }
    }
}
