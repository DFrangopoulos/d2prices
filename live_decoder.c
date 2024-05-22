#include<stdio.h>
#include<stdlib.h>
#include<stdint.h>
#include<stdbool.h>
#include<string.h>

#include "variable_fetchers.h"
#include "db_operations.h"

#pragma clang diagnostic push
#pragma ide diagnostic ignored "EndlessLoop"


uint8_t read8();
uint8_t get_nibble(uint8_t nibble, bool upper);
uint8_t get_byte(uint8_t upper, uint8_t lower);
void convert_to_byte_stream(uint8_t *input_stream, uint16_t input_stream_len_nibbles, uint8_t **output_stream);

uint16_t get_protocol_id(uint8_t *stream);
uint8_t get_pkt_s_field_s(uint8_t *stream);
uint16_t get_pkt_s(uint8_t *stream, uint8_t pkt_s_field_s);


// Protocol ID 14 bits
// PAcket Size Field Size 2 bits
// Packet Size x bytes
//objectGID(varInt) ----- UPD!!!
//objectType(Int) 4 bytes
//itemTypeDescriptions.length(Short) 2 bytes
//--> loop over itemTypeDescriptions.length (UnsignedShort)
//--> objectUID(varInt) 1-4 bytes
//--> objectGID(varInt) 1-4 bytes ----- UPD!!!
//--> objectType(Int) 4 bytes
//--> effects.length(Short) 2 bytes
//----> loop over effects.length
//----> effects[n].getTypeId(short) 2*n bytes
//----> actionId(VarShort) 2*n bytes --- UPD!!
//--> prices.length(short) 2 bytes
//----> loop over prices.length 
//----> prices[n](varlong) 1-8 * n bytes

