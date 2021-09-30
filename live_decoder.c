#include<stdio.h>
#include<stdlib.h>
#include<stdint.h>
#include<stdbool.h>
#include<string.h>
#include<sqlite3.h>

#define MAX_BUFFER 65536

uint8_t read8();
uint8_t get_nibble(uint8_t nibble, bool upper);
uint8_t get_byte(uint8_t upper, uint8_t lower);
void convert_to_byte_stream(uint8_t *input_stream, uint16_t input_stream_len_nibbles, uint8_t **output_stream);

uint16_t get_protocol_id(uint8_t *stream);
uint8_t get_pkt_s_field_s(uint8_t *stream);
uint16_t get_pkt_s(uint8_t *stream, uint8_t pkt_s_field_s);
uint32_t get_objectType(uint8_t *stream, uint16_t stream_byte_cursor);
uint16_t get_itemTypeDescriptions_len(uint8_t *stream, uint16_t stream_byte_cursor);
uint64_t convert_to_long(uint8_t *stream, uint16_t *stream_byte_cursor, uint8_t max_bytes);
uint32_t get_objectUID(uint8_t *stream, uint16_t *stream_byte_cursor);
uint16_t get_objectGID(uint8_t *stream, uint16_t *stream_byte_cursor);
uint64_t get_price(uint8_t *stream, uint16_t *stream_byte_cursor);

void check_sql(char **sql_query, sqlite3 **db, char **err_msg, int *code, uint32_t objectUID, uint32_t objectType, uint16_t objectGID, uint64_t pricex1, uint64_t pricex10, uint64_t pricex100);
int query_row_result(void *data,int col_num, char **col_data, char **col_label);
void add_entry(char **sql_query, sqlite3 **db, char **err_msg, int *code, uint32_t objectUID, uint32_t objectType, uint16_t objectGID, uint64_t pricex1, uint64_t pricex10, uint64_t pricex100);
void update_entry(char **sql_query, sqlite3 **db, char **err_msg, int *code, uint32_t objectUID, uint32_t objectType, uint16_t objectGID, uint64_t pricex1, uint64_t pricex10, uint64_t pricex100);

// Protocol ID 14 bits
// PAcket Size Field Size 2 bits
// Packet Size x bytes
//objectType(Int) 4 bytes
//itemTypeDescriptions.length(Short) 2 bytes
//--> loop over itemTypeDescriptions.length
//--> objectUID(varInt) 1-4 bytes
//--> objectGID(varShort) 1-2 bytes
//--> objectType(Int) 4 bytes
//--> effects.length(Short) 2 bytes
//----> loop over effects.length total 4*n bytes
//----> effects[n].getTypeId(short) 2*n bytes
//----> actionId(short) 2*n bytes
//--> prices.length(short) 2 bytes
//----> loop over prices.length 
//----> prices[n](varlong) 1-8 * n bytes

