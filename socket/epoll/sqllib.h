#ifndef MYSQL_H_INCLUDED
#define MYSQL_H_INCLUDED

#include <iostream>
#include <mysql/mysql.h>

#include <mysql_connection.h>  
#include <mysql_driver.h>  
#include <cppconn/driver.h>  
#include <cppconn/exception.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>
#include <cppconn/prepared_statement.h>

using namespace std;
using namespace sql;

#define SQL_ADDRESS			("tcp://localhost:3306")
#define SQL_USER			("root")
#define SQL_PASSWORD		("333333")

typedef struct
{
	string name;
	string pwd;
}login_info;

typedef struct
{
	string name;
	string pwd;
	string email;
	string sex;
	string age;
}register_info;

class WeSQL
{
public:
	Driver *driver;
	Connection *conn;
	Statement *stmt;
	ResultSet *res;
	PreparedStatement *pstmt;
	
	WeSQL()
	{
		driver = get_driver_instance();
		conn = driver->connect(SQL_ADDRESS, SQL_USER, SQL_PASSWORD); 
		conn->setSchema("bt");
		stmt = conn->createStatement();
	};
	bool Create_table()
	{
		stmt = conn->createStatement();
		stmt->execute("DROP TABLE IF EXISTS userinfo");
		stmt->execute("CREATE TABLE userinfo(id int primary key, name varchar(20), pwd varchar(20), email varchar(50), sex varchar(2), age varchar(2))");
		stmt->execute("alter table userinfo change id id int auto_increment;");
		return true;
	};
	bool Login(login_info& usr)
	{
		pstmt = conn->prepareStatement("SELECT name, pwd FROM userinfo where name=(?) and pwd=(?)");
		pstmt->setString(1, usr.name);
		pstmt->setString(2, usr.pwd);
		res = pstmt->executeQuery();

		while (res->next())		// 如果存在此name & pwd的用户,则返回true
			if(NULL != res)
				return true;
		return false;
	};
	bool Register(register_info& usr)
	{
		// 确认此用户名或邮箱是否已经注册过
		pstmt = conn->prepareStatement(("SELECT name, email FROM userinfo where name=(?) or email=(?)"));
		pstmt->setString(1, usr.name);
		pstmt->setString(2, usr.email);
		res = pstmt->executeQuery();
		while (res->next())	
			if(NULL != res)
				return false;	// 如果存在此name | email的用户,则返回false

		// 没有被注册过,添加此用户到数据库
		pstmt = conn->prepareStatement("INSERT INTO userinfo(id, name, pwd, email, sex, age) VALUES (?,?,?,?,?,?)");
	    pstmt->setInt(1,14);				// ???: 不录入id会报错,还需要录入id时自动获取自增id值
	    pstmt->setString(2, usr.name);
	    pstmt->setString(3, usr.pwd);
	    pstmt->setString(4, usr.email);
	    pstmt->setString(5, usr.sex);
	    pstmt->setString(6, usr.age);
	    pstmt->executeUpdate();

		return true;
	};

	~WeSQL(){	cout<<"析构函数执行完毕"<<endl;};
};

WeSQL wesql;

#endif