int main(int argc, char **argv){



    //Constants
    uint16_t protocol_id = 0;
    uint8_t pkt_s_field_s = 0;
    uint16_t pkt_s = 0;
    uint32_t objectType = 0;
    uint16_t itemTypeDescriptions_len = 0;
    //only keep 1 (only doing ressources)
    uint32_t objectUID =0;
    uint16_t objectGID =0;
    uint16_t effects_length = 0;
    //skip effects do not exist in ressources skip packet entirely if non zero
    uint16_t effect_typeID = 0;
    uint16_t actionID = 0;
    uint32_t effect_value = 0;
    uint16_t prices_length = 0;
    uint64_t pricex1 = 0;
    uint64_t pricex10 = 0;
    uint64_t pricex100 = 0;

    uint16_t target_id = 0;
    if(argc == 2){
        target_id = (uint16_t)atoi(argv[1]);
    }
    else{
        return 1;
    }

    //Init SQLiteDB
    sqlite3 *db;
    if(init_db("./new_db.db", &db))
        return 1;

    uint32_t packet_count = 0;
    uint8_t *latest_packet_raw = NULL;
    uint8_t *latest_packet= NULL;
    char *sql_query = NULL;

    latest_packet_raw = (uint8_t *)malloc(sizeof(uint8_t)*MAX_BUFFER);
    latest_packet= (uint8_t *)malloc(sizeof(uint8_t)*MAX_BUFFER);
    sql_query = (char *)malloc(sizeof(char)*MAX_BUFFER);
    if(latest_packet_raw==NULL || latest_packet==NULL || sql_query==NULL){
        exit(1);
    }

    do{
        memset(latest_packet_raw,0x00,MAX_BUFFER);
        memset(latest_packet,0x00,MAX_BUFFER);

        uint16_t count = 0;
        uint16_t stream_byte_cursor = 0;
        do{
            //read to buffer
            latest_packet_raw[count] = read8();
            count++;
            //check for newline(packetdelimiter)
            if (latest_packet_raw[count-1] == 0x0a){
                packet_count++;
                break;

            }
        }while(true);
        //check that there are is an even number of nibbles excluding the newline
        if (count > 3 && ((count-1) % 2 == 0 )){

            convert_to_byte_stream(latest_packet_raw,count,&latest_packet);

            protocol_id = get_protocol_id(latest_packet);
            stream_byte_cursor++;
            if (protocol_id == target_id){
                pkt_s_field_s = get_pkt_s_field_s(latest_packet);
                stream_byte_cursor++;
                if(pkt_s_field_s > 0){
                    pkt_s = get_pkt_s(latest_packet, pkt_s_field_s);
                    stream_byte_cursor = stream_byte_cursor + pkt_s_field_s;
                    if(pkt_s>0){
                        //Get objectGID
                        if(get_varInt(latest_packet, &stream_byte_cursor)>0) {
                            //Get objectType
                            objectType = get_Int(latest_packet, &stream_byte_cursor);
                            if (objectType > 0) {
                                itemTypeDescriptions_len = get_Short(latest_packet, &stream_byte_cursor);
                                if (itemTypeDescriptions_len == 1) {
                                    //assuming there is only 1 description
                                    objectUID = get_varInt(latest_packet, &stream_byte_cursor);
                                    if (objectUID > 0) {
                                        objectGID = get_varInt(latest_packet, &stream_byte_cursor);
                                        if (objectGID > 0) {
                                            //skip second object type
                                            get_Int(latest_packet, &stream_byte_cursor);

                                            effects_length = get_Short(latest_packet, &stream_byte_cursor);

                                            if (effects_length == 0) {
                                                prices_length = get_Short(latest_packet, &stream_byte_cursor);

                                                if (prices_length == 3) {
                                                    //assuming that there are always 3 prices x1 x10 x100
                                                    pricex1 = get_varLong(latest_packet, &stream_byte_cursor);
                                                    pricex10 = get_varLong(latest_packet, &stream_byte_cursor);
                                                    pricex100 = get_varLong(latest_packet, &stream_byte_cursor);
                                                    printf("%03d --> ID:%d (%02x%02x) PKT_S_Field_S:%d PKT_S:%d Type:%d UID:%d GID:%d x1:%ld x10:%ld x100:%ld\n",
                                                           packet_count, protocol_id, latest_packet[0], latest_packet[1], pkt_s_field_s, pkt_s, objectType,
                                                           objectUID, objectGID, pricex1, pricex10, pricex100);
                                                    check_sql(&sql_query, &db, objectUID, objectType,
                                                              objectGID, pricex1, pricex10, pricex100);

                                                }

                                            }
                                            else if(effects_length == 1 && (objectType == 12 || objectType == 33 || objectType == 49 || objectType == 69)) {

                                                effect_typeID = get_Short(latest_packet, &stream_byte_cursor);
                                                actionID = get_varShort(latest_packet, &stream_byte_cursor);
                                                effect_value = get_varInt(latest_packet, &stream_byte_cursor);

                                                printf("Found %d with type %d - effect type ID %d , effect category %d and effect value %d-- ",objectGID,objectType,effect_typeID,actionID,effect_value);

                                                prices_length = get_Short(latest_packet, &stream_byte_cursor);
                                                printf("price length %d\n",prices_length);
                                                if (prices_length == 3) {
                                                    //assuming that there are always 3 prices x1 x10 x100
                                                    pricex1 = get_varLong(latest_packet, &stream_byte_cursor);
                                                    pricex10 = get_varLong(latest_packet, &stream_byte_cursor);
                                                    pricex100 = get_varLong(latest_packet, &stream_byte_cursor);
                                                    printf("%03d --> ID:%d (%02x%02x) PKT_S_Field_S:%d PKT_S:%d Type:%d UID:%d GID:%d x1:%ld x10:%ld x100:%ld\n",
                                                           packet_count, protocol_id, latest_packet[0], latest_packet[1], pkt_s_field_s, pkt_s, objectType,
                                                           objectUID, objectGID, pricex1, pricex10, pricex100);
                                                    check_sql(&sql_query, &db, objectUID, objectType,
                                                              objectGID, pricex1, pricex10, pricex100);
                                                }
                                            }
                                            else {
                                                printf("%03d --> ID:%d (%02x%02x) PKT_S_Field_S:%d PKT_S:%d Type:%d UID:%d GID:%d Dropped\n",
                                                       packet_count, protocol_id, latest_packet[0], latest_packet[1], pkt_s_field_s, pkt_s, objectType,
                                                       objectUID, objectGID);
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }  
                }
            }       
        }
    }while(true);

    free(latest_packet_raw);
    free(latest_packet);
    free(sql_query);

    sqlite3_close(db);

    return 0;
}

//Read 1 character from stdin
uint8_t read8(){
    return (uint8_t) getchar();
}

//Convert nibble stream to byte stream
void convert_to_byte_stream(uint8_t *input_stream, uint16_t input_stream_len_nibbles, uint8_t **output_stream){

    uint16_t output_counter = 0;
    for(uint16_t i=0; i<input_stream_len_nibbles;i++){

        (*output_stream)[output_counter] = get_byte(input_stream[i], input_stream[i+1]);
        i++;
        output_counter++;
    }
    return;
}

//Convert nibbles to byte
//Return byte
uint8_t get_byte(uint8_t upper, uint8_t lower){
    return (get_nibble(upper,true) + get_nibble(lower,false));
}

//Convert char to nibble
//Return byte (half-byte)
uint8_t get_nibble(uint8_t nibble, bool upper){

    uint8_t out;

    switch(nibble){

        case '0':
            out = 0x00;
            break;
        case '1':
            out = 0x01;
            break;
        case '2':
            out = 0x02;
            break;
        case '3':
            out = 0x03;
            break;
        case '4':
            out = 0x04;
            break;
        case '5':
            out = 0x05;
            break;
        case '6':
            out = 0x06;
            break;
        case '7':
            out = 0x07;
            break;
        case '8':
            out = 0x08;
            break;
        case '9':
            out = 0x09;
            break;
        case 'A':
            out = 0x0A;
            break;
        case 'B':
            out = 0x0B;
            break;
        case 'C':
            out = 0x0C;
            break;
        case 'D':
            out = 0x0D;
            break;
        case 'E':
            out = 0x0E;
            break;
        case 'F':
            out = 0x0F;
            break;
        case 'a':
            out = 0x0A;
            break;
        case 'b':
            out = 0x0B;
            break;
        case 'c':
            out = 0x0C;
            break;
        case 'd':
            out = 0x0D;
            break;
        case 'e':
            out = 0x0E;
            break;
        case 'f':
            out = 0x0F;
            break;
        default:
            out = 0x00;
    }

    if(upper){
        out = out << 4;
    }

    return out;

}

//Get Protocol id
uint16_t get_protocol_id(uint8_t *stream){

    return (stream[0] << 6) + (stream[1] >> 2);
}

//Get Packet Size Field Size
uint8_t get_pkt_s_field_s(uint8_t *stream){
    return (stream[1] & 0x03);
}

//Get Packet Size
uint16_t get_pkt_s(uint8_t *stream, uint8_t pkt_s_field_s){

    uint16_t pkt_s = 0;
    //max 2 bytes
    if(pkt_s_field_s<3){
        for (uint16_t i=2;i<(2+pkt_s_field_s);i++){
            pkt_s = pkt_s + (stream[i] << (8*(1+pkt_s_field_s-i)));
        }
    }
    return pkt_s;
}





#pragma clang diagnostic pop