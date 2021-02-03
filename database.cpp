#include"database.h"

MysqlDB::MysqlDB()
{
    if( mysql_init( &mysql ) == NULL )
    {
        cout << "init error, line: " << __LINE__ << endl;
        exit(-1);
    }
}
MysqlDB::~MysqlDB()
{
    mysql_close( &mysql );
}

void MysqlDB::connect( string host, string user, string passwd,  string database )
    {
        //成功返回MYSQL指向的指针，失败返回NULL
        if( !mysql_real_connect( &mysql, host.c_str(), user.c_str(), passwd.c_str(), database.c_str(), 0, NULL, 0 ) )
        {
            cout << "connect error, line: " << __LINE__ << endl;
            exit(-1);
        }
    }

void MysqlDB::add(char* m_id,char* m_name,char* m_sex,char* m_birthday)
{
    string id=m_id, name=m_name, sex=m_sex, birthday=m_birthday;
    string sql = "insert into student values('" + id + "', '" + name + "', '" + sex + "', '" + birthday + "');";
    cout << sql << endl;
    mysql_query( &mysql, sql.c_str() );
}

void MysqlDB::del(char* m_id)
{
    string id=m_id;
    string sql = "delete from student where id='" + id +"';";
    cout << sql << endl;
    mysql_query( &mysql, sql.c_str() );
}

void MysqlDB::update(char* m_id,char* m_filed,char* m_value)
{
    string id=m_id, filed=m_filed,value=m_value;
    string sql = "update student set " + filed +"='" + value + "' where ID='" + id + "';";
    cout << sql << endl;
    mysql_query( &mysql, sql.c_str());
}

void MysqlDB::print()
{
    // string sql = "select * from info where name = '" + name + "';";  //要有''
    string sql = "select * from student;";
    //成功返回0
    mysql_query( &mysql, sql.c_str() );
    //获取查询查询结果；成功返回result的指针，失败返回NULL
    result = mysql_store_result( &mysql );
    if( !result )
    {
        cout << "result error, line : " << __LINE__ << endl;
        return ;
    }

    int num;
    num = mysql_num_fields( result );  //返回字段个数
    for( int i = 0; i < num; i++ )
    {
        field = mysql_fetch_field_direct( result, i );  //返回字段类型
        cout << field->name << "\t\t";  //输出字段名
    }
    cout << endl;

    while( row = mysql_fetch_row( result ), row  != NULL )
    {
        for( int i = 0; i < num; i++ )
        {
            cout << row[i] << "\t\t";
        }
        cout << endl;
    }
}

void MysqlDB::write_table(){
    // string sql = "select * from info where name = '" + name + "';";  //要有''
    string sql = "select * from student;";
    //成功返回0
    mysql_query( &mysql, sql.c_str() );
    //获取查询查询结果；成功返回result的指针，失败返回NULL
    result = mysql_store_result( &mysql );
    if( !result )
    {
        cout << "result error, line : " << __LINE__ << endl;
        return;
    }
    char *table=new char[1000];
    char* cur=table;
    int num;
    num = mysql_num_fields( result );  //返回字段个数
    for( int i = 0; i < num; i++ )
    {
        field = mysql_fetch_field_direct( result, i );  //返回字段类型
        strcpy(cur,field->name);
        cur+=strlen(field->name);
        strcpy(cur,"\t\t");
        cur+=strlen("\t\t");
    }
    strcpy(cur,"\n");
    cur+=strlen("\n");

    while( row = mysql_fetch_row( result ), row  != NULL )
    {
        for( int i = 0; i < num; i++ )
        {
            strcpy(cur,row[i]);
            cur+=strlen(row[i]);
            strcpy(cur,"\t\t");
            cur+=strlen("\t\t");
        }
        strcpy(cur,"\n");
        cur+=strlen("\n");
    }
    ofstream outfile;
	outfile.open("var/www/html/show.html");
    outfile << table;
    outfile.close();
    delete []table;
}
