#include "base64.h"
#include <inttypes.h>
#include <string.h>


static int base64_decode_char(char c)
{
	if(c >= 'A' && c <= 'Z')
	{
		return c - 'A';
	}
	else if(c >= 'a' && c <= 'z')
	{
		return c - 'a' + 26;
	}
	else if(c >= '0' && c <= '9')
	{
		return c - '0' + 52;
	}
	else if(c == '+')
	{
		return 62;
	}
	else if(c == '/')
	{
		return 63;
	}
	else
	{
		return 0;
	}
}

void base64_decode(unsigned char* enc_data_out, int* dec_length , unsigned char* dec_data_out)
{
	int i;
	unsigned long tmpdata = 0;
	*dec_length = 3;
	for(i=0; i<4; i++)
	{
		if(enc_data_out[i] == '=')
		{
			(*dec_length)--;
		}
		tmpdata = (tmpdata << 6) | base64_decode_char(enc_data_out[i]);
	}
	if(*dec_length>0) dec_data_out[0] = (unsigned char)(tmpdata >> 16);
	if(*dec_length>1) dec_data_out[1] = (unsigned char)(tmpdata >> 8);
	if(*dec_length>2) dec_data_out[2] = (unsigned char)(tmpdata);
}

char base64chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

void base64_encode(unsigned char* data, int dataLength, unsigned char* enc_data)
{
	unsigned long n;
	int padCount = dataLength%3;
	/* these three 8-bit (ASCII) characters become one 24-bit number */
	n = ((unsigned long)data[0]) << 16;

	if(dataLength>1) n += ((unsigned long)data[1]) << 8;

	if(dataLength>2) n += data[2];

	enc_data[0] = base64chars[(unsigned char)(n >> 18) & 63];
	enc_data[1] = base64chars[(unsigned char)(n >> 12) & 63];

	/*
	* if we have only two bytes available, then their encoding is
	* spread out over three chars
	*/
	if(dataLength>1) enc_data[2] = base64chars[(unsigned char)(n >> 6) & 63];
	/*
	* if we have all three bytes available, then their encoding is spread
	* out over four characters
	*/
	if(dataLength>2) enc_data[3] = base64chars[(unsigned char)n & 63];

	if (padCount > 0)
	{
		for (; padCount < 3; padCount++)
		{
			enc_data[padCount+1] = '=';
		}
	}
}