int main(int argc, char **argv){

    //SQLDB
    sqlite3 *db;
    char *err_msg = 0;

    int code = sqlite3_open("./test.db", &db);

    if(code != SQLITE_OK){
        sqlite3_close(db);
        return 1;
    }

    char *sql_table_creation = "CREATE TABLE if not exists Ressources(UID INT, Type INT, GID INT, Pricex1 INT, Pricex10 INT, Pricex100 INT);";

    code = sqlite3_exec(db, sql_table_creation, 0, 0, &err_msg);
    
    if(code != SQLITE_OK ){
        printf("SQL error: %s\n", err_msg);
        sqlite3_free(err_msg);        
        sqlite3_close(db);
        return 1;
    }

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
                        objectType = get_objectType(latest_packet, stream_byte_cursor);
                        stream_byte_cursor = stream_byte_cursor + 4;
                        if(objectType>0){
                            itemTypeDescriptions_len = get_itemTypeDescriptions_len(latest_packet, stream_byte_cursor);
                            stream_byte_cursor = stream_byte_cursor + 2;
                            if(itemTypeDescriptions_len==1){
                                //assuming there is only 1 description
                                objectUID=get_objectUID(latest_packet, &stream_byte_cursor);
                                if(objectUID>0){
                                    objectGID=get_objectGID(latest_packet, &stream_byte_cursor);
                                    if(objectGID>0){
                                        //skip second object type
                                        stream_byte_cursor=stream_byte_cursor+4;

                                        effects_length = get_itemTypeDescriptions_len(latest_packet, stream_byte_cursor);
                                        stream_byte_cursor=stream_byte_cursor+2;
                                        if(effects_length==0){
                                            prices_length = get_itemTypeDescriptions_len(latest_packet, stream_byte_cursor);
                                            stream_byte_cursor=stream_byte_cursor+2;
                                            if(prices_length==3){
                                                //assuming that there are always 3 prices x1 x10 x100
                                                pricex1 = get_price(latest_packet, &stream_byte_cursor);
                                                pricex10 = get_price(latest_packet, &stream_byte_cursor);
                                                pricex100 = get_price(latest_packet, &stream_byte_cursor);
                                                printf("%03d --> ID:%d PKT_S_Field_S:%d PKT_S:%d Type:%d UID:%d GID:%d x1:%ld x10:%ld x100:%ld\n",packet_count,protocol_id,pkt_s_field_s,pkt_s,objectType,objectUID,objectGID,pricex1,pricex10,pricex100);
                                                check_sql(&sql_query, &db, &err_msg, &code, objectUID, objectType, objectGID, pricex1, pricex10, pricex100);

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

//Get Object Type
uint32_t get_objectType(uint8_t *stream, uint16_t stream_byte_cursor){

    return (stream[stream_byte_cursor] << 24) + (stream[stream_byte_cursor+1] << 16) + (stream[stream_byte_cursor+2] << 8) +(stream[stream_byte_cursor+3]);
}

//Get itemTypeDescriptions_len
uint16_t get_itemTypeDescriptions_len(uint8_t *stream, uint16_t stream_byte_cursor){
    return (stream[stream_byte_cursor] << 8) + (stream[stream_byte_cursor+1]);
}

//Get objectUID which is varaible from 1 to 4 bytes
uint32_t get_objectUID(uint8_t *stream, uint16_t *stream_byte_cursor){
    return (uint32_t)convert_to_long(stream,stream_byte_cursor,4);
}

//Get objectGID which is varaible from 1 to 2 bytes
uint16_t get_objectGID(uint8_t *stream, uint16_t *stream_byte_cursor){
    return (uint16_t)convert_to_long(stream,stream_byte_cursor,2);
}

//Get Price which is varaible from 1 to 8 bytes
uint64_t get_price(uint8_t *stream, uint16_t *stream_byte_cursor){
    return convert_to_long(stream,stream_byte_cursor,8);
}

//Get Variable Length Variable
uint64_t convert_to_long(uint8_t *stream, uint16_t *stream_byte_cursor, uint8_t max_bytes){

    uint8_t current;
    uint8_t offset = 0;
    bool hasNext = false;
    uint64_t output = 0x0;

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

void check_sql(char **sql_query, sqlite3 **db, char **err_msg, int *code, uint32_t objectUID, uint32_t objectType, uint16_t objectGID, uint64_t pricex1, uint64_t pricex10, uint64_t pricex100){

    memset(*sql_query,0x00,MAX_BUFFER);
    sprintf(*sql_query,"SELECT GID FROM Ressources WHERE GID == %d ;",objectGID);

    bool success = false;
    *code = sqlite3_exec(*db, *sql_query, query_row_result, &success, err_msg);

    if(success){
        update_entry(sql_query, db, err_msg, code, objectUID, objectType, objectGID, pricex1, pricex10, pricex100);
    }
    else{
        add_entry(sql_query, db, err_msg, code, objectUID, objectType, objectGID, pricex1, pricex10, pricex100);
    }
    
    if(*code != SQLITE_OK ){
        printf("SQL error %d : %s\n", *code, *err_msg);
        return;
    }
}

void add_entry(char **sql_query, sqlite3 **db, char **err_msg, int *code, uint32_t objectUID, uint32_t objectType, uint16_t objectGID, uint64_t pricex1, uint64_t pricex10, uint64_t pricex100){

    memset(*sql_query,0x00,MAX_BUFFER);
    sprintf(*sql_query,"INSERT INTO Ressources(UID, Type, GID, Pricex1, Pricex10, Pricex100) VALUES (%d,%d,%d,%ld,%ld,%ld);",objectUID,objectType,objectGID,pricex1,pricex10,pricex100);

    *code = sqlite3_exec(*db, *sql_query, 0, 0, err_msg);
    
    if(*code != SQLITE_OK ){
        printf("SQL error %d : %s\n", *code, *err_msg);
        return;
    }
}

void update_entry(char **sql_query, sqlite3 **db, char **err_msg, int *code, uint32_t objectUID, uint32_t objectType, uint16_t objectGID, uint64_t pricex1, uint64_t pricex10, uint64_t pricex100){

    memset(*sql_query,0x00,MAX_BUFFER);
    sprintf(*sql_query,"UPDATE Ressources SET UID = %d, Type = %d, Pricex1 = %ld, Pricex10 = %ld, Pricex100 = %ld WHERE GID == %d;",objectUID,objectType,pricex1,pricex10,pricex100,objectGID);

    *code = sqlite3_exec(*db, *sql_query, 0, 0, err_msg);
    
    if(*code != SQLITE_OK ){
        printf("SQL error %d : %s\n", *code, *err_msg);
        return;
    }
}

//Callback function executed for each row in table
int query_row_result(void *data,int col_num, char **col_data, char **col_label)
{
    //Extract URL (should be last column)
    if(col_num>0)
    {
        *(bool*)data=true;
    }
    return EXIT_SUCCESS;
}
