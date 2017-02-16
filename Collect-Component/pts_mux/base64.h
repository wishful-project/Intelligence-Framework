#ifndef _BASE64_H_
#define _BASE64_H_

void base64_decode(unsigned char* enc_data_out, int* dec_length , unsigned char* dec_data_out);
void base64_encode(unsigned char* data, int dataLength, unsigned char* enc_data);

#endif /*_BASE64_H_*/
