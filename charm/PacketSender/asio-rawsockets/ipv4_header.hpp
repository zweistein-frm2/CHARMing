#ifndef IPV4_HEADER_HPP
#define IPV4_HEADER_HPP
#pragma once
#include <numeric>
#include <algorithm>
#include <iostream>
#include <boost/array.hpp>
#include <boost/asio.hpp>
#include <boost/cstdint.hpp>

///@ brief Mockup ipv4_header for with no options.
//
//  IPv4 wire format:
//
//  0                   1                   2                   3
//  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
// +-------+-------+-------+-------+-------+-------+-------+------+   ---
// |version|header |    type of    |    total length in bytes     |    ^
// |  (4)  | length|    service    |                              |    |
// +-------+-------+-------+-------+-------+-------+-------+------+    |
// |        identification         |flags|    fragment offset     |    |
// +-------+-------+-------+-------+-------+-------+-------+------+  20 bytes
// | time to live  |   protocol    |       header checksum        |    |
// +-------+-------+-------+-------+-------+-------+-------+------+    |
// |                      source IPv4 address                     |    |
// +-------+-------+-------+-------+-------+-------+-------+------+    |
// |                   destination IPv4 address                   |    v
// +-------+-------+-------+-------+-------+-------+-------+------+   ---
// /                       options (if any)                       /
// +-------+-------+-------+-------+-------+-------+-------+------+
class ipv4_header
{
public:
  ipv4_header() { std::fill(buffer_.begin(), buffer_.end(), 0); }

  void version(boost::uint8_t value) {
    buffer_[0] = (value << 4) | (buffer_[0] & 0x0F);
  }

  void header_length(boost::uint8_t value)
  {
    buffer_[0] = (value & 0x0F) | (buffer_[0] & 0xF0);
  }

  void type_of_service(boost::uint8_t value) { buffer_[1] = value; }
  void total_length(boost::uint16_t value) { encode16(2, value); }
  void identification(boost::uint16_t value) { encode16(4, value); }

  void dont_fragment(bool value)
  {
    buffer_[6] ^= (-value ^ buffer_[6]) & 0x40;
  }

  void more_fragments(bool value)
  {
    buffer_[6] ^= (-value ^ buffer_[6]) & 0x20;
  }

  void fragment_offset(boost::uint16_t value)
  {
     // Preserve flags.
     auto flags = static_cast<uint16_t>(buffer_[6] & 0xE0) << 8;
     encode16(6, (value & 0x1FFF) | flags);
  }

  void time_to_live(boost::uint8_t value) { buffer_[8] = value; }
  void protocol(boost::uint8_t value) { buffer_[9] = value; }
  void checksum(boost::uint16_t value) { encode16(10, value); }

  void source_address(boost::asio::ip::address_v4 value)
  {
    auto bytes = value.to_bytes();
    std::copy(bytes.begin(), bytes.end(), &buffer_[12]);
  }

  void destination_address(boost::asio::ip::address_v4 value)
  {
    auto bytes = value.to_bytes();
    std::copy(bytes.begin(), bytes.end(), &buffer_[16]);
  }

public:

  std::size_t size() const { return buffer_.size(); }

  const boost::array<uint8_t, 20>& data() const { return buffer_; }

private:

  void encode16(boost::uint8_t index, boost::uint16_t value)
  {
    buffer_[index] = (value >> 8) & 0xFF;
    buffer_[index + 1] = value & 0xFF;
  }

  boost::array<uint8_t, 20> buffer_;
};

void calculate_checksum(ipv4_header& header)
{
  // Zero out the checksum.
  header.checksum(0);

  // Checksum is the 16-bit one's complement of the one's complement sum of
  // all 16-bit words in the header.

  // Sum all 16-bit words.
  auto data = header.data();
  boost::uint16_t sum = std::accumulate<boost::uint16_t*, boost::uint32_t>(
    reinterpret_cast<boost::uint16_t*>(&data[0]),
    reinterpret_cast<boost::uint16_t*>(&data[0] + data.size()),
    0);

  // Fold 32-bit into 16-bits.
  while (sum >> 16)
  {
    sum = (sum & 0xFFFF) + (sum >> 16);
  }

  header.checksum(~sum);
}

///@brief Mockup IPv4 UDP header.
//
// UDP wire format:
//
//  0                   1                   2                   3
//  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
// +-------+-------+-------+-------+-------+-------+-------+------+   ---
// |          source port          |      destination port        |    ^
// +-------+-------+-------+-------+-------+-------+-------+------+  8 bytes
// |            length             |          checksum            |    v
// +-------+-------+-------+-------+-------+-------+-------+------+   ---
// /                        data (if any)                         /
// +-------+-------+-------+-------+-------+-------+-------+------+
class udp_header
{
public:
  udp_header() { std::fill(buffer_.begin(), buffer_.end(), 0); }

  void source_port(boost::uint16_t value)      { encode16(0, value); }
  void destination_port(boost::uint16_t value) { encode16(2, value); }
  void length(boost::uint16_t value)           { encode16(4, value); }
  void checksum(boost::uint16_t value)         { encode16(6, value); }

public:

  std::size_t size() const { return buffer_.size(); }

  const boost::array<uint8_t, 8>& data() const { return buffer_; }

private:

  void encode16(boost::uint8_t index, boost::uint16_t value)
  {
    buffer_[index] = (value >> 8) & 0xFF;
    buffer_[index + 1] = value & 0xFF;
  }

  boost::array<uint8_t, 8> buffer_;
};

#endif

