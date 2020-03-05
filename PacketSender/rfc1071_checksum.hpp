#ifndef __RFC_1071_CHECKSUM_HPP
#define __RFC_1071_CHECKSUM_HPP
#pragma once
unsigned short comp_chksum(void * _addr, int len)
{
	unsigned short* addr = (unsigned short*)_addr;
	/**
	*Quelle: RFC 1071
	*Calculates the Internet-checksum
	*Valid for the IP, ICMP, TCP or UDP header
	* *addr : Pointer to the Beginning of the data
	*(Checksummenfeld muss Null sein)
	*len : length of the data (in bytes)
	*Return : Checksum in network-byte-order
	**/
	long sum = 0; 
	while (len > 1) { 
		sum += *(addr++); 
		len -= 2; 
	}
	if (len > 0)sum += *addr; 
	while (sum >> 16) sum = ((sum & 0xffff) + (sum >> 16));
	sum = ~sum;
	return((unsigned short)sum);
}

unsigned short comp_chksum2buffers(const void * _addr1, int len1, const void* _addr2, int len2 )
{
	/**
	*Quelle: RFC 1071
	*Calculates the Internet-checksum
	*Valid for the IP, ICMP, TCP or UDP header
	* *addr : Pointer to the Beginning of the data
	*(Checksummenfeld muss Null sein)
	*len : length of the data (in bytes)
	*Return : Checksum in network-byte-order
	**/
	unsigned short* addr1 = (unsigned short*)_addr1;
	unsigned short* addr2 = (unsigned short*)_addr2;
	long sum = 0;
	while (len1 > 1) {
		sum += *(addr1++);
		len1 -= 2;
	}
	if (len1 > 0)sum += *addr1;

	while (len2 > 1) {
		sum += *(addr2++);
		len2 -= 2;
	}
	if (len2 > 0)sum += *addr2;


	while (sum >> 16) sum = ((sum & 0xffff) + (sum >> 16));
	sum = ~sum;
	return((unsigned short)sum);
}
#endif
