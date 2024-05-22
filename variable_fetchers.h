//
// Created by uws on 5/22/24.
//

#ifndef D2PRICES_VARIABLE_FETCHERS_H
#define D2PRICES_VARIABLE_FETCHERS_H

#include<stdint.h>

//Get Object Type
uint32_t get_Int(uint8_t *stream, uint16_t *stream_byte_cursor){
    uint32_t output = (stream[*stream_byte_cursor] << 24) + (stream[*stream_byte_cursor+1] << 16) + (stream[*stream_byte_cursor+2] << 8) +(stream[*stream_byte_cursor+3]);
    *stream_byte_cursor = *stream_byte_cursor + 4;
    return output;
}

//Get itemTypeDescriptions_len
uint16_t get_Short(uint8_t *stream, uint16_t *stream_byte_cursor){
    uint16_t output = (stream[*stream_byte_cursor] << 8) + (stream[*stream_byte_cursor+1]);
    *stream_byte_cursor = *stream_byte_cursor + 2;
    return output;
}


//Get Variable Length Variable
uint64_t get_varLen(uint8_t *stream, uint16_t *stream_byte_cursor, uint8_t max_bytes){

    uint8_t current;
    uint8_t offset = 0;
    bool hasNext = false;
    uint64_t output = 0;

    for(uint16_t i = *stream_byte_cursor;i<0xFFFF;i++){

        //Check if there is a next value
        hasNext = (stream[i] & 0x80) == 0x80;
        //add to total
        output = output + ((stream[i] & 0x7F) << offset);

        //increment stream_byte_cursor
        (*stream_byte_cursor)++;

        //increment offset
        offset = offset + 7;
        if(!hasNext || offset > (max_bytes*8)){
            break;
        }
    }
    return output;
}

//Get variable from 1 to 2 bytes
uint16_t get_varShort(uint8_t *stream, uint16_t *stream_byte_cursor){
    return (uint16_t) get_varLen(stream, stream_byte_cursor, 2);
}
//Get variable from 1 to 4 bytes
uint32_t get_varInt(uint8_t *stream, uint16_t *stream_byte_cursor){
    return (uint32_t) get_varLen(stream, stream_byte_cursor, 4);
}
//Get variable from 1 to 8 bytes
uint64_t get_varLong(uint8_t *stream, uint16_t *stream_byte_cursor){
    return get_varLen(stream, stream_byte_cursor, 8);
}
#endif //D2PRICES_VARIABLE_FETCHERS_H
