//
// Created by uws on 5/22/24.
//

#ifndef D2PRICES_DB_OPERATIONS_H
#define D2PRICES_DB_OPERATIONS_H

#include<stdio.h>
#include<sqlite3.h>
#include<stdbool.h>

#include "config.h"


uint8_t open_db(char *db_path, sqlite3 **db) {
    if(sqlite3_open(db_path, db) != SQLITE_OK)
        return 1;
    return 0;
}

uint8_t create_table(sqlite3 **db) {

    char *err_msg;
    char *sql_table_creation = "CREATE TABLE if not exists Ressources(UID INT, Type INT, GID INT, Pricex1 INT, Pricex10 INT, Pricex100 INT);";

    if(sqlite3_exec(*db, sql_table_creation, 0, 0, &err_msg) != SQLITE_OK ){
        printf("SQL error: %s\n", err_msg);
        sqlite3_free(err_msg);
        sqlite3_close(*db);
        return 1;
    }
    return 0;
}

uint8_t init_db(char *db_path, sqlite3 **db) {
    if(open_db(db_path,db))
        return 1;
    if(create_table(db))
        return 1;
    return 0;
}

void add_entry(char **sql_query, sqlite3 **db, uint32_t objectUID, uint32_t objectType, uint16_t objectGID, uint64_t pricex1, uint64_t pricex10, uint64_t pricex100){

    char *err_msg;
    int code;

    memset(*sql_query,0x00,MAX_BUFFER);
    sprintf(*sql_query,"INSERT INTO Ressources(UID, Type, GID, Pricex1, Pricex10, Pricex100) VALUES (%d,%d,%d,%ld,%ld,%ld);",objectUID,objectType,objectGID,pricex1,pricex10,pricex100);

    code = sqlite3_exec(*db, *sql_query, 0, 0, &err_msg);

    if(code != SQLITE_OK ){
        printf("SQL error %d : %s\n", code, err_msg);
        return;
    }
}

void update_entry(char **sql_query, sqlite3 **db, uint32_t objectUID, uint32_t objectType, uint16_t objectGID, uint64_t pricex1, uint64_t pricex10, uint64_t pricex100){

    char *err_msg;
    int code;

    memset(*sql_query,0x00,MAX_BUFFER);
    sprintf(*sql_query,"UPDATE Ressources SET UID = %d, Type = %d, Pricex1 = %ld, Pricex10 = %ld, Pricex100 = %ld WHERE GID == %d;",objectUID,objectType,pricex1,pricex10,pricex100,objectGID);

    code = sqlite3_exec(*db, *sql_query, 0, 0, &err_msg);

    if(code != SQLITE_OK ){
        printf("SQL error %d : %s\n", code, err_msg);
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

void check_sql(char **sql_query, sqlite3 **db, uint32_t objectUID, uint32_t objectType, uint16_t objectGID, uint64_t pricex1, uint64_t pricex10, uint64_t pricex100){

    char *err_msg;
    int code;

    memset(*sql_query,0x00,MAX_BUFFER);
    sprintf(*sql_query,"SELECT GID FROM Ressources WHERE GID == %d ;",objectGID);

    bool success = false;
    code = sqlite3_exec(*db, *sql_query, query_row_result, &success, &err_msg);

    if(success){
        update_entry(sql_query, db, objectUID, objectType, objectGID, pricex1, pricex10, pricex100);
    }
    else{
        add_entry(sql_query, db, objectUID, objectType, objectGID, pricex1, pricex10, pricex100);
    }

    if(code != SQLITE_OK ){
        printf("SQL error %d : %s\n", code, err_msg);
        return;
    }
}

#endif //D2PRICES_DB_OPERATIONS_H
