#ifndef DATABASE_H
#define DATABASE_H

#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <string.h>
#include <string>
#include <mysql/mysql.h>
#include <fstream>
using namespace std;

typedef struct student{
    char* id;
    char* name;
    char* sex;
    char* birthday;
}student;

class MysqlDB
{
private:
    MYSQL mysql;
    MYSQL_ROW row;
    MYSQL_RES *result;
    MYSQL_FIELD *field;
public:
    MysqlDB();
    ~MysqlDB();
    void connect(string host, string user, string passwd,  string database);
    void add(char* m_id,char* m_name,char* m_sex,char* m_birthday);
    void del(char* m_id);
    void update(char* m_id,char* m_filed,char* m_value);
    void print();
    void write_table();
};
#endif
