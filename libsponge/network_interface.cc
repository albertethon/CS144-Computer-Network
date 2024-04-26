#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"

#include <iostream>
#include <set>

// Dummy implementation of a network interface
// Translates from {IP datagram, next hop address} to link-layer frame, and from link-layer frame to IP datagram

// For Lab 5, please replace with a real implementation that passes the
// automated checks run by `make check_lab5`.

// You will need to add private members to the class declaration in `network_interface.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface(const EthernetAddress &ethernet_address, const Address &ip_address)
    : _ethernet_address(ethernet_address), _ip_address(ip_address) {
    cerr << "DEBUG: Network interface has Ethernet address " << to_string(_ethernet_address) << " and IP address "
         << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but may also be another host if directly connected to the same network as the destination)
//! (Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) with the Address::ipv4_numeric() method.)
void NetworkInterface::send_datagram(const InternetDatagram &dgram, const Address &next_hop) {
    // convert IP address of next hop to raw 32-bit representation (used in ARP header)
    const uint32_t next_hop_ip = next_hop.ipv4_numeric();

    // 查询 ARP 缓存
    EthernetFrame ethernetframe;

    // 如果存在该IP - MAC映射 则直接发送, 将负载设为dgram的序列化
    if (_arp_cache.find(next_hop_ip) != _arp_cache.end()) {
        ethernetframe.header() = {/* dst  */ _arp_cache[next_hop_ip],
                                  /* src  */ _ethernet_address,
                                  /* type */ EthernetHeader::TYPE_IPv4};
        ethernetframe.payload() = dgram.serialize();
        _frames_out.push(ethernetframe);
        return;
    }
    // 否则先存储这个InternetDatagram
    _queued_IPData[next_hop_ip].push(dgram);
    // 如果不存在该 IP 则应当发送ARP探测查询TPA对应的THA
    //  1. 如果已经发送过且未超时则不用重复发送, 超时由tick自动发送
    if (_arp_sent.find(next_hop_ip) != _arp_sent.end())
        return;
    //  2. 没发送过该ip的arp_request
    _ARP_Request(next_hop_ip);
}

void NetworkInterface::_ARP_Request(const uint32_t next_hop_ip) {
    EthernetFrame ethernetframe;
    ethernetframe.header() = {/* dst  */ ETHERNET_BROADCAST,
                              /* src  */ _ethernet_address,
                              /* type */ EthernetHeader::TYPE_ARP};
    ARPMessage arp_msg;
    /* OP  */ arp_msg.opcode = arp_msg.OPCODE_REQUEST;
    /* SHA */ arp_msg.sender_ethernet_address = _ethernet_address;
    /* SPA */ arp_msg.sender_ip_address = _ip_address.ipv4_numeric();
    /* TPA */ arp_msg.target_ip_address = next_hop_ip;
    ethernetframe.payload() = arp_msg.serialize();
    _frames_out.push(ethernetframe);

    // 更新arp_sent队列中对应IP arp 的等待时间
    _arp_sent[next_hop_ip] = 0;
}

void NetworkInterface::_ARP_Reply(ARPMessage &&target_reply) {
    EthernetFrame ethernetframe;
    ethernetframe.header() = {/* dst  */ target_reply.sender_ethernet_address,
                              /* src  */ _ethernet_address,
                              /* type */ EthernetHeader::TYPE_ARP};

    /*  OP  */ target_reply.opcode = target_reply.OPCODE_REPLY;
    /*  THA */ target_reply.target_ethernet_address = target_reply.sender_ethernet_address;
    /*  TPA */ target_reply.target_ip_address = target_reply.sender_ip_address;
    /*  SHA */ target_reply.sender_ethernet_address = _ethernet_address;
    /*  SPA */ target_reply.sender_ip_address = _ip_address.ipv4_numeric();

    // 学习这个map
    _arp_cache[target_reply.target_ip_address] = target_reply.target_ethernet_address;
    _arp_time[make_pair(target_reply.target_ip_address, target_reply.target_ethernet_address)] = 0;

    ethernetframe.payload() = target_reply.serialize();  // 序列化并给入payload

    _frames_out.push(ethernetframe);  // 发送
}

//! \param[in] frame the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame &frame) {
    const EthernetHeader &eheader = frame.header();
    const BufferList &ebody = frame.payload();

    // 无视目标MAC和当前不同的包(除了广播)
    if (eheader.dst != _ethernet_address && eheader.dst != ETHERNET_BROADCAST)
        return nullopt;

    // 如果收到了ARP 包
    if (eheader.type == EthernetHeader::TYPE_ARP) {
        ARPMessage arp_msg;
        if (arp_msg.parse(ebody) == ParseResult::NoError) {
            // 如果收到了ARP reply, 且dst就是本机MAC, 移除arp_sent中对应项, 并发送之前等待的 InternetDatagram 包
            if (arp_msg.opcode == arp_msg.OPCODE_REPLY && eheader.dst == _ethernet_address) {
                auto to_ip = arp_msg.sender_ip_address;
                if (_arp_sent.find(to_ip) != _arp_sent.end()) {
                    // 找到sent 队列, 此时移除
                    _arp_sent.erase(to_ip);
                    // 从收到的ARP reply学习map
                    // 维护一个arp_cache, 并且维护该pair的过期时间
                    _arp_cache[to_ip] = arp_msg.sender_ethernet_address;
                    _arp_time[make_pair(to_ip, arp_msg.sender_ethernet_address)] = 0;
                    // 发送之前排队等候的包
                    while (!_queued_IPData[to_ip].empty()) {
                        auto qframe = _queued_IPData[to_ip].front();
                        _queued_IPData[to_ip].pop();
                        send_datagram(qframe, Address::from_ipv4_numeric(to_ip));
                    }
                }
            }

            // 如果收到了ARP request, 且ARP的TPA 是本机IP, 则返回本机MAC
            else if (arp_msg.opcode == arp_msg.OPCODE_REQUEST &&
                     arp_msg.target_ip_address == _ip_address.ipv4_numeric()) {
                // 从收到的ARP request中 学习map
                _ARP_Reply(std::move(arp_msg));  // 回复本机MAC
            }
            return nullopt;
        }
    }
    // 收到IP datagram
    else if (eheader.type == EthernetHeader::TYPE_IPv4) {
        optional<InternetDatagram> ret_InternetData = InternetDatagram();
        if (ret_InternetData->parse(ebody.concatenate()) == ParseResult::NoError) {
            if (ret_InternetData.has_value()) {
                return ret_InternetData;
            }
        }
    }
    return nullopt;
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick(const size_t ms_since_last_tick) {
    for (auto &it : _arp_sent) {
        it.second += ms_since_last_tick;
        // arp请求大于5000秒就重发
        if (it.second >= 5000) {
            _ARP_Request(it.first);
        }
    }
    std::set<pair<uint32_t, EthernetAddress>> remove_set{};
    for (auto &it : _arp_time) {
        it.second += ms_since_last_tick;
        // ip映射大于30秒则过期
        if (it.second >= 30000) {
            _arp_cache.erase(it.first.first);
            remove_set.insert(it.first);
        }
    }
    for (auto it : remove_set) {
        _arp_time.erase(it);
    }
}
