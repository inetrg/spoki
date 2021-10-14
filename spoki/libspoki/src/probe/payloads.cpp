/*
 * This file is part of the CAF spoki driver.
 *
 * Copyright (C) 2018-2021
 * Authors: Raphael Hiesgen
 *
 * All rights reserved.
 *
 * Report any bugs, questions or comments to raphael.hiesgen@haw-hamburg.de
 *
 */

#include "spoki/probe/payloads.hpp"

#include <iomanip>
#include <sstream>

namespace spoki::probe {

namespace {

struct hex {
  hex(char c) : c(static_cast<unsigned char>(c)) {
    // nop
  }
  unsigned char c;
};

inline std::ostream& operator<<(std::ostream& o, const hex& hc) {
  return (o << std::setw(2) << std::setfill('0') << std::hex
            << static_cast<int>(hc.c));
}

} // namespace

payload_map get_payloads() {
  payload_map payloads;
  // Found theses in nmap, https://nmap.org. See https://nmap.org/data/COPYING
  // for nmap license. This software is open source and does not have a
  // commercial license. See 'nmap-payloads' file.
  payloads[7] = {'\x0D', '\x0A', '\x0D', '\x0A'};
  // DNS
  payloads[53] = {'\x00', '\x00', '\x10', '\x00', '\x00', '\x00',
                  '\x00', '\x00', '\x00', '\x00', '\x00', '\x00'};
  // QUIC
  payloads[80] = {'\r', '1', '2', '3', '4', '5', '6',
                  '7',  '8', 'Q', '9', '9', '9', '\x00'};
  // RPCCheck
  payloads[111] = {'\x72', '\xFE', '\x1D', '\x13', '\x00', '\x00', '\x00',
                   '\x00', '\x00', '\x00', '\x00', '\x02', '\x00', '\x01',
                   '\x86', '\xA0', '\x00', '\x01', '\x97', '\x7C', '\x00',
                   '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00',
                   '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00',
                   '\x00', '\x00', '\x00', '\x00', '\x00'};
  // NTPRequst
  payloads[123] = {'\xE3', '\x00', '\x04', '\xFA', '\x00', '\x01', '\x00',
                   '\x00', '\x00', '\x01', '\x00', '\x00', '\x00', '\x00',
                   '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00',
                   '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00',
                   '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00',
                   '\x00', '\x00', '\x00', '\x00', '\x00', '\xC5', '\x4F',
                   '\x23', '\x4B', '\x71', '\xB1', '\x52', '\xF3'};
  // NBTStat
  payloads[137] = {'\x80', '\xF0', '\x00', '\x10', '\x00', '\x01', '\x00',
                   '\x00', '\x00', '\x00', '\x00', '\x00',
                   //"\x20CKAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\x00\x00\x21\x00\x01";
                   '\x20', 'C', 'K', 'A', 'A', 'A', 'A', 'A', 'A', 'A', 'A',
                   'A', 'A', 'A', 'A', 'A', 'A', 'A', 'A', 'A', 'A', 'A', 'A',
                   'A', 'A', 'A', 'A', 'A', 'A', 'A', 'A', 'A', 'A', '\x00',
                   '\x00', '\x21', '\x00', '\x01'};
  // SNMPv3GetRequest
  payloads[161] = {'\x30', '\x3A', '\x02', '\x01', '\x03', '\x30', '\x0F',
                   '\x02', '\x02', '\x4A', '\x69', '\x02', '\x03', '\x00',
                   '\xFF', '\xE3', '\x04', '\x01', '\x04', '\x02', '\x01',
                   '\x03', '\x04', '\x10', '\x30', '\x0E', '\x04', '\x00',
                   '\x02', '\x01', '\x00', '\x02', '\x01', '\x00', '\x04',
                   '\x00', '\x04', '\x00', '\x04', '\x00', '\x30', '\x12',
                   '\x04', '\x00', '\x04', '\x00', '\xA0', '\x0C', '\x02',
                   '\x02', '\x37', '\xF0', '\x02', '\x01', '\x00', '\x02',
                   '\x01', '\x00', '\x30', '\x00'};
  // Sqlping (trips a Snort rule with SID 2049) - "MS-SQL ping attempt"
  // payloads[1434] = "', '\x02";
  // xdmcp
  payloads[177] = {'\x00', '\x01', '\x00', '\x02', '\x00', '\x01', '\x00'};
  // Connectionless LDAP
  payloads[389] = {'\x30', '\x84', '\x00', '\x00', '\x00', '\x2d', '\x02',
                   '\x01', '\x07', '\x63', '\x84', '\x00', '\x00', '\x00',
                   '\x24', '\x04', '\x00', '\x0a', '\x01', '\x00', '\x0a',
                   '\x01', '\x00', '\x02', '\x01', '\x00', '\x02', '\x01',
                   '\x64', '\x01', '\x01', '\x00', '\x87', '\x0b', '\x6f',
                   '\x62', '\x6a', '\x65', '\x63', '\x74', '\x43', '\x6c',
                   '\x61', '\x73', '\x73', '\x30', '\x84', '\x00', '\x00',
                   '\x00', '\x00'};
  // svrloc
  payloads[427] = {'\x02', '\x01', '\x00', '\x00', '6', ' ', '\x00', '\x00',
                   '\x00', '\x00', '\x00', '\x01', '\x00', '\x02', 'e', 'n',
                   '\x00', '\x00', '\x00', '\x15',
                   // Not sure if we should "break" the string here, but there
                   // is a warning ...
                   //"service:service-agent', '\x00', '\x07default', '\x00',
                   //'\x00', '\x00', '\x00";
                   's', 'e', 'r', 'v', 'i', 'c', 'e', ':', 's', 'e', 'r', 'v',
                   'i', 'c', 'e', '-', 'a', 'g', 'e', 'n', 't', '\x00', '\x07',
                   'd', 'e', 'f', 'a', 'u', 'l', 't', '\x00', '\x00', '\x00',
                   '\x00'};
  // DTLS
  std::vector<char>
    dtls_str = {// DTLS 1.0, length 52
                '\x16', '\xfe', '\xff', '\x00', '\x00', '\x00', '\x00', '\x00',
                '\x00', '\x00', '\x00', '\x00', '\x36',
                // ClientHello, length 40, sequence 0, offset 0
                '\x01', '\x00', '\x00', '\x2a', '\x00', '\x00', '\x00', '\x00',
                '\x00', '\x00', '\x00', '\x2a',
                // DTLS 1.2
                '\xfe', '\xfd',
                // Random
                '\x00', '\x00', '\x00', '\x00', '\x7c', '\x77', '\x40', '\x1e',
                '\x8a', '\xc8', '\x22', '\xa0', '\xa0', '\x18', '\xff', '\x93',
                '\x08', '\xca', '\xac', '\x0a', '\x64', '\x2f', '\xc9', '\x22',
                '\x64', '\xbc', '\x08', '\xa8', '\x16', '\x89', '\x19', '\x3f',
                // Session id length 0, cookie length 0
                '\x00', '\x00',
                // Cipher suites, mandatory TLS_RSA_WITH_AES_128_CBC_SHA
                '\x00', '\x02', '\x00', '\x2f',
                // Compressors (NULL)
                '\x01', '\x00'};
  payloads[443] = dtls_str;
  payloads[853] = dtls_str;
  payloads[4433] = dtls_str;
  payloads[4740] = dtls_str;
  payloads[5349] = dtls_str;
  payloads[5684] = dtls_str;
  payloads[5868] = dtls_str;
  payloads[6514] = dtls_str;
  payloads[6636] = dtls_str;
  payloads[8232] = dtls_str;
  payloads[10161] = dtls_str;
  payloads[10162] = dtls_str;
  payloads[12346] = dtls_str;
  payloads[12446] = dtls_str;
  payloads[12546] = dtls_str;
  payloads[12646] = dtls_str;
  payloads[12746] = dtls_str;
  payloads[12846] = dtls_str;
  payloads[12946] = dtls_str;
  payloads[13046] = dtls_str;
  // Internet Key Exchange v1
  payloads[500] = {// Initiator cookie 0x0011223344556677, responder cookie
                   // 0x0000000000000000.
                   '\x00', '\x11', '\x22', '\x33', '\x44', '\x55', '\x66',
                   '\x77', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00',
                   '\x00', '\x00',
                   // Version 1, Main Mode, flags 0x00, message ID 0x00000000,
                   // length 192.
                   '\x01', '\x10', '\x02', '\x00', '\x00', '\x00', '\x00',
                   '\x00', '\x00', '\x00', '\x00', '\xC0',
                   // Security Association payload, length 164, IPSEC, IDENTITY.
                   '\x00', '\x00', '\x00', '\xA4', '\x00', '\x00', '\x00',
                   '\x01', '\x00', '\x00', '\x00', '\x01',
                   // Proposal 1, length 152, ISAKMP, 4 transforms.
                   '\x00', '\x00', '\x00', '\x98', '\x01', '\x01', '\x00',
                   '\x04',
                   // Transform 1, 3DES-CBC, SHA, PSK, group 2.
                   '\x03', '\x00', '\x00', '\x24', '\x01', '\x01', '\x00',
                   '\x00', '\x80', '\x01', '\x00', '\x05', '\x80', '\x02',
                   '\x00', '\x02', '\x80', '\x03', '\x00', '\x01', '\x80',
                   '\x04', '\x00', '\x02', '\x80', '\x0B', '\x00', '\x01',
                   '\x00', '\x0C', '\x00', '\x04', '\x00', '\x00', '\x00',
                   '\x01',
                   // Transform 2, 3DES-CBC, MD5, PSK, group 2.
                   '\x03', '\x00', '\x00', '\x24', '\x02', '\x01', '\x00',
                   '\x00', '\x80', '\x01', '\x00', '\x05', '\x80', '\x02',
                   '\x00', '\x01', '\x80', '\x03', '\x00', '\x01', '\x80',
                   '\x04', '\x00', '\x02', '\x80', '\x0B', '\x00', '\x01',
                   '\x00', '\x0C', '\x00', '\x04', '\x00', '\x00', '\x00',
                   '\x01',
                   // Transform 3, DES-CBC, SHA, PSK, group 2.
                   '\x03', '\x00', '\x00', '\x24', '\x03', '\x01', '\x00',
                   '\x00', '\x80', '\x01', '\x00', '\x01', '\x80', '\x02',
                   '\x00', '\x02', '\x80', '\x03', '\x00', '\x01', '\x80',
                   '\x04', '\x00', '\x02', '\x80', '\x0B', '\x00', '\x01',
                   '\x00', '\x0C', '\x00', '\x04', '\x00', '\x00', '\x00',
                   '\x01',
                   // Transform 4, DES-CBC, MD5, PSK, group 2.
                   '\x00', '\x00', '\x00', '\x24', '\x04', '\x01', '\x00',
                   '\x00', '\x80', '\x01', '\x00', '\x01', '\x80', '\x02',
                   '\x00', '\x01', '\x80', '\x03', '\x00', '\x01', '\x80',
                   '\x04', '\x00', '\x02', '\x80', '\x0B', '\x00', '\x01',
                   '\x00', '\x0C', '\x00', '\x04', '\x00', '\x00', '\x00',
                   '\x01'};
  // RIP v1
  payloads[520] = {'\x01', '\x01', '\x00', '\x00', '\x00', '\x00',
                   '\x00', '\x00', '\x00', '\x00', '\x00', '\x00',
                   '\x00', '\x00', '\x00', '\x00', '\x00', '\x00',
                   '\x00', '\x00', '\x00', '\x00', '\x00', '\x10'};
  // IPMI
  payloads[623] = {'\x06', '\x00', '\xff', '\x07', '\x00', '\x00',
                   '\x00', '\x00', '\x00', '\x00', '\x00', '\x00',
                   '\x00', '\x09', '\x20', '\x18', '\xc8', '\x81',
                   '\x00', '\x38', '\x8e', '\x04', '\xb5'};
  // serialnumberd
  payloads[626] = {'S', 'N', 'Q', 'U', 'E', 'R', 'Y', ':', ' ', '1',
                   '2', '7', '.', '0', '.', '0', '.', '1', ':', 'A',
                   'A', 'A', 'A', 'A', 'A', ':', 'x', 's', 'v', 'r'};
  // OpenVPN
  payloads[1194] = {'8',    'd',    '\xc1', 'x',  '\x01', '\xb8', '\x9b',
                    '\xcb', '\x8f', '\0',   '\0', '\0',   '\0',   '\0'};
  // Citrix MetaFrame
  payloads[1604] = {'\x1e', '\x00', '\x01', '\x30', '\x02', '\xfd',
                    '\xa8', '\xe3', '\x00', '\x00', '\x00', '\x00',
                    '\x00', '\x00', '\x00', '\x00', '\x00', '\x00',
                    '\x00', '\x00', '\x00', '\x00', '\x00', '\x00',
                    '\x00', '\x00', '\x00', '\x00', '\x00', '\x00'};
  // RADIUS
  std::vector<char> radius_str = {'\x01', '\x00', '\x00', '\x14', '\x00',
                                  '\x00', '\x00', '\x00', '\x00', '\x00',
                                  '\x00', '\x00', '\x00', '\x00', '\x00',
                                  '\x00', '\x00', '\x00', '\x00', '\x00'};
  payloads[1645] = radius_str;
  payloads[1812] = radius_str;
  // NFS v2
  payloads[2049] = {'\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00',
                    '\x00', '\x00', '\x00', '\x00', '\x02', '\x00', '\x01',
                    '\x86', '\xA3', '\x00', '\x00', '\x00', '\x02', '\x00',
                    '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00',
                    '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00',
                    '\x00', '\x00', '\x00', '\x00', '\x00'};
  // Freelancer game server
  payloads[2302] = {'\x00', '\x02', '\xf1', '\x26', '\x01', '\x26', '\xf0',
                    '\x90', '\xa6', '\xf0', '\x26', '\x57', '\x4e', '\xac',
                    '\xa0', '\xec', '\xf8', '\x68', '\xe4', '\x8d', '\x21'};
  // Apple Remote Desktop
  payloads[3283] = {'\0', '\x14', '\0', '\x01', '\x03'};
  // Sub Service Tag Discovery protocols
  payloads[6481] = {'[', 'P', 'R', 'O', 'B', 'E', ']', ' ', '0', '0', '0', '0'};
  // NAT-PMP
  payloads[5351] = {'\x00', '\x00'};
  // DNS Service Discovery
  payloads[5353] = {'\x00', '\x00', '\x00', '\x00', '\x00', '\x01', '\x00',
                    '\x00', '\x00', '\x00', '\x00', '\x00', '\x09', '_',
                    's',    'e',    'r',    'v',    'i',    'c',    'e',
                    's',    '\x07', '_',    'd',    'n',    's',    '-',
                    's',    'd',    '\x04', '_',    'u',    'd',    'p',
                    '\x05', 'l',    'o',    'c',    'a',    'l',    '\x00',
                    '\x00', '\x0C', '\x00', '\x01'};
  // CoAP
  payloads[5683] = {'@', '\x01', '\x01', '\xce', '\xbb', '.', 'w',
                    'e', 'l',    'l',    '-',    'k',    'n', 'o',
                    'w', 'n',    '\x04', 'c',    'o',    'r', 'e'};
  // Amanda
  payloads[10080] = {'A', 'm', 'a', 'n', 'd', 'a', ' ', '2',  '.', '6', ' ',
                     'R', 'E', 'Q', ' ', 'H', 'A', 'N', 'D',  'L', 'E', ' ',
                     '0', '0', '0', '-', '0', '0', '0', '0',  '0', '0', '0',
                     '0', ' ', 'S', 'E', 'Q', ' ', '0', '\n', 'S', 'E', 'R',
                     'V', 'I', 'C', 'E', ' ', 'n', 'o', 'o',  'p', '\n'};
  // VxWorks Wind River Debugger
  payloads[17185] = {// Random XID
                     '\x00', '\x00', '\x00', '\x00',
                     // RPC version 2 procedure call
                     '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00',
                     '\x02',
                     // WDB version 1
                     '\x55', '\x55', '\x55', '\x55', '\x00', '\x00', '\x00',
                     '\x01',
                     // WDB_TARGET_PING
                     '\x00', '\x00', '\x00', '\x00',
                     // RPC Auth NULL
                     '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00',
                     '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00',
                     '\x00', '\x00',
                     // Checksum
                     '\xff', '\xff', '\x55', '\x13',
                     // WDB wrapper (length and sequence number)
                     '\x00', '\x00', '\x00', '\x30', '\x00', '\x00', '\x00',
                     '\x01',
                     // Empty data?
                     '\x00', '\x00', '\x00', '\x02', '\x00', '\x00', '\x00',
                     '\x00', '\x00', '\x00', '\x00', '\x00'};
  // Quake 2 and Quake 3 game servers
  std::vector<char> quake_str = {'\xff', '\xff', '\xff', '\xff', 'g', 'e', 't',
                                 's',    't',    'a',    't',    'u', 's'};
  payloads[26000] = quake_str;
  payloads[26001] = quake_str;
  payloads[26002] = quake_str;
  payloads[26003] = quake_str;
  payloads[26004] = quake_str;
  payloads[27960] = quake_str;
  payloads[27961] = quake_str;
  payloads[27962] = quake_str;
  payloads[27963] = quake_str;
  payloads[27964] = quake_str;
  payloads[30720] = quake_str;
  payloads[30721] = quake_str;
  payloads[30722] = quake_str;
  payloads[30723] = quake_str;
  payloads[30724] = quake_str;
  payloads[44400] = quake_str;
  // Murmur 1.2.X
  payloads[64738] = {'\x00', '\x00', '\x00', '\x00', 'a', 'b',
                     'c',    'd',    'e',    'f',    'g', 'h'};
  // Ventrilo 2.1.2+
  payloads[3784] = {'\x01', '\xe7', '\xe5', '\x75', '\x31', '\xa3',
                    '\x17', '\x0b', '\x21', '\xcf', '\xbf', '\x2b',
                    '\x99', '\x4e', '\xdd', '\x19', '\xac', '\xde',
                    '\x08', '\x5f', '\x8b', '\x24', '\x0a', '\x11',
                    '\x19', '\xb6', '\x73', '\x6f', '\xad', '\x28',
                    '\x13', '\xd2', '\x0a', '\xb9', '\x12', '\x75'};
  // TeamSpeak 2
  payloads[8767] = {'\xf4', '\xbe', '\x03', '\x00', '\x00', '\x00', '\x00',
                    '\x00', '\x00', '\x00', '\x00', '\x00', '\x01', '\x00',
                    '\x00', '\x00', '2',    'x',    '\xba', '\x85', '\t',
                    'T',    'e',    'a',    'm',    'S',    'p',    'e',
                    'a',    'k',    '\x00', '\x00', '\x00', '\x00', '\x00',
                    '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00',
                    '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00',
                    '\x00', '\n',   'W',    'i',    'n',    'd',    'o',
                    'w',    's',    ' ',    'X',    'P',    '\x00', '\x00',
                    '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00',
                    '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00',
                    '\x00', '\x00', '\x00', '\x02', '\x00', '\x00', '\x00',
                    ' ',    '\x00', '<',    '\x00', '\x00', '\x01', '\x00',
                    '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00',
                    '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00',
                    '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00',
                    '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00',
                    '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00',
                    '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00',
                    '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00',
                    '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00',
                    '\x00', '\x00', '\x00', '\x08', 'n',    'i',    'c',
                    'k',    'n',    'a',    'm',    'e',    '\x00', '\x00',
                    '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00',
                    '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00',
                    '\x00', '\x00', '\x00', '\x00', '\x00'};
  // TeamSpeak 3
  payloads[9987] = {'\x05', '\xca', '\x7f', '\x16', '\x9c', '\x11', '\xf9',
                    '\x89', '\x00', '\x00', '\x00', '\x00', '\x02', '\x9d',
                    '\x74', '\x8b', '\x45', '\xaa', '\x7b', '\xef', '\xb9',
                    '\x9e', '\xfe', '\xad', '\x08', '\x19', '\xba', '\xcf',
                    '\x41', '\xe0', '\x16', '\xa2', '\x32', '\x6c', '\xf3',
                    '\xcf', '\xf4', '\x8e', '\x3c', '\x44', '\x83', '\xc8',
                    '\x8d', '\x51', '\x45', '\x6f', '\x90', '\x95', '\x23',
                    '\x3e', '\x00', '\x97', '\x2b', '\x1c', '\x71', '\xb2',
                    '\x4e', '\xc0', '\x61', '\xf1', '\xd7', '\x6f', '\xc5',
                    '\x7e', '\xf6', '\x48', '\x52', '\xbf', '\x82', '\x6a',
                    '\xa2', '\x3b', '\x65', '\xaa', '\x18', '\x7a', '\x17',
                    '\x38', '\xc3', '\x81', '\x27', '\xc3', '\x47', '\xfc',
                    '\xa7', '\x35', '\xba', '\xfc', '\x0f', '\x9d', '\x9d',
                    '\x72', '\x24', '\x9d', '\xfc', '\x02', '\x17', '\x6d',
                    '\x6b', '\xb1', '\x2d', '\x72', '\xc6', '\xe3', '\x17',
                    '\x1c', '\x95', '\xd9', '\x69', '\x99', '\x57', '\xce',
                    '\xdd', '\xdf', '\x05', '\xdc', '\x03', '\x94', '\x56',
                    '\x04', '\x3a', '\x14', '\xe5', '\xad', '\x9a', '\x2b',
                    '\x14', '\x30', '\x3a', '\x23', '\xa3', '\x25', '\xad',
                    '\xe8', '\xe6', '\x39', '\x8a', '\x85', '\x2a', '\xc6',
                    '\xdf', '\xe5', '\x5d', '\x2d', '\xa0', '\x2f', '\x5d',
                    '\x9c', '\xd7', '\x2b', '\x24', '\xfb', '\xb0', '\x9c',
                    '\xc2', '\xba', '\x89', '\xb4', '\x1b', '\x17', '\xa2',
                    '\xb6'};
  // Memcached
  payloads[11211] = {'\0', '\x01', '\0', '\0', '\0', '\x01', '\0', '\0', 'v',
                     'e',  'r',    's',  'i',  'o',  'n',    '\r', '\n'};
  return payloads;
}

payload_str_map get_payload_hex_strs() {
  payload_str_map pl_strs;
  auto payloads = get_payloads();
  for (auto& p : payloads)
    pl_strs[p.first] = to_hex_string(p.second);
  return pl_strs;
}

std::string to_hex_string(const std::vector<char>& buf) {
  std::stringstream s;
  for (auto c : buf)
    s << hex{c};
  return s.str();
}

} // namespace spoki::probe